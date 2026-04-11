# Implementation Summary - Personal Record Vault

## What Changed

The old employee-management schema and role-based masking flow were replaced.

New schema:

### `users`

- `id`
- `username`
- `password_hash`

### `personal_records`

- `id`
- `user_id`
- `Gender`
- `encrypted_dek`
- `CCCD_cipher`
- `SDT_cipher`
- `Email_cipher`

## Security Model

- Passwords are stored as `Argon2id` encoded hashes.
- Each personal record gets its own random DEK.
- CCCD, phone, and email are encrypted with Blowfish using that DEK.
- The DEK is wrapped into `encrypted_dek` using a KEK derived from the user's password.
- The server does not store KEK.
- The server only keeps a session KEK in memory after a successful login.

## App Model

The application is now self-service:

- One user logs in
- The user views only their own profile
- The user edits only their own profile
- The user can delete only their own account

The old admin/user role model is removed from the main flow.

## New Core Pieces

- `src/core/PasswordHasher.*`
  - Argon2id password hashing
  - KEK derivation from password + username
- `src/core/DatabaseHelper.*`
  - schema initialization
  - bootstrap default account
  - registration
  - authentication
  - profile fetch/update/delete

## New Network Flow

Requests:

- `REQ_LOGIN`
- `REQ_FETCH_PROFILE`
- `REQ_REGISTER`
- `REQ_UPDATE_PROFILE`
- `REQ_DELETE_ACCOUNT`
- `REQ_GET_TOTAL`
- `REQ_LOGOUT`

## Bootstrap Behavior

If the database has no users, the server creates:

- username: `admin`
- password: `admin12345`

## Build Status

Release build currently succeeds for:

- `Server.exe`
- `CSATApp.exe`
