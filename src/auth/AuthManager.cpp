#include "AuthManager.h"
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <sstream>

namespace chapadb {

std::string AuthManager::hashPassword(const std::string& password) {
    const std::string salt = "chapaBD_2024_salt";
    std::string salted = salt + password + salt;
    uint64_t hash = 5381;
    for (unsigned char c : salted) {
        hash = ((hash << 5) + hash) ^ c;
    }
    std::ostringstream oss;
    oss << std::hex << hash;
    return oss.str();
}

AuthManager& AuthManager::instance() {
    static AuthManager inst;
    return inst;
}

void AuthManager::init(const std::string& dataDir) {
    m_dataDir = dataDir;
    load();
    // Создаём пользователя admin если нет ни одного
    if (m_users.empty()) {
        UserInfo admin;
        admin.name         = "admin";
        admin.passwordHash = hashPassword("admin");
        admin.role         = UserRole::ADMIN;
        m_users.push_back(std::move(admin));
        save();
    }
}

// ── Бинарный формат users.dat ─────────────────────────────────────────
// [uint32_t] magic=0xCBD4
// [uint32_t] count
// для каждого пользователя:
//   [str]     name
//   [str]     passwordHash
//   [uint8_t] role
//   [uint32_t] privCount
//   для каждой привилегии:
//     [str]     db
//     [str]     table
//     [uint8_t] flags (sel|ins|upd|del)

static void writeU8 (std::ostream& os, uint8_t  v) { os.write(reinterpret_cast<const char*>(&v), 1); }
static void writeU32(std::ostream& os, uint32_t v) { os.write(reinterpret_cast<const char*>(&v), 4); }
static uint8_t  readU8 (std::istream& is) { uint8_t  v; is.read(reinterpret_cast<char*>(&v), 1); return v; }
static uint32_t readU32(std::istream& is) { uint32_t v; is.read(reinterpret_cast<char*>(&v), 4); return v; }

static void writeStr(std::ostream& os, const std::string& s) {
    uint32_t len = static_cast<uint32_t>(s.size());
    writeU32(os, len);
    os.write(s.data(), len);
}
static std::string readStr(std::istream& is) {
    uint32_t len = readU32(is);
    std::string s(len, '\0');
    is.read(&s[0], len);
    return s;
}

void AuthManager::save() const {
    std::string path = m_dataDir + "/users.dat";
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs) throw std::runtime_error("Не удалось открыть файл пользователей для записи");

    writeU32(ofs, 0xCBD4);
    writeU32(ofs, static_cast<uint32_t>(m_users.size()));
    for (const auto& u : m_users) {
        writeStr(ofs, u.name);
        writeStr(ofs, u.passwordHash);
        writeU8(ofs, static_cast<uint8_t>(u.role));
        writeU32(ofs, static_cast<uint32_t>(u.privileges.size()));
        for (const auto& p : u.privileges) {
            writeStr(ofs, p.db);
            writeStr(ofs, p.table);
            uint8_t flags = 0;
            if (p.canSelect) flags |= 0x01;
            if (p.canInsert) flags |= 0x02;
            if (p.canUpdate) flags |= 0x04;
            if (p.canDelete) flags |= 0x08;
            writeU8(ofs, flags);
        }
    }
}

void AuthManager::load() {
    std::string path = m_dataDir + "/users.dat";
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return;

    uint32_t magic = readU32(ifs);
    if (magic != 0xCBD4) return;

    uint32_t count = readU32(ifs);
    m_users.clear();
    m_users.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        UserInfo u;
        u.name         = readStr(ifs);
        u.passwordHash = readStr(ifs);
        u.role         = static_cast<UserRole>(readU8(ifs));
        uint32_t privCount = readU32(ifs);
        for (uint32_t p = 0; p < privCount; ++p) {
            TablePrivilege tp;
            tp.db    = readStr(ifs);
            tp.table = readStr(ifs);
            uint8_t flags = readU8(ifs);
            tp.canSelect = (flags & 0x01) != 0;
            tp.canInsert = (flags & 0x02) != 0;
            tp.canUpdate = (flags & 0x04) != 0;
            tp.canDelete = (flags & 0x08) != 0;
            u.privileges.push_back(std::move(tp));
        }
        m_users.push_back(std::move(u));
    }
}

// ── API ───────────────────────────────────────────────────────────────

bool AuthManager::authenticate(const std::string& username, const std::string& password) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string hash = hashPassword(password);
    for (const auto& u : m_users) {
        if (u.name == username && u.passwordHash == hash) return true;
    }
    return false;
}

bool AuthManager::userExists(const std::string& username) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& u : m_users) {
        if (u.name == username) return true;
    }
    return false;
}

void AuthManager::createUser(const std::string& username, const std::string& password, UserRole role) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& u : m_users) {
        if (u.name == username)
            throw std::runtime_error("Пользователь '" + username + "' уже существует");
    }
    UserInfo u;
    u.name         = username;
    u.passwordHash = hashPassword(password);
    u.role         = role;
    m_users.push_back(std::move(u));
    save();
}

void AuthManager::dropUser(const std::string& username) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (username == "admin")
        throw std::runtime_error("Нельзя удалить пользователя admin");
    auto it = std::remove_if(m_users.begin(), m_users.end(),
                              [&](const UserInfo& u){ return u.name == username; });
    if (it == m_users.end())
        throw std::runtime_error("Пользователь '" + username + "' не найден");
    m_users.erase(it, m_users.end());
    save();
}

std::vector<UserInfo> AuthManager::listUsers() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_users;
}

UserRole AuthManager::getRole(const std::string& username) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& u : m_users) {
        if (u.name == username) return u.role;
    }
    return UserRole::READONLY;
}

void AuthManager::grantPrivilege(const std::string& username, const std::string& db,
                                  const std::string& table,
                                  bool sel, bool ins, bool upd, bool del) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& u : m_users) {
        if (u.name != username) continue;
        for (auto& p : u.privileges) {
            if (p.db == db && p.table == table) {
                if (sel) p.canSelect = true;
                if (ins) p.canInsert = true;
                if (upd) p.canUpdate = true;
                if (del) p.canDelete = true;
                save();
                return;
            }
        }
        TablePrivilege tp;
        tp.db = db; tp.table = table;
        tp.canSelect = sel; tp.canInsert = ins;
        tp.canUpdate = upd; tp.canDelete = del;
        u.privileges.push_back(std::move(tp));
        save();
        return;
    }
    throw std::runtime_error("Пользователь '" + username + "' не найден");
}

void AuthManager::revokePrivilege(const std::string& username, const std::string& db,
                                   const std::string& table,
                                   bool sel, bool ins, bool upd, bool del) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& u : m_users) {
        if (u.name != username) continue;
        for (auto& p : u.privileges) {
            if (p.db == db && p.table == table) {
                if (sel) p.canSelect = false;
                if (ins) p.canInsert = false;
                if (upd) p.canUpdate = false;
                if (del) p.canDelete = false;
                save();
                return;
            }
        }
        return;
    }
    throw std::runtime_error("Пользователь '" + username + "' не найден");
}

bool AuthManager::checkPrivilege(const std::string& username, const std::string& db,
                                  const std::string& table,
                                  bool TablePrivilege::* field) const {
    for (const auto& u : m_users) {
        if (u.name != username) continue;
        if (u.role == UserRole::ADMIN) return true;
        for (const auto& p : u.privileges) {
            if ((p.db == db || p.db == "*") &&
                (p.table == table || p.table == "*")) {
                return p.*field;
            }
        }
        if (u.role == UserRole::READONLY && field == &TablePrivilege::canSelect) return true;
        return false;
    }
    return false;
}

bool AuthManager::canSelect(const std::string& u, const std::string& db, const std::string& t) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return checkPrivilege(u, db, t, &TablePrivilege::canSelect);
}
bool AuthManager::canInsert(const std::string& u, const std::string& db, const std::string& t) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return checkPrivilege(u, db, t, &TablePrivilege::canInsert);
}
bool AuthManager::canUpdate(const std::string& u, const std::string& db, const std::string& t) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return checkPrivilege(u, db, t, &TablePrivilege::canUpdate);
}
bool AuthManager::canDelete(const std::string& u, const std::string& db, const std::string& t) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return checkPrivilege(u, db, t, &TablePrivilege::canDelete);
}

} 
