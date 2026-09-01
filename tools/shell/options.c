#include "options.h"
#include "string.h"

static u8 copy_token(const char *src, u32 length, char *dst, u32 size) {
    if (length >= size) return 0;
    for (u32 i = 0; i < length; i++) dst[i] = src[i];
    dst[length] = 0;
    return 1;
}

static const gsh_option_spec_t *find_spec(const gsh_option_spec_t *specs,
                                          u32 count, char short_name,
                                          const char *long_name) {
    for (u32 i = 0; i < count; i++) {
        if (long_name) {
            if (specs[i].long_name && strcmp(specs[i].long_name, long_name) == 0) return &specs[i];
        } else if (specs[i].short_name == short_name) {
            return &specs[i];
        }
    }
    return NULL;
}

static gsh_options_error_t add_option(gsh_options_t *result,
                                      const gsh_option_spec_t *spec,
                                      char short_name, const char *long_name,
                                      const char *value) {
    if (result->option_count >= GSH_OPTION_MAX) return GSH_OPTIONS_TOO_MANY_OPTIONS;
    gsh_option_t *option = &result->options[result->option_count++];
    option->short_name = short_name;
    option->has_value = value != NULL;
    option->long_name[0] = 0;
    option->value[0] = 0;
    if (long_name) {
        u32 length = strlen(long_name);
        if (!copy_token(long_name, length, option->long_name, sizeof(option->long_name))) {
            return GSH_OPTIONS_INVALID_ARGUMENT;
        }
    }
    if (value && !copy_token(value, strlen(value), option->value, sizeof(option->value))) {
        return GSH_OPTIONS_INVALID_ARGUMENT;
    }
    (void)spec;
    return GSH_OPTIONS_OK;
}

static gsh_options_error_t add_positional(gsh_options_t *result, const char *token, u32 length) {
    if (result->positional_count >= GSH_POSITIONAL_MAX) return GSH_OPTIONS_TOO_MANY_POSITIONALS;
    if (!copy_token(token, length, result->positional[result->positional_count], GSH_OPTION_TOKEN_MAX)) {
        return GSH_OPTIONS_INVALID_ARGUMENT;
    }
    result->positional_count++;
    return GSH_OPTIONS_OK;
}

static void set_error(gsh_options_t *result, gsh_options_error_t error,
                      const char *name, char short_name) {
    result->error = error;
    result->error_short_name = short_name;
    result->error_name[0] = 0;
    if (name) {
        u32 length = strlen(name);
        if (length >= sizeof(result->error_name)) length = sizeof(result->error_name) - 1;
        for (u32 i = 0; i < length; i++) result->error_name[i] = name[i];
        result->error_name[length] = 0;
    }
}

gsh_options_error_t gsh_parse_options(const char *args,
                                      const gsh_option_spec_t *specs,
                                      u32 spec_count,
                                      gsh_options_t *result) {
    if (!result) return GSH_OPTIONS_INVALID_ARGUMENT;
    memset(result, 0, sizeof(*result));
    if (!args) return GSH_OPTIONS_OK;

    const char *cursor = args;
    while (*cursor) {
        while (*cursor == ' ' || *cursor == '\t') cursor++;
        if (!*cursor) break;
        const char *start = cursor;
        while (*cursor && *cursor != ' ' && *cursor != '\t') cursor++;
        u32 length = (u32)(cursor - start);

        if (result->parsing_stopped || start[0] != '-' || length == 1) {
            gsh_options_error_t error = add_positional(result, start, length);
            if (error != GSH_OPTIONS_OK) { set_error(result, error, NULL, 0); return error; }
            continue;
        }
        if (length == 2 && start[1] == '-') {
            result->parsing_stopped = 1;
            continue;
        }

        if (start[1] == '-') {
            const char *name_start = start + 2;
            u32 name_length = length - 2;
            const char *equals = NULL;
            for (u32 i = 0; i < name_length; i++) {
                if (name_start[i] == '=') { equals = name_start + i; break; }
            }
            if (equals) name_length = (u32)(equals - name_start);
            char name[GSH_OPTION_NAME_MAX];
            if (!copy_token(name_start, name_length, name, sizeof(name))) {
                set_error(result, GSH_OPTIONS_INVALID_ARGUMENT, name_start, 0); return result->error;
            }
            const gsh_option_spec_t *spec = find_spec(specs, spec_count, 0, name);
            if (!spec) { set_error(result, GSH_OPTIONS_UNKNOWN_OPTION, name, 0); return result->error; }
            const char *value = equals ? equals + 1 : NULL;
            if (!value && spec->requires_argument) {
                const char *next = cursor;
                while (*next == ' ' || *next == '\t') next++;
                if (!*next) { set_error(result, GSH_OPTIONS_MISSING_ARGUMENT, name, 0); return result->error; }
                cursor = next;
                value = cursor;
                while (*cursor && *cursor != ' ' && *cursor != '\t') cursor++;
                char value_copy[GSH_OPTION_VALUE_MAX];
                if (!copy_token(value, (u32)(cursor - value), value_copy, sizeof(value_copy))) {
                    set_error(result, GSH_OPTIONS_INVALID_ARGUMENT, name, 0); return result->error;
                }
                gsh_options_error_t error = add_option(result, spec, 0, name, value_copy);
                if (error != GSH_OPTIONS_OK) { set_error(result, error, name, 0); return error; }
                continue;
            }
            if (spec->requires_argument && (!value || !*value)) {
                set_error(result, GSH_OPTIONS_MISSING_ARGUMENT, name, 0); return result->error;
            }
            if (!value && !spec->optional_argument) value = NULL;
            gsh_options_error_t error = add_option(result, spec, 0, name, value);
            if (error != GSH_OPTIONS_OK) { set_error(result, error, name, 0); return error; }
            continue;
        }

        for (u32 i = 1; i < length; i++) {
            char short_name = start[i];
            const gsh_option_spec_t *spec = find_spec(specs, spec_count, short_name, NULL);
            if (!spec) { set_error(result, GSH_OPTIONS_UNKNOWN_OPTION, NULL, short_name); return result->error; }
            const char *value = NULL;
            if (spec->requires_argument || spec->optional_argument) {
                if (i + 1 < length) {
                    value = start + i + 1;
                    i = length;
                } else if (spec->requires_argument) {
                    const char *next = cursor;
                    while (*next == ' ' || *next == '\t') next++;
                    if (!*next) { set_error(result, GSH_OPTIONS_MISSING_ARGUMENT, NULL, short_name); return result->error; }
                    cursor = next;
                    value = cursor;
                    while (*cursor && *cursor != ' ' && *cursor != '\t') cursor++;
                    char value_copy[GSH_OPTION_VALUE_MAX];
                    if (!copy_token(value, (u32)(cursor - value), value_copy, sizeof(value_copy))) {
                        set_error(result, GSH_OPTIONS_INVALID_ARGUMENT, NULL, short_name); return result->error;
                    }
                    value = value_copy;
                }
                gsh_options_error_t error = add_option(result, spec, short_name, NULL, value);
                if (error != GSH_OPTIONS_OK) { set_error(result, error, NULL, short_name); return error; }
                break;
            }
            gsh_options_error_t error = add_option(result, spec, short_name, NULL, NULL);
            if (error != GSH_OPTIONS_OK) { set_error(result, error, NULL, short_name); return error; }
        }
    }
    return GSH_OPTIONS_OK;
}

const gsh_option_t *gsh_find_option(const gsh_options_t *parsed, char short_name,
                                    const char *long_name) {
    if (!parsed) return NULL;
    for (u32 i = 0; i < parsed->option_count; i++) {
        const gsh_option_t *option = &parsed->options[i];
        if ((long_name && strcmp(option->long_name, long_name) == 0) ||
            (!long_name && option->short_name == short_name)) return option;
    }
    return NULL;
}
