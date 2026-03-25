# Qt6 GUI Application Setup Guide - CSAT_BMTT Project

## 📋 Tổng Quan

Phương Án 1 đã được triển khai với **Qt6 Framework**. Giao diện người dùng được xây dựng hoàn toàn bằng C++/Qt, không thay đổi logic nghiệp vụ hiện tại.

---

## 🛠️ **Bước 1: Cài Đặt Qt6**

### Tùy Chọn A: Qt Online Installer (Khuyên dùng)

1. Tải từ: https://www.qt.io/download
2. Chọn **Open Source**
3. Chọn phiên bản: **Qt 6.7 LTS** (hoặc mới nhất)
4. Chọn compiler: **MSVC 2022** (hoặc phù hợp với máy bạn)
5. Cài đặt tại: `C:\Qt\6.7` (mặc định)

### Tùy Chọn B: Cài từ Visual Studio

Nếu có Visual Studio, có thể cài Qt6 extension:
- Extensions → Manage Extensions → Search "Qt" → Install Qt6 tools

---

## 🗂️ **Cấu Trúc Dự Án Hiện Tại**

```
CSAT_BMTT_Project/
├── src/
│   ├── ui/                    ← Qt UI Files (NEW)
│   │   ├── MainWindow.h/cpp
│   │   ├── MainWindow.ui      ← Qt Designer file
│   │   ├── CMakeLists.txt
│   │   ├── StyleSheets.qss
│   │   └── resources.qrc
│   ├── core/                  ← Giữ nguyên
│   │   ├── DatabaseHelper.h/cpp
│   │   ├── MaskingLogic.h/cpp
│   │   └── ...
│   ├── network/               ← Giữ nguyên
│   ├── apps/
│   │   ├── main.cpp           ← Console app (cũ)
│   │   └── main_qt.cpp        ← Qt GUI app (NEW)
│   └── CMakeLists.txt
├── CMakeLists.txt             ← Updated với Qt support
└── Build/                     ← Output thư mục
```

---

## 🏗️ **Bước 2: Cấu Hình Build**

### Cách 1: Dùng CMake (Command Line)

```bash
# 1. Xóa build cũ
rmdir /s /q Build

# 2. Tạo build folder mới
mkdir Build
cd Build

# 3. Configure với Qt path
# Điều chỉnh Qt path nếu cần (phiên bản MSVC và năm có thể khác)
cmake .. ^
  -DCMAKE_PREFIX_PATH=C:/Qt/6.7/msvc2022_64 ^
  -G "Visual Studio 17 2022" ^
  -DBUILD_QT_GUI=ON

# 4. Build (Release)
cmake --build . --config Release -j4

# Hoặc Debug
cmake --build . --config Debug -j4
```

### Cách 2: Dùng Visual Studio IDE

```bash
# Mở Build folder như project trong VS
cd Build
start CSAT_BMTT_Project.sln
```

Trong Visual Studio:
- Chọn target: **CSATApp**
- Chọn config: **Release** hoặc **Debug**
- Nhấn **Build**

---

## 📊 **Build Output**

Sau khi build thành công:

```
Build/bin/Release/    (hoặc Debug/)
├── CSATApp.exe            ← Main GUI application
├── Qt6Core.dll
├── Qt6Gui.dll
├── Qt6Widgets.dll
├── Qt6Sql.dll
├── Qt6Network.dll
├── mysqlcppconn8-2-vs14.dll
├── mysqlcppconn-9-vs14.dll
├── libssl-3-x64.dll
└── libcrypto-3-x64.dll
```

---

## ▶️ **Bước 3: Chạy Ứng Dụng**

### Từ Command Line

```bash
# Từ Build folder
cd bin/Release
.\CSATApp.exe
```

### Từ Visual Studio

```bash
# Chọn CSATApp trong Solution Explorer
# Nhấn F5 (Debug) hoặc Ctrl+F5 (Release without debugging)
```

---

## 🔧 **Cấu Hình Database**

File: `src/ui/MainWindow.cpp` (dòng ~40)

```cpp
dbHelper = std::make_unique<DatabaseHelper>(
    "localhost",         // Database host
    3306,                // Port
    "root",              // Username
    "password",          // ← Thay đổi password của bạn
    "CSAT_BMTT"          // Database name
);
```

Thay đổi credential tương ứng với setup MySQL của bạn.

---

## 🎨 **Chỉnh Sửa Giao Diện**

### Cách 1: Qt Designer (Trực quan)

```bash
# Mở file .ui trong Qt Creator
cd src/ui
# Bật Qt Creator → File → Open → MainWindow.ui
```

Trong Qt Designer:
- Drag-drop components
- Edit properties
- Save → Tự động generate C++

### Cách 2: Edit C++ Code Trực Tiếp

- File: `src/ui/MainWindow.cpp`
- Thay đổi logic widget
- Rebuild

### Cách 3: Styling (CSS-like)

- File: `src/ui/StyleSheets.qss`
- Thiết kế toàn bộ theme ứng dụng
- Load vào MainWindow.cpp

---

## 📋 **File Quan Trọng**

| File | Mục Đích |
|------|---------|
| `src/ui/MainWindow.ui` | UI Layout (Qt Designer format) |
| `src/ui/MainWindow.h` | Header class |
| `src/ui/MainWindow.cpp` | Implementation |
| `src/ui/CMakeLists.txt` | Build config cho UI library |
| `src/apps/main_qt.cpp` | Entry point Qt app |
| `CMakeLists.txt` | Root CMake config (Qt support thêm) |

---

## ❌ **Khắc Phục Lỗi Thường Gặp**

### Lỗi 1: "Qt6 not found"

**Nguyên nhân**: Qt path không đúng hoặc Qt chưa cài

**Giải pháp**:
```bash
# Kiểm tra path Qt
dir C:\Qt\6.7\msvc2022_64

# Nếu error, điều chỉnh path trong cmake configure step
cmake .. -DCMAKE_PREFIX_PATH=YOUR_QT_PATH
```

### Lỗi 2: "Cannot find Qt6Sql.dll"

**Nguyên nhân**: Missing Qt DLL

**Giải pháp**:
```bash
# Ensure dll copy in CMakeLists.txt ran
# Kiểm tra Build/bin/Release/ có đầy đủ Qt DLLs không
```

### Lỗi 3: "Database connection failed"

**Nguyên nhân**: Wrong database credentials

**Giải pháp**:
- Edit `src/ui/MainWindow.cpp` dòng ~40
- Update host, user, password
- Rebuild

### Lỗi 4: "LINK : fatal error LNK1104: cannot open file 'mysqlcppconn.lib'"

**Nguyên nhân**: MySQL lib path không tìm thấy

**Giải pháp**:
```bash
# Kiểm tra path MySQL
dir C:\mysql-connector-c++-8.3.0-winx64\lib64\vs14

# Nếu sai, update CMakeLists.txt
set(MYSQL_LIB_DIR "YOUR_MYSQL_PATH/lib64/vs14")
```

---

## 🚀 **Workflow Phát Triển**

```
1. Mở MainWindow.ui trong Qt Designer
   ↓
2. Chỉnh sửa giao diện (kéo-thả)
   ↓
3. Lưu → Tự động generate ui_MainWindow.h
   ↓
4. Viết logic trong MainWindow.cpp
   ↓
5. Build project (cmake --build .)
   ↓
6. Chạy CSATApp.exe
   ↓
7. Kiểm tra → Quay lại bước 1 nếu cần sửa UI
```

---

## 📝 **Quan Trọng: Không Thay Đổi Logic Hiện Tại**

- ✅ DatabaseHelper.h/cpp → Giữ nguyên
- ✅ MaskingLogic.h/cpp → Giữ nguyên
- ✅ Blowfish.h/cpp → Giữ nguyên
- ✅ NetworkData.h → Giữ nguyên

UI chỉ gọi các hàm hiện tại từ DatabaseHelper, không modify.

---

## 📦 **Dependencies**

| Thành Phần | Phiên Bản |
|-----------|----------|
| Qt6 | 6.7 LTS (hoặc mới nhất) |
| MSVC | 2022 (hoặc 2019) |
| CMake | 3.16+ |
| MySQL Connector/C++ | 8.3.0 |

---

## 🔗 **Tài Liệu Tham Khảo**

- Qt6 Docs: https://doc.qt.io/qt-6/
- Qt Creator: https://doc.qt.io/qtcreator/
- CMake Qt: https://cmake.org/cmake/help/latest/module/FindQt6.html

---

## ✅ **Checklist Hoàn Thành**

- [ ] Cài đặt Qt6.7
- [ ] Cài đặt MSVC 2022
- [ ] Git clone/pull project mới
- [ ] Cấu hình CMake với Qt path
- [ ] Build project thành công
- [ ] Chạy CSATApp.exe
- [ ] Kiểm tra login/logout
- [ ] Xem được danh sách nhân viên
- [ ] Thay đổi role → Kiểm tra masking
- [ ] Tất cả logic hiện tại hoạt động chính xác

---

**Nếu gặp vấn đề, hãy check:**
1. Qt path đúng chưa?
2. MSVC version là gì?
3. MySQL credential đúng chưa?
4. Có đầy đủ DLL files trong Build/bin/?

