#pragma once
#include <string>
#include <vector>
#include <mutex>

namespace chapadb {

enum class UserRole { ADMIN = 0, READWRITE = 1, READONLY = 2 };

/// Привилегия на конкретную таблицу
struct TablePrivilege {
    std::string db;
    std::string table;
    bool canSelect = false;
    bool canInsert = false;
    bool canUpdate = false;
    bool canDelete = false;
};

struct UserInfo {
    std::string name;
    std::string passwordHash; // хэш пароля 
    UserRole    role = UserRole::READWRITE;
    std::vector<TablePrivilege> privileges;
};

/**
 * <summary>
 * Singleton. Управляет пользователями, паролями и правами доступа.
 * Данные хранятся в data/users.dat.
 * </summary>
 */
class AuthManager {
public:
    static AuthManager& instance();

    void init(const std::string& dataDir);

    /// Аутентификация: возвращает true если пароль верный
    bool authenticate(const std::string& username, const std::string& password) const;

    /// Создание нового пользователя
    void createUser(const std::string& username, const std::string& password, UserRole role);

    /// Удаление пользователя
    void dropUser(const std::string& username);

    /// Список всех пользователей
    std::vector<UserInfo> listUsers() const;

    /// Проверяет существование пользователя
    bool userExists(const std::string& username) const;

    /// Выдаёт привилегию на таблицу
    void grantPrivilege(const std::string& username, const std::string& db,
                        const std::string& table, bool sel, bool ins, bool upd, bool del);

    /// Отзывает привилегию на таблицу
    void revokePrivilege(const std::string& username, const std::string& db,
                         const std::string& table, bool sel, bool ins, bool upd, bool del);

    /// Проверяет право доступа для пользователя
    bool canSelect(const std::string& username, const std::string& db, const std::string& table) const;
    bool canInsert(const std::string& username, const std::string& db, const std::string& table) const;
    bool canUpdate(const std::string& username, const std::string& db, const std::string& table) const;
    bool canDelete(const std::string& username, const std::string& db, const std::string& table) const;

    /// Возвращает роль пользователя (или READONLY если не найден)
    UserRole getRole(const std::string& username) const;

    static std::string hashPassword(const std::string& password);

private:
    AuthManager() = default;
    AuthManager(const AuthManager&) = delete;
    AuthManager& operator=(const AuthManager&) = delete;

    void save() const;
    void load();

    bool checkPrivilege(const std::string& username, const std::string& db,
                        const std::string& table,
                        bool TablePrivilege::* field) const;

    std::string              m_dataDir;
    std::vector<UserInfo>    m_users;
    mutable std::mutex       m_mutex;
};

} 
