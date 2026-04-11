# Legacy Note

The old role-based masking design is no longer part of the active system.

It was replaced by a new vault-style design:

- `users` stores authentication data
- `personal_records` stores encrypted personal data
- passwords use `Argon2id`
- a per-record DEK encrypts CCCD, phone, and email
- the DEK is wrapped with a KEK derived from the user's password
- the server does not store KEK

Current source of truth:

- `src/core/PasswordHasher.*`
- `src/core/DatabaseHelper.*`
- `src/server/TCP_Server.cpp`
- `src/client/network/NetworkClient.*`
