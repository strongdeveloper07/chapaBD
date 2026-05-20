#include "WebApi.h"
#include "../catalog/Catalog.h"
#include "../lexer/Lexer.h"
#include "../parser/Parser.h"
#include "../executor/Executor.h"
#include <sstream>
#include <variant>

namespace chapadb {

std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

std::string valueToJson(const Value& v) {
    return std::visit([](const auto& val) -> std::string {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, std::monostate>) return "null";
        if constexpr (std::is_same_v<T, bool>)  return val ? "true" : "false";
        if constexpr (std::is_same_v<T, int32_t>) return std::to_string(val);
        if constexpr (std::is_same_v<T, double>) {
            std::ostringstream oss;
            oss << val;
            return oss.str();
        }
        if constexpr (std::is_same_v<T, std::string>) return "\"" + jsonEscape(val) + "\"";
        return "null";
    }, v);
}

std::string resultToJson(const QueryResult& result) {
    if (!result.success) {
        return "{\"error\":\"" + jsonEscape(result.errorMessage) + "\"}";
    }

    // SELECT-результат
    if (!result.columns.empty()) {
        std::string json = "{\"columns\":[";
        for (size_t i = 0; i < result.columns.size(); ++i) {
            if (i) json += ",";
            json += "\"" + jsonEscape(result.columns[i]) + "\"";
        }
        json += "],\"rows\":[";
        for (size_t r = 0; r < result.rows.size(); ++r) {
            if (r) json += ",";
            json += "[";
            const auto& row = result.rows[r];
            for (size_t c = 0; c < row.values.size(); ++c) {
                if (c) json += ",";
                json += valueToJson(row.values[c]);
            }
            json += "]";
        }
        json += "],\"count\":" + std::to_string(result.affectedRows) + "}";
        return json;
    }

    // DML-результат
    if (result.affectedRows > 0 && result.message.empty()) {
        return "{\"affected\":" + std::to_string(result.affectedRows) + "}";
    }

    // DDL-результат / USE
    return "{\"message\":\"" + jsonEscape(result.message) + "\","
           "\"affected\":" + std::to_string(result.affectedRows) + "}";
}

/// Выполняет один SQL запрос через Lexer → Parser → Executor
static QueryResult runQuery(const std::string& sql) {
    try {
        Lexer  lexer(sql);
        auto   tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        auto   ast = parser.parse();
        Executor exec;
        return exec.execute(*ast);
    } catch (const std::exception& e) {
        QueryResult r;
        r.success      = false;
        r.errorMessage = e.what();
        return r;
    }
}

void registerWebApi(HttpServer& server) {
    // OPTIONS (предзапрос CORS)
    server.on("OPTIONS", "*", [](const HttpRequest&) {
        HttpResponse r;
        r.status = 200;
        r.body   = "";
        return r;
    });

    // POST /api/query — выполнить SQL
    server.on("POST", "/api/query", [](const HttpRequest& req) {
        std::string sql;
        std::string db;

        auto extract = [&](const std::string& key) -> std::string {
            std::string search = "\"" + key + "\"";
            auto pos = req.body.find(search);
            if (pos == std::string::npos) return "";
            pos = req.body.find(':', pos + search.size());
            if (pos == std::string::npos) return "";
            ++pos;
            while (pos < req.body.size() && (req.body[pos] == ' ' || req.body[pos] == '\t')) ++pos;
            if (pos >= req.body.size()) return "";
            if (req.body[pos] == '"') {
                ++pos;
                std::string val;
                while (pos < req.body.size() && req.body[pos] != '"') {
                    if (req.body[pos] == '\\' && pos + 1 < req.body.size()) {
                        ++pos;
                        char esc = req.body[pos];
                        if      (esc == 'n')  val += '\n';
                        else if (esc == 't')  val += '\t';
                        else if (esc == '"')  val += '"';
                        else if (esc == '\\') val += '\\';
                        else val += esc;
                    } else {
                        val += req.body[pos];
                    }
                    ++pos;
                }
                return val;
            }
            return "";
        };

        sql = extract("sql");
        db  = extract("db");

        if (sql.empty()) {
            HttpResponse r; r.status = 400;
            r.body = "{\"error\":\"Поле sql обязательно\"}";
            return r;
        }

        // Устанавливаем активную БД если передана
        if (!db.empty() && db != Catalog::instance().currentDatabase()) {
            auto setDb = runQuery("USE " + db + ";");
            if (!setDb.success) {
                HttpResponse r; r.status = 400;
                r.body = resultToJson(setDb);
                return r;
            }
        }

        QueryResult result = runQuery(sql);
        HttpResponse resp;
        resp.status = result.success ? 200 : 400;
        resp.body   = resultToJson(result);
        return resp;
    });

    // GET /api/databases — список баз данных
    server.on("GET", "/api/databases", [](const HttpRequest&) {
        auto& storage = Catalog::instance().storage();
        auto dbs = storage.listDatabases();
        std::string current = Catalog::instance().currentDatabase();

        std::string json = "{\"databases\":[";
        for (size_t i = 0; i < dbs.size(); ++i) {
            if (i) json += ",";
            json += "{\"name\":\"" + jsonEscape(dbs[i]) + "\","
                    "\"current\":" + (dbs[i] == current ? "true" : "false") + "}";
        }
        json += "],\"current\":\"" + jsonEscape(current) + "\"}";

        HttpResponse resp;
        resp.body = json;
        return resp;
    });

    // GET /api/tables?db=mydb — список таблиц базы данных
    server.on("GET", "/api/tables", [](const HttpRequest& req) {
        // Извлекаем параметр db из пути
        std::string db;
        auto qpos = req.path.find('?');
        if (qpos != std::string::npos) {
            std::string query = req.path.substr(qpos + 1);
            auto dbpos = query.find("db=");
            if (dbpos != std::string::npos) {
                db = query.substr(dbpos + 3);
                auto amp = db.find('&');
                if (amp != std::string::npos) db = db.substr(0, amp);
            }
        }
        if (db.empty()) db = Catalog::instance().currentDatabase();
        if (db.empty()) {
            HttpResponse r; r.status = 400;
            r.body = "{\"error\":\"База данных не указана\"}";
            return r;
        }

        auto& storage = Catalog::instance().storage();
        auto tables = storage.loadSchema(db);

        std::string json = "{\"database\":\"" + jsonEscape(db) + "\",\"tables\":[";
        for (size_t i = 0; i < tables.size(); ++i) {
            if (i) json += ",";
            json += "{\"name\":\"" + jsonEscape(tables[i].name) + "\","
                    "\"columns\":[";
            const auto& cols = tables[i].columns;
            for (size_t c = 0; c < cols.size(); ++c) {
                if (c) json += ",";
                std::string typeName;
                switch (cols[c].type) {
                    case DataType::INT:     typeName = "INT";     break;
                    case DataType::FLOAT:   typeName = "FLOAT";   break;
                    case DataType::BOOL:    typeName = "BOOL";    break;
                    case DataType::TEXT:    typeName = "TEXT";    break;
                    case DataType::VARCHAR: typeName = "VARCHAR(" + std::to_string(cols[c].param) + ")"; break;
                }
                json += "{\"name\":\"" + jsonEscape(cols[c].name) + "\","
                        "\"type\":\"" + typeName + "\"}";
            }
            json += "]}";
        }
        json += "]}";

        HttpResponse resp;
        resp.body = json;
        return resp;
    });

    // GET / — отдаём HTML страницу
    server.on("GET", "/", [](const HttpRequest&) {
        HttpResponse resp;
        resp.contentType = "text/html";
        resp.body = R"html(<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>chapaBD — Веб-клиент</title>
<style>
:root {
  --bg: #0f1117;
  --surface: #1a1d2e;
  --surface2: #252840;
  --accent: #6c63ff;
  --accent2: #ff6b9d;
  --text: #e2e8f0;
  --text2: #94a3b8;
  --border: #2d3748;
  --success: #48bb78;
  --error: #fc8181;
  --warn: #f6ad55;
  --font: 'Segoe UI', system-ui, sans-serif;
  --mono: 'Cascadia Code', 'Fira Code', 'Consolas', monospace;
}
* { box-sizing: border-box; margin: 0; padding: 0; }
body { background: var(--bg); color: var(--text); font-family: var(--font); height: 100vh; display: flex; flex-direction: column; overflow: hidden; }

/* ── Шапка ── */
.header { background: var(--surface); border-bottom: 1px solid var(--border); padding: 12px 20px; display: flex; align-items: center; gap: 12px; flex-shrink: 0; }
.logo { font-size: 22px; font-weight: 700; background: linear-gradient(135deg, var(--accent), var(--accent2)); -webkit-background-clip: text; -webkit-text-fill-color: transparent; background-clip: text; letter-spacing: -0.5px; }
.logo span { font-weight: 400; font-size: 14px; -webkit-text-fill-color: var(--text2); }
.header-status { margin-left: auto; font-size: 13px; color: var(--text2); display: flex; align-items: center; gap: 8px; }
.dot { width: 8px; height: 8px; border-radius: 50%; background: var(--success); box-shadow: 0 0 6px var(--success); animation: pulse 2s infinite; }
@keyframes pulse { 0%,100%{opacity:1}50%{opacity:.5} }

/* ── Основной layout ── */
.main { display: flex; flex: 1; overflow: hidden; }

/* ── Левая панель ── */
.sidebar { width: 260px; background: var(--surface); border-right: 1px solid var(--border); display: flex; flex-direction: column; flex-shrink: 0; overflow: hidden; }
.sidebar-header { padding: 14px 16px; font-size: 12px; font-weight: 600; color: var(--text2); text-transform: uppercase; letter-spacing: 0.8px; border-bottom: 1px solid var(--border); display: flex; align-items: center; justify-content: space-between; }
.sidebar-body { flex: 1; overflow-y: auto; padding: 8px 0; }
.sidebar-body::-webkit-scrollbar { width: 4px; }
.sidebar-body::-webkit-scrollbar-thumb { background: var(--border); border-radius: 2px; }

.db-item { cursor: pointer; }
.db-header { display: flex; align-items: center; gap: 8px; padding: 8px 16px; font-size: 13px; font-weight: 500; color: var(--text); transition: background .15s; user-select: none; }
.db-header:hover { background: var(--surface2); }
.db-header.active { color: var(--accent); }
.db-icon { font-size: 15px; }
.db-arrow { margin-left: auto; font-size: 10px; color: var(--text2); transition: transform .2s; }
.db-arrow.open { transform: rotate(90deg); }
.db-tables { padding-left: 16px; }
.tbl-item { display: flex; align-items: center; gap: 8px; padding: 6px 16px 6px 8px; font-size: 12px; color: var(--text2); cursor: pointer; border-radius: 4px; transition: all .15s; }
.tbl-item:hover { background: var(--surface2); color: var(--text); }
.tbl-item.active { background: rgba(108,99,255,.15); color: var(--accent); }
.tbl-icon { font-size: 13px; }

.col-list { padding-left: 24px; }
.col-item { display: flex; align-items: center; gap: 6px; padding: 3px 8px; font-size: 11px; color: var(--text2); }
.col-type { font-size: 10px; background: var(--surface2); padding: 1px 5px; border-radius: 3px; color: var(--accent); font-family: var(--mono); }

.refresh-btn { background: none; border: none; cursor: pointer; color: var(--text2); font-size: 14px; padding: 2px 6px; border-radius: 4px; transition: all .15s; }
.refresh-btn:hover { color: var(--accent); background: var(--surface2); }

/* ── Правая часть ── */
.workspace { flex: 1; display: flex; flex-direction: column; overflow: hidden; }

/* ── Редактор ── */
.editor-panel { background: var(--surface); border-bottom: 1px solid var(--border); padding: 16px; flex-shrink: 0; }
.editor-label { font-size: 11px; font-weight: 600; color: var(--text2); text-transform: uppercase; letter-spacing: 0.8px; margin-bottom: 8px; display: flex; align-items: center; gap: 8px; }
.db-badge { background: rgba(108,99,255,.2); color: var(--accent); font-size: 10px; padding: 2px 8px; border-radius: 10px; }
.editor-wrap { position: relative; }
textarea#sql-input {
  width: 100%; min-height: 120px; max-height: 250px;
  background: var(--bg); color: var(--text);
  border: 1px solid var(--border); border-radius: 8px;
  padding: 12px; font-family: var(--mono); font-size: 13px; line-height: 1.6;
  resize: vertical; outline: none; transition: border-color .2s;
}
textarea#sql-input:focus { border-color: var(--accent); box-shadow: 0 0 0 3px rgba(108,99,255,.1); }
.editor-actions { display: flex; align-items: center; gap: 10px; margin-top: 10px; }
.btn { padding: 8px 18px; border-radius: 6px; border: none; cursor: pointer; font-size: 13px; font-weight: 500; transition: all .2s; display: flex; align-items: center; gap: 6px; }
.btn-primary { background: var(--accent); color: #fff; }
.btn-primary:hover { background: #5a52d5; transform: translateY(-1px); box-shadow: 0 4px 12px rgba(108,99,255,.3); }
.btn-primary:active { transform: translateY(0); }
.btn-secondary { background: var(--surface2); color: var(--text); border: 1px solid var(--border); }
.btn-secondary:hover { background: var(--border); }
.kbd { font-size: 11px; color: var(--text2); background: var(--surface2); padding: 2px 6px; border-radius: 4px; border: 1px solid var(--border); font-family: var(--mono); }
.exec-time { margin-left: auto; font-size: 12px; color: var(--text2); }

/* ── Результаты ── */
.results-panel { flex: 1; overflow: hidden; display: flex; flex-direction: column; }
.results-header { padding: 12px 16px; font-size: 12px; font-weight: 600; color: var(--text2); text-transform: uppercase; letter-spacing: 0.8px; border-bottom: 1px solid var(--border); background: var(--surface); display: flex; align-items: center; gap: 10px; flex-shrink: 0; }
.badge { padding: 2px 8px; border-radius: 10px; font-size: 11px; font-weight: 500; }
.badge-ok  { background: rgba(72,187,120,.15); color: var(--success); }
.badge-err { background: rgba(252,129,129,.15); color: var(--error); }
.badge-row { background: rgba(108,99,255,.15); color: var(--accent); }

.results-body { flex: 1; overflow: auto; padding: 16px; }
.results-body::-webkit-scrollbar { width: 6px; height: 6px; }
.results-body::-webkit-scrollbar-thumb { background: var(--border); border-radius: 3px; }

.result-table { width: 100%; border-collapse: collapse; font-size: 13px; }
.result-table th { background: var(--surface2); color: var(--text2); font-weight: 600; font-size: 11px; text-transform: uppercase; letter-spacing: 0.5px; padding: 10px 14px; text-align: left; border-bottom: 2px solid var(--border); white-space: nowrap; }
.result-table td { padding: 9px 14px; border-bottom: 1px solid var(--border); color: var(--text); white-space: nowrap; font-family: var(--mono); font-size: 12px; }
.result-table tr:hover td { background: var(--surface2); }
.result-table td.null-val { color: var(--text2); font-style: italic; }
.result-table td.num-val { color: #79c0ff; }
.result-table td.bool-val { color: var(--warn); }

.result-msg { display: flex; align-items: center; gap: 10px; padding: 16px; background: var(--surface2); border-radius: 8px; }
.result-msg.ok  { border-left: 3px solid var(--success); }
.result-msg.err { border-left: 3px solid var(--error); }
.result-msg-icon { font-size: 20px; }
.result-msg-text { font-size: 14px; }
.result-msg-sub  { font-size: 12px; color: var(--text2); margin-top: 4px; font-family: var(--mono); }

.empty-state { text-align: center; padding: 60px 20px; color: var(--text2); }
.empty-icon  { font-size: 48px; margin-bottom: 12px; }
.empty-text  { font-size: 14px; }
.empty-hint  { font-size: 12px; margin-top: 6px; font-family: var(--mono); color: var(--accent); }

/* Подсветка синтаксиса (упрощённая) */
</style>
</head>
<body>

<div class="header">
  <div class="logo">chapaBD <span>v1.0</span></div>
  <div class="header-status">
    <div class="dot" id="status-dot"></div>
    <span id="status-text">Подключён</span>
  </div>
</div>

<div class="main">
  <!-- Боковая панель -->
  <div class="sidebar">
    <div class="sidebar-header">
      Обозреватель
      <button class="refresh-btn" onclick="loadDatabases()" title="Обновить">⟳</button>
    </div>
    <div class="sidebar-body" id="db-tree">
      <div style="padding:20px;text-align:center;color:var(--text2);font-size:13px">Загрузка...</div>
    </div>
  </div>

  <!-- Рабочая область -->
  <div class="workspace">
    <div class="editor-panel">
      <div class="editor-label">
        SQL Редактор
        <span class="db-badge" id="current-db-badge">нет БД</span>
      </div>
      <div class="editor-wrap">
        <textarea id="sql-input" placeholder="Введите SQL запрос...&#10;&#10;Примеры:&#10;  SELECT * FROM users ORDER BY id DESC LIMIT 10;&#10;  SELECT COUNT(*), AVG(age) FROM users;&#10;  INSERT INTO users VALUES (1, 'Alice', 25);" spellcheck="false"></textarea>
      </div>
      <div class="editor-actions">
        <button class="btn btn-primary" onclick="executeQuery()">▶ Выполнить</button>
        <button class="btn btn-secondary" onclick="clearEditor()">✕ Очистить</button>
        <span style="font-size:12px;color:var(--text2)">или <span class="kbd">Ctrl+Enter</span></span>
        <div class="exec-time" id="exec-time"></div>
      </div>
    </div>

    <div class="results-panel">
      <div class="results-header">
        Результат
        <span class="badge" id="result-badge" style="display:none"></span>
      </div>
      <div class="results-body" id="results-body">
        <div class="empty-state">
          <div class="empty-icon">🗄️</div>
          <div class="empty-text">Введите SQL запрос и нажмите <b>Выполнить</b></div>
          <div class="empty-hint">SELECT * FROM table WHERE condition ORDER BY col LIMIT 10;</div>
        </div>
      </div>
    </div>
  </div>
</div>

<script>
let currentDb = '';

// ── Утилиты ──────────────────────────────────────────────────

function escHtml(s) {
  if (s === null || s === undefined) return '<span class="null-val">NULL</span>';
  return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}

function classifyValue(v) {
  if (v === null) return 'null-val';
  if (typeof v === 'number') return 'num-val';
  if (typeof v === 'boolean') return 'bool-val';
  return '';
}

function displayValue(v) {
  if (v === null) return 'NULL';
  if (typeof v === 'boolean') return v ? 'TRUE' : 'FALSE';
  return escHtml(String(v));
}

// ── Загрузка дерева БД ────────────────────────────────────────

async function loadDatabases() {
  try {
    const r = await fetch('/api/databases');
    const data = await r.json();
    currentDb = data.current || '';
    updateDbBadge();

    const tree = document.getElementById('db-tree');
    if (!data.databases || data.databases.length === 0) {
      tree.innerHTML = '<div style="padding:16px;text-align:center;color:var(--text2);font-size:12px">Нет баз данных.<br>Создайте: <code>CREATE DATABASE mydb;</code></div>';
      return;
    }

    tree.innerHTML = data.databases.map(db => `
      <div class="db-item" id="db-${db.name}">
        <div class="db-header ${db.name===currentDb?'active':''}" onclick="toggleDb('${db.name}')">
          <span class="db-icon">🗄️</span>
          <span>${escHtml(db.name)}</span>
          <span class="db-arrow" id="arrow-${db.name}">▶</span>
        </div>
        <div class="db-tables" id="tables-${db.name}" style="display:none"></div>
      </div>
    `).join('');

    // Автораскрываем активную БД
    if (currentDb) expandDb(currentDb);
  } catch(e) {
    document.getElementById('db-tree').innerHTML =
      '<div style="padding:16px;color:var(--error);font-size:12px">Ошибка загрузки</div>';
  }
}

async function expandDb(dbName) {
  const tablesDiv = document.getElementById('tables-' + dbName);
  if (!tablesDiv) return;
  tablesDiv.style.display = 'block';
  const arrow = document.getElementById('arrow-' + dbName);
  if (arrow) { arrow.classList.add('open'); }

  try {
    const r = await fetch('/api/tables?db=' + encodeURIComponent(dbName));
    const data = await r.json();
    if (!data.tables || data.tables.length === 0) {
      tablesDiv.innerHTML = '<div style="padding:6px 16px;font-size:11px;color:var(--text2)">Нет таблиц</div>';
      return;
    }
    tablesDiv.innerHTML = data.tables.map(tbl => `
      <div class="tbl-item" onclick="selectTable('${dbName}','${tbl.name}')">
        <span class="tbl-icon">📋</span>
        <span>${escHtml(tbl.name)}</span>
      </div>
      <div class="col-list" id="cols-${dbName}-${tbl.name}" style="display:none">
        ${tbl.columns.map(c => `
          <div class="col-item">
            <span>⚬</span>
            <span>${escHtml(c.name)}</span>
            <span class="col-type">${c.type}</span>
          </div>
        `).join('')}
      </div>
    `).join('');
  } catch(e) {}
}

function toggleDb(dbName) {
  const tablesDiv = document.getElementById('tables-' + dbName);
  if (!tablesDiv) return;
  if (tablesDiv.style.display === 'none') {
    setActiveDb(dbName);
    expandDb(dbName);
  } else {
    tablesDiv.style.display = 'none';
    const arrow = document.getElementById('arrow-' + dbName);
    if (arrow) arrow.classList.remove('open');
  }
}

async function setActiveDb(dbName) {
  if (dbName === currentDb) return;
  await fetch('/api/query', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({sql: 'USE ' + dbName + ';', db: ''})
  });
  currentDb = dbName;
  updateDbBadge();
  // Обновляем визуальный стиль
  document.querySelectorAll('.db-header').forEach(h => h.classList.remove('active'));
  const h = document.querySelector('#db-' + dbName + ' .db-header');
  if (h) h.classList.add('active');
}

function selectTable(dbName, tblName) {
  setActiveDb(dbName);
  const sql = 'SELECT * FROM ' + tblName + ' LIMIT 100;';
  document.getElementById('sql-input').value = sql;
  executeQuery();
}

function updateDbBadge() {
  const badge = document.getElementById('current-db-badge');
  badge.textContent = currentDb || 'нет БД';
}

// ── Выполнение запроса ────────────────────────────────────────

async function executeQuery() {
  const sql = document.getElementById('sql-input').value.trim();
  if (!sql) return;

  const t0 = performance.now();
  const badge = document.getElementById('result-badge');
  const body  = document.getElementById('results-body');
  const timeEl = document.getElementById('exec-time');

  body.innerHTML = '<div style="padding:20px;text-align:center;color:var(--text2)">⏳ Выполняется...</div>';

  try {
    const r = await fetch('/api/query', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({sql, db: currentDb})
    });
    const data = await r.json();
    const elapsed = (performance.now() - t0).toFixed(1);
    timeEl.textContent = elapsed + ' мс';

    if (data.error) {
      badge.className = 'badge badge-err';
      badge.textContent = 'Ошибка';
      badge.style.display = '';
      body.innerHTML = `
        <div class="result-msg err">
          <div class="result-msg-icon">✗</div>
          <div>
            <div class="result-msg-text">Ошибка выполнения</div>
            <div class="result-msg-sub">${escHtml(data.error)}</div>
          </div>
        </div>`;
    } else if (data.columns) {
      // SELECT — таблица
      badge.className = 'badge badge-row';
      badge.textContent = data.count + ' строк';
      badge.style.display = '';

      // Обновляем дерево если была DDL команда
      if (/CREATE\s+DATABASE|DROP\s+DATABASE/i.test(sql)) loadDatabases();

      const headers = data.columns.map(c => `<th>${escHtml(c)}</th>`).join('');
      const rowsHtml = data.rows.map(row =>
        '<tr>' + row.map((v,i) =>
          `<td class="${classifyValue(v)}">${displayValue(v)}</td>`
        ).join('') + '</tr>'
      ).join('');
      body.innerHTML = `<table class="result-table"><thead><tr>${headers}</tr></thead><tbody>${rowsHtml}</tbody></table>`;

      if (data.count === 0) {
        body.innerHTML += '<div style="padding:16px;text-align:center;color:var(--text2);font-size:13px">Запрос вернул 0 строк</div>';
      }
    } else {
      // DML / DDL
      badge.className = 'badge badge-ok';
      badge.textContent = 'OK';
      badge.style.display = '';

      const affected = data.affected > 0 ? ` (затронуто строк: ${data.affected})` : '';
      const msg = data.message || ('Затронуто строк: ' + data.affected);
      body.innerHTML = `
        <div class="result-msg ok">
          <div class="result-msg-icon">✓</div>
          <div>
            <div class="result-msg-text">${escHtml(msg)}</div>
            <div class="result-msg-sub">${elapsed} мс</div>
          </div>
        </div>`;

      // Обновляем дерево после DDL
      if (/CREATE|DROP/i.test(sql)) loadDatabases();
    }
  } catch(e) {
    body.innerHTML = `<div class="result-msg err"><div class="result-msg-icon">✗</div><div class="result-msg-text">Ошибка соединения: ${e.message}</div></div>`;
  }
}

function clearEditor() {
  document.getElementById('sql-input').value = '';
  document.getElementById('sql-input').focus();
}

// ── Клавиатурные сочетания ────────────────────────────────────

document.addEventListener('DOMContentLoaded', () => {
  document.getElementById('sql-input').addEventListener('keydown', e => {
    if (e.ctrlKey && e.key === 'Enter') {
      e.preventDefault();
      executeQuery();
    }
  });
  loadDatabases();
});
</script>
</body>
</html>)html";
        return resp;
    });
}

}
