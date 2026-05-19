#ifndef AUTH_H
#define AUTH_H

#include "common.h"

#define INPUT_BUFFER_SIZE 32

typedef struct {
    char username[32];
    char password[32];
    u8 registered;
    u8 authenticated;
} user_session_t;

extern user_session_t kernel_auth;

void auth_bootstrap(void);
u8 auth_prompt_password(const char *prompt, char *password, u32 max_len);
u8 auth_is_authorized(void);
void auth_authorize(void);
u8 auth_verify_password(const char *username, const char *password);
void auth_show_login_prompt(void);

#endif /* AUTH_H */