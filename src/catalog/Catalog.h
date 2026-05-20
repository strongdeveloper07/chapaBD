#pragma once
#include "../storage/StorageEngine.h"
#include <string>
#include <memory>
#include <mutex>

namespace chapadb {

/**
 * <summary>
 * Singleton-каталог: хранит ссылку на StorageEngine и текущую активную БД.
 * Все операции с метаданными проходят через Catalog.
 * </summary>
 */
class Catalog {
public:
    static Catalog& instance();

    void init(const std::string& dataDir);

    StorageEngine& storage();

    // Текущая активная база данных
    const std::string& currentDatabase() const;
    void               setCurrentDatabase(const std::string& name);
    bool               hasCurrentDatabase() const;

private:
    Catalog() = default;
    Catalog(const Catalog&) = delete;
    Catalog& operator=(const Catalog&) = delete;

    std::unique_ptr<StorageEngine> m_storage;
    std::string                    m_currentDb;
    mutable std::mutex             m_mutex;
};

} // namespace chapadb
