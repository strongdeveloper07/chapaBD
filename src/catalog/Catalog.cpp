#include "Catalog.h"
#include <stdexcept>

namespace chapadb {

Catalog& Catalog::instance() {
    static Catalog cat;
    return cat;
}

void Catalog::init(const std::string& dataDir) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_storage = std::make_unique<StorageEngine>(dataDir);
}

StorageEngine& Catalog::storage() {
    if (!m_storage) throw std::runtime_error("Каталог не инициализирован");
    return *m_storage;
}

const std::string& Catalog::currentDatabase() const {
    return m_currentDb;
}

void Catalog::setCurrentDatabase(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentDb = name;
}

bool Catalog::hasCurrentDatabase() const {
    return !m_currentDb.empty();
}

}
