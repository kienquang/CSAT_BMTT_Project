#!/bin/bash
# ======== BUILD SCRIPT FOR PERSONAL RECORD VAULT ========

echo ""
echo "========== Personal Record Vault Build =========="
echo ""

if ! command -v cmake &> /dev/null; then
    echo "[ERROR] CMake not found! Please install CMake first."
    exit 1
fi

if [ ! -d "Build" ]; then
    echo "[*] Creating Build directory..."
    mkdir Build
fi

cd Build

if [ ! -f "CMakeCache.txt" ]; then
    echo "[*] Configuring project..."
    cmake .. -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_QT_GUI=ON
    if [ $? -ne 0 ]; then
        echo "[ERROR] CMake configure failed!"
        exit 1
    fi
fi

echo "[*] Building project..."
cmake --build . --config Release

if [ $? -ne 0 ]; then
    echo "[ERROR] Build failed!"
    exit 1
fi

echo ""
echo "[SUCCESS] Build completed successfully!"
echo ""
echo "Output files:"
echo "  - Server: ./bin/Release/Server"
echo "  - GUI:    ./bin/Release/CSATApp"
echo ""
