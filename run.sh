#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SERVER_BIN="$SCRIPT_DIR/build/bin/chapadb_server"

HOST="0.0.0.0"
PORT="5432"
DATA_DIR="$SCRIPT_DIR/data"

# Разбираем аргументы
while [[ $# -gt 0 ]]; do
    case "$1" in
        --host) HOST="$2"; shift 2 ;;
        --port) PORT="$2"; shift 2 ;;
        --data) DATA_DIR="$2"; shift 2 ;;
        *) echo "Неизвестный аргумент: $1"; exit 1 ;;
    esac
done

# Если бинарник отсутствует — собираем
if [ ! -f "$SERVER_BIN" ]; then
    echo "Бинарник сервера не найден, запускаем сборку..."
    "$SCRIPT_DIR/build.sh"
fi

mkdir -p "$DATA_DIR"

echo "Запуск chapaBD сервера на $HOST:$PORT (данные: $DATA_DIR)"
exec "$SERVER_BIN" --host "$HOST" --port "$PORT" --data "$DATA_DIR"
