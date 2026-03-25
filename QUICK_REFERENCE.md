# Quick Reference - Qt6 GUI Application

## ⚡ 3 Bước Nhanh Nhất

### 1️⃣ Cài Qt6
```bash
# Tải từ https://www.qt.io/download
# Chọn Qt 6.7 LTS + MSVC 2022 → Cài tại C:\Qt\6.7
```

### 2️⃣ Build
```bash
cd c:\Users\ADMIN\Desktop\CSAT_BMTT_Project-main
rmdir /s /q Build
mkdir Build
cd Build
cmake .. -DCMAKE_PREFIX_PATH=C:/Qt/6.7/msvc2022_64 -G "Visual Studio 17 2022" -DBUILD_QT_GUI=ON
cmake --build . --config Release -j4
```

### 3️⃣ Chạy
```bash
cd bin/Release
CSATApp.exe
```

---

## 📁 File Cần Biết

| File | Tác Dụng |
|------|---------|
| `src/ui/MainWindow.ui` | Qt Designer - drag drop UI |
| `src/ui/MainWindow.h/cpp` | Logic chính |
| `src/ui/EmployeeDialog.h/cpp` | Dialog thêm/sửa (future) |
| `src/ui/StyleSheets.qss` | Styling (CSS-like) |
| `src/apps/main_qt.cpp` | Entry point |
| `CMakeLists.txt` | Qt configuration |

---

## 🎨 Chỉnh Sửa Giao Diện

### Mở Qt Designer
```bash
cd src/ui
# Kính Qt Creator → File → Open → MainWindow.ui
```

### Workflow
1. Open MainWindow.ui
2. Drag-drop components
3. Edit properties
4. Save
5. Rebuild: `cmake --build . --config Release`

---

## 🔧 Thay Đổi Database Credentials

File: `src/ui/MainWindow.cpp` (dòng ~40)

```cpp
dbHelper = std::make_unique<DatabaseHelper>(
    "localhost",    // Host
    3306,           // Port
    "root",         // User
    "YOUR_PASSWORD", // ← Thay đổi
    "CSAT_BMTT"     // Database
);
```

---

## 🐛 Lỗi Phổ Biến

| Lỗi | Giải Pháp |
|-----|----------|
| Qt6 not found | Check Qt path: `C:\Qt\6.7\msvc2022_64` |
| mysqlcppconn.lib not found | Check MySQL path trong CMakeLists.txt |
| Missing Qt DLLs | Check `Build/bin/Release/` có đầy đủ `Qt6*.dll` |
| Database connection failed | Check credentials ở `src/ui/MainWindow.cpp` |

---

## 📋 Kiểm Tra Từng Bước

```bash
# 1. Build thành công?
cmake --build . --config Release

# 2. Executable tồn tại?
dir Build\bin\Release\CSATApp.exe

# 3. Chạy được?
Build\bin\Release\CSATApp.exe

# 4. Database connect được?
# Check console output khi chạy
```

---

## 🎯 Tính Năng Hiện Tại

✅ Xem danh sách nhân viên
✅ Role-based masking (Admin/User)
✅ Tìm kiếm nhân viên
✅ Xóa nhân viên
✅ Thống kê tổng số

🔙 TODO: Thêm/Sửa nhân viên (placeholder prepared)

---

## 🚀 Next: Implement Add/Edit

Trong `src/ui/MainWindow.cpp`, replace TODO:

```cpp
void MainWindow::onAddEmployee() {
    EmployeeDialog dialog(EmployeeDialog::AddMode, this);
    if (dialog.exec() == QDialog::Accepted) {
        auto data = dialog.getEmployeeData();
        // TODO: Call dbHelper->InsertNhanVien(...)
        loadEmployeeData();
    }
}

void MainWindow::onEditEmployee() {
    EmployeeDialog dialog(EmployeeDialog::EditMode, this);
    if (dialog.exec() == QDialog::Accepted) {
        auto data = dialog.getEmployeeData();
        // TODO: Call dbHelper->UpdateNhanVien(id, ...)
        loadEmployeeData();
    }
}
```

---

## 📚 Tài Liệu

| Loại | Link |
|------|------|
| Qt6 Doc | https://doc.qt.io/qt-6/ |
| CMake Qt | https://cmake.org/cmake/help/latest/module/FindQt6.html |
| Setup Guide | Xem `QT_GUI_SETUP.md` |
| Implementation | Xem `IMPLEMENTATION_SUMMARY.md` |

---

## ✅ Validation Script

```cpp
// Chạy CSATApp.exe và kiểm tra:
1. ✅ Connection success message in console
2. ✅ Employee table populated
3. ✅ Role dropdown works (Admin/User)
4. ✅ Change role → data re-masks automatically
5. ✅ Search filter works
6. ✅ Delete button confirms before deleting
7. ✅ Total count updates
8. ✅ No console errors
```

---

**Status**: ✅ Ready to use!
**Next**: Cài Qt6 → Build → Run → Thêm features
