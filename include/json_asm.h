/*
 * json-asm: Fast JSON parser/serializer in hand-optimized assembly
 *
 * Public API header
 */

#ifndef JSON_ASM_H
#define JSON_ASM_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Version */
#define JSON_ASM_VERSION_MAJOR 1
#define JSON_ASM_VERSION_MINOR 0
#define JSON_ASM_VERSION_PATCH 0
#define JSON_ASM_VERSION_STRING "1.0.0"

/* Export macros */
#if defined(_WIN32) || defined(__CYGWIN__)
    #ifdef JSON_ASM_BUILDING
        #define JSON_API __declspec(dllexport)
    #else
        #define JSON_API __declspec(dllimport)
    #endif
#elif defined(__GNUC__) && __GNUC__ >= 4
    #define JSON_API __attribute__((visibility("default")))
#else
    #define JSON_API
#endif

/* ============================================================================
 * Types
 * ============================================================================ */

/* JSON value types */
typedef enum json_type {
    JSON_NULL    = 0,
    JSON_FALSE   = 1,
    JSON_TRUE    = 3,
    JSON_INT     = 4,
    JSON_FLOAT   = 5,
    JSON_STRING  = 6,
    JSON_ARRAY   = 8,
    JSON_OBJECT  = 9
} json_type;

/* Opaque types */
typedef struct json_doc json_doc;
typedef struct json_val json_val;

/* Parse options */
typedef struct json_parse_options {
    uint32_t flags;             /* Parse flags (see JSON_PARSE_*) */
    size_t max_depth;           /* Maximum nesting depth (0 = unlimited) */
    void *user_data;            /* User data for callbacks */
} json_parse_options;

/* Parse flags */
#define JSON_PARSE_DEFAULT          0x00
#define JSON_PARSE_ALLOW_COMMENTS   0x01  /* Allow // and slash-star comments */
#define JSON_PARSE_ALLOW_TRAILING   0x02  /* Allow trailing commas */
#define JSON_PARSE_ALLOW_INF_NAN    0x04  /* Allow Infinity and NaN */
#define JSON_PARSE_INSITU           0x08  /* In-situ parsing (modifies input) */

/* Stringify options */
typedef struct json_stringify_options {
    uint32_t flags;             /* Stringify flags (see JSON_STRINGIFY_*) */
    uint32_t indent;            /* Spaces per indent level (0 = minified) */
    const char *newline;        /* Newline string (NULL = "\n") */
} json_stringify_options;

/* Stringify flags */
#define JSON_STRINGIFY_DEFAULT      0x00
#define JSON_STRINGIFY_PRETTY       0x01  /* Pretty print with indentation */
#define JSON_STRINGIFY_ESCAPE_SLASH 0x02  /* Escape forward slashes */
#define JSON_STRINGIFY_ESCAPE_UNI   0x04  /* Escape non-ASCII as \uXXXX */

/* Error codes */
typedef enum json_error {
    JSON_OK = 0,
    JSON_ERROR_MEMORY,          /* Memory allocation failed */
    JSON_ERROR_SYNTAX,          /* Invalid JSON syntax */
    JSON_ERROR_DEPTH,           /* Maximum nesting depth exceeded */
    JSON_ERROR_NUMBER,          /* Invalid number format */
    JSON_ERROR_STRING,          /* Invalid string (bad escape, etc.) */
    JSON_ERROR_UTF8,            /* Invalid UTF-8 encoding */
    JSON_ERROR_IO,              /* File I/O error */
    JSON_ERROR_TYPE             /* Type mismatch */
} json_error;

/* Error info */
typedef struct json_error_info {
    json_error code;            /* Error code */
    size_t position;            /* Byte position in input */
    size_t line;                /* Line number (1-based) */
    size_t column;              /* Column number (1-based) */
    const char *message;        /* Human-readable error message */
} json_error_info;

/* CPU features (for introspection) */
typedef enum json_cpu_feature {
    /* x86-64 features */
    JSON_CPU_SSE42      = 1 << 0,
    JSON_CPU_AVX2       = 1 << 1,
    JSON_CPU_AVX512F    = 1 << 2,
    JSON_CPU_AVX512BW   = 1 << 3,
    JSON_CPU_AVX512VL   = 1 << 4,
    JSON_CPU_BMI1       = 1 << 5,
    JSON_CPU_BMI2       = 1 << 6,
    JSON_CPU_POPCNT     = 1 << 7,
    JSON_CPU_LZCNT      = 1 << 8,
    /* ARM64 features */
    JSON_CPU_NEON       = 1 << 16,
    JSON_CPU_SVE        = 1 << 17,
    JSON_CPU_SVE2       = 1 << 18,
    JSON_CPU_DOTPROD    = 1 << 19,
    JSON_CPU_SHA3       = 1 << 20
} json_cpu_feature;

/* ============================================================================
 * Library Initialization
 * ============================================================================ */

/* Initialize the library (optional, called automatically on first use) */
JSON_API void json_init(void);

/* Get detected CPU features */
JSON_API uint32_t json_get_cpu_features(void);

/* Get version string */
JSON_API const char *json_version(void);

/* ============================================================================
 * Parsing
 * ============================================================================ */

/* Parse JSON string */
JSON_API json_doc *json_parse(const char *json, size_t len);

/* Parse JSON string with options */
JSON_API json_doc *json_parse_opts(const char *json, size_t len,
                                    const json_parse_options *opts);

/* Parse JSON file */
JSON_API json_doc *json_parse_file(const char *path);

/* Parse JSON file with options */
JSON_API json_doc *json_parse_file_opts(const char *path,
                                         const json_parse_options *opts);

/* Get last parse error (call after json_parse returns NULL) */
JSON_API json_error_info json_get_error(void);

/* ============================================================================
 * Document Operations
 * ============================================================================ */

/* Get document root value */
JSON_API json_val *json_doc_root(json_doc *doc);

/* Free document and all values */
JSON_API void json_doc_free(json_doc *doc);

/* Get document memory usage in bytes */
JSON_API size_t json_doc_memory(json_doc *doc);

/* Get number of values in document */
JSON_API size_t json_doc_count(json_doc *doc);

/* ============================================================================
 * Value Type Inspection
 * ============================================================================ */

/* Get value type */
JSON_API json_type json_get_type(json_val *val);

/* Type checking predicates */
JSON_API bool json_is_null(json_val *val);
JSON_API bool json_is_bool(json_val *val);
JSON_API bool json_is_true(json_val *val);
JSON_API bool json_is_false(json_val *val);
JSON_API bool json_is_int(json_val *val);
JSON_API bool json_is_float(json_val *val);
JSON_API bool json_is_number(json_val *val);
JSON_API bool json_is_string(json_val *val);
JSON_API bool json_is_array(json_val *val);
JSON_API bool json_is_object(json_val *val);
JSON_API bool json_is_container(json_val *val);

/* ============================================================================
 * Value Accessors
 * ============================================================================ */

/* Get boolean value (returns false for non-bool values) */
JSON_API bool json_get_bool(json_val *val);

/* Get integer value (returns 0 for non-number values) */
JSON_API int64_t json_get_int(json_val *val);

/* Get unsigned integer value (returns 0 for non-number/negative values) */
JSON_API uint64_t json_get_uint(json_val *val);

/* Get floating point value (returns 0.0 for non-number values) */
JSON_API double json_get_num(json_val *val);

/* Get string value (returns NULL for non-string values) */
JSON_API const char *json_get_str(json_val *val);

/* Get string length (returns 0 for non-string values) */
JSON_API size_t json_get_str_len(json_val *val);

/* ============================================================================
 * Object Operations
 * ============================================================================ */

/* Get object member by key (returns NULL if not found or not an object) */
JSON_API json_val *json_obj_get(json_val *obj, const char *key);

/* Get object member by key with length */
JSON_API json_val *json_obj_getn(json_val *obj, const char *key, size_t key_len);

/* Check if object contains key */
JSON_API bool json_obj_has(json_val *obj, const char *key);

/* Get number of object members */
JSON_API size_t json_obj_size(json_val *obj);

/* Object iteration */
JSON_API json_val *json_obj_first(json_val *obj);
JSON_API json_val *json_obj_next(json_val *val);
JSON_API const char *json_obj_key(json_val *val);
JSON_API size_t json_obj_key_len(json_val *val);

/* ============================================================================
 * Array Operations
 * ============================================================================ */

/* Get array element by index (returns NULL if out of bounds or not an array) */
JSON_API json_val *json_arr_get(json_val *arr, size_t index);

/* Get number of array elements */
JSON_API size_t json_arr_size(json_val *arr);

/* Array iteration */
JSON_API json_val *json_arr_first(json_val *arr);
JSON_API json_val *json_arr_next(json_val *val);

/* ============================================================================
 * Serialization
 * ============================================================================ */

/* Stringify value to newly allocated string (caller must free) */
JSON_API char *json_stringify(json_val *val);

/* Stringify with options */
JSON_API char *json_stringify_opts(json_val *val, const json_stringify_options *opts);

/* Stringify to buffer (returns bytes written, or required size if too small) */
JSON_API size_t json_stringify_buf(json_val *val, char *buf, size_t buf_len);

/* Stringify document root */
JSON_API char *json_doc_stringify(json_doc *doc);

/* ============================================================================
 * Iteration Macros
 * ============================================================================ */

/* Iterate over object members */
#define json_obj_foreach(obj, val) \
    for (json_val *val = json_obj_first(obj); val; val = json_obj_next(val))

/* Iterate over array elements */
#define json_arr_foreach(arr, val) \
    for (json_val *val = json_arr_first(arr); val; val = json_arr_next(val))

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/* Compare two JSON values for equality */
JSON_API bool json_equals(json_val *a, json_val *b);

/* Deep copy a value into a new document */
JSON_API json_doc *json_clone(json_val *val);

/* Get human-readable type name */
JSON_API const char *json_type_name(json_type type);

/* Get human-readable error message */
JSON_API const char *json_error_string(json_error err);

#ifdef __cplusplus
}
#endif

#endif /* JSON_ASM_H */
