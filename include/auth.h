/*
 * Galio Kernel
 *
 * Copyright (C) 2026 S.M Israfil
 *
 * This file is part of Galio.
 *
 * Galio is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * Galio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Galio. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef AUTH_H
#define AUTH_H

#include "common.h"

#define INPUT_BUFFER_SIZE 32

typedef struct {
    char username[32];
    char password[32];
    u8 registered;
    u8 authenticated;
    u8 authorized;
    u32 uid;
    u32 gid;
} user_session_t;

extern user_session_t kernel_auth;

void auth_bootstrap(void);
u8 auth_prompt_password(const char *prompt, char *password, u32 max_len);
u8 auth_is_authorized(void);
void auth_authorize(void);
void session_login(const char *username, u32 uid);
void session_logout(void);
user_session_t *session_current(void);
u8 auth_verify_password(const char *username, const char *password);
i32 auth_change_password(const char *old_password, const char *new_password);
i32 auth_change_username(const char *new_username, u8 require_root);
void auth_show_login_prompt(void);

#endif /* AUTH_H */