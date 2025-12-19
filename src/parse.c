/*
 * json-asm: JSON parser implementation
 */

#include "internal.h"
#include <math.h>
#include <errno.h>
#include <ctype.h>

/* Character classification table (for future table-driven state machine) */
__attribute__((unused))
static const uint8_t char_class[256] = {
    /* 0x00-0x1F: Control characters */
    [0x00] = CC_EOF, [0x01] = CC_CTRL, [0x02] = CC_CTRL, [0x03] = CC_CTRL,
    [0x04] = CC_CTRL, [0x05] = CC_CTRL, [0x06] = CC_CTRL, [0x07] = CC_CTRL,
    [0x08] = CC_CTRL, [0x09] = CC_SPACE, [0x0A] = CC_SPACE, [0x0B] = CC_CTRL,
    [0x0C] = CC_CTRL, [0x0D] = CC_SPACE, [0x0E] = CC_CTRL, [0x0F] = CC_CTRL,
    [0x10] = CC_CTRL, [0x11] = CC_CTRL, [0x12] = CC_CTRL, [0x13] = CC_CTRL,
    [0x14] = CC_CTRL, [0x15] = CC_CTRL, [0x16] = CC_CTRL, [0x17] = CC_CTRL,
    [0x18] = CC_CTRL, [0x19] = CC_CTRL, [0x1A] = CC_CTRL, [0x1B] = CC_CTRL,
    [0x1C] = CC_CTRL, [0x1D] = CC_CTRL, [0x1E] = CC_CTRL, [0x1F] = CC_CTRL,

    /* 0x20-0x2F: Space and punctuation */
    [' ']  = CC_SPACE, ['!']  = CC_OTHER, ['"']  = CC_QUOTE, ['#']  = CC_OTHER,
    ['$']  = CC_OTHER, ['%']  = CC_OTHER, ['&']  = CC_OTHER, ['\''] = CC_OTHER,
    ['(']  = CC_OTHER, [')']  = CC_OTHER, ['*']  = CC_OTHER, ['+']  = CC_OTHER,
    [',']  = CC_COMMA, ['-']  = CC_MINUS, ['.']  = CC_OTHER, ['/']  = CC_OTHER,

    /* 0x30-0x39: Digits */
    ['0']  = CC_DIGIT, ['1']  = CC_DIGIT, ['2']  = CC_DIGIT, ['3']  = CC_DIGIT,
    ['4']  = CC_DIGIT, ['5']  = CC_DIGIT, ['6']  = CC_DIGIT, ['7']  = CC_DIGIT,
    ['8']  = CC_DIGIT, ['9']  = CC_DIGIT,

    /* 0x3A-0x40: More punctuation */
    [':']  = CC_COLON, [';']  = CC_OTHER, ['<']  = CC_OTHER, ['=']  = CC_OTHER,
    ['>']  = CC_OTHER, ['?']  = CC_OTHER, ['@']  = CC_OTHER,

    /* 0x41-0x5A: Uppercase letters */
    ['A']  = CC_ALPHA, ['B']  = CC_ALPHA, ['C']  = CC_ALPHA, ['D']  = CC_ALPHA,
    ['E']  = CC_ALPHA, ['F']  = CC_ALPHA, ['G']  = CC_ALPHA, ['H']  = CC_ALPHA,
    ['I']  = CC_ALPHA, ['J']  = CC_ALPHA, ['K']  = CC_ALPHA, ['L']  = CC_ALPHA,
    ['M']  = CC_ALPHA, ['N']  = CC_ALPHA, ['O']  = CC_ALPHA, ['P']  = CC_ALPHA,
    ['Q']  = CC_ALPHA, ['R']  = CC_ALPHA, ['S']  = CC_ALPHA, ['T']  = CC_ALPHA,
    ['U']  = CC_ALPHA, ['V']  = CC_ALPHA, ['W']  = CC_ALPHA, ['X']  = CC_ALPHA,
    ['Y']  = CC_ALPHA, ['Z']  = CC_ALPHA,

    /* 0x5B-0x60: Brackets and more punctuation */
    ['[']  = CC_LBRACK, ['\\'] = CC_ESCAPE, [']']  = CC_RBRACK,
    ['^']  = CC_OTHER, ['_']  = CC_OTHER, ['`']  = CC_OTHER,

    /* 0x61-0x7A: Lowercase letters */
    ['a']  = CC_ALPHA, ['b']  = CC_ALPHA, ['c']  = CC_ALPHA, ['d']  = CC_ALPHA,
    ['e']  = CC_ALPHA, ['f']  = CC_ALPHA, ['g']  = CC_ALPHA, ['h']  = CC_ALPHA,
    ['i']  = CC_ALPHA, ['j']  = CC_ALPHA, ['k']  = CC_ALPHA, ['l']  = CC_ALPHA,
    ['m']  = CC_ALPHA, ['n']  = CC_ALPHA, ['o']  = CC_ALPHA, ['p']  = CC_ALPHA,
    ['q']  = CC_ALPHA, ['r']  = CC_ALPHA, ['s']  = CC_ALPHA, ['t']  = CC_ALPHA,
    ['u']  = CC_ALPHA, ['v']  = CC_ALPHA, ['w']  = CC_ALPHA, ['x']  = CC_ALPHA,
    ['y']  = CC_ALPHA, ['z']  = CC_ALPHA,

    /* 0x7B-0x7F: Braces and more */
    ['{']  = CC_LBRACE, ['|']  = CC_OTHER, ['}']  = CC_RBRACE,
    ['~']  = CC_OTHER, [0x7F] = CC_CTRL,

    /* 0x80-0xFF: Extended ASCII (UTF-8 continuation bytes, etc.) */
    /* Default to CC_OTHER for all extended bytes */
};

/* Parser context */
typedef struct {
    const char *input;
    size_t len;
    size_t pos;
    size_t line;
    size_t col;
    struct json_doc *doc;
    uint32_t flags;
    size_t max_depth;
    size_t depth;
} parser_ctx;

/* Forward declarations */
static struct json_val *parse_value(parser_ctx *ctx);

/* Skip whitespace */
static void skip_ws(parser_ctx *ctx) {
    while (ctx->pos < ctx->len) {
        char c = ctx->input[ctx->pos];
        if (c == ' ' || c == '\t' || c == '\r') {
            ctx->pos++;
            ctx->col++;
        } else if (c == '\n') {
            ctx->pos++;
            ctx->line++;
            ctx->col = 1;
        } else {
            break;
        }
    }
}

/* Check and consume a character */
static bool consume(parser_ctx *ctx, char expected) {
    skip_ws(ctx);
    if (ctx->pos < ctx->len && ctx->input[ctx->pos] == expected) {
        ctx->pos++;
        ctx->col++;
        return true;
    }
    return false;
}

/* Peek current character */
static inline char peek(parser_ctx *ctx) {
    skip_ws(ctx);
    if (ctx->pos >= ctx->len) return '\0';
    return ctx->input[ctx->pos];
}

/* Parse null literal */
static struct json_val *parse_null(parser_ctx *ctx) {
    if (ctx->pos + 4 <= ctx->len &&
        ctx->input[ctx->pos] == 'n' &&
        ctx->input[ctx->pos + 1] == 'u' &&
        ctx->input[ctx->pos + 2] == 'l' &&
        ctx->input[ctx->pos + 3] == 'l') {
        ctx->pos += 4;
        ctx->col += 4;
        struct json_val *val = arena_alloc_val(ctx->doc);
        if (!val) return NULL;
        val_set_type(val, JSON_NULL);
        return val;
    }
    set_error(JSON_ERROR_SYNTAX, ctx->pos, ctx->line, ctx->col, "Expected 'null'");
    return NULL;
}

/* Parse true literal */
static struct json_val *parse_true(parser_ctx *ctx) {
    if (ctx->pos + 4 <= ctx->len &&
        ctx->input[ctx->pos] == 't' &&
        ctx->input[ctx->pos + 1] == 'r' &&
        ctx->input[ctx->pos + 2] == 'u' &&
        ctx->input[ctx->pos + 3] == 'e') {
        ctx->pos += 4;
        ctx->col += 4;
        struct json_val *val = arena_alloc_val(ctx->doc);
        if (!val) return NULL;
        val_set_type(val, JSON_TRUE);
        return val;
    }
    set_error(JSON_ERROR_SYNTAX, ctx->pos, ctx->line, ctx->col, "Expected 'true'");
    return NULL;
}

/* Parse false literal */
static struct json_val *parse_false(parser_ctx *ctx) {
    if (ctx->pos + 5 <= ctx->len &&
        ctx->input[ctx->pos] == 'f' &&
        ctx->input[ctx->pos + 1] == 'a' &&
        ctx->input[ctx->pos + 2] == 'l' &&
        ctx->input[ctx->pos + 3] == 's' &&
        ctx->input[ctx->pos + 4] == 'e') {
        ctx->pos += 5;
        ctx->col += 5;
        struct json_val *val = arena_alloc_val(ctx->doc);
        if (!val) return NULL;
        val_set_type(val, JSON_FALSE);
        return val;
    }
    set_error(JSON_ERROR_SYNTAX, ctx->pos, ctx->line, ctx->col, "Expected 'false'");
    return NULL;
}

/* Parse a number */
static struct json_val *parse_number(parser_ctx *ctx) {
    const char *start = ctx->input + ctx->pos;
    size_t remaining = ctx->len - ctx->pos;
    bool is_float = false;

    size_t i = 0;

    /* Optional minus */
    if (i < remaining && start[i] == '-') {
        i++;
    }

    /* Integer part */
    if (i >= remaining || !isdigit((unsigned char)start[i])) {
        set_error(JSON_ERROR_NUMBER, ctx->pos, ctx->line, ctx->col, "Invalid number");
        return NULL;
    }

    /* Check for leading zero */
    if (start[i] == '0' && i + 1 < remaining && isdigit((unsigned char)start[i + 1])) {
        set_error(JSON_ERROR_NUMBER, ctx->pos, ctx->line, ctx->col, "Leading zeros not allowed");
        return NULL;
    }

    while (i < remaining && isdigit((unsigned char)start[i])) {
        i++;
    }

    /* Fractional part */
    if (i < remaining && start[i] == '.') {
        is_float = true;
        i++;
        if (i >= remaining || !isdigit((unsigned char)start[i])) {
            set_error(JSON_ERROR_NUMBER, ctx->pos, ctx->line, ctx->col, "Expected digit after decimal point");
            return NULL;
        }
        while (i < remaining && isdigit((unsigned char)start[i])) {
            i++;
        }
    }

    /* Exponent part */
    if (i < remaining && (start[i] == 'e' || start[i] == 'E')) {
        is_float = true;
        i++;
        if (i < remaining && (start[i] == '+' || start[i] == '-')) {
            i++;
        }
        if (i >= remaining || !isdigit((unsigned char)start[i])) {
            set_error(JSON_ERROR_NUMBER, ctx->pos, ctx->line, ctx->col, "Expected digit in exponent");
            return NULL;
        }
        while (i < remaining && isdigit((unsigned char)start[i])) {
            i++;
        }
    }

    struct json_val *val = arena_alloc_val(ctx->doc);
    if (!val) return NULL;

    if (is_float) {
        /* Parse as double */
        char *end;
        errno = 0;
        double d = strtod(start, &end);
        if (errno == ERANGE) {
            set_error(JSON_ERROR_NUMBER, ctx->pos, ctx->line, ctx->col, "Number out of range");
            return NULL;
        }
        val_set_type(val, JSON_FLOAT);
        val->float_val = d;
    } else {
        /* Parse as integer */
        char *end;
        errno = 0;
        long long ll = strtoll(start, &end, 10);
        if (errno == ERANGE) {
            /* Fall back to double for very large integers */
            double d = strtod(start, &end);
            val_set_type(val, JSON_FLOAT);
            val->float_val = d;
        } else {
            val_set_type(val, JSON_INT);
            int64_t ival = (int64_t)ll;
            /* Store value in 60-bit payload */
            val_set_payload(val, (uint64_t)ival & 0x0FFFFFFFFFFFFFFFULL);
        }
    }

    ctx->pos += i;
    ctx->col += i;
    return val;
}

/* Parse hex digit */
static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Parse Unicode escape sequence */
static int parse_unicode_escape(parser_ctx *ctx, uint32_t *codepoint) {
    if (ctx->pos + 4 > ctx->len) {
        return -1;
    }

    uint32_t cp = 0;
    for (int i = 0; i < 4; i++) {
        int d = hex_digit(ctx->input[ctx->pos + i]);
        if (d < 0) return -1;
        cp = (cp << 4) | (uint32_t)d;
    }

    ctx->pos += 4;
    ctx->col += 4;
    *codepoint = cp;
    return 0;
}

/* Encode codepoint as UTF-8 */
static size_t encode_utf8(uint32_t cp, char *buf) {
    if (cp < 0x80) {
        buf[0] = (char)cp;
        return 1;
    } else if (cp < 0x800) {
        buf[0] = (char)(0xC0 | (cp >> 6));
        buf[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    } else if (cp < 0x10000) {
        buf[0] = (char)(0xE0 | (cp >> 12));
        buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    } else if (cp <= 0x10FFFF) {
        buf[0] = (char)(0xF0 | (cp >> 18));
        buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[3] = (char)(0x80 | (cp & 0x3F));
        return 4;
    }
    return 0;
}

/* Parse string */
static struct json_val *parse_string(parser_ctx *ctx) {
    if (ctx->pos >= ctx->len || ctx->input[ctx->pos] != '"') {
        set_error(JSON_ERROR_SYNTAX, ctx->pos, ctx->line, ctx->col, "Expected '\"'");
        return NULL;
    }
    ctx->pos++;
    ctx->col++;

    /* First pass: find string length and check for escapes */
    size_t start = ctx->pos;
    size_t len = 0;
    bool has_escapes = false;

    while (ctx->pos < ctx->len) {
        char c = ctx->input[ctx->pos];
        if (c == '"') {
            break;
        }
        if (c == '\\') {
            has_escapes = true;
            ctx->pos++;
            ctx->col++;
            if (ctx->pos >= ctx->len) {
                set_error(JSON_ERROR_STRING, ctx->pos, ctx->line, ctx->col, "Unterminated escape");
                return NULL;
            }
            char esc = ctx->input[ctx->pos];
            switch (esc) {
                case '"': case '\\': case '/': case 'b':
                case 'f': case 'n': case 'r': case 't':
                    len++;
                    ctx->pos++;
                    ctx->col++;
                    break;
                case 'u': {
                    ctx->pos++;
                    ctx->col++;
                    uint32_t cp;
                    if (parse_unicode_escape(ctx, &cp) < 0) {
                        set_error(JSON_ERROR_STRING, ctx->pos, ctx->line, ctx->col, "Invalid unicode escape");
                        return NULL;
                    }
                    /* Handle surrogate pairs */
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (ctx->pos + 2 > ctx->len ||
                            ctx->input[ctx->pos] != '\\' ||
                            ctx->input[ctx->pos + 1] != 'u') {
                            set_error(JSON_ERROR_STRING, ctx->pos, ctx->line, ctx->col, "Expected surrogate pair");
                            return NULL;
                        }
                        ctx->pos += 2;
                        ctx->col += 2;
                        uint32_t cp2;
                        if (parse_unicode_escape(ctx, &cp2) < 0) {
                            set_error(JSON_ERROR_STRING, ctx->pos, ctx->line, ctx->col, "Invalid unicode escape");
                            return NULL;
                        }
                        if (cp2 < 0xDC00 || cp2 > 0xDFFF) {
                            set_error(JSON_ERROR_STRING, ctx->pos, ctx->line, ctx->col, "Invalid low surrogate");
                            return NULL;
                        }
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (cp2 - 0xDC00);
                    }
                    char utf8[4];
                    len += encode_utf8(cp, utf8);
                    break;
                }
                default:
                    set_error(JSON_ERROR_STRING, ctx->pos, ctx->line, ctx->col, "Invalid escape sequence");
                    return NULL;
            }
        } else if ((unsigned char)c < 0x20) {
            set_error(JSON_ERROR_STRING, ctx->pos, ctx->line, ctx->col, "Control character in string");
            return NULL;
        } else {
            len++;
            ctx->pos++;
            ctx->col++;
        }
    }

    if (ctx->pos >= ctx->len) {
        set_error(JSON_ERROR_STRING, ctx->pos, ctx->line, ctx->col, "Unterminated string");
        return NULL;
    }

    /* Skip closing quote */
    ctx->pos++;
    ctx->col++;

    struct json_val *val = arena_alloc_val(ctx->doc);
    if (!val) return NULL;

    /* Check for short string optimization */
    if (!has_escapes && len <= JSON_SHORT_STR_MAX) {
        val_set_type(val, JSON_STRING_SHORT);
        val->tag_payload = JSON_STRING_SHORT | ((uint64_t)len << 4);
        char *dst = ((char *)&val->tag_payload) + 1;
        memcpy(dst, ctx->input + start, len);
        return val;
    }

    /* Allocate string in string arena */
    char *str = arena_alloc_string(ctx->doc, len);
    if (!str) return NULL;

    /* Second pass: copy string with escape processing */
    if (!has_escapes) {
        memcpy(str, ctx->input + start, len);
        str[len] = '\0';
    } else {
        /* Re-parse from start position */
        size_t src_pos = start;
        size_t dst_pos = 0;
        size_t src_end = ctx->pos - 1; /* Exclude closing quote */

        while (src_pos < src_end) {
            char c = ctx->input[src_pos];
            if (c == '\\') {
                src_pos++;
                char esc = ctx->input[src_pos++];
                switch (esc) {
                    case '"':  str[dst_pos++] = '"'; break;
                    case '\\': str[dst_pos++] = '\\'; break;
                    case '/':  str[dst_pos++] = '/'; break;
                    case 'b':  str[dst_pos++] = '\b'; break;
                    case 'f':  str[dst_pos++] = '\f'; break;
                    case 'n':  str[dst_pos++] = '\n'; break;
                    case 'r':  str[dst_pos++] = '\r'; break;
                    case 't':  str[dst_pos++] = '\t'; break;
                    case 'u': {
                        uint32_t cp = 0;
                        for (int i = 0; i < 4; i++) {
                            cp = (cp << 4) | (uint32_t)hex_digit(ctx->input[src_pos++]);
                        }
                        if (cp >= 0xD800 && cp <= 0xDBFF) {
                            src_pos += 2; /* Skip \u */
                            uint32_t cp2 = 0;
                            for (int i = 0; i < 4; i++) {
                                cp2 = (cp2 << 4) | (uint32_t)hex_digit(ctx->input[src_pos++]);
                            }
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (cp2 - 0xDC00);
                        }
                        dst_pos += encode_utf8(cp, str + dst_pos);
                        break;
                    }
                }
            } else {
                str[dst_pos++] = c;
                src_pos++;
            }
        }
        str[dst_pos] = '\0';
    }

    val_set_type(val, JSON_STRING_LONG);
    val_set_payload(val, len);  /* Store length in payload, not str_len (which overlaps with next) */
    val->str_ptr = str;
    return val;
}

/* Parse array */
static struct json_val *parse_array(parser_ctx *ctx) {
    if (!consume(ctx, '[')) {
        set_error(JSON_ERROR_SYNTAX, ctx->pos, ctx->line, ctx->col, "Expected '['");
        return NULL;
    }

    if (ctx->max_depth > 0 && ctx->depth >= ctx->max_depth) {
        set_error(JSON_ERROR_DEPTH, ctx->pos, ctx->line, ctx->col, "Maximum depth exceeded");
        return NULL;
    }
    ctx->depth++;

    struct json_val *arr = arena_alloc_val(ctx->doc);
    if (!arr) return NULL;
    val_set_type(arr, JSON_ARRAY);

    /* Empty array */
    if (peek(ctx) == ']') {
        consume(ctx, ']');
        ctx->depth--;
        return arr;
    }

    struct json_val *first = NULL;
    struct json_val *prev = NULL;

    while (1) {
        struct json_val *elem = parse_value(ctx);
        if (!elem) return NULL;

        if (!first) {
            first = elem;
            arr->child = first;
        } else {
            prev->next = elem;
        }
        prev = elem;

        if (peek(ctx) == ']') {
            consume(ctx, ']');
            break;
        }
        if (!consume(ctx, ',')) {
            set_error(JSON_ERROR_SYNTAX, ctx->pos, ctx->line, ctx->col, "Expected ',' or ']'");
            return NULL;
        }
        /* Allow trailing comma if flag is set */
        if ((ctx->flags & JSON_PARSE_ALLOW_TRAILING) && peek(ctx) == ']') {
            consume(ctx, ']');
            break;
        }
    }

    ctx->depth--;
    return arr;
}

/* Parse object */
static struct json_val *parse_object(parser_ctx *ctx) {
    if (!consume(ctx, '{')) {
        set_error(JSON_ERROR_SYNTAX, ctx->pos, ctx->line, ctx->col, "Expected '{'");
        return NULL;
    }

    if (ctx->max_depth > 0 && ctx->depth >= ctx->max_depth) {
        set_error(JSON_ERROR_DEPTH, ctx->pos, ctx->line, ctx->col, "Maximum depth exceeded");
        return NULL;
    }
    ctx->depth++;

    struct json_val *obj = arena_alloc_val(ctx->doc);
    if (!obj) return NULL;
    val_set_type(obj, JSON_OBJECT);

    /* Empty object */
    if (peek(ctx) == '}') {
        consume(ctx, '}');
        ctx->depth--;
        return obj;
    }

    struct json_val *first = NULL;
    struct json_val *prev = NULL;

    while (1) {
        /* Parse key */
        if (peek(ctx) != '"') {
            set_error(JSON_ERROR_SYNTAX, ctx->pos, ctx->line, ctx->col, "Expected string key");
            return NULL;
        }
        struct json_val *key = parse_string(ctx);
        if (!key) return NULL;

        /* Colon */
        if (!consume(ctx, ':')) {
            set_error(JSON_ERROR_SYNTAX, ctx->pos, ctx->line, ctx->col, "Expected ':'");
            return NULL;
        }

        /* Parse value */
        struct json_val *value = parse_value(ctx);
        if (!value) return NULL;

        /* Link key to value */
        key->child = value;

        if (!first) {
            first = key;
            obj->child = first;
        } else {
            prev->next = key;
        }
        prev = key;

        if (peek(ctx) == '}') {
            consume(ctx, '}');
            break;
        }
        if (!consume(ctx, ',')) {
            set_error(JSON_ERROR_SYNTAX, ctx->pos, ctx->line, ctx->col, "Expected ',' or '}'");
            return NULL;
        }
        /* Allow trailing comma if flag is set */
        if ((ctx->flags & JSON_PARSE_ALLOW_TRAILING) && peek(ctx) == '}') {
            consume(ctx, '}');
            break;
        }
    }

    ctx->depth--;
    return obj;
}

/* Parse any value */
static struct json_val *parse_value(parser_ctx *ctx) {
    char c = peek(ctx);

    switch (c) {
        case 'n': return parse_null(ctx);
        case 't': return parse_true(ctx);
        case 'f': return parse_false(ctx);
        case '"': return parse_string(ctx);
        case '[': return parse_array(ctx);
        case '{': return parse_object(ctx);
        case '-':
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            return parse_number(ctx);
        case '\0':
            set_error(JSON_ERROR_SYNTAX, ctx->pos, ctx->line, ctx->col, "Unexpected end of input");
            return NULL;
        default:
            set_error(JSON_ERROR_SYNTAX, ctx->pos, ctx->line, ctx->col, "Unexpected character");
            return NULL;
    }
}

/* Main parse function */
struct json_doc *parse_json(const char *json, size_t len,
                            const json_parse_options *opts) {
    /* Estimate arena size: assume ~1 value per 4 chars, each value is 24 bytes.
     * Use at least 64KB to avoid reallocation for small inputs. */
    size_t estimated_values = (len / 4) + 1;
    size_t arena_size = estimated_values * sizeof(struct json_val);
    if (arena_size < 64 * 1024) arena_size = 64 * 1024;
    struct json_doc *doc = arena_create(arena_size);
    if (!doc) {
        set_error(JSON_ERROR_MEMORY, 0, 0, 0, "Failed to allocate document");
        return NULL;
    }

    parser_ctx ctx = {
        .input = json,
        .len = len,
        .pos = 0,
        .line = 1,
        .col = 1,
        .doc = doc,
        .flags = opts ? opts->flags : JSON_PARSE_DEFAULT,
        .max_depth = opts ? opts->max_depth : 0,
        .depth = 0
    };

    struct json_val *root = parse_value(&ctx);
    if (!root) {
        arena_destroy(doc);
        return NULL;
    }

    doc->root = root;

    /* Check for trailing content */
    skip_ws(&ctx);
    if (ctx.pos < ctx.len) {
        set_error(JSON_ERROR_SYNTAX, ctx.pos, ctx.line, ctx.col, "Trailing content after JSON");
        arena_destroy(doc);
        return NULL;
    }

    return doc;
}

/* ============================================================================
 * Scalar Fallback Implementations
 * ============================================================================ */

size_t scan_string_scalar(const char *str, size_t len) {
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)str[i];
        if (c == '"' || c == '\\' || c < 0x20) {
            return i;
        }
    }
    return len;
}

size_t find_structural_scalar(const char *str, size_t len, uint64_t *mask) {
    uint64_t m = 0;
    size_t count = len < 64 ? len : 64;
    for (size_t i = 0; i < count; i++) {
        char c = str[i];
        if (c == '{' || c == '}' || c == '[' || c == ']' ||
            c == ':' || c == ',' || c == '"') {
            m |= (1ULL << i);
        }
    }
    *mask = m;
    return count;
}

int64_t parse_int_scalar(const char *str, size_t len, size_t *consumed) {
    int64_t result = 0;
    bool negative = false;
    size_t i = 0;

    if (i < len && str[i] == '-') {
        negative = true;
        i++;
    }

    while (i < len && str[i] >= '0' && str[i] <= '9') {
        result = result * 10 + (str[i] - '0');
        i++;
    }

    if (consumed) *consumed = i;
    return negative ? -result : result;
}

double parse_float_scalar(const char *str, size_t len, size_t *consumed) {
    char buf[64];
    size_t copy_len = len < 63 ? len : 63;
    memcpy(buf, str, copy_len);
    buf[copy_len] = '\0';

    char *end;
    double result = strtod(buf, &end);
    if (consumed) *consumed = (size_t)(end - buf);
    return result;
}
