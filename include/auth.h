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