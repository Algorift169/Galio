#include "script.h"
#include "vfs.h"
#include "path.h"
#include "kprintf.h"
#include "string.h"

#define GSH_SCRIPT_MAX_SOURCE 16384
#define GSH_SCRIPT_MAX_LINES 256
#define GSH_SCRIPT_MAX_VARIABLES 64
#define GSH_SCRIPT_MAX_NAME 32
#define GSH_SCRIPT_MAX_COMMAND 256
#define GSH_SCRIPT_MAX_STRING 128
#define GSH_SCRIPT_MAX_ARRAY 32
#define GSH_SCRIPT_MAX_FUNCTIONS 16
#define GSH_SCRIPT_MAX_PARAMETERS 8

typedef enum { SCRIPT_INTEGER, SCRIPT_STRING, SCRIPT_ARRAY } script_value_type_t;
typedef struct {
    char name[GSH_SCRIPT_MAX_NAME];
    char string_value[GSH_SCRIPT_MAX_STRING];
    i32 value;
    i32 array[GSH_SCRIPT_MAX_ARRAY];
    u32 array_count;
    script_value_type_t type;
    u8 used;
} script_variable_t;
typedef struct {
    char name[GSH_SCRIPT_MAX_NAME];
    char parameters[GSH_SCRIPT_MAX_PARAMETERS][GSH_SCRIPT_MAX_NAME];
    u32 parameter_count;
    u32 first_line;
    u32 last_line;
    u8 used;
} script_function_t;
typedef struct {
    const char *cursor;
    script_variable_t variables[GSH_SCRIPT_MAX_VARIABLES];
    script_function_t functions[GSH_SCRIPT_MAX_FUNCTIONS];
} expression_t;
typedef struct { const char *start; const char *end; } script_line_t;

static expression_t interactive_expression;
static u8 interactive_expression_initialized = 0;

static script_variable_t *script_variable(expression_t *expr, const char *name, u32 length, u8 create) {
    u32 i;
    for (i = 0; i < GSH_SCRIPT_MAX_VARIABLES; i++) {
        if (expr->variables[i].used && strlen(expr->variables[i].name) == length &&
            strncmp(expr->variables[i].name, name, length) == 0) return &expr->variables[i];
    }
    if (!create) return NULL;
    for (i = 0; i < GSH_SCRIPT_MAX_VARIABLES; i++) {
        if (!expr->variables[i].used) {
            u32 copy = length < GSH_SCRIPT_MAX_NAME - 1 ? length : GSH_SCRIPT_MAX_NAME - 1;
            memcpy(expr->variables[i].name, name, copy);
            expr->variables[i].name[copy] = 0;
            expr->variables[i].type = SCRIPT_INTEGER;
            expr->variables[i].used = 1;
            return &expr->variables[i];
        }
    }
    return NULL;
}

static void expression_spaces(expression_t *expr) {
    while (*expr->cursor == ' ' || *expr->cursor == '\t') expr->cursor++;
}

static i32 expression_parse(expression_t *expr, int minimum_precedence, char *lvalue, u32 lvalue_size);

static i32 expression_primary(expression_t *expr, char *lvalue, u32 lvalue_size) {
    i32 value = 0;
    expression_spaces(expr);
    lvalue[0] = 0;
    if (*expr->cursor == '(') {
        expr->cursor++;
        value = expression_parse(expr, 1, lvalue, lvalue_size);
        expression_spaces(expr);
        if (*expr->cursor == ')') expr->cursor++;
        return value;
    }
    if (*expr->cursor >= '0' && *expr->cursor <= '9') {
        while (*expr->cursor >= '0' && *expr->cursor <= '9') {
            value = value * 10 + (*expr->cursor - '0');
            expr->cursor++;
        }
        return value;
    }
    if ((*expr->cursor >= 'a' && *expr->cursor <= 'z') || (*expr->cursor >= 'A' && *expr->cursor <= 'Z') || *expr->cursor == '_') {
        const char *start = expr->cursor;
        u32 length = 0;
        while ((*expr->cursor >= 'a' && *expr->cursor <= 'z') || (*expr->cursor >= 'A' && *expr->cursor <= 'Z') ||
               (*expr->cursor >= '0' && *expr->cursor <= '9') || *expr->cursor == '_') { expr->cursor++; length++; }
        if (length < lvalue_size) { memcpy(lvalue, start, length); lvalue[length] = 0; }
        if (length == 4 && strncmp(start, "true", 4) == 0) return 1;
        if (length == 5 && strncmp(start, "false", 5) == 0) return 0;
        {
            script_variable_t *variable = script_variable(expr, start, length, 1);
            expression_spaces(expr);
            if (variable && variable->type == SCRIPT_ARRAY && *expr->cursor == '[') {
                char index_name[GSH_SCRIPT_MAX_NAME];
                i32 index;
                expr->cursor++;
                index = expression_parse(expr, 1, index_name, sizeof(index_name));
                expression_spaces(expr);
                if (*expr->cursor == ']') expr->cursor++;
                lvalue[0] = 0;
                return index >= 0 && (u32)index < variable->array_count ? variable->array[index] : 0;
            }
            return variable && variable->type == SCRIPT_INTEGER ? variable->value : 0;
        }
    }
    return 0;
}

static i32 expression_unary(expression_t *expr, char *lvalue, u32 lvalue_size) {
    expression_spaces(expr);
    if (strncmp(expr->cursor, "not ", 4) == 0) {
        expr->cursor += 4;
        return !expression_unary(expr, lvalue, lvalue_size);
    }
    if ((*expr->cursor == '+' || *expr->cursor == '-') && expr->cursor[1] == *expr->cursor) {
        char name[GSH_SCRIPT_MAX_NAME];
        i32 value;
        char op = *expr->cursor;
        expr->cursor += 2;
        value = expression_unary(expr, name, sizeof(name)) + (op == '+' ? 1 : -1);
        if (name[0]) { script_variable_t *variable = script_variable(expr, name, strlen(name), 1); if (variable) variable->value = value; }
        lvalue[0] = 0;
        return value;
    }
    if (*expr->cursor == '!' || *expr->cursor == '~' || *expr->cursor == '-' || *expr->cursor == '+') {
        char op = *expr->cursor++;
        i32 value = expression_unary(expr, lvalue, lvalue_size);
        if (op == '!') return !value;
        if (op == '~') return ~value;
        if (op == '-') return -value;
        return value;
    }
    return expression_primary(expr, lvalue, lvalue_size);
}

static int expression_precedence(const char *cursor, int *width) {
    *width = 1;
    if ((strncmp(cursor, "and", 3) == 0 || strncmp(cursor, "or", 2) == 0 ||
         strncmp(cursor, "in", 2) == 0 || strncmp(cursor, "is", 2) == 0) &&
        (cursor[2] == ' ' || cursor[2] == '\t' || cursor[2] == 0)) {
        if (strncmp(cursor, "and", 3) == 0) { *width = 3; return 3; }
        if (strncmp(cursor, "or", 2) == 0) { *width = 2; return 2; }
        *width = 2; return 7;
    }
    if ((cursor[0] == '=' && cursor[1] == '=') || (cursor[0] == '!' && cursor[1] == '=') ||
        (cursor[0] == '<' && cursor[1] == '=') || (cursor[0] == '>' && cursor[1] == '=') ||
        (cursor[0] == '&' && cursor[1] == '&') || (cursor[0] == '|' && cursor[1] == '|') ||
        (cursor[0] == '<' && cursor[1] == '<') || (cursor[0] == '>' && cursor[1] == '>') ||
        (cursor[0] == '+' && cursor[1] == '=') || (cursor[0] == '-' && cursor[1] == '=') ||
        (cursor[0] == '*' && cursor[1] == '=') || (cursor[0] == '/' && cursor[1] == '=') ||
        (cursor[0] == '%' && cursor[1] == '=')) *width = 2;
    if ((*cursor == '=' && cursor[1] != '=') || (cursor[0] == '+' && cursor[1] == '=') || (cursor[0] == '-' && cursor[1] == '=') ||
        (cursor[0] == '*' && cursor[1] == '=') || (cursor[0] == '/' && cursor[1] == '=') || (cursor[0] == '%' && cursor[1] == '=')) return 1;
    if (cursor[0] == '|' && cursor[1] == '|') return 2;
    if (cursor[0] == '&' && cursor[1] == '&') return 3;
    if (*cursor == '|' && cursor[1] != '|') return 4;
    if (*cursor == '^') return 5;
    if (*cursor == '&' && cursor[1] != '&') return 6;
    if ((cursor[0] == '=' && cursor[1] == '=') || (cursor[0] == '!' && cursor[1] == '=')) return 7;
    if ((cursor[0] == '<' || cursor[0] == '>') && cursor[1] != cursor[0] && cursor[1] != '=') return 8;
    if ((cursor[0] == '<' && cursor[1] == '<') || (cursor[0] == '>' && cursor[1] == '>')) return 9;
    if (*cursor == '+' || *cursor == '-') return 10;
    if (*cursor == '*' || *cursor == '/' || *cursor == '%') return 11;
    return 0;
}

static i32 expression_apply(i32 left, i32 right, const char *op) {
    if (strcmp(op, "=") == 0) return right;
    if (strcmp(op, "+=") == 0) return left + right;
    if (strcmp(op, "-=") == 0) return left - right;
    if (strcmp(op, "*=") == 0) return left * right;
    if (strcmp(op, "/=") == 0) return right ? left / right : 0;
    if (strcmp(op, "%=") == 0) return right ? left % right : 0;
    if (strcmp(op, "||") == 0) return left || right;
    if (strcmp(op, "&&") == 0) return left && right;
    if (strcmp(op, "and") == 0) return left && right;
    if (strcmp(op, "or") == 0) return left || right;
    if (strcmp(op, "is") == 0) return left == right;
    if (strcmp(op, "in") == 0) return left == right;
    if (strcmp(op, "==") == 0) return left == right;
    if (strcmp(op, "!=") == 0) return left != right;
    if (strcmp(op, "<=") == 0) return left <= right;
    if (strcmp(op, ">=") == 0) return left >= right;
    if (strcmp(op, "<<") == 0) return left << right;
    if (strcmp(op, ">>") == 0) return left >> right;
    if (strcmp(op, "|") == 0) return left | right;
    if (strcmp(op, "^") == 0) return left ^ right;
    if (strcmp(op, "&") == 0) return left & right;
    if (strcmp(op, "<") == 0) return left < right;
    if (strcmp(op, ">") == 0) return left > right;
    if (strcmp(op, "+") == 0) return left + right;
    if (strcmp(op, "-") == 0) return left - right;
    if (strcmp(op, "*") == 0) return left * right;
    if (strcmp(op, "/") == 0) return right ? left / right : 0;
    if (strcmp(op, "%") == 0) return right ? left % right : 0;
    return 0;
}

static i32 expression_parse(expression_t *expr, int minimum_precedence, char *lvalue, u32 lvalue_size) {
    i32 left = expression_unary(expr, lvalue, lvalue_size);
    expression_spaces(expr);
    if (lvalue[0] && (expr->cursor[0] == '+' || expr->cursor[0] == '-') &&
        expr->cursor[1] == expr->cursor[0]) {
        script_variable_t *variable = script_variable(expr, lvalue, strlen(lvalue), 1);
        if (variable) {
            variable->value += expr->cursor[0] == '+' ? 1 : -1;
            left = variable->value;
        }
        expr->cursor += 2;
        lvalue[0] = 0;
    }
    for (;;) {
        int width, precedence;
        char op[4];
        char left_name[GSH_SCRIPT_MAX_NAME];
        expression_spaces(expr);
        precedence = expression_precedence(expr->cursor, &width);
        if (precedence < minimum_precedence || precedence == 0) break;
        op[0] = expr->cursor[0];
        op[1] = width > 1 ? expr->cursor[1] : 0;
        op[2] = width > 2 ? expr->cursor[2] : 0;
        op[3] = 0;
        memcpy(left_name, lvalue, sizeof(left_name));
        expr->cursor += width;
        { char right_name[GSH_SCRIPT_MAX_NAME]; i32 right = expression_parse(expr, precedence + (precedence == 1 ? 0 : 1), right_name, sizeof(right_name));
          left = expression_apply(left, right, op);
          if (precedence == 1 && left_name[0]) { script_variable_t *variable = script_variable(expr, left_name, strlen(left_name), 1); if (variable) variable->value = left; } }
        lvalue[0] = 0;
    }
    expression_spaces(expr);
    if (*expr->cursor == '?' && minimum_precedence <= 1) {
        i32 when_true;
        i32 when_false;
        char unused[GSH_SCRIPT_MAX_NAME];
        expr->cursor++;
        when_true = expression_parse(expr, 1, unused, sizeof(unused));
        expression_spaces(expr);
        if (*expr->cursor == ':') expr->cursor++;
        when_false = expression_parse(expr, 1, unused, sizeof(unused));
        left = left ? when_true : when_false;
        lvalue[0] = 0;
    }
    return left;
}

static int script_expression(expression_t *expr, const char *text) {
    char name[GSH_SCRIPT_MAX_NAME];
    expr->cursor = text;
    return expression_parse(expr, 1, name, sizeof(name));
}

static script_variable_t *script_find_variable(expression_t *expr, const char *name) {
    return script_variable(expr, name, strlen(name), 0);
}

static void script_set_string(expression_t *expr, const char *name, const char *value) {
    script_variable_t *variable = script_variable(expr, name, strlen(name), 1);
    if (!variable) return;
    strncpy(variable->string_value, value ? value : "", GSH_SCRIPT_MAX_STRING - 1);
    variable->string_value[GSH_SCRIPT_MAX_STRING - 1] = 0;
    variable->type = SCRIPT_STRING;
}

static void script_set_array(expression_t *expr, const char *name, const char *text) {
    script_variable_t *variable = script_variable(expr, name, strlen(name), 1);
    const char *cursor = text;
    if (!variable) return;
    variable->array_count = 0;
    while (*cursor && variable->array_count < GSH_SCRIPT_MAX_ARRAY) {
        i32 value = 0;
        while (*cursor == ' ' || *cursor == '\t' || *cursor == ',' || *cursor == '{' || *cursor == '[') cursor++;
        if (*cursor == ']' || *cursor == '}') break;
        while (*cursor >= '0' && *cursor <= '9') { value = value * 10 + (*cursor - '0'); cursor++; }
        variable->array[variable->array_count++] = value;
        while (*cursor && *cursor != ',' && *cursor != ']' && *cursor != '}') cursor++;
    }
    variable->type = SCRIPT_ARRAY;
}

static void script_render_text(expression_t *expr, const char *text) {
    while (*text) {
        if (*text == '{') {
            const char *end = text + 1;
            char name[GSH_SCRIPT_MAX_NAME];
            u32 length = 0;
            while (*end && *end != '}' && length + 1 < sizeof(name)) name[length++] = *end++;
            if (*end == '}' && length > 0) {
                script_variable_t *variable;
                name[length] = 0;
                variable = script_find_variable(expr, name);
                if (variable && variable->type == SCRIPT_STRING) kprintf("%s", variable->string_value);
                else if (variable) kprintf("%d", variable->value);
                text = end + 1;
                continue;
            }
        }
        kprintf("%c", *text++);
    }
}

static void script_declare_one(expression_t *expr, const char *text, u32 length) {
    char part[GSH_SCRIPT_MAX_COMMAND];
    char name[GSH_SCRIPT_MAX_NAME];
    const char *cursor;
    const char *equals;
    u32 part_length = length;
    u32 name_length = 0;

    if (part_length >= sizeof(part)) part_length = sizeof(part) - 1;
    memcpy(part, text, part_length);
    part[part_length] = 0;
    cursor = part;
    while (*cursor == ' ' || *cursor == '\t') cursor++;
    while (cursor[name_length] && cursor[name_length] != ' ' && cursor[name_length] != '\t' &&
           cursor[name_length] != '=' && cursor[name_length] != '[') name_length++;
    if (name_length == 0) return;
    if (name_length >= sizeof(name)) name_length = sizeof(name) - 1;
    memcpy(name, cursor, name_length);
    name[name_length] = 0;
    equals = cursor + name_length;
    while (*equals == ' ' || *equals == '\t' || *equals == '[' || *equals == ']') equals++;
    if (*equals == '=') equals++;
    while (*equals == ' ' || *equals == '\t') equals++;

    if (*equals == '"' || *equals == '\'') {
        char quote = *equals++;
        char value[GSH_SCRIPT_MAX_STRING];
        u32 used = 0;
        while (*equals && *equals != quote && used + 1 < sizeof(value)) value[used++] = *equals++;
        value[used] = 0;
        script_set_string(expr, name, value);
    } else if (*equals == '[' || *equals == '{') {
        script_set_array(expr, name, equals);
    } else {
        script_variable_t *variable = script_variable(expr, name, name_length, 1);
        if (variable) {
            variable->value = *equals ? script_expression(expr, equals) : 0;
            variable->type = SCRIPT_INTEGER;
        }
    }
}

static void script_declare_variables(expression_t *expr, const char *text) {
    const char *start = text;
    const char *cursor = text;
    int in_single_quote = 0;
    int in_double_quote = 0;
    u32 bracket_depth = 0;

    while (*cursor) {
        if (*cursor == '\'' && !in_double_quote) in_single_quote = !in_single_quote;
        else if (*cursor == '"' && !in_single_quote) in_double_quote = !in_double_quote;
        else if (!in_single_quote && !in_double_quote && (*cursor == '[' || *cursor == '{')) bracket_depth++;
        else if (!in_single_quote && !in_double_quote && (*cursor == ']' || *cursor == '}') && bracket_depth > 0) bracket_depth--;
        else if (!in_single_quote && !in_double_quote && bracket_depth == 0 && *cursor == ',') {
            script_declare_one(expr, start, (u32)(cursor - start));
            start = cursor + 1;
        }
        cursor++;
    }
    if (cursor > start) script_declare_one(expr, start, (u32)(cursor - start));
}

static int script_is_expression_statement(const char *text) {
    const char *cursor = text;
    int has_operator = 0;
    while (*cursor == ' ' || *cursor == '\t') cursor++;
    if (!*cursor) return 0;
    if (*cursor == '(' || (*cursor >= '0' && *cursor <= '9')) has_operator = 1;
    while (*cursor) {
        if (*cursor == '+' || *cursor == '-' || *cursor == '*' || *cursor == '/' ||
            *cursor == '%' || *cursor == '&' || *cursor == '|' || *cursor == '^' ||
            *cursor == '<' || *cursor == '>' || *cursor == '=' || *cursor == '!' ||
            *cursor == '?' || *cursor == '~') {
            has_operator = 1;
            break;
        }
        cursor++;
    }
    return has_operator;
}

static int script_handle_drift_line(const char *start, const char *end, expression_t *expr) {
    char text[GSH_SCRIPT_MAX_COMMAND];
    u32 length = (u32)(end - start);
    const char *cursor;
    if (length >= sizeof(text)) length = sizeof(text) - 1;
    memcpy(text, start, length);
    text[length] = 0;
    if (strncmp(text, "say", 3) == 0 && (text[3] == ' ' || text[3] == '\t' || text[3] == 0)) {
        cursor = text + 3;
        while (*cursor == ' ' || *cursor == '\t') cursor++;
        if (*cursor == '"' || *cursor == '\'') {
            char quote = *cursor++;
            char value[GSH_SCRIPT_MAX_STRING];
            u32 used = 0;
            while (*cursor && *cursor != quote && used + 1 < sizeof(value)) value[used++] = *cursor++;
            value[used] = 0;
            script_render_text(expr, value);
            kprintf("\n");
        } else {
            script_variable_t *variable = script_find_variable(expr, cursor);
            if (variable && variable->type == SCRIPT_STRING) kprintf("%s\n", variable->string_value);
            else if (variable && variable->type == SCRIPT_ARRAY) kprintf("[array]\n");
            else kprintf("%d\n", script_expression(expr, cursor));
        }
        return 1;
    }
    if (strncmp(text, "var ", 4) == 0) {
        script_declare_variables(expr, text + 4);
        return 1;
    }
    if ((*text >= 'a' && *text <= 'z') || (*text >= 'A' && *text <= 'Z') || *text == '_') {
        const char *equals = text;
        char name[GSH_SCRIPT_MAX_NAME];
        u32 name_length = 0;
        while (*equals && *equals != '=') equals++;
        if (*equals == '=' && equals != text && equals[-1] != '=' && equals[1] != '=') {
            while (text[name_length] && text + name_length < equals && text[name_length] != ' ') name_length++;
            if (name_length >= sizeof(name)) name_length = sizeof(name) - 1;
            memcpy(name, text, name_length); name[name_length] = 0;
            equals++;
            while (*equals == ' ' || *equals == '\t') equals++;
            if (*equals == '"' || *equals == '\'') {
                char quote = *equals++;
                char value[GSH_SCRIPT_MAX_STRING];
                u32 used = 0;
                while (*equals && *equals != quote && used + 1 < sizeof(value)) value[used++] = *equals++;
                value[used] = 0;
                script_set_string(expr, name, value);
            } else if (*equals == '[' || *equals == '{') {
                script_set_array(expr, name, equals);
            } else {
                script_expression(expr, text);
            }
            return 1;
        }
    }
    if (script_is_expression_statement(text)) {
        script_expression(expr, text);
        return 1;
    }
    return 0;
}

static void script_expand(const char *source, char *out, u32 size, expression_t *expr);
static const char *script_find_text(const char *text, const char *needle);

static const char *script_find_colon(const char *text)
{
    int in_single_quote = 0;
    int in_double_quote = 0;

    while (*text) {
        if (*text == '\'' && !in_double_quote) in_single_quote = !in_single_quote;
        else if (*text == '"' && !in_single_quote) in_double_quote = !in_double_quote;
        else if (*text == ':' && !in_single_quote && !in_double_quote) return text;
        text++;
    }
    return NULL;
}

static char *script_find_character(char *text, char character)
{
    while (*text && *text != character) text++;
    return *text == character ? text : NULL;
}

static char *script_find_last_character(char *text, char character)
{
    char *result = NULL;
    while (*text) {
        if (*text == character) result = text;
        text++;
    }
    return result;
}

static int script_execute_body(const char *body, expression_t *expr,
                               gsh_script_command_fn execute, void *context)
{
    char command[GSH_SCRIPT_MAX_COMMAND];
    u32 length;

    while (*body == ' ' || *body == '\t') body++;
    if (*body == '\0') return 0;
    if (script_handle_drift_line(body, body + strlen(body), expr)) return 1;
    if (!execute) return 0;
    length = strlen(body);
    if (length >= sizeof(command)) length = sizeof(command) - 1;
    memcpy(command, body, length);
    command[length] = 0;
    script_expand(command, command, sizeof(command), expr);
    return execute(command, context) == 0;
}

static int script_execute_inline_repeat(const char *line, expression_t *expr,
                                        gsh_script_command_fn execute, void *context)
{
    const char *header = line + 6;
    const char *colon = script_find_colon(header);
    const char *body;
    char header_text[GSH_SCRIPT_MAX_COMMAND];
    char *open;
    char *close;
    char *range;
    char *end_expression;
    char *step_expression = NULL;
    char counter[GSH_SCRIPT_MAX_NAME];
    i32 start;
    i32 end;
    i32 step;
    i32 value;
    int exclusive_upper = 0;
    int exclusive_lower = 0;
    u32 length;

    if (!colon) return 0;
    body = colon + 1;
    while (*header == ' ' || *header == '\t') header++;
    length = (u32)(colon - header);
    while (length > 0 && (header[length - 1] == ' ' || header[length - 1] == '\t')) length--;
    if (length == 0 || length >= sizeof(header_text)) return 0;
    memcpy(header_text, header, length);
    header_text[length] = 0;

    /* `repeat 10 : command` is the count form; its counter is not exposed. */
    if (script_find_character(header_text, '(') == NULL) {
        i32 count = script_expression(expr, header_text);
        if (count < 0 || count > 100000) return 0;
        for (value = 0; value < count; value++) {
            if (!script_execute_body(body, expr, execute, context)) return 0;
        }
        return 1;
    }

    open = script_find_character(header_text, '(');
    close = script_find_last_character(header_text, ')');
    if (!close || close < open) return 0;
    *open = 0;
    while (*header_text == ' ' || *header_text == '\t') memmove(header_text, header_text + 1, strlen(header_text));
    if (strncmp(header_text, "var ", 4) == 0) memmove(header_text, header_text + 4, strlen(header_text + 4) + 1);
    length = strlen(header_text);
    while (length > 0 && (header_text[length - 1] == ' ' || header_text[length - 1] == '\t')) header_text[--length] = 0;
    if (length == 0 || length >= sizeof(counter)) return 0;
    strcpy(counter, header_text);
    *close = 0;
    range = (char *)script_find_text(open + 1, "...");
    if (range) {
        if (range[3] == '<') exclusive_upper = 1;
        else if (range[3] == '>') exclusive_lower = 1;
    } else {
        range = (char *)script_find_text(open + 1, "..");
    }
    if (!range) return 0;
    if (range[0] == '.' && range[1] == '.' && range[2] == '.') end_expression = range + 3 + (exclusive_upper || exclusive_lower);
    else end_expression = range + 2;
    *range = 0;
    {
        char *comma = script_find_character(end_expression, ',');
        if (comma) {
            *comma = 0;
            step_expression = comma + 1;
        }
    }
    start = script_expression(expr, open + 1);
    end = script_expression(expr, end_expression);
    if (step_expression) {
        script_variable_t *variable = script_variable(expr, counter, strlen(counter), 1);
        if (!variable) return 0;
        variable->type = SCRIPT_INTEGER;
        variable->value = start;
        step = script_expression(expr, step_expression) - start;
    } else {
        step = start <= end ? 1 : -1;
    }
    if (step == 0) return 0;
    for (value = start; (step > 0 && (exclusive_upper ? value < end : value <= end)) ||
                        (step < 0 && (exclusive_lower ? value > end : value >= end)); value += step) {
        script_variable_t *variable = script_variable(expr, counter, strlen(counter), 1);
        if (!variable) return 0;
        variable->type = SCRIPT_INTEGER;
        variable->value = value;
        if (!script_execute_body(body, expr, execute, context)) return 0;
        if (value > 100000000 - step || value < -100000000 - step) return 0;
    }
    return 1;
}

static int script_execute_inline_condition(const char *line, expression_t *expr,
                                           gsh_script_command_fn execute, void *context)
{
    const char *keyword = line;
    const char *colon;
    const char *condition_start;
    char condition[GSH_SCRIPT_MAX_COMMAND];
    u32 length;
    int is_unless = 0;
    int is_while = 0;
    int condition_value;
    u32 iterations = 0;

    if (strncmp(keyword, "unless", 6) == 0 && (keyword[6] == ' ' || keyword[6] == '\t')) {
        condition_start = keyword + 6;
        is_unless = 1;
    } else if (strncmp(keyword, "while", 5) == 0 && (keyword[5] == ' ' || keyword[5] == '\t')) {
        condition_start = keyword + 5;
        is_while = 1;
    } else if (strncmp(keyword, "if", 2) == 0 && (keyword[2] == ' ' || keyword[2] == '\t')) {
        condition_start = keyword + 2;
    } else {
        return 0;
    }
    colon = script_find_colon(condition_start);
    if (!colon) return 0;
    while (*condition_start == ' ' || *condition_start == '\t') condition_start++;
    length = (u32)(colon - condition_start);
    while (length > 0 && (condition_start[length - 1] == ' ' || condition_start[length - 1] == '\t')) length--;
    if (length == 0 || length >= sizeof(condition)) return 0;
    memcpy(condition, condition_start, length);
    condition[length] = 0;

    if (is_while) {
        while (script_expression(expr, condition) != 0 && iterations++ < 100000) {
            if (!script_execute_body(colon + 1, expr, execute, context)) return 0;
        }
        return iterations < 100000;
    }

    condition_value = script_expression(expr, condition) != 0;
    if (is_unless) condition_value = !condition_value;
    return condition_value ? script_execute_body(colon + 1, expr, execute, context) : 1;
}

static int script_execute_inline_for(const char *line, expression_t *expr,
                                     gsh_script_command_fn execute, void *context) {
    const char *header = line + 3;
    const char *colon = header;
    const char *body;
    char clauses[3][GSH_SCRIPT_MAX_COMMAND];
    u32 clause_index = 0;
    u32 clause_length = 0;
    int in_single_quote = 0;
    int in_double_quote = 0;
    u32 iterations = 0;

    while (*colon) {
        if (*colon == '\'' && !in_double_quote) in_single_quote = !in_single_quote;
        else if (*colon == '"' && !in_single_quote) in_double_quote = !in_double_quote;
        if (!in_single_quote && !in_double_quote && *colon == ':') break;
        colon++;
    }
    if (*colon != ':') return 0;
    body = colon + 1;
    while (*body == ' ' || *body == '\t') body++;
    while (*header == ' ' || *header == '\t' || *header == '(') header++;
    {
        const char *end = colon;
        while (end > header && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == ')')) end--;
        while (*header && header < end && clause_index < 3) {
            if (*header == '\'' && !in_double_quote) in_single_quote = !in_single_quote;
            else if (*header == '"' && !in_single_quote) in_double_quote = !in_double_quote;
            if (!in_single_quote && !in_double_quote && *header == ',') {
                clauses[clause_index][clause_length] = 0;
                clause_index++;
                clause_length = 0;
                header++;
                while (*header == ' ' || *header == '\t') header++;
                continue;
            }
            if (clause_length + 1 < sizeof(clauses[0])) clauses[clause_index][clause_length++] = *header;
            header++;
        }
        if (clause_index < 3) {
            clauses[clause_index][clause_length] = 0;
            clause_index++;
        }
    }
    if (clause_index != 3 || *body == 0) return 0;
    while (clauses[0][0] == ' ' || clauses[0][0] == '\t') memmove(clauses[0], clauses[0] + 1, strlen(clauses[0]));
    if (strncmp(clauses[0], "var ", 4) == 0) script_handle_drift_line(clauses[0], clauses[0] + strlen(clauses[0]), expr);
    else script_expression(expr, clauses[0]);
    while (script_expression(expr, clauses[1]) != 0 && iterations++ < 100000) {
        if (!script_handle_drift_line(body, body + strlen(body), expr)) {
            char command[GSH_SCRIPT_MAX_COMMAND];
            u32 length = strlen(body);
            if (length >= sizeof(command)) length = sizeof(command) - 1;
            memcpy(command, body, length);
            command[length] = 0;
            script_expand(command, command, sizeof(command), expr);
            if (!execute || execute(command, context) != 0) return 0;
        }
        script_expression(expr, clauses[2]);
    }
    return 1;
}

int gsh_script_execute_line_with_command(const char *line,
                                         gsh_script_command_fn execute,
                                         void *context) {
    const char *end;
    if (!line) return 0;
    if (!interactive_expression_initialized) {
        memset(&interactive_expression, 0, sizeof(interactive_expression));
        interactive_expression_initialized = 1;
    }
    if (strncmp(line, "repeat", 6) == 0 && (line[6] == ' ' || line[6] == '\t')) {
        if (script_execute_inline_repeat(line, &interactive_expression, execute, context)) return 1;
    }
    if (script_execute_inline_condition(line, &interactive_expression, execute, context)) return 1;
    if (strncmp(line, "for", 3) == 0 && (line[3] == ' ' || line[3] == '(')) {
        if (script_execute_inline_for(line, &interactive_expression, execute, context)) return 1;
    }
    end = line;
    while (*end) end++;
    if (script_handle_drift_line(line, end, &interactive_expression)) return 1;
    if (execute) {
        char command[GSH_SCRIPT_MAX_COMMAND];
        u32 length = (u32)(end - line);
        if (length >= sizeof(command)) length = sizeof(command) - 1;
        memcpy(command, line, length);
        command[length] = 0;
        script_expand(command, command, sizeof(command), &interactive_expression);
        return execute(command, context) == 0;
    }
    return 0;
}

int gsh_script_execute_line(const char *line) {
    return gsh_script_execute_line_with_command(line, NULL, NULL);
}

static int script_find_function(expression_t *expr, const char *name) {
    u32 i;
    for (i = 0; i < GSH_SCRIPT_MAX_FUNCTIONS; i++) {
        if (expr->functions[i].used && strcmp(expr->functions[i].name, name) == 0) return (int)i;
    }
    return -1;
}

static void script_trim(const char **start, const char **end) {
    while (*start < *end && (**start == ' ' || **start == '\t')) (*start)++;
    while (*end > *start && (*(*end - 1) == ' ' || *(*end - 1) == '\t')) (*end)--;
}

static int script_line_is(const script_line_t *line, const char *word) {
    const char *start = line->start; const char *end = line->end;
    script_trim(&start, &end);
    return (u32)(end - start) == strlen(word) && strncmp(start, word, strlen(word)) == 0;
}

static int script_line_starts(const script_line_t *line, const char *word) {
    const char *start = line->start;
    const char *end = line->end;
    u32 length = strlen(word);
    script_trim(&start, &end);
    return (u32)(end - start) >= length && strncmp(start, word, length) == 0;
}

static const char *script_find_text(const char *text, const char *needle) {
    u32 needle_length = strlen(needle);
    if (needle_length == 0) return text;
    while (*text) {
        if (strncmp(text, needle, needle_length) == 0) return text;
        text++;
    }
    return NULL;
}

static int script_is_block_opener(const script_line_t *line) {
    return script_line_starts(line, "if ") || script_line_starts(line, "unless ") ||
           script_line_starts(line, "while ") || script_line_starts(line, "for ") ||
           script_line_starts(line, "repeat ") || script_line_starts(line, "each ") ||
           script_line_starts(line, "when ") || script_line_starts(line, "fun ");
}

static int script_expand_variable(expression_t *expr, const char **cursor,
                                  char *out, u32 *used, u32 size,
                                  char closing) {
    const char *start = *cursor;
    char name[GSH_SCRIPT_MAX_NAME];
    char number[16];
    script_variable_t *variable;
    u32 length = 0;
    u32 number_length = 0;
    i32 value;

    while (((*start >= 'a' && *start <= 'z') || (*start >= 'A' && *start <= 'Z') ||
            (*start >= '0' && *start <= '9') || *start == '_') &&
           length + 1 < sizeof(name)) {
        name[length++] = *start++;
    }
    if (length == 0 || (closing != 0 && *start != closing) ||
        (closing == 0 && ((*start >= 'a' && *start <= 'z') ||
                          (*start >= 'A' && *start <= 'Z') ||
                          (*start >= '0' && *start <= '9') || *start == '_'))) return 0;
    name[length] = 0;
    variable = script_variable(expr, name, length, 0);
    if (!variable || variable->type != SCRIPT_INTEGER) return 0;
    value = variable->value;
    if (value < 0) {
        number[number_length++] = '-';
        value = -value;
    }
    do {
        number[number_length++] = (char)('0' + value % 10);
        value /= 10;
    } while (value && number_length < sizeof(number));
    while (number_length && *used + 1 < size) out[(*used)++] = number[--number_length];
    *cursor = start + (closing != 0 ? 1 : 0);
    return 1;
}

static void script_expand(const char *source, char *out, u32 size, expression_t *expr) {
    char input[GSH_SCRIPT_MAX_COMMAND];
    u32 used = 0, i;
    strncpy(input, source, sizeof(input) - 1); input[sizeof(input) - 1] = 0;
    for (i = 0; input[i] && used + 1 < size; i++) {
        if (input[i] == '$' || input[i] == '{' || input[i] == '(') {
            const char *cursor = input + i + 1;
            char closing = input[i] == '{' ? '}' : input[i] == '(' ? ')' : 0;
            if (*cursor == '{') {
                closing = '}';
                cursor++;
            }
            if (script_expand_variable(expr, &cursor, out, &used, size, closing)) {
                i = (u32)(cursor - input) - 1;
                continue;
            }
        }
        out[used++] = input[i];
    }
    out[used] = 0;
}

static int script_run_range(script_line_t *lines, u32 first, u32 last, expression_t *expr,
                            gsh_script_command_fn execute, void *context, u32 depth) {
    u32 i;
    if (depth > 32) return -1;
    for (i = first; i < last; i++) {
        const char *start = lines[i].start; const char *end = lines[i].end;
        char command[GSH_SCRIPT_MAX_COMMAND];
        script_trim(&start, &end);
        if (start == end || *start == '#') continue;
        if (script_handle_drift_line(start, end, expr)) continue;
        if (script_line_is(&lines[i], "endif") || script_line_is(&lines[i], "endwhile") ||
            script_line_is(&lines[i], "end") || script_line_is(&lines[i], "else") ||
            script_line_starts(&lines[i], "elif ")) continue;
        if (script_line_starts(&lines[i], "fun ")) {
            u32 nested = 1, j, function_index = 0;
            const char *header = start + 4;
            const char *open = header;
            script_function_t *function = NULL;
            while (*open && *open != '(') open++;
            for (function_index = 0; function_index < GSH_SCRIPT_MAX_FUNCTIONS; function_index++) {
                if (!expr->functions[function_index].used) { function = &expr->functions[function_index]; break; }
            }
            if (!function) return -1;
            memset(function, 0, sizeof(*function));
            function->used = 1;
            {
                u32 name_length = (u32)(open - header);
                while (name_length && header[name_length - 1] == ' ') name_length--;
                if (name_length >= sizeof(function->name)) name_length = sizeof(function->name) - 1;
                memcpy(function->name, header, name_length); function->name[name_length] = 0;
            }
            if (*open == '(') {
                const char *parameter = open + 1;
                while (*parameter && *parameter != ')') {
                    while (*parameter == ' ' || *parameter == ',') parameter++;
                    if (*parameter == ')') break;
                    {
                        u32 parameter_length = 0;
                        while (parameter[parameter_length] && parameter[parameter_length] != ',' && parameter[parameter_length] != ')' && parameter[parameter_length] != ' ') parameter_length++;
                        if (function->parameter_count < GSH_SCRIPT_MAX_PARAMETERS) {
                            if (parameter_length >= GSH_SCRIPT_MAX_NAME) parameter_length = GSH_SCRIPT_MAX_NAME - 1;
                            memcpy(function->parameters[function->parameter_count], parameter, parameter_length);
                            function->parameters[function->parameter_count][parameter_length] = 0;
                            function->parameter_count++;
                        }
                        parameter += parameter_length;
                    }
                }
            }
            for (j = i + 1; j < last; j++) {
                if (script_is_block_opener(&lines[j])) nested++;
                if (script_line_is(&lines[j], "end") && --nested == 0) break;
            }
            if (j == last) return -1;
            function->first_line = i + 1;
            function->last_line = j;
            i = j;
            continue;
        }
        if (script_line_starts(&lines[i], "if ") || script_line_starts(&lines[i], "unless ")) {
            u32 nested = 1, j, branch_count = 0, branch_start[8], branch_end[8];
            u8 branch_is_else[8];
            int drift_end = 0;
            branch_start[0] = i + 1;
            for (j = i + 1; j < last; j++) {
                if (script_is_block_opener(&lines[j])) nested++;
                if (script_line_is(&lines[j], "end")) {
                    if (nested == 1) { drift_end = 1; break; }
                    nested--;
                } else if (script_line_is(&lines[j], "endif")) {
                    if (nested == 1) break;
                    nested--;
                }
                if (nested == 1 && (script_line_starts(&lines[j], "elif ") || script_line_is(&lines[j], "else")) && branch_count < 7) {
                    branch_end[branch_count] = j;
                    branch_is_else[branch_count] = script_line_is(&lines[j], "else");
                    branch_count++;
                    branch_start[branch_count] = j + 1;
                }
            }
            if (j == last) return -1;
            branch_end[branch_count] = j;
            {
                u32 branch;
                int selected = 0;
                for (branch = 0; branch <= branch_count; branch++) {
                    int condition = 0;
                    if (branch == 0) condition = script_expression(expr, start + (start[0] == 'u' ? 7 : 3)) != 0;
                    else if (!branch_is_else[branch - 1]) condition = script_expression(expr, lines[branch_end[branch - 1]].start + 4) != 0;
                    else condition = 1;
                    if (!selected && condition) {
                        script_run_range(lines, branch_start[branch], branch_end[branch], expr, execute, context, depth + 1);
                        selected = 1;
                    }
                }
            }
            (void)drift_end;
            i = j; continue;
        }
        if (script_line_starts(&lines[i], "when ")) {
            u32 nested = 1, j, case_count = 0, case_lines[8], case_ends[8];
            i32 subject = script_expression(expr, start + 5);
            for (j = i + 1; j < last; j++) {
                const char *case_start = lines[j].start;
                const char *case_end = lines[j].end;
                script_trim(&case_start, &case_end);
                if (script_is_block_opener(&lines[j])) nested++;
                if (script_line_is(&lines[j], "end")) {
                    if (nested == 1) break;
                    nested--;
                }
                if (nested == 1 && case_count < 8 && case_start < case_end) {
                    const char *colon = case_start;
                    while (colon < case_end && *colon != ':') colon++;
                    if (colon < case_end) {
                        case_lines[case_count] = j;
                        case_ends[case_count] = j + 1;
                        case_count++;
                    }
                }
            }
            if (j == last) return -1;
            {
                u32 case_index;
                int selected = 0;
                for (case_index = 0; case_index < case_count; case_index++) {
                    const char *case_start = lines[case_lines[case_index]].start;
                    const char *case_end = lines[case_lines[case_index]].end;
                    const char *colon = case_start;
                    char case_text[64];
                    u32 case_length;
                    script_trim(&case_start, &case_end);
                    while (colon < case_end && *colon != ':') colon++;
                    case_length = (u32)(colon - case_start);
                    if (case_length >= sizeof(case_text)) case_length = sizeof(case_text) - 1;
                    memcpy(case_text, case_start, case_length); case_text[case_length] = 0;
                    if (case_index + 1 < case_count) case_ends[case_index] = case_lines[case_index + 1];
                    else case_ends[case_index] = j;
                    if (strncmp(case_text, "else", 4) == 0 || (!selected && script_expression(expr, case_text) == subject)) {
                        if (!selected) {
                            script_run_range(lines, case_lines[case_index] + 1, case_ends[case_index], expr, execute, context, depth + 1);
                            selected = 1;
                        }
                    }
                }
            }
            i = j; continue;
        }
        if ((u32)(end - start) > 6 && strncmp(start, "while ", 6) == 0) {
            u32 nested = 1, j;
            for (j = i + 1; j < last; j++) { if (script_is_block_opener(&lines[j])) nested++; if ((script_line_is(&lines[j], "endwhile") || script_line_is(&lines[j], "end")) && --nested == 0) break; }
            if (j == last) return -1;
            while (script_expression(expr, start + 6) != 0) {
                int result = script_run_range(lines, i + 1, j, expr, execute, context, depth + 1);
                if (result == 2 || result == 1) break;
            }
            i = j; continue;
        }
        if (script_line_starts(&lines[i], "repeat ") || script_line_starts(&lines[i], "for ") ||
            script_line_starts(&lines[i], "each ")) {
            u32 nested = 1, j;
            char loop_name[GSH_SCRIPT_MAX_NAME];
            i32 loop_start = 0, loop_end = 0;
            int is_each = script_line_starts(&lines[i], "each ");
            const char *header = start + (is_each ? 5 : (start[0] == 'f' ? 4 : 7));
            for (j = i + 1; j < last; j++) {
                if (script_is_block_opener(&lines[j])) nested++;
                if (script_line_is(&lines[j], "end") && --nested == 0) break;
            }
            if (j == last) return -1;
            if (!is_each && start[0] == 'r') {
                const char *count_start = header;
                const char *count_end = count_start;
                char count_text[64];
                u32 count_length;
                i32 count;
                while (*count_end && *count_end != ':') count_end++;
                while (count_start < count_end && (*count_start == ' ' || *count_start == '\t')) count_start++;
                while (count_end > count_start && (count_end[-1] == ' ' || count_end[-1] == '\t')) count_end--;
                count_length = (u32)(count_end - count_start);
                if (count_length == 0 || count_length >= sizeof(count_text)) return -1;
                memcpy(count_text, count_start, count_length);
                count_text[count_length] = 0;
                count = script_expression(expr, count_text);
                if (count < 0 || count > 100000) return -1;
                for (u32 iteration = 0; iteration < (u32)count; iteration++) {
                    int result = script_run_range(lines, i + 1, j, expr, execute, context, depth + 1);
                    if (result == 1 || result == 2) break;
                    if (result == 3) continue;
                }
            } else if (is_each) {
                const char *name_start = header;
                const char *in_word;
                u32 name_length;
                while (*name_start == ' ') name_start++;
                if (strncmp(name_start, "var ", 4) == 0) name_start += 4;
                in_word = name_start;
                while (*in_word && *in_word != ' ') in_word++;
                name_length = (u32)(in_word - name_start);
                if (name_length >= sizeof(loop_name)) name_length = sizeof(loop_name) - 1;
                memcpy(loop_name, name_start, name_length); loop_name[name_length] = 0;
                while (*in_word == ' ') in_word++;
                if (strncmp(in_word, "in ", 3) == 0) in_word += 3;
                if (script_find_text(in_word, "...") != NULL) {
                    const char *dots = script_find_text(in_word, "...");
                    char left[32];
                    u32 left_length = (u32)(dots - in_word);
                    if (left_length >= sizeof(left)) left_length = sizeof(left) - 1;
                    memcpy(left, in_word, left_length); left[left_length] = 0;
                    loop_start = script_expression(expr, left);
                    loop_end = script_expression(expr, dots + 3) - 1;
                    for (loop_start = loop_start; loop_start <= loop_end; loop_start++) {
                        script_variable_t *variable = script_variable(expr, loop_name, strlen(loop_name), 1);
                        if (variable) { variable->type = SCRIPT_INTEGER; variable->value = loop_start; }
                        if (script_run_range(lines, i + 1, j, expr, execute, context, depth + 1) == 2) break;
                    }
                } else {
                    script_variable_t *source = script_find_variable(expr, in_word);
                    if (source && source->type == SCRIPT_ARRAY) {
                        u32 item;
                        for (item = 0; item < source->array_count; item++) {
                            script_variable_t *variable = script_variable(expr, loop_name, strlen(loop_name), 1);
                            if (variable) { variable->type = SCRIPT_INTEGER; variable->value = source->array[item]; }
                            if (script_run_range(lines, i + 1, j, expr, execute, context, depth + 1) == 2) break;
                        }
                    }
                }
            } else if (start[0] == 'f') {
                const char *comma = header;
                const char *second;
                const char *third;
                char init[64], condition[64], increment[64];
                while (*comma && *comma != ',') comma++;
                second = comma && *comma ? comma + 1 : comma;
                while (second && *second && *second != ',') second++;
                third = second && *second ? second + 1 : second;
                if (comma && second) {
                    u32 init_length = (u32)(comma - header);
                    u32 condition_length = (u32)(second - (comma + 1));
                    u32 increment_length = (u32)(start + strlen(start) - third);
                    if (init_length >= sizeof(init)) init_length = sizeof(init) - 1;
                    if (condition_length >= sizeof(condition)) condition_length = sizeof(condition) - 1;
                    if (increment_length >= sizeof(increment)) increment_length = sizeof(increment) - 1;
                    memcpy(init, header, init_length); init[init_length] = 0;
                    memcpy(condition, comma + 1, condition_length); condition[condition_length] = 0;
                    memcpy(increment, third, increment_length); increment[increment_length] = 0;
                    if (strncmp(init, "var ", 4) == 0) {
                        char *equals = init + 4;
                        while (*equals && *equals != '=') equals++;
                        if (*equals) {
                            u32 name_length = (u32)(equals - (init + 4));
                            while (name_length && init[4 + name_length - 1] == ' ') name_length--;
                            if (name_length >= sizeof(loop_name)) name_length = sizeof(loop_name) - 1;
                            memcpy(loop_name, init + 4, name_length); loop_name[name_length] = 0;
                            loop_start = script_expression(expr, equals + 1);
                        }
                    } else script_expression(expr, init);
                    while (script_expression(expr, condition) != 0) {
                        script_variable_t *variable = script_find_variable(expr, loop_name);
                        if (variable) variable->value = loop_start;
                        { int result = script_run_range(lines, i + 1, j, expr, execute, context, depth + 1); if (result == 2) break; }
                        script_expression(expr, increment);
                    }
                }
            } else {
                const char *var_start = header;
                const char *open = header;
                const char *dots;
                u32 name_length;
                while (*var_start == ' ') var_start++;
                if (strncmp(var_start, "var ", 4) == 0) var_start += 4;
                open = var_start;
                while (*open && *open != ' ') open++;
                name_length = (u32)(open - var_start);
                if (name_length >= sizeof(loop_name)) name_length = sizeof(loop_name) - 1;
                memcpy(loop_name, var_start, name_length); loop_name[name_length] = 0;
                while (*open && *open != '(') open++;
                if (*open == '(') {
                    dots = script_find_text(open + 1, "...");
                    if (dots) {
                        char left[32];
                        u32 left_length = (u32)(dots - (open + 1));
                        if (left_length >= sizeof(left)) left_length = sizeof(left) - 1;
                        memcpy(left, open + 1, left_length); left[left_length] = 0;
                        loop_start = script_expression(expr, left);
                        loop_end = script_expression(expr, dots + 3) - 1;
                        for (; loop_start <= loop_end; loop_start++) {
                            script_variable_t *variable = script_variable(expr, loop_name, strlen(loop_name), 1);
                            if (variable) { variable->type = SCRIPT_INTEGER; variable->value = loop_start; }
                            if (script_run_range(lines, i + 1, j, expr, execute, context, depth + 1) == 2) break;
                        }
                    }
                }
            }
            i = j; continue;
        }
        if ((u32)(end - start) > 4 && (strncmp(start, "let ", 4) == 0 || strncmp(start, "set ", 4) == 0)) { script_expression(expr, start + 4); continue; }
        if (strncmp(start, "break", 5) == 0) return 2;
        if (strncmp(start, "continue", 8) == 0) return 3;
        if (strncmp(start, "return", 6) == 0) return 1;
        if (start[0] >= 'a' && start[0] <= 'z') {
            const char *open = start;
            char function_name[GSH_SCRIPT_MAX_NAME];
            u32 name_length = 0;
            while (open[name_length] >= 'a' && open[name_length] <= 'z') name_length++;
            if (start[name_length] == '(' && name_length < sizeof(function_name)) {
                int function_index;
                memcpy(function_name, start, name_length); function_name[name_length] = 0;
                function_index = script_find_function(expr, function_name);
                if (function_index >= 0) {
                    script_function_t *function = &expr->functions[function_index];
                    const char *argument = start + name_length + 1;
                    u32 parameter = 0;
                    while (*argument && *argument != ')' && parameter < function->parameter_count) {
                        char argument_text[64];
                        u32 argument_length = 0;
                        while (*argument == ' ' || *argument == ',') argument++;
                        while (*argument && *argument != ',' && *argument != ')' && argument_length + 1 < sizeof(argument_text)) argument_text[argument_length++] = *argument++;
                        argument_text[argument_length] = 0;
                        if (argument_text[0] == '"' || argument_text[0] == '\'') {
                            u32 value_length = argument_length > 1 ? argument_length - 2 : 0;
                            char value[GSH_SCRIPT_MAX_STRING];
                            if (value_length >= sizeof(value)) value_length = sizeof(value) - 1;
                            memcpy(value, argument_text + 1, value_length); value[value_length] = 0;
                            script_set_string(expr, function->parameters[parameter], value);
                        } else {
                            script_variable_t *variable = script_variable(expr, function->parameters[parameter], strlen(function->parameters[parameter]), 1);
                            if (variable) { variable->type = SCRIPT_INTEGER; variable->value = script_expression(expr, argument_text); }
                        }
                        parameter++;
                    }
                    script_run_range(lines, function->first_line, function->last_line, expr, execute, context, depth + 1);
                    continue;
                }
            }
        }
        { u32 length = (u32)(end - start); if (length >= sizeof(command)) length = sizeof(command) - 1; memcpy(command, start, length); command[length] = 0; script_expand(command, command, sizeof(command), expr); if (execute(command, context) != 0) return 1; }
    }
    return 0;
}

static int script_run_source(const char *source, expression_t *expr, gsh_script_command_fn execute, void *context) {
    script_line_t lines[GSH_SCRIPT_MAX_LINES]; u32 count = 0; const char *cursor = source;
    while (*cursor && count < GSH_SCRIPT_MAX_LINES) { lines[count].start = cursor; while (*cursor && *cursor != '\n') cursor++; lines[count++].end = cursor; if (*cursor) cursor++; }
    return script_run_range(lines, 0, count, expr, execute, context, 0);
}

static void script_strip_comments(char *source) {
    char *cursor = source;
    int in_block = 0;
    while (*cursor) {
        if (!in_block && cursor[0] == '/' && cursor[1] == '/') {
            while (*cursor && *cursor != '\n') *cursor++ = ' ';
        } else if (!in_block && cursor[0] == '/' && cursor[1] == '*') {
            *cursor++ = ' '; *cursor++ = ' '; in_block = 1;
        } else if (in_block && cursor[0] == '*' && cursor[1] == '/') {
            *cursor++ = ' '; *cursor++ = ' '; in_block = 0;
        } else if (in_block) {
            if (*cursor != '\n') *cursor = ' ';
            cursor++;
        } else {
            cursor++;
        }
    }
}

int gsh_script_run_file(const char *path, const char *current_dir, gsh_script_command_fn execute, void *context) {
    char resolved[VFS_MAX_PATH]; char source[GSH_SCRIPT_MAX_SOURCE + 1]; vfs_entry_t *entry; u32 size; expression_t expr;
    if (!path || !execute) return -1;
    if ((path[0] == '.' && path[1] == '/') && !vfs_find(path)) {
        path_resolve(current_dir, path + 2, resolved, sizeof(resolved));
    } else if (path[0] == '/') {
        strncpy(resolved, path, sizeof(resolved) - 1);
        resolved[sizeof(resolved) - 1] = 0;
    } else {
        path_resolve(current_dir, path, resolved, sizeof(resolved));
    }
    entry = vfs_find(resolved);
    if (!entry || entry->is_dir) { kprintf("run: script not found: %s\n", path); return -1; }
    size = entry->size > GSH_SCRIPT_MAX_SOURCE ? GSH_SCRIPT_MAX_SOURCE : entry->size; size = vfs_read(resolved, source, size); source[size] = 0;
    script_strip_comments(source);
    memset(&expr, 0, sizeof(expr)); return script_run_source(source, &expr, execute, context);
}