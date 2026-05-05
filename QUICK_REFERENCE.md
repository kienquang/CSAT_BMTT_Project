# Quick Reference - Personal Record Vault

## Overview

The project now runs as a Qt GUI client talking to a TCP server.

- Authentication table: `users`
- Vault table: `personal_records`
- Password storage: `Argon2id`
- Record encryption: Blowfish with per-record DEK
- DEK wrapping: KEK derived from the user's password at login time

## Build

```powershell
cd C:\Users\ADMIN\Desktop\CSAT_BMTT_Project-main
cmake --build Build --config Release
```

Output:

- `Build\bin\Release\Server.exe`
- `Build\bin\Release\CSATApp.exe`

## Run

```powershell
.\scripts\run_server.bat
.\scripts\run_client.bat
```

## Runtime Config

Set values in `.env`:

- `APP_SERVER_HOST`
- `APP_SERVER_PORT`
- `APP_DB_HOST`
- `APP_DB_PORT`
- `APP_DB_USER`
- `APP_DB_PASSWORD`
- `APP_DB_NAME`

## Default Bootstrap Account

If the database has no users yet, the server creates:

- username: `admin`
- password: `admin12345`

## Main Files

- `src/core/DatabaseHelper.*`: schema creation, user auth, DEK wrap/unwrap, record CRUD
- `src/core/PasswordHasher.*`: Argon2id password hash and KEK derivation
- `src/server/TCP_Server.cpp`: session login, session KEK in memory, profile endpoints
- `src/client/network/NetworkClient.*`: TCP protocol client
- `src/client/gui/LoginDialog.*`: login and registration
- `src/client/gui/MainWindow.*`: self-service profile view/edit/delete

## Current GUI Flow

1. Open `CSATApp.exe`
2. Login with `username/password`
3. View your personal record
4. Edit your own profile
5. Delete your own account if needed

## Notes

- The server does not store KEK.
- KEK is derived from the login password and kept only for the active session.
- The old admin/user masking model is no longer used.
