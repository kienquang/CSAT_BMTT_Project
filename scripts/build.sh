#!/bin/bash
# ======== BUILD SCRIPT FOR CSAT_BMTT_PROJECT (Linux/Mac) ========

echo ""
echo "========== CSAT_BMTT Build System =========="
echo ""

# Kiểm tra CMake
if ! command -v cmake &> /dev/null; then
    echo "[ERROR] CMake not found! Please install CMake first."
    exit 1
fi

# Tạo thư mục build
if [ ! -d "build" ]; then
    echo "[*] Creating build directory..."
    mkdir build
fi

cd build

# Generate build files
echo "[*] Generating build files..."
cmake .. -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release

if [ $? -ne 0 ]; then
    echo "[ERROR] CMake generation failed!"
    exit 1
fi

# Build project
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
echo "  - Server: ./bin/Server"
echo "  - Client: ./bin/Client"
echo "  - TestMasking: ./bin/TestMasking"
echo ""
