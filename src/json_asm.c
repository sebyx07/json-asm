/*
 * json-asm: Main API implementation
 */

#define JSON_ASM_BUILDING
#include "internal.h"

/* ============================================================================
 * Global State
 * ============================================================================ */

struct json_ops g_json_ops;
uint32_t g_cpu_features = 0;
bool g_initialized = false;
_Thread_local json_error_info g_last_error = {0};

/* ============================================================================
 * Initialization
 * ============================================================================ */

JSON_API void json_init(void) {
    if (g_initialized) return;

    g_cpu_features = cpu_detect_features();

    /* Select best implementation based on CPU features */
#if defined(USE_SCALAR_ONLY)
    /* Scalar-only build (no NASM or unknown architecture) */
    g_json_ops.scan_string = scan_string_scalar;
    g_json_ops.find_structural = find_structural_scalar;
    g_json_ops.parse_int = parse_int_scalar;
#elif defined(JSON_ARCH_X86_64)
    #ifndef NO_AVX512
    if ((g_cpu_features & (JSON_CPU_AVX512F | JSON_CPU_AVX512BW)) ==
        (JSON_CPU_AVX512F | JSON_CPU_AVX512BW)) {
        g_json_ops.scan_string = scan_string_avx512;
        g_json_ops.find_structural = find_structural_avx512;
        g_json_ops.parse_int = parse_int_avx512;
    } else
    #endif
    if (g_cpu_features & JSON_CPU_AVX2) {
        g_json_ops.scan_string = scan_string_avx2;
        g_json_ops.find_structural = find_structural_avx2;
        g_json_ops.parse_int = parse_int_avx2;
    } else {
        g_json_ops.scan_string = scan_string_sse42;
        g_json_ops.find_structural = find_structural_sse42;
        g_json_ops.parse_int = parse_int_sse42;
    }
#elif defined(JSON_ARCH_ARM64)
    #ifndef NO_SVE
    if (g_cpu_features & JSON_CPU_SVE2) {
        g_json_ops.scan_string = scan_string_sve2;
        g_json_ops.find_structural = find_structural_sve2;
        g_json_ops.parse_int = parse_int_sve2;
    } else if (g_cpu_features & JSON_CPU_SVE) {
        g_json_ops.scan_string = scan_string_sve;
        g_json_ops.find_structural = find_structural_sve;
        g_json_ops.parse_int = parse_int_sve;
    } else
    #endif
    {
        g_json_ops.scan_string = scan_string_neon;
        g_json_ops.find_structural = find_structural_neon;
        g_json_ops.parse_int = parse_int_neon;
    }
#else
    g_json_ops.scan_string = scan_string_scalar;
    g_json_ops.find_structural = find_structural_scalar;
    g_json_ops.parse_int = parse_int_scalar;
#endif

    g_json_ops.parse_float = parse_float_scalar;
    g_initialized = true;
}

JSON_API uint32_t json_get_cpu_features(void) {
    if (!g_initialized) json_init();
    return g_cpu_features;
}

JSON_API const char *json_version(void) {
    return JSON_ASM_VERSION_STRING;
}

/* ============================================================================
 * Parsing API
 * ============================================================================ */

JSON_API json_doc *json_parse(const char *json, size_t len) {
    return json_parse_opts(json, len, NULL);
}

JSON_API json_doc *json_parse_opts(const char *json, size_t len,
                                    const json_parse_options *opts) {
    if (!g_initialized) json_init();
    if (!json || len == 0) {
        set_error(JSON_ERROR_SYNTAX, 0, 1, 1, "Empty input");
        return NULL;
    }
    return parse_json(json, len, opts);
}

JSON_API json_doc *json_parse_file(const char *path) {
    return json_parse_file_opts(path, NULL);
}

JSON_API json_doc *json_parse_file_opts(const char *path,
                                         const json_parse_options *opts) {
    if (!path) {
        set_error(JSON_ERROR_IO, 0, 0, 0, "NULL path");
        return NULL;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        set_error(JSON_ERROR_IO, 0, 0, 0, "Cannot open file");
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fclose(f);
        set_error(JSON_ERROR_IO, 0, 0, 0, "Empty or invalid file");
        return NULL;
    }

    char *buf = malloc((size_t)size);
    if (!buf) {
        fclose(f);
        set_error(JSON_ERROR_MEMORY, 0, 0, 0, "Memory allocation failed");
        return NULL;
    }

    size_t read = fread(buf, 1, (size_t)size, f);
    fclose(f);

    if (read != (size_t)size) {
        free(buf);
        set_error(JSON_ERROR_IO, 0, 0, 0, "Read error");
        return NULL;
    }

    json_doc *doc = json_parse_opts(buf, (size_t)size, opts);
    free(buf);
    return doc;
}

JSON_API json_error_info json_get_error(void) {
    return g_last_error;
}

/* ============================================================================
 * Document Operations
 * ============================================================================ */

JSON_API json_val *json_doc_root(json_doc *doc) {
    return doc ? doc->root : NULL;
}

JSON_API void json_doc_free(json_doc *doc) {
    if (doc) arena_destroy(doc);
}

JSON_API size_t json_doc_memory(json_doc *doc) {
    if (!doc) return 0;
    return sizeof(struct json_doc) + doc->arena_size + doc->strings_size;
}

JSON_API size_t json_doc_count(json_doc *doc) {
    return doc ? doc->value_count : 0;
}

/* ============================================================================
 * Value Type Inspection
 * ============================================================================ */

JSON_API json_type json_get_type(json_val *val) {
    if (!val) return JSON_NULL;
    json_type t = val_get_type(val);
    /* Normalize string types */
    if (t == JSON_STRING_SHORT || t == JSON_STRING_LONG) return JSON_STRING;
    return t;
}

JSON_API bool json_is_null(json_val *val) {
    return val && val_get_type(val) == JSON_NULL;
}

JSON_API bool json_is_bool(json_val *val) {
    if (!val) return false;
    json_type t = val_get_type(val);
    return t == JSON_FALSE || t == JSON_TRUE;
}

JSON_API bool json_is_true(json_val *val) {
    return val && val_get_type(val) == JSON_TRUE;
}

JSON_API bool json_is_false(json_val *val) {
    return val && val_get_type(val) == JSON_FALSE;
}

JSON_API bool json_is_int(json_val *val) {
    return val && val_get_type(val) == JSON_INT;
}

JSON_API bool json_is_float(json_val *val) {
    return val && val_get_type(val) == JSON_FLOAT;
}

JSON_API bool json_is_number(json_val *val) {
    if (!val) return false;
    json_type t = val_get_type(val);
    return t == JSON_INT || t == JSON_FLOAT;
}

JSON_API bool json_is_string(json_val *val) {
    if (!val) return false;
    json_type t = val_get_type(val);
    return t == JSON_STRING_SHORT || t == JSON_STRING_LONG;
}

JSON_API bool json_is_array(json_val *val) {
    return val && val_get_type(val) == JSON_ARRAY;
}

JSON_API bool json_is_object(json_val *val) {
    return val && val_get_type(val) == JSON_OBJECT;
}

JSON_API bool json_is_container(json_val *val) {
    if (!val) return false;
    json_type t = val_get_type(val);
    return t == JSON_ARRAY || t == JSON_OBJECT;
}

/* ============================================================================
 * Value Accessors
 * ============================================================================ */

JSON_API bool json_get_bool(json_val *val) {
    return val && val_get_type(val) == JSON_TRUE;
}

JSON_API int64_t json_get_int(json_val *val) {
    if (!val) return 0;
    json_type t = val_get_type(val);
    if (t == JSON_INT) {
        uint64_t payload = val_get_payload(val);
        /* Sign-extend from 60 bits to 64 bits */
        if (payload & (1ULL << 59)) {
            /* Negative number - set upper 4 bits */
            return (int64_t)(payload | 0xF000000000000000ULL);
        }
        return (int64_t)payload;
    }
    if (t == JSON_FLOAT) {
        return (int64_t)val->float_val;
    }
    return 0;
}

JSON_API uint64_t json_get_uint(json_val *val) {
    int64_t v = json_get_int(val);
    return v < 0 ? 0 : (uint64_t)v;
}

JSON_API double json_get_num(json_val *val) {
    if (!val) return 0.0;
    json_type t = val_get_type(val);
    if (t == JSON_FLOAT) {
        return val->float_val;
    }
    if (t == JSON_INT) {
        return (double)json_get_int(val);
    }
    return 0.0;
}

JSON_API const char *json_get_str(json_val *val) {
    if (!val) return NULL;
    json_type t = val_get_type(val);
    if (t == JSON_STRING_SHORT) {
        return val_short_str_ptr(val);
    }
    if (t == JSON_STRING_LONG) {
        return val->str_ptr;
    }
    return NULL;
}

JSON_API size_t json_get_str_len(json_val *val) {
    if (!val) return 0;
    json_type t = val_get_type(val);
    if (t == JSON_STRING_SHORT) {
        return val_short_str_len(val);
    }
    if (t == JSON_STRING_LONG) {
        return (size_t)val_get_payload(val);  /* Length stored in payload, not str_len */
    }
    return 0;
}

/* ============================================================================
 * Object Operations
 * ============================================================================ */

JSON_API json_val *json_obj_get(json_val *obj, const char *key) {
    if (!key) return NULL;
    return json_obj_getn(obj, key, strlen(key));
}

JSON_API json_val *json_obj_getn(json_val *obj, const char *key, size_t key_len) {
    if (!obj || !key || val_get_type(obj) != JSON_OBJECT) return NULL;

    json_val *child = obj->child;
    while (child) {
        /* Each object child is a key-value pair: child is key, child->next is value */
        size_t child_key_len = json_get_str_len(child);
        if (child_key_len == key_len) {
            const char *child_key = json_get_str(child);
            if (child_key && memcmp(child_key, key, key_len) == 0) {
                return child->child; /* Value is stored as child of key node */
            }
        }
        child = child->next;
    }
    return NULL;
}

JSON_API bool json_obj_has(json_val *obj, const char *key) {
    return json_obj_get(obj, key) != NULL;
}

JSON_API size_t json_obj_size(json_val *obj) {
    if (!obj || val_get_type(obj) != JSON_OBJECT) return 0;
    size_t count = 0;
    json_val *child = obj->child;
    while (child) {
        count++;
        child = child->next;
    }
    return count;
}

JSON_API json_val *json_obj_first(json_val *obj) {
    if (!obj || val_get_type(obj) != JSON_OBJECT) return NULL;
    return obj->child;
}

JSON_API json_val *json_obj_next(json_val *val) {
    return val ? val->next : NULL;
}

JSON_API const char *json_obj_key(json_val *val) {
    return json_get_str(val);
}

JSON_API size_t json_obj_key_len(json_val *val) {
    return json_get_str_len(val);
}

/* ============================================================================
 * Array Operations
 * ============================================================================ */

JSON_API json_val *json_arr_get(json_val *arr, size_t index) {
    if (!arr || val_get_type(arr) != JSON_ARRAY) return NULL;

    json_val *child = arr->child;
    size_t i = 0;
    while (child && i < index) {
        child = child->next;
        i++;
    }
    return child;
}

JSON_API size_t json_arr_size(json_val *arr) {
    if (!arr || val_get_type(arr) != JSON_ARRAY) return 0;
    size_t count = 0;
    json_val *child = arr->child;
    while (child) {
        count++;
        child = child->next;
    }
    return count;
}

JSON_API json_val *json_arr_first(json_val *arr) {
    if (!arr || val_get_type(arr) != JSON_ARRAY) return NULL;
    return arr->child;
}

JSON_API json_val *json_arr_next(json_val *val) {
    return val ? val->next : NULL;
}

/* ============================================================================
 * Serialization API
 * ============================================================================ */

JSON_API char *json_stringify(json_val *val) {
    return json_stringify_opts(val, NULL);
}

JSON_API char *json_stringify_opts(json_val *val, const json_stringify_options *opts) {
    if (!val) return NULL;
    return stringify_value(val, opts);
}

JSON_API size_t json_stringify_buf(json_val *val, char *buf, size_t buf_len) {
    char *str = json_stringify(val);
    if (!str) return 0;

    size_t len = strlen(str);
    if (buf && buf_len > len) {
        memcpy(buf, str, len + 1);
    }
    free(str);
    return len;
}

JSON_API char *json_doc_stringify(json_doc *doc) {
    return doc ? json_stringify(doc->root) : NULL;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

JSON_API bool json_equals(json_val *a, json_val *b) {
    if (a == b) return true;
    if (!a || !b) return false;

    json_type ta = json_get_type(a);
    json_type tb = json_get_type(b);
    if (ta != tb) return false;

    switch (ta) {
        case JSON_NULL:
        case JSON_TRUE:
        case JSON_FALSE:
            return true;

        case JSON_INT:
            return json_get_int(a) == json_get_int(b);

        case JSON_FLOAT:
            return json_get_num(a) == json_get_num(b);

        case JSON_STRING: {
            size_t la = json_get_str_len(a);
            size_t lb = json_get_str_len(b);
            if (la != lb) return false;
            return memcmp(json_get_str(a), json_get_str(b), la) == 0;
        }

        case JSON_ARRAY: {
            json_val *ca = json_arr_first(a);
            json_val *cb = json_arr_first(b);
            while (ca && cb) {
                if (!json_equals(ca, cb)) return false;
                ca = json_arr_next(ca);
                cb = json_arr_next(cb);
            }
            return ca == NULL && cb == NULL;
        }

        case JSON_OBJECT: {
            if (json_obj_size(a) != json_obj_size(b)) return false;
            json_obj_foreach(a, key) {
                json_val *va = key->child;
                json_val *vb = json_obj_getn(b, json_obj_key(key), json_obj_key_len(key));
                if (!vb || !json_equals(va, vb)) return false;
            }
            return true;
        }
    }
    return false;
}

JSON_API json_doc *json_clone(json_val *val) {
    char *str = json_stringify(val);
    if (!str) return NULL;
    json_doc *doc = json_parse(str, strlen(str));
    free(str);
    return doc;
}

JSON_API const char *json_type_name(json_type type) {
    switch (type) {
        case JSON_NULL:   return "null";
        case JSON_TRUE:   return "true";
        case JSON_FALSE:  return "false";
        case JSON_INT:    return "integer";
        case JSON_FLOAT:  return "float";
        case JSON_STRING: return "string";
        case JSON_ARRAY:  return "array";
        case JSON_OBJECT: return "object";
        default:          return "unknown";
    }
}

JSON_API const char *json_error_string(json_error err) {
    switch (err) {
        case JSON_OK:           return "No error";
        case JSON_ERROR_MEMORY: return "Memory allocation failed";
        case JSON_ERROR_SYNTAX: return "Invalid JSON syntax";
        case JSON_ERROR_DEPTH:  return "Maximum nesting depth exceeded";
        case JSON_ERROR_NUMBER: return "Invalid number format";
        case JSON_ERROR_STRING: return "Invalid string";
        case JSON_ERROR_UTF8:   return "Invalid UTF-8 encoding";
        case JSON_ERROR_IO:     return "File I/O error";
        case JSON_ERROR_TYPE:   return "Type mismatch";
        default:                return "Unknown error";
    }
}
