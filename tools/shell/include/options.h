#ifndef GSH_OPTIONS_H
#define GSH_OPTIONS_H

#include "common.h"

#define GSH_OPTION_MAX 16
#define GSH_POSITIONAL_MAX 16
#define GSH_OPTION_NAME_MAX 64
#define GSH_OPTION_VALUE_MAX 256
#define GSH_OPTION_TOKEN_MAX 256

typedef struct {
    char short_name;
    const char *long_name;
    u8 requires_argument;
    u8 optional_argument;
} gsh_option_spec_t;

typedef struct {
    char short_name;
    char long_name[GSH_OPTION_NAME_MAX];
    char value[GSH_OPTION_VALUE_MAX];
    u8 has_value;
} gsh_option_t;

typedef enum {
    GSH_OPTIONS_OK = 0,
    GSH_OPTIONS_UNKNOWN_OPTION,
    GSH_OPTIONS_MISSING_ARGUMENT,
    GSH_OPTIONS_TOO_MANY_OPTIONS,
    GSH_OPTIONS_TOO_MANY_POSITIONALS,
    GSH_OPTIONS_INVALID_ARGUMENT
} gsh_options_error_t;

typedef struct {
    gsh_option_t options[GSH_OPTION_MAX];
    u32 option_count;
    char positional[GSH_POSITIONAL_MAX][GSH_OPTION_TOKEN_MAX];
    u32 positional_count;
    u8 parsing_stopped;
    gsh_options_error_t error;
    char error_name[GSH_OPTION_NAME_MAX];
    char error_short_name;
} gsh_options_t;

/* Parse options before the first positional argument, using command specs. */
gsh_options_error_t gsh_parse_options(const char *args,
                                      const gsh_option_spec_t *specs,
                                      u32 spec_count,
                                      gsh_options_t *result);

const gsh_option_t *gsh_find_option(const gsh_options_t *parsed, char short_name,
                                    const char *long_name);

#endif /* GSH_OPTIONS_H */
