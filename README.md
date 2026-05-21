<div align="center">

```
 ██████╗██╗  ██╗ █████╗ ██████╗  █████╗ ██████╗ ██████╗ 
██╔════╝██║  ██║██╔══██╗██╔══██╗██╔══██╗██╔══██╗██╔══██╗
██║     ███████║███████║██████╔╝███████║██████╔╝██║  ██║
██║     ██╔══██║██╔══██║██╔═══╝ ██╔══██║██╔══██╗██║  ██║
╚██████╗██║  ██║██║  ██║██║     ██║  ██║██████╔╝██████╔╝
 ╚═════╝╚═╝  ╚═╝╚═╝  ╚═╝╚═╝     ╚═╝  ╚═╝╚═════╝ ╚═════╝
```

**Реляционная СУБД, написанная с нуля на C++17**

[![C++17](https://img.shields.io/badge/C++-17-00599C?style=flat&logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![CMake](https://img.shields.io/badge/CMake-3.14+-064F8C?style=flat&logo=cmake&logoColor=white)](CMakeLists.txt)
[![Tests](https://img.shields.io/badge/тесты-85%20passing-brightgreen?style=flat&logo=googletest)](tests/)
[![Pattern](https://img.shields.io/badge/паттерн-Visitor-8A2BE2?style=flat)](src/executor/Executor.h)
[![Storage](https://img.shields.io/badge/хранилище-бинарный%20формат-orange?style=flat)](src/storage/)

[Быстрый старт](#-быстрый-старт) · [SQL](#-поддерживаемый-sql) · [Архитектура](#-архитектура) · [Тесты](#-тестирование)

</div>

---

<div align="center">
  <img src="docs/demo.svg" alt="chapaBD demo" width="780">
</div>

---

## ✨ Возможности

<table>
<tr>
<td width="50%">

**Ядро СУБД**
- Полный DDL: `CREATE / DROP DATABASE / TABLE`
- Полный DML: `SELECT / INSERT / UPDATE / DELETE`
- `WHERE` с `AND / OR / NOT` и всеми операторами сравнения
- Постоянное бинарное хранилище с soft-delete

</td>
<td width="50%">

**Инфраструктура**
- TCP-сервер + C++ клиентская библиотека (`libchapadb.so`)
- Интерактивный CLI с ASCII-таблицами
- Встроенный HTTP-сервер с веб-редактором SQL
- Паттерн **Visitor** в исполнителе запросов

</td>
</tr>
<tr>
<td>

**Расширенный SQL**
- `ORDER BY`, `LIMIT`, `OFFSET`
- Агрегаты: `COUNT`, `SUM`, `AVG`, `MIN`, `MAX`
- `GROUP BY`, `HAVING`, `INNER JOIN`

</td>
<td>

**Система ролей**
- Пользователи с паролями и ролями (`ADMIN / READWRITE / READONLY`)
- `GRANT / REVOKE` привилегий на таблицы
- Аутентификация через `AUTH`

</td>
</tr>
</table>

---

## 🚀 Быстрый старт

```bash
# 1. Сборка (определяет ОС, устанавливает зависимости, собирает проект)
./build.sh

# 2. Запуск сервера (TCP :5432 + веб-интерфейс :6432)
./run.sh --host 127.0.0.1 --port 5432

# 3. CLI клиент — в другом терминале
./build/bin/chapadb_cli --host 127.0.0.1 --port 5432

# 4. Веб-интерфейс — открыть в браузере
open http://127.0.0.1:6432
```

`run.sh` автоматически запустит `build.sh`, если бинарник отсутствует.

**Артефакты сборки в `build/`:**

| Файл | Описание |
|------|----------|
| `bin/chapadb_server` | Исполняемый файл СУБД |
| `bin/chapadb_cli` | Интерактивный CLI клиент |
| `lib/libchapadb.so` | Динамическая клиентская библиотека |

---

## 📋 Поддерживаемый SQL

<details>
<summary><b>DDL — управление базами данных и таблицами</b></summary>

```sql
CREATE DATABASE mydb;
DROP DATABASE mydb;
USE mydb;

CREATE TABLE users (
    id     INT,
    name   VARCHAR(50),
    age    INT,
    active BOOL,
    score  FLOAT
);

DROP TABLE users;
```
</details>

<details>
<summary><b>DML — работа с данными</b></summary>

```sql
-- INSERT: одна или несколько строк за раз
INSERT INTO users VALUES (1, 'Alice', 30, TRUE, 9.5), (2, 'Bob', 25, FALSE, 7.2);
INSERT INTO users (id, name) VALUES (3, 'Charlie');

-- SELECT: проекция, WHERE, ORDER BY, LIMIT
SELECT * FROM users;
SELECT id, name FROM users WHERE age > 18 AND active = TRUE;
SELECT * FROM users ORDER BY age DESC LIMIT 10;
SELECT * FROM users WHERE NOT (score < 5 OR score > 10);

-- UPDATE с WHERE
UPDATE users SET score = 10.0, active = TRUE WHERE id = 1;

-- DELETE с WHERE
DELETE FROM users WHERE active = FALSE;
```
</details>

<details>
<summary><b>Агрегаты, GROUP BY, JOIN</b></summary>

```sql
-- Агрегатные функции
SELECT COUNT(*), AVG(age), MIN(score), MAX(score) FROM users;

-- GROUP BY + HAVING
SELECT department, COUNT(*), AVG(salary)
FROM employees
GROUP BY department
HAVING AVG(salary) > 50000;

-- INNER JOIN
SELECT name, total
FROM users INNER JOIN orders ON user_id = id
WHERE total > 100;
```
</details>

<details>
<summary><b>Управление пользователями и ролями</b></summary>

```sql
-- Создание пользователей
CREATE USER alice WITH PASSWORD 'secret' ROLE READONLY;
CREATE USER bob   WITH PASSWORD 'pass'   ROLE READWRITE;
DROP USER bob;
SHOW USERS;

-- Аутентификация в текущей сессии
AUTH alice 'secret';

-- Привилегии
GRANT ALL ON orders TO alice;
GRANT SELECT, INSERT ON orders TO bob;
REVOKE DELETE ON orders FROM bob;
```

| Роль | Права |
|------|-------|
| `ADMIN` | Все операции + управление пользователями |
| `READWRITE` | `SELECT`, `INSERT`, `UPDATE`, `DELETE` |
| `READONLY` | Только `SELECT` |

</details>

---

## 🏗 Архитектура

### Паттерн проектирования: Visitor

Исполнитель запросов [`Executor`](src/executor/Executor.h) реализует интерфейс `ASTVisitor` и обходит узлы AST-дерева, выполняя соответствующие операции над хранилищем. Каждый SQL-оператор — отдельный узел с методом `accept(visitor)`.

```
SQL-строка
    │
    ▼
┌─────────┐    токены    ┌──────────┐    AST    ┌──────────┐    QueryResult
│  Lexer  │ ──────────► │  Parser  │ ────────► │ Executor │ ────────────►
└─────────┘             └──────────┘           └──────────┘
                                                    │
                                              ASTVisitor
                                                    │
                                               ┌────▼─────┐
                                               │ Catalog  │
                                               │(Singleton)│
                                               └────┬─────┘
                                                    │
                                            ┌───────▼────────┐
                                            │ StorageEngine  │
                                            │ (бинарный диск)│
                                            └────────────────┘
```

**Почему Visitor:** позволяет добавлять новые операции над AST (оптимизатор, планировщик) без изменения существующих классов узлов.

### Структура проекта

```
chapaBD/
├── src/
│   ├── common/     — Value, Row, QueryResult, DataType
│   ├── lexer/      — токенизатор SQL
│   ├── parser/
│   │   ├── ast/    — ASTNode, Statements (19 типов)
│   │   └── Parser  — рекурсивно-нисходящий парсер
│   ├── executor/   — Executor : ASTVisitor
│   ├── storage/    — бинарный движок (soft-delete)
│   ├── catalog/    — Singleton: активная БД + StorageEngine
│   ├── auth/       — пользователи, роли, привилегии
│   ├── network/    — TCP-сервер (POSIX sockets + threads)
│   └── web/        — HTTP-сервер + REST API + HTML/JS UI
├── lib/            — C++ клиентская библиотека (pimpl)
├── client/         — CLI клиент с ASCII-таблицами
└── tests/          — Google Test: 85 тестов
```

### Бинарный формат хранилища

```
data/
├── catalog.dat          — список всех баз данных
├── users.dat            — пользователи и привилегии
└── <db>/
    ├── schema.dat       — схема таблиц
    └── <table>.dat      — строки (флаг deleted = soft-delete)
```

---

## 🧪 Тестирование

Проект покрыт **85 Google Tests** в 4 наборах:

| Набор | Тестов | Что проверяется |
|-------|--------|-----------------|
| `test_lexer` | 20 | токенизация, ключевые слова, литералы, escape, ошибки |
| `test_parser` | 27 | DDL/DML AST, все типы значений, зарезервированные слова как идентификаторы |
| `test_executor` | 31 | полный цикл CREATE→INSERT→SELECT→UPDATE→DELETE, WHERE-условия |
| `test_persistence` | 7 | данные выживают после перезапуска (MVP-критерий #1) |

```bash
# Сборка и запуск тестов
./build.sh
build/bin/test_lexer
build/bin/test_parser
build/bin/test_executor
build/bin/test_persistence
```

---

## 📦 C++ клиентская библиотека

```cpp
#include <chapadb/Client.h>

chapadb::Client client;
client.connect("127.0.0.1", 5432);

auto result = client.execute("SELECT * FROM users WHERE age > 18;");
if (result.success) {
    for (const auto& row : result.rows) {
        // row.values — вектор Value (variant<monostate, int32_t, double, bool, string>)
    }
}

client.disconnect();
```

Клиент использует **pimpl**-паттерн: детали TCP-соединения скрыты за непрозрачным указателем, ABI стабилен.

---

## ⚙️ Зависимости

Только стандартная библиотека **C++17** и **POSIX API**. Внешних зависимостей нет. Google Test скачивается автоматически через CMake FetchContent при сборке тестов.
