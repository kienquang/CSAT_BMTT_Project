# Qt GUI Setup Guide - Personal Record Vault

## Stack

- CMake 3.16+
- Qt 6.x with MSVC 2022
- MySQL Connector/C++ 8.x
- Windows build target

## Configure

If `Build` is not configured yet:

```powershell
cd C:\Users\ADMIN\Desktop\CSAT_BMTT_Project-main
cmake -S . -B Build -G "Visual Studio 17 2022" -DCMAKE_PREFIX_PATH=C:/Qt/6.11.0/msvc2022_64 -DBUILD_QT_GUI=ON
```

## Build

```powershell
cmake --build Build --config Release
```

## Run

```powershell
Build\bin\Release\Server.exe
Build\bin\Release\CSATApp.exe
```

## GUI Files

- `src/client/gui/MainWindow.ui`
- `src/client/gui/MainWindow.cpp`
- `src/client/gui/MainWindow.h`
- `src/client/gui/LoginDialog.cpp`
- `src/client/gui/EmployeeDialog.cpp`

## What The GUI Does Now

- Login with `username/password`
- Register a new account
- Show one personal profile row for the logged-in user
- Edit username, gender, CCCD, phone, email, password
- Delete the current account

## Troubleshooting

### Qt not found

Check `CMAKE_PREFIX_PATH` points to your Qt installation.

### MySQL connection failed

Check `.env` values for:

- `APP_DB_HOST`
- `APP_DB_PORT`
- `APP_DB_USER`
- `APP_DB_PASSWORD`
- `APP_DB_NAME`

### Login request failed

Check both client and server can read the same `APP_LOGIN_BLOWFISH_KEY`.

### GUI opens but login fails

Start the server first. On an empty database the server bootstraps:

- `admin / admin12345`
