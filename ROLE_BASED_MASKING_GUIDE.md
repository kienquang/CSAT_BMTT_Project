# 🔐 Role-Based Data Masking Implementation Guide

## 📋 Tong Quan
He thong da duoc trien khai **Role-Based Masking** - che giau du lieu nhay cam khac nhau tuy theo vai tro cua user (Admin hoac User).

---

## 🏗️ Kiến Trúc Hệ Thống

### **Quy Trinh Du Lieu:**
```
1. Lay du lieu tu Database (Encrypted HEX)
   ↓
2. Giai ma bang Blowfish Cipher
   ↓
3. Kiem tra vai tro cua user (Admin/User)
   ↓
4. Ap dung mask tuy role
   ↓
5. Gui cho user
```

---

## 🔧 Cac Thay Doi Da Thuc Hien

### **1️⃣ MaskingLogic.h/cpp - Them Ho Tro Role-Based**

#### Ham Moi:
```cpp
// Enum de dinh nghia vai tro
enum class UserRole {
    ADMIN,  // Co quyen xem du lieu goc (plaintext)
    USER    // Chi xem du lieu da mask
};

// Ham role-aware
void MaskCCCDByRole(char* buffer, UserRole role, size_t bufferSize = 20);
void MaskPhoneByRole(char* buffer, UserRole role, size_t bufferSize = 20);
void MaskSalaryByRole(char* buffer, UserRole role, size_t bufferSize = 20);

UserRole StringToRole(const string& roleStr);  // Chuyen string → enum
```

#### Hanh Vi:
| Du Lieu | Admin | User |
|---------|-------|------|
| CCCD: `001202037855` | `001202037855` | `****037855` |
| SDT: `0912345678` | `0912345678` | `09****5678` |
| Luong: `50000000` | `50000000` | `***` |

---

### **2️⃣ DatabaseHelper.h/cpp - Them Ham Truy Van Role-Aware**

#### Ham Moi:
```cpp
// Tao database views cho masking
bool CreateRoleBasedViews();

// Truy van voi role-based masking
bool GetNhanVienByIdWithRole(int id, nhanvien& result, const string& currentUserRole);
vector<nhanvien> GetAllNhanVienWithRole(const string& currentUserRole);
vector<nhanvien> GetNhanVienByRoleWithMask(const string& vai_tro, const string& currentUserRole);
```

#### Database Views Duoc Tao:
```sql
-- View cho USER role: Khong che giau (encrypted data)
CREATE VIEW v_nhanvien_user AS
SELECT id, ten_nv, vai_tro, 
       cccd_cipher AS cccd_cipher_encrypted, 
       sdt_cipher AS sdt_cipher_encrypted, 
       luong_cipher AS luong_cipher_encrypted 
FROM nhanvien;

-- View cho ADMIN role: Day du du lieu encrypted
CREATE VIEW v_nhanvien_admin AS
SELECT id, ten_nv, vai_tro, cccd_cipher, sdt_cipher, luong_cipher 
FROM nhanvien;
```

---

### **3️⃣ main.cpp - Cap Nhat Giao Dien**

#### Bien Global:
```cpp
string currentUserRole = "User";  // Vai tro cua user hien tai
```

#### Menu Moi:
```
8. Thay doi vai tro (Admin/User)
9. Xem database views
```

#### Ham Moi:
```cpp
void ChangeUserRole();           // Chuyen doi vai tro
void DisplayDatabaseViews();     // Hien thi thong tin views
```

---

## 💾 Co Che Luu Tru & Bao Mat

### **Trong Database:**
- ✅ Tat ca du lieu nhay cam duoc **ma hoa Blowfish**
- ✅ Luu duoi dang **HEX string** (khong plaintext)
- ✅ Key: `MatMaHoc@NIST2025`

### **Khi Truy Van:**
1. Lay du lieu encrypted tu DB
2. Giai ma de co plaintext
3. **Ap dung mask dua vai tro** (moi!)
4. Tra ve masked data cho user

---

## 🔐 Chien Luoc Masking Theo Role

### **Admin Role:**
```
Co quyen nhin thay PLAINTEXT cua tat ca du lieu nhay cam
- CCCD: 001202037855 ✓ (day du)
- SDT: 0912345678 ✓ (day du)
- Luong: 50000000 ✓ (day du)
```

### **User Role:**
```
CHI thay du lieu da mask 
- CCCD: ****037855 (che phan dau, giu 4 cuoi)
- SDT: 09****5678 (giu 2 dau + 4 cuoi)
- Luong: *** (che toan bo)
```

---

## 📝 Ví Dụ Sử Dụng

### **Kỹ Sư A (User Role) - Xem nhân viên ID=1:**
```
ID: 1
Tên: Nguyễn Văn A
Vai trò: Admin
CCCD: ****037855          ← Masked
SDT: 09****5678           ← Masked
Luong: ***                ← Masked
```

### **Ky Su B (Admin Role) - Xem nhan vien ID=1:**
```
ID: 1
Ten: Nguyen Van A
Vai tro: Admin
CCCD: 001202037855        ← Plaintext
SDT: 0912345678           ← Plaintext
Luong: 50000000           ← Plaintext
```

---

## 🧪 Cach Test

### **Test 1: User Role**
```
1. Chay chuong trinh
2. Menu → 8 → Chon "2. Chuyen sang User"
3. Menu → 1 (Xem tat ca) hoac 2 (Tim theo ID)
4. Kiem tra: Du lieu nhay cam se hien thi dang masked (****)
```

### **Test 2: Admin Role**
```
1. Menu → 8 → Chon "1. Chuyen sang Admin"
2. Menu → 1 (Xem tat ca) hoac 2 (Tim theo ID)
3. Kiem tra: Nhin thay du lieu plaintext day du
```

### **Test 3: Database Views**
```
1. Menu → 9 (Xem database views)
2. Kiem tra: Thong tin ve 2 views da duoc tao
```

---

## 📊 Database Schema

### **Table: nhanvien**
```
id (INT, PK)
ten_nv (VARCHAR)
vai_tro (VARCHAR) - 'Admin' hoac 'User'
cccd_cipher (VARCHAR) - Ma hoa Blowfish HEX
sdt_cipher (VARCHAR) - Ma hoa Blowfish HEX
luong_cipher (VARCHAR) - Ma hoa Blowfish HEX
```

### **Views:**
- `v_nhanvien_user` - Cho user view
- `v_nhanvien_admin` - Cho admin view

---

## 🚀 Loi Ich Cua Role-Based Masking

| Loi Ich | Mo Ta |
|---------|-------|
| **Bao Mat Du Lieu** | User khong the truy cap du lieu nhay cam |
| **Kiem Soat Truy Cap** | Admin co quyen xem full, User co quyen xem masked |
| **Compliance** | Tuan thu cac tieu chuan bao mat du lieu |
| **Linh Hoat** | De mo rong voi nhieu role khac nhau |
| **Bao Mat O Tang Application** | Che giau du lieu o code, khong phu thuoc vao DB |

---

## ⚙️ Mo Rong Trong Tuong Lai

### **Co the them:**
1. **Audit Log** - Ghi nhan ai/khi/cai gi truy cap
2. **Role Hierarchy** - CEO > Manager > Staff > Intern
3. **Dynamic Masking** - Mask rules khac nhau cho tung cot
4. **Conditional Access** - Che giau based on time/location
5. **Database-Level Security** - Row-Level Security (RLS)

---

## 📞 Troubleshooting

### **Van de: Views khong duoc tao**
**Giai phap:** Chac chan MySQL server dang chay va database duoc ket noi

### **Van de: Du lieu khong masked**
**Giai phap:** Kiem tra `currentUserRole` co dung "Admin" hoac "User" khong

### **Van de: Giai ma that bai**
**Giai phap:** Xac nhan Blowfish key: `MatMaHoc@NIST2025` co dung khong

---

## 📄 Tong Ket Implement

✅ Them UserRole enum vao MaskingLogic  
✅ Them 3 ham mask role-based vao MaskingLogic  
✅ Them ham StringToRole converter  
✅ Them CreateRoleBasedViews vao DatabaseHelper  
✅ Them 3 ham query role-aware vao DatabaseHelper  
✅ Update main.cpp với currentUserRole global variable  
✅ Thêm menu item 8 & 9 cho role management  
✅ Tạo hàm ChangeUserRole() & DisplayDatabaseViews()  

**Hệ thống đã sẵn sàng sử dụng role-based masking! 🎉**
