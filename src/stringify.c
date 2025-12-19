/*
 * json-asm: JSON stringification/serialization
 */

#include "internal.h"
#include <math.h>

/* String buffer with automatic growth */
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} strbuf;

static bool strbuf_init(strbuf *sb, size_t initial_cap) {
    sb->data = malloc(initial_cap);
    if (!sb->data) return false;
    sb->len = 0;
    sb->cap = initial_cap;
    return true;
}

static void strbuf_free(strbuf *sb) {
    free(sb->data);
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
}

static bool strbuf_grow(strbuf *sb, size_t needed) {
    if (sb->len + needed <= sb->cap) return true;

    size_t new_cap = sb->cap * 2;
    while (new_cap < sb->len + needed) {
        new_cap *= 2;
    }

    char *new_data = realloc(sb->data, new_cap);
    if (!new_data) return false;

    sb->data = new_data;
    sb->cap = new_cap;
    return true;
}

static bool strbuf_append(strbuf *sb, const char *str, size_t len) {
    if (!strbuf_grow(sb, len)) return false;
    memcpy(sb->data + sb->len, str, len);
    sb->len += len;
    return true;
}

static bool strbuf_append_char(strbuf *sb, char c) {
    return strbuf_append(sb, &c, 1);
}

static bool strbuf_append_str(strbuf *sb, const char *str) {
    return strbuf_append(sb, str, strlen(str));
}

/* Hex digits for \uXXXX escapes */
static const char hex_digits[] = "0123456789abcdef";

/* Stringify a string value with escaping */
static bool stringify_string(strbuf *sb, const char *str, size_t len) {
    if (!strbuf_append_char(sb, '"')) return false;

    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)str[i];

        switch (c) {
            case '"':
                if (!strbuf_append(sb, "\\\"", 2)) return false;
                break;
            case '\\':
                if (!strbuf_append(sb, "\\\\", 2)) return false;
                break;
            case '\b':
                if (!strbuf_append(sb, "\\b", 2)) return false;
                break;
            case '\f':
                if (!strbuf_append(sb, "\\f", 2)) return false;
                break;
            case '\n':
                if (!strbuf_append(sb, "\\n", 2)) return false;
                break;
            case '\r':
                if (!strbuf_append(sb, "\\r", 2)) return false;
                break;
            case '\t':
                if (!strbuf_append(sb, "\\t", 2)) return false;
                break;
            default:
                if (c < 0x20) {
                    /* Control character - escape as \uXXXX */
                    char esc[7];
                    esc[0] = '\\';
                    esc[1] = 'u';
                    esc[2] = '0';
                    esc[3] = '0';
                    esc[4] = hex_digits[(c >> 4) & 0xF];
                    esc[5] = hex_digits[c & 0xF];
                    esc[6] = '\0';
                    if (!strbuf_append(sb, esc, 6)) return false;
                } else {
                    if (!strbuf_append_char(sb, (char)c)) return false;
                }
                break;
        }
    }

    if (!strbuf_append_char(sb, '"')) return false;
    return true;
}

/* Stringify a number */
static bool stringify_number(strbuf *sb, struct json_val *val) {
    char buf[64];
    int len;

    json_type t = val_get_type(val);
    if (t == JSON_INT) {
        int64_t ival = json_get_int(val);
        len = snprintf(buf, sizeof(buf), "%lld", (long long)ival);
    } else {
        double d = val->float_val;
        if (isnan(d)) {
            len = snprintf(buf, sizeof(buf), "null"); /* JSON doesn't support NaN */
        } else if (isinf(d)) {
            len = snprintf(buf, sizeof(buf), "null"); /* JSON doesn't support Infinity */
        } else {
            /* Use enough precision to round-trip */
            len = snprintf(buf, sizeof(buf), "%.17g", d);
        }
    }

    return strbuf_append(sb, buf, (size_t)len);
}

/* Forward declaration */
static bool stringify_value_impl(strbuf *sb, struct json_val *val,
                                  const json_stringify_options *opts,
                                  int depth);

/* Stringify with optional indentation */
static bool stringify_indent(strbuf *sb, const json_stringify_options *opts, int depth) {
    if (!opts || !(opts->flags & JSON_STRINGIFY_PRETTY)) return true;

    const char *nl = opts->newline ? opts->newline : "\n";
    if (!strbuf_append_str(sb, nl)) return false;

    for (int i = 0; i < depth; i++) {
        for (uint32_t j = 0; j < opts->indent; j++) {
            if (!strbuf_append_char(sb, ' ')) return false;
        }
    }
    return true;
}

/* Stringify an array */
static bool stringify_array(strbuf *sb, struct json_val *arr,
                            const json_stringify_options *opts, int depth) {
    if (!strbuf_append_char(sb, '[')) return false;

    struct json_val *elem = arr->child;
    bool first = true;

    while (elem) {
        if (!first) {
            if (!strbuf_append_char(sb, ',')) return false;
        }
        first = false;

        if (opts && (opts->flags & JSON_STRINGIFY_PRETTY)) {
            if (!stringify_indent(sb, opts, depth + 1)) return false;
        }

        if (!stringify_value_impl(sb, elem, opts, depth + 1)) return false;
        elem = elem->next;
    }

    if (!first && opts && (opts->flags & JSON_STRINGIFY_PRETTY)) {
        if (!stringify_indent(sb, opts, depth)) return false;
    }

    if (!strbuf_append_char(sb, ']')) return false;
    return true;
}

/* Stringify an object */
static bool stringify_object(strbuf *sb, struct json_val *obj,
                             const json_stringify_options *opts, int depth) {
    if (!strbuf_append_char(sb, '{')) return false;

    struct json_val *key = obj->child;
    bool first = true;

    while (key) {
        if (!first) {
            if (!strbuf_append_char(sb, ',')) return false;
        }
        first = false;

        if (opts && (opts->flags & JSON_STRINGIFY_PRETTY)) {
            if (!stringify_indent(sb, opts, depth + 1)) return false;
        }

        /* Stringify key */
        const char *key_str = json_get_str(key);
        size_t key_len = json_get_str_len(key);
        if (!stringify_string(sb, key_str, key_len)) return false;

        if (!strbuf_append_char(sb, ':')) return false;

        if (opts && (opts->flags & JSON_STRINGIFY_PRETTY)) {
            if (!strbuf_append_char(sb, ' ')) return false;
        }

        /* Stringify value */
        if (!stringify_value_impl(sb, key->child, opts, depth + 1)) return false;

        key = key->next;
    }

    if (!first && opts && (opts->flags & JSON_STRINGIFY_PRETTY)) {
        if (!stringify_indent(sb, opts, depth)) return false;
    }

    if (!strbuf_append_char(sb, '}')) return false;
    return true;
}

/* Stringify any value */
static bool stringify_value_impl(strbuf *sb, struct json_val *val,
                                  const json_stringify_options *opts,
                                  int depth) {
    if (!val) {
        return strbuf_append_str(sb, "null");
    }

    int t = (int)val_get_type(val);

    switch (t) {
        case JSON_NULL:
            return strbuf_append_str(sb, "null");

        case JSON_FALSE:
            return strbuf_append_str(sb, "false");

        case JSON_TRUE:
            return strbuf_append_str(sb, "true");

        case JSON_INT:
        case JSON_FLOAT:
            return stringify_number(sb, val);

        case JSON_STRING_SHORT:
        case JSON_STRING_LONG: {
            const char *str = json_get_str(val);
            size_t len = json_get_str_len(val);
            return stringify_string(sb, str, len);
        }

        case JSON_ARRAY:
            return stringify_array(sb, val, opts, depth);

        case JSON_OBJECT:
            return stringify_object(sb, val, opts, depth);

        default:
            return strbuf_append_str(sb, "null");
    }
}

/* Main stringify function */
char *stringify_value(struct json_val *val, const json_stringify_options *opts) {
    strbuf sb;
    if (!strbuf_init(&sb, 1024)) {
        return NULL;
    }

    if (!stringify_value_impl(&sb, val, opts, 0)) {
        strbuf_free(&sb);
        return NULL;
    }

    /* Null-terminate */
    if (!strbuf_append_char(&sb, '\0')) {
        strbuf_free(&sb);
        return NULL;
    }

    /* Return ownership to caller */
    return sb.data;
}
