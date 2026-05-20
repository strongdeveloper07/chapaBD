# chapaBD

Реляционная СУБД, реализованная на C++17 с нуля. Поддерживает полный SQL-синтаксис, TCP-клиент/сервер архитектуру, бинарное хранилище данных, систему ролей и встроенный веб-интерфейс.

---

## Быстрый старт

```bash
# Сборка
./build.sh

# Запуск (порт по умолчанию 5432, веб-интерфейс на 6432)
./run.sh --host 127.0.0.1 --port 5432

# CLI клиент (в другом терминале)
./build/bin/chapadb_cli --host 127.0.0.1 --port 5432

# Веб-интерфейс — открыть в браузере: http://127.0.0.1:6432
```

---

## Архитектура

### Паттерн проектирования: Visitor

Исполнитель запросов (`Executor`) реализует интерфейс `ASTVisitor` и посещает узлы AST-дерева для выполнения соответствующих операций над хранилищем. Каждый тип SQL-оператора — отдельный узел AST с методом `accept(visitor)`.

**Почему Visitor**: позволяет добавлять новые операции над AST (оптимизатор, планировщик) без изменения существующих узлов дерева.

### Структура проекта

```
src/
├── common/       # Общие типы: DataType, Value, Row, QueryResult
├── lexer/        # Лексер: токенизация SQL → поток токенов
├── parser/
│   ├── ast/      # ASTNode, ASTVisitor, все Statement-классы
│   └── Parser    # Рекурсивно-нисходящий парсер → AST
├── executor/     # Executor: реализует ASTVisitor, выполняет запросы
├── storage/      # StorageEngine: бинарный формат файлов на диске
├── catalog/      # Catalog (Singleton): управляет StorageEngine + активная БД
├── auth/         # AuthManager (Singleton): пользователи, роли, привилегии
├── network/      # TCP-сервер (Server, Session)
├── web/          # Встроенный HTTP-сервер + REST API + HTML/JS UI
└── main.cpp      # Точка входа сервера
lib/              # C++ клиентская библиотека (libchapadb.so)
client/           # CLI клиент (chapadb_cli)
```

### Бинарный формат хранилища

- `data/catalog.dat` — список всех баз данных
- `data/<db>/schema.dat` — схема таблиц базы данных
- `data/<db>/<table>.dat` — данные таблицы (soft-delete через флаг)
- `data/users.dat` — пользователи и привилегии

---

## MVP: поддерживаемый SQL

### DDL

```sql
CREATE DATABASE mydb;
DROP DATABASE mydb;
CREATE TABLE users (id INT, name VARCHAR(50), age INT, active BOOL, score FLOAT);
DROP TABLE users;
USE mydb;
```

### DML

```sql
-- INSERT (одна или несколько строк)
INSERT INTO users VALUES (1, 'Alice', 30, TRUE, 9.5), (2, 'Bob', 25, FALSE, 7.2);
INSERT INTO users (id, name) VALUES (3, 'Charlie');

-- SELECT с проекцией и WHERE
SELECT * FROM users;
SELECT id, name FROM users WHERE age > 18 AND active = TRUE;
SELECT * FROM users WHERE NOT (score < 5 OR score > 10);

-- UPDATE с WHERE
UPDATE users SET score = 10.0, active = TRUE WHERE id = 1;

-- DELETE с WHERE
DELETE FROM users WHERE active = FALSE;
```

---

## Post-MVP: расширенный SQL

### ORDER BY / LIMIT / OFFSET

```sql
SELECT * FROM users ORDER BY age DESC, name ASC;
SELECT * FROM products ORDER BY price ASC LIMIT 10 OFFSET 20;
```

### Агрегатные функции и GROUP BY

```sql
SELECT COUNT(*) FROM users;
SELECT SUM(score), AVG(age), MIN(age), MAX(age) FROM users;
SELECT department, COUNT(*), AVG(salary) FROM employees GROUP BY department;
```

### INNER JOIN

```sql
SELECT name, total FROM users INNER JOIN orders ON user_id = id WHERE total > 100;
```

---

## Post-MVP: система ролей

```sql
-- Управление пользователями
CREATE USER alice WITH PASSWORD 'secret' ROLE READONLY;
CREATE USER bob WITH PASSWORD 'pass' ROLE READWRITE;
DROP USER bob;
SHOW USERS;

-- Аутентификация в текущей сессии
AUTH alice 'secret';

-- Привилегии
GRANT ALL ON tablename TO alice;
GRANT SELECT, INSERT ON orders TO bob;
REVOKE DELETE ON orders FROM bob;
```

| Роль | Права |
|------|-------|
| ADMIN | Все операции |
| READWRITE | SELECT, INSERT, UPDATE, DELETE |
| READONLY | Только SELECT |

---

## Post-MVP: веб-интерфейс

При запуске сервера автоматически стартует веб-интерфейс:

```bash
./run.sh --port 5432
# Веб-интерфейс: http://localhost:6432
```

**Возможности:**
- Обозреватель баз данных и таблиц с деревом схем
- SQL-редактор с горячей клавишей `Ctrl+Enter`
- Отображение результатов в виде таблицы
- REST API: `POST /api/query`, `GET /api/databases`, `GET /api/tables?db=name`

---

## Скрипты

### build.sh

Определяет ОС → устанавливает зависимости (g++, cmake, make) → собирает проект.

**Артефакты в `build/`:**
- `build/bin/chapadb_server` — сервер СУБД
- `build/bin/chapadb_cli` — CLI клиент  
- `build/lib/libchapadb.so` — клиентская динамическая библиотека

### run.sh

```bash
./run.sh [--host HOST] [--port PORT] [--data DIR] [--web-port PORT] [--no-web]
```

Если бинарник отсутствует — автоматически запускает `build.sh`.

---

## C++ клиентская библиотека

```cpp
#include <chapadb/Client.h>

chapadb::Client client;
client.connect("127.0.0.1", 5432);

auto result = client.execute("SELECT * FROM users WHERE age > 18 ORDER BY name;");
if (result.success) {
    for (const auto& row : result.rows) {
        // row.values — вектор Value (variant<monostate, int32_t, double, bool, string>)
    }
}
client.disconnect();
```

---

## Зависимости

Только стандартная библиотека C++17 и POSIX API. Внешних зависимостей нет.
