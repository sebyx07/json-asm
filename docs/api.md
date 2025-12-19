# API Reference

Complete C API documentation for json-asm.

## Table of Contents

- [Types](#types)
- [Parsing](#parsing)
- [Document Management](#document-management)
- [Value Inspection](#value-inspection)
- [Object Operations](#object-operations)
- [Array Operations](#array-operations)
- [Serialization](#serialization)
- [Mutation](#mutation)
- [Error Handling](#error-handling)
- [Memory Management](#memory-management)
- [Thread Safety](#thread-safety)

---

## Types

### json_doc

```c
typedef struct json_doc json_doc;
```

Opaque document handle. Represents a parsed JSON document and owns all memory associated with it.

### json_val

```c
typedef struct json_val json_val;
```

Opaque value handle. Points to a value within a document. Valid only while the parent document exists.

### json_type

```c
typedef enum {
    JSON_NULL    = 0,
    JSON_BOOL    = 1,
    JSON_NUMBER  = 2,
    JSON_STRING  = 3,
    JSON_ARRAY   = 4,
    JSON_OBJECT  = 5
} json_type;
```

Value type enumeration.

### json_err

```c
typedef struct {
    int code;           // Error code
    size_t position;    // Byte offset in input
    size_t line;        // Line number (1-indexed)
    size_t column;      // Column number (1-indexed)
    const char *msg;    // Human-readable message
} json_err;
```

Error information structure.

### Error Codes

```c
#define JSON_OK                 0   // Success
#define JSON_ERR_NOMEM          1   // Memory allocation failed
#define JSON_ERR_SYNTAX         2   // Invalid JSON syntax
#define JSON_ERR_UNEXPECTED     3   // Unexpected character
#define JSON_ERR_UNTERMINATED   4   // Unterminated string
#define JSON_ERR_ESCAPE         5   // Invalid escape sequence
#define JSON_ERR_UTF8           6   // Invalid UTF-8 encoding
#define JSON_ERR_NUMBER         7   // Invalid number format
#define JSON_ERR_DEPTH          8   // Nesting too deep
#define JSON_ERR_IO             9   // File I/O error
```

---

## Parsing

### json_parse

```c
json_doc *json_parse(const char *json, size_t len);
```

Parse a JSON string.

**Parameters:**
- `json` - Pointer to JSON data (does not need to be null-terminated)
- `len` - Length of JSON data in bytes

**Returns:** Document handle on success, `NULL` on error. Use `json_last_error()` for details.

**Example:**
```c
const char *data = "{\"key\": \"value\"}";
json_doc *doc = json_parse(data, strlen(data));
if (!doc) {
    json_err err = json_last_error();
    fprintf(stderr, "Error at line %zu: %s\n", err.line, err.msg);
}
```

### json_parse_file

```c
json_doc *json_parse_file(const char *path);
```

Parse JSON from a file. The file is memory-mapped for optimal performance.

**Parameters:**
- `path` - File path

**Returns:** Document handle on success, `NULL` on error.

### json_parse_opts

```c
json_doc *json_parse_opts(const char *json, size_t len, json_parse_flags flags);
```

Parse with options.

**Flags:**
```c
#define JSON_PARSE_DEFAULT      0x00  // Strict RFC 8259 parsing
#define JSON_PARSE_COMMENTS     0x01  // Allow // and /* */ comments
#define JSON_PARSE_TRAILING     0x02  // Allow trailing commas
#define JSON_PARSE_INF_NAN      0x04  // Allow Infinity and NaN
#define JSON_PARSE_RELAXED      0x07  // All relaxed options
```

---

## Document Management

### json_doc_root

```c
json_val *json_doc_root(json_doc *doc);
```

Get the root value of a document.

**Parameters:**
- `doc` - Document handle

**Returns:** Root value handle.

### json_doc_free

```c
void json_doc_free(json_doc *doc);
```

Free a document and all associated memory. All `json_val` pointers from this document become invalid.

**Parameters:**
- `doc` - Document handle (can be `NULL`)

### json_doc_copy

```c
json_doc *json_doc_copy(json_doc *doc);
```

Create a deep copy of a document.

**Returns:** New document handle, or `NULL` on allocation failure.

---

## Value Inspection

### json_get_type

```c
json_type json_get_type(json_val *val);
```

Get the type of a value.

### json_is_null / json_is_bool / json_is_num / json_is_str / json_is_arr / json_is_obj

```c
bool json_is_null(json_val *val);
bool json_is_bool(json_val *val);
bool json_is_num(json_val *val);
bool json_is_str(json_val *val);
bool json_is_arr(json_val *val);
bool json_is_obj(json_val *val);
```

Type checking predicates. Return `true` if the value is of the specified type.

### json_get_bool

```c
bool json_get_bool(json_val *val);
```

Get boolean value. Returns `false` if not a boolean.

### json_get_num

```c
double json_get_num(json_val *val);
```

Get number as double. Returns `0.0` if not a number.

### json_get_int

```c
int64_t json_get_int(json_val *val);
```

Get number as 64-bit integer. Truncates fractional part. Returns `0` if not a number.

### json_get_uint

```c
uint64_t json_get_uint(json_val *val);
```

Get number as unsigned 64-bit integer.

### json_get_str

```c
const char *json_get_str(json_val *val);
```

Get string value. Returns `NULL` if not a string. The returned pointer is valid until the document is freed.

### json_get_str_len

```c
size_t json_get_str_len(json_val *val);
```

Get string length in bytes. Useful for strings containing null bytes.

### json_num_is_int

```c
bool json_num_is_int(json_val *val);
```

Check if a number value can be represented exactly as an integer.

---

## Object Operations

### json_obj_get

```c
json_val *json_obj_get(json_val *obj, const char *key);
```

Get a value by key. Returns `NULL` if not found or if `obj` is not an object.

**Parameters:**
- `obj` - Object value
- `key` - Null-terminated key string

**Performance:** O(n) linear search. For frequent lookups on the same object, use `json_obj_get_idx()` with a hash table.

### json_obj_getn

```c
json_val *json_obj_getn(json_val *obj, const char *key, size_t len);
```

Get a value by key with explicit length. Allows keys containing null bytes.

### json_obj_size

```c
size_t json_obj_size(json_val *obj);
```

Get the number of key-value pairs.

### json_obj_iter

```c
typedef struct {
    const char *key;
    size_t key_len;
    json_val *val;
} json_obj_entry;

json_obj_entry json_obj_iter(json_val *obj, size_t *iter);
```

Iterate over object entries.

**Example:**
```c
size_t iter = 0;
json_obj_entry entry;
while ((entry = json_obj_iter(obj, &iter)).key != NULL) {
    printf("%s: ", entry.key);
    // ... process entry.val
}
```

---

## Array Operations

### json_arr_get

```c
json_val *json_arr_get(json_val *arr, size_t index);
```

Get element by index. Returns `NULL` if out of bounds.

**Performance:** O(1) direct access.

### json_arr_size

```c
size_t json_arr_size(json_val *arr);
```

Get array length.

### json_arr_iter

```c
json_val *json_arr_iter(json_val *arr, size_t *iter);
```

Iterate over array elements.

**Example:**
```c
size_t iter = 0;
json_val *elem;
while ((elem = json_arr_iter(arr, &iter)) != NULL) {
    // ... process elem
}
```

---

## Serialization

### json_stringify

```c
char *json_stringify(json_val *val);
```

Serialize value to a newly allocated string.

**Returns:** Null-terminated JSON string. Caller must free with `json_free()`.

### json_stringify_buf

```c
size_t json_stringify_buf(json_val *val, char *buf, size_t len);
```

Serialize into a provided buffer.

**Parameters:**
- `val` - Value to serialize
- `buf` - Output buffer (can be `NULL` to query size)
- `len` - Buffer size

**Returns:** Number of bytes written (excluding null terminator), or required size if buffer too small.

### json_stringify_opts

```c
char *json_stringify_opts(json_val *val, json_write_flags flags);
```

Serialize with options.

**Flags:**
```c
#define JSON_WRITE_DEFAULT      0x00  // Compact output
#define JSON_WRITE_PRETTY       0x01  // Pretty print with 2-space indent
#define JSON_WRITE_INDENT_4     0x02  // Use 4-space indent (with PRETTY)
#define JSON_WRITE_INDENT_TAB   0x04  // Use tab indent (with PRETTY)
#define JSON_WRITE_ESCAPE_UNI   0x08  // Escape non-ASCII as \uXXXX
#define JSON_WRITE_INF_NAN      0x10  // Write Infinity/NaN (non-standard)
```

### json_stringify_file

```c
int json_stringify_file(json_val *val, const char *path, json_write_flags flags);
```

Write JSON directly to a file.

**Returns:** 0 on success, -1 on error.

---

## Mutation

json-asm supports in-place mutation for building JSON documents.

### json_mut_doc

```c
json_mut_doc *json_mut_doc_new(void);
```

Create a new mutable document.

### json_mut_obj / json_mut_arr

```c
json_mut_val *json_mut_obj(json_mut_doc *doc);
json_mut_val *json_mut_arr(json_mut_doc *doc);
```

Create empty object or array.

### json_mut_null / json_mut_bool / json_mut_int / json_mut_real / json_mut_str

```c
json_mut_val *json_mut_null(json_mut_doc *doc);
json_mut_val *json_mut_bool(json_mut_doc *doc, bool val);
json_mut_val *json_mut_int(json_mut_doc *doc, int64_t val);
json_mut_val *json_mut_real(json_mut_doc *doc, double val);
json_mut_val *json_mut_str(json_mut_doc *doc, const char *str);
json_mut_val *json_mut_strn(json_mut_doc *doc, const char *str, size_t len);
```

Create primitive values.

### json_mut_obj_add

```c
bool json_mut_obj_add(json_mut_val *obj, const char *key, json_mut_val *val);
```

Add a key-value pair to an object.

### json_mut_arr_append

```c
bool json_mut_arr_append(json_mut_val *arr, json_mut_val *val);
```

Append a value to an array.

### json_mut_doc_set_root

```c
void json_mut_doc_set_root(json_mut_doc *doc, json_mut_val *root);
```

Set the document root value.

### json_mut_stringify

```c
char *json_mut_stringify(json_mut_doc *doc);
```

Serialize a mutable document.

---

## Error Handling

### json_last_error

```c
json_err json_last_error(void);
```

Get the last error for the current thread. Thread-local storage ensures thread safety.

### json_strerror

```c
const char *json_strerror(int code);
```

Get error message for an error code.

---

## Memory Management

### json_free

```c
void json_free(void *ptr);
```

Free memory allocated by json-asm (e.g., from `json_stringify()`).

### Custom Allocator

```c
typedef struct {
    void *(*malloc)(size_t size);
    void *(*realloc)(void *ptr, size_t size);
    void (*free)(void *ptr);
} json_allocator;

void json_set_allocator(json_allocator *alloc);
```

Set a custom allocator. Must be called before any other json-asm functions.

---

## Thread Safety

- All functions are reentrant and thread-safe
- A `json_doc` can be read from multiple threads simultaneously
- Mutation of a `json_mut_doc` requires external synchronization
- Error information is stored in thread-local storage
- Custom allocators must be thread-safe if used from multiple threads
