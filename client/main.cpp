#include "../lib/include/chapadb/Client.h"
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cstdlib>

// ─────────────────────────── ASCII-таблица ───────────────────────────

/// Рисует горизонтальную разделительную линию таблицы
static void printTableLine(const std::vector<size_t>& widths) {
    std::cout << '+';
    for (size_t w : widths) {
        std::cout << std::string(w + 2, '-') << '+';
    }
    std::cout << '\n';
}

/// Рисует строку данных таблицы
static void printTableRow(const std::vector<std::string>& cells,
                           const std::vector<size_t>& widths) {
    std::cout << '|';
    for (size_t i = 0; i < cells.size(); ++i) {
        std::string cell = (i < widths.size()) ? cells[i] : "";
        // Правое выравнивание для чисел, левое — для строк
        bool isNum = !cell.empty() &&
                     (std::isdigit(static_cast<unsigned char>(cell[0])) || cell[0] == '-');
        if (isNum) {
            std::cout << ' ' << std::right << std::setw(static_cast<int>(widths[i])) << cell << ' ';
        } else {
            std::cout << ' ' << std::left  << std::setw(static_cast<int>(widths[i])) << cell << ' ';
        }
        std::cout << '|';
    }
    std::cout << '\n';
}

/// Выводит результат SELECT в виде ASCII-таблицы
static void printResultTable(const chapadb::ClientResult& res) {
    if (res.columns.empty()) return;

    size_t numCols = res.columns.size();
    std::vector<size_t> widths(numCols);

    // Ширина = max(длина заголовка, длина самого длинного значения)
    for (size_t c = 0; c < numCols; ++c) {
        widths[c] = res.columns[c].size();
    }
    for (const auto& row : res.rows) {
        for (size_t c = 0; c < numCols && c < row.values.size(); ++c) {
            widths[c] = std::max(widths[c], row.values[c].size());
        }
    }

    printTableLine(widths);
    printTableRow(res.columns, widths);
    printTableLine(widths);

    for (const auto& row : res.rows) {
        std::vector<std::string> cells;
        for (size_t c = 0; c < numCols; ++c) {
            cells.push_back(c < row.values.size() ? row.values[c] : "");
        }
        printTableRow(cells, widths);
    }
    printTableLine(widths);
    std::cout << res.rows.size() << " row(s) in set\n";
}

// ─────────────────────────── CLI ─────────────────────────────────────

static void printHelp() {
    std::cout << "Команды:\n"
              << "  \\q, exit  - выход\n"
              << "  \\h, help  - эта справка\n"
              << "  SQL-запрос заканчивается на ';'\n";
}

static void handleResult(const chapadb::ClientResult& res) {
    if (!res.success) {
        std::cerr << "ERROR: " << res.errorMessage << '\n';
        return;
    }

    if (!res.columns.empty()) {
        printResultTable(res);
    } else if (res.affectedRows > 0 || !res.message.empty()) {
        if (!res.message.empty()) {
            std::cout << "Query OK: " << res.message << '\n';
        } else {
            std::cout << "Affected rows: " << res.affectedRows << '\n';
        }
    } else {
        std::cout << "OK\n";
    }
}

int main(int argc, char* argv[]) {
    std::string host = "localhost";
    int         port = 5432;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) host = argv[++i];
        else if (arg == "--port" && i + 1 < argc) port = std::stoi(argv[++i]);
    }

    std::cout << "chapaBD CLI v1.0.0\n"
              << "Подключение к " << host << ":" << port << "...\n";

    chapadb::Client client;
    if (!client.connect(host, port)) {
        std::cerr << "Не удалось подключиться к серверу " << host << ":" << port << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "Подключено. Введите SQL-запрос (завершите ';') или \\q для выхода.\n\n";

    std::string buffer;
    bool        multiline = false;

    while (true) {
        if (multiline) std::cout << "      -> ";
        else           std::cout << "chapaBD> ";
        std::cout.flush();

        std::string line;
        if (!std::getline(std::cin, line)) break; // EOF

        // Обрезаем пробелы с обоих концов
        size_t start = line.find_first_not_of(" \t\r");
        size_t end   = line.find_last_not_of(" \t\r");
        if (start == std::string::npos) {
            if (buffer.empty()) continue;
            line = "";
        } else {
            line = line.substr(start, end - start + 1);
        }

        // Специальные команды
        if (!multiline && (line == "\\q" || line == "exit" || line == "quit")) {
            std::cout << "До свидания!\n";
            break;
        }
        if (!multiline && (line == "\\h" || line == "help")) {
            printHelp();
            continue;
        }
        if (line.empty() && buffer.empty()) continue;

        if (!buffer.empty()) buffer += ' ';
        buffer += line;

        // Проверяем, завершён ли запрос (заканчивается на ';')
        size_t semPos = buffer.rfind(';');
        if (semPos == std::string::npos) {
            multiline = true;
            continue;
        }

        // Выполняем запрос
        std::string sql = buffer.substr(0, semPos + 1);
        buffer.clear();
        multiline = false;

        if (sql.empty() || sql == ";") continue;

        auto result = client.execute(sql);
        handleResult(result);
        std::cout << '\n';
    }

    client.disconnect();
    return EXIT_SUCCESS;
}
