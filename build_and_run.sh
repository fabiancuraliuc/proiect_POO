#!/usr/bin/env bash
# ─── Build & Run script for Traffic Arcade ─────────────────────────────────
set -e

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

echo "==================================================="
echo "  Traffic Arcade — Block-Pixel NCurses Simulator"
echo "==================================================="

# Install dependencies if needed
if ! dpkg -s libncursesw5-dev &>/dev/null 2>&1; then
    echo "[*] Installing ncursesw dev package..."
    sudo apt-get install -y libncursesw5-dev pkg-config cmake build-essential
fi

# Configure
echo "[*] Configuring CMake..."
cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release

# Build
echo "[*] Building..."
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo ""
echo "[OK] Build successful!  Binary: $BUILD_DIR/TrafficSimulator"
echo "     Running now..."
echo ""

# Run
exec "$BUILD_DIR/TrafficSimulator"
