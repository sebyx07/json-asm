/*
 * json-asm: Internal header
 */

#ifndef JSON_ASM_INTERNAL_H
#define JSON_ASM_INTERNAL_H

#include "json_asm.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Platform Detection
 * ============================================================================ */

#if defined(__x86_64__) || defined(_M_X64)
    #define JSON_ARCH_X86_64 1
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define JSON_ARCH_ARM64 1
#else
    #define JSON_ARCH_SCALAR 1
#endif

/* ============================================================================
 * Value Node Layout (24 bytes)
 *
 * Bytes 0-7:   4-bit type tag + 60-bit payload
 * Bytes 8-15:  next sibling pointer OR string length
 * Bytes 16-23: first child pointer OR string pointer
 * ============================================================================ */

#define JSON_TAG_MASK    0x0FULL
#define JSON_TAG_SHIFT   0
#define JSON_PAYLOAD_SHIFT 4

/* Value node (24 bytes) */
struct json_val {
    uint64_t tag_payload;       /* 4-bit tag + 60-bit inline payload */
    union {
        struct json_val *next;  /* Next sibling (objects/arrays) */
        size_t str_len;         /* String length (strings) */
        int64_t int_ext;        /* Extended integer bits */
    };
    union {
        struct json_val *child; /* First child (objects/arrays) */
        const char *str_ptr;    /* String pointer (long strings) */
        double float_val;       /* Float value */
    };
};

/* Document structure */
struct json_doc {
    uint8_t *arena;             /* Node arena (64-byte aligned) */
    size_t arena_size;          /* Arena capacity */
    size_t arena_used;          /* Arena bytes used */
    uint8_t *strings;           /* Long string storage */
    size_t strings_size;        /* String storage capacity */
    size_t strings_used;        /* String storage used */
    struct json_val *root;      /* Root value */
    size_t value_count;         /* Number of values */
    uint32_t cpu_features;      /* Detected CPU features */
};

/* ============================================================================
 * Type Tag Helpers
 * ============================================================================ */

static inline json_type val_get_type(const struct json_val *v) {
    return (json_type)(v->tag_payload & JSON_TAG_MASK);
}

static inline void val_set_type(struct json_val *v, json_type t) {
    v->tag_payload = (v->tag_payload & ~JSON_TAG_MASK) | (uint64_t)t;
}

static inline uint64_t val_get_payload(const struct json_val *v) {
    return v->tag_payload >> JSON_PAYLOAD_SHIFT;
}

static inline void val_set_payload(struct json_val *v, uint64_t p) {
    v->tag_payload = (v->tag_payload & JSON_TAG_MASK) | (p << JSON_PAYLOAD_SHIFT);
}

/* Short string: type 6, payload contains length (3 bits) + chars (up to 7 bytes) */
#define JSON_STRING_SHORT  6
#define JSON_STRING_LONG   7
#define JSON_SHORT_STR_MAX 7

static inline bool val_is_short_string(const struct json_val *v) {
    return (v->tag_payload & JSON_TAG_MASK) == JSON_STRING_SHORT;
}

static inline size_t val_short_str_len(const struct json_val *v) {
    return (v->tag_payload >> 4) & 0x07;
}

static inline const char *val_short_str_ptr(const struct json_val *v) {
    return ((const char *)&v->tag_payload) + 1;
}

/* ============================================================================
 * Arena Allocator
 * ============================================================================ */

/* Arena functions (arena.c) */
struct json_doc *arena_create(size_t initial_size);
void arena_destroy(struct json_doc *doc);
struct json_val *arena_alloc_val(struct json_doc *doc);
char *arena_alloc_string(struct json_doc *doc, size_t len);

/* ============================================================================
 * CPU Feature Detection
 * ============================================================================ */

/* CPU detection (cpu_detect.c) */
uint32_t cpu_detect_features(void);

/* ============================================================================
 * SIMD Function Pointers (runtime dispatch)
 * ============================================================================ */

/* String scanning - find quote, backslash, or control char */
typedef size_t (*scan_string_fn)(const char *str, size_t len);

/* Structural char detection - find {}[]":, */
typedef size_t (*find_structural_fn)(const char *str, size_t len, uint64_t *mask);

/* Number parsing */
typedef int64_t (*parse_int_fn)(const char *str, size_t len, size_t *consumed);
typedef double (*parse_float_fn)(const char *str, size_t len, size_t *consumed);

/* Operations table */
struct json_ops {
    scan_string_fn scan_string;
    find_structural_fn find_structural;
    parse_int_fn parse_int;
    parse_float_fn parse_float;
};

/* Global ops (set during init) */
extern struct json_ops g_json_ops;
extern uint32_t g_cpu_features;
extern bool g_initialized;

/* Implementation variants */
#ifdef JSON_ARCH_X86_64
    /* AVX-512 */
    size_t scan_string_avx512(const char *str, size_t len);
    size_t find_structural_avx512(const char *str, size_t len, uint64_t *mask);
    int64_t parse_int_avx512(const char *str, size_t len, size_t *consumed);
    /* AVX2 */
    size_t scan_string_avx2(const char *str, size_t len);
    size_t find_structural_avx2(const char *str, size_t len, uint64_t *mask);
    int64_t parse_int_avx2(const char *str, size_t len, size_t *consumed);
    /* SSE4.2 */
    size_t scan_string_sse42(const char *str, size_t len);
    size_t find_structural_sse42(const char *str, size_t len, uint64_t *mask);
    int64_t parse_int_sse42(const char *str, size_t len, size_t *consumed);
#endif

#ifdef JSON_ARCH_ARM64
    /* SVE2 */
    size_t scan_string_sve2(const char *str, size_t len);
    size_t find_structural_sve2(const char *str, size_t len, uint64_t *mask);
    int64_t parse_int_sve2(const char *str, size_t len, size_t *consumed);
    /* SVE */
    size_t scan_string_sve(const char *str, size_t len);
    size_t find_structural_sve(const char *str, size_t len, uint64_t *mask);
    int64_t parse_int_sve(const char *str, size_t len, size_t *consumed);
    /* NEON */
    size_t scan_string_neon(const char *str, size_t len);
    size_t find_structural_neon(const char *str, size_t len, uint64_t *mask);
    int64_t parse_int_neon(const char *str, size_t len, size_t *consumed);
#endif

/* Scalar fallback */
size_t scan_string_scalar(const char *str, size_t len);
size_t find_structural_scalar(const char *str, size_t len, uint64_t *mask);
int64_t parse_int_scalar(const char *str, size_t len, size_t *consumed);
double parse_float_scalar(const char *str, size_t len, size_t *consumed);

/* ============================================================================
 * Parser State Machine
 * ============================================================================ */

/* Character classes (16 classes for table indexing) */
enum char_class {
    CC_SPACE = 0,   /* Space, tab, CR, LF */
    CC_LBRACE,      /* { */
    CC_RBRACE,      /* } */
    CC_LBRACK,      /* [ */
    CC_RBRACK,      /* ] */
    CC_COLON,       /* : */
    CC_COMMA,       /* , */
    CC_QUOTE,       /* " */
    CC_DIGIT,       /* 0-9 */
    CC_MINUS,       /* - */
    CC_ALPHA,       /* Letters (for true/false/null) */
    CC_ESCAPE,      /* Backslash */
    CC_CTRL,        /* Control characters */
    CC_OTHER,       /* Everything else */
    CC_EOF,         /* End of input */
    CC_INVALID      /* Invalid byte */
};

/* Parser state */
struct parse_state {
    const char *input;          /* Input JSON */
    size_t len;                 /* Input length */
    size_t pos;                 /* Current position */
    size_t line;                /* Current line */
    size_t col;                 /* Current column */
    struct json_doc *doc;       /* Output document */
    json_error error;           /* Error code */
    char error_msg[256];        /* Error message */
};

/* Parse functions (parse.c) */
struct json_doc *parse_json(const char *json, size_t len,
                            const json_parse_options *opts);

/* ============================================================================
 * Stringify
 * ============================================================================ */

/* Stringify buffer */
struct stringify_buf {
    char *data;
    size_t len;
    size_t cap;
};

/* Stringify functions (stringify.c) */
char *stringify_value(struct json_val *val, const json_stringify_options *opts);

/* ============================================================================
 * Error Handling
 * ============================================================================ */

/* Thread-local error info */
extern _Thread_local json_error_info g_last_error;

static inline void set_error(json_error code, size_t pos, size_t line,
                             size_t col, const char *msg) {
    g_last_error.code = code;
    g_last_error.position = pos;
    g_last_error.line = line;
    g_last_error.column = col;
    g_last_error.message = msg;
}

#endif /* JSON_ASM_INTERNAL_H */
