#!/usr/bin/env bash
set -euo pipefail

# Определяем ОС
OS="$(uname -s)"
echo "Операционная система: $OS"

install_deps_linux() {
    if command -v apt-get &>/dev/null; then
        echo "Установка зависимостей через apt-get..."
        sudo apt-get update -qq
        sudo apt-get install -y g++ cmake make
    elif command -v yum &>/dev/null; then
        echo "Установка зависимостей через yum..."
        sudo yum install -y gcc-c++ cmake make
    elif command -v pacman &>/dev/null; then
        echo "Установка зависимостей через pacman..."
        sudo pacman -S --noconfirm gcc cmake make
    else
        echo "Менеджер пакетов не найден, предполагаем что зависимости установлены"
    fi
}

install_deps_macos() {
    if ! command -v cmake &>/dev/null; then
        echo "Установка cmake через Homebrew..."
        brew install cmake
    fi
}

# Проверяем наличие необходимых инструментов
if ! command -v g++ &>/dev/null && ! command -v c++ &>/dev/null; then
    if [ "$OS" = "Linux" ]; then install_deps_linux; fi
fi

if ! command -v cmake &>/dev/null; then
    if   [ "$OS" = "Linux" ];  then install_deps_linux;
    elif [ "$OS" = "Darwin" ]; then install_deps_macos;
    fi
fi

# Директория сборки рядом со скриптом
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

echo "Создаём директорию сборки: $BUILD_DIR"
mkdir -p "$BUILD_DIR"

echo "Запускаем CMake..."
cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release

echo "Компилируем..."
cmake --build "$BUILD_DIR" --parallel "$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

echo ""
echo "Сборка завершена успешно!"
echo "  Сервер:            $BUILD_DIR/bin/chapadb_server"
echo "  CLI клиент:        $BUILD_DIR/bin/chapadb_cli"
echo "  Клиентская lib:    $BUILD_DIR/lib/libchapadb.so"
