# Examples

Usage examples and common patterns for json-asm.

## Table of Contents

- [Basic Parsing](#basic-parsing)
- [Accessing Values](#accessing-values)
- [Iterating Collections](#iterating-collections)
- [Error Handling](#error-handling)
- [Serialization](#serialization)
- [Building JSON](#building-json)
- [File Operations](#file-operations)
- [Memory Management](#memory-management)
- [Performance Tips](#performance-tips)
- [Complete Examples](#complete-examples)

---

## Basic Parsing

### Parse a JSON String

```c
#include <json_asm.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *json = "{\"name\": \"Alice\", \"age\": 30}";

    json_doc *doc = json_parse(json, strlen(json));
    if (!doc) {
        json_err err = json_last_error();
        fprintf(stderr, "Parse error at line %zu, column %zu: %s\n",
                err.line, err.column, err.msg);
        return 1;
    }

    // Use the document...

    json_doc_free(doc);
    return 0;
}
```

### Parse with Options

```c
// Allow comments and trailing commas
json_doc *doc = json_parse_opts(json, len, JSON_PARSE_COMMENTS | JSON_PARSE_TRAILING);

// Parse relaxed JSON (all extensions)
json_doc *doc = json_parse_opts(json, len, JSON_PARSE_RELAXED);
```

---

## Accessing Values

### Get Values by Type

```c
json_val *root = json_doc_root(doc);

// Object access
json_val *name = json_obj_get(root, "name");
if (name && json_is_str(name)) {
    printf("Name: %s\n", json_get_str(name));
}

// Number access
json_val *age = json_obj_get(root, "age");
if (age && json_is_num(age)) {
    printf("Age: %lld\n", (long long)json_get_int(age));
}

// Boolean access
json_val *active = json_obj_get(root, "active");
if (active && json_is_bool(active)) {
    printf("Active: %s\n", json_get_bool(active) ? "true" : "false");
}

// Null check
json_val *data = json_obj_get(root, "data");
if (data && json_is_null(data)) {
    printf("Data is null\n");
}
```

### Nested Access

```c
// JSON: {"user": {"profile": {"email": "alice@example.com"}}}

json_val *root = json_doc_root(doc);
json_val *user = json_obj_get(root, "user");
json_val *profile = user ? json_obj_get(user, "profile") : NULL;
json_val *email = profile ? json_obj_get(profile, "email") : NULL;

if (email && json_is_str(email)) {
    printf("Email: %s\n", json_get_str(email));
}
```

### Safe Access Helper

```c
// Helper function for deep access
const char *json_path_str(json_val *root, const char *path) {
    char *p = strdup(path);
    char *token = strtok(p, ".");
    json_val *current = root;

    while (token && current) {
        current = json_obj_get(current, token);
        token = strtok(NULL, ".");
    }

    free(p);
    return (current && json_is_str(current)) ? json_get_str(current) : NULL;
}

// Usage
const char *email = json_path_str(root, "user.profile.email");
```

---

## Iterating Collections

### Iterate Over Object

```c
json_val *obj = json_doc_root(doc);
size_t iter = 0;
json_obj_entry entry;

while ((entry = json_obj_iter(obj, &iter)).key != NULL) {
    printf("Key: %s, Type: %d\n", entry.key, json_get_type(entry.val));
}
```

### Iterate Over Array

```c
json_val *arr = json_obj_get(root, "items");
size_t iter = 0;
json_val *elem;

while ((elem = json_arr_iter(arr, &iter)) != NULL) {
    if (json_is_str(elem)) {
        printf("Item: %s\n", json_get_str(elem));
    }
}
```

### Access Array by Index

```c
json_val *arr = json_obj_get(root, "numbers");
size_t len = json_arr_size(arr);

for (size_t i = 0; i < len; i++) {
    json_val *elem = json_arr_get(arr, i);
    printf("[%zu] = %lld\n", i, (long long)json_get_int(elem));
}
```

---

## Error Handling

### Detailed Error Information

```c
json_doc *doc = json_parse(json, len);
if (!doc) {
    json_err err = json_last_error();

    fprintf(stderr, "JSON Parse Error:\n");
    fprintf(stderr, "  Code: %d (%s)\n", err.code, json_strerror(err.code));
    fprintf(stderr, "  Position: byte %zu\n", err.position);
    fprintf(stderr, "  Location: line %zu, column %zu\n", err.line, err.column);
    fprintf(stderr, "  Message: %s\n", err.msg);

    return 1;
}
```

### Error Code Handling

```c
json_doc *doc = json_parse(json, len);
if (!doc) {
    json_err err = json_last_error();

    switch (err.code) {
        case JSON_ERR_NOMEM:
            fprintf(stderr, "Out of memory\n");
            break;
        case JSON_ERR_SYNTAX:
            fprintf(stderr, "Invalid syntax at line %zu\n", err.line);
            break;
        case JSON_ERR_UTF8:
            fprintf(stderr, "Invalid UTF-8 encoding\n");
            break;
        default:
            fprintf(stderr, "Parse error: %s\n", err.msg);
    }
}
```

---

## Serialization

### Compact Output

```c
json_val *root = json_doc_root(doc);
char *json = json_stringify(root);

printf("%s\n", json);
json_free(json);
```

### Pretty Print

```c
char *json = json_stringify_opts(root, JSON_WRITE_PRETTY);
printf("%s\n", json);
json_free(json);

// With 4-space indent
char *json = json_stringify_opts(root, JSON_WRITE_PRETTY | JSON_WRITE_INDENT_4);

// With tab indent
char *json = json_stringify_opts(root, JSON_WRITE_PRETTY | JSON_WRITE_INDENT_TAB);
```

### Write to Buffer

```c
char buffer[4096];
size_t written = json_stringify_buf(root, buffer, sizeof(buffer));

if (written < sizeof(buffer)) {
    printf("%s\n", buffer);
} else {
    fprintf(stderr, "Buffer too small, need %zu bytes\n", written + 1);
}
```

### Query Required Size

```c
// Pass NULL to get required size
size_t needed = json_stringify_buf(root, NULL, 0);

char *buffer = malloc(needed + 1);
json_stringify_buf(root, buffer, needed + 1);
printf("%s\n", buffer);
free(buffer);
```

---

## Building JSON

### Create Object

```c
json_mut_doc *doc = json_mut_doc_new();

// Create root object
json_mut_val *root = json_mut_obj(doc);
json_mut_doc_set_root(doc, root);

// Add fields
json_mut_obj_add(root, "name", json_mut_str(doc, "Alice"));
json_mut_obj_add(root, "age", json_mut_int(doc, 30));
json_mut_obj_add(root, "active", json_mut_bool(doc, true));
json_mut_obj_add(root, "score", json_mut_real(doc, 95.5));

// Serialize
char *json = json_mut_stringify(doc);
printf("%s\n", json);  // {"name":"Alice","age":30,"active":true,"score":95.5}

json_free(json);
json_mut_doc_free(doc);
```

### Create Array

```c
json_mut_doc *doc = json_mut_doc_new();

json_mut_val *arr = json_mut_arr(doc);
json_mut_doc_set_root(doc, arr);

json_mut_arr_append(arr, json_mut_int(doc, 1));
json_mut_arr_append(arr, json_mut_int(doc, 2));
json_mut_arr_append(arr, json_mut_int(doc, 3));

char *json = json_mut_stringify(doc);
printf("%s\n", json);  // [1,2,3]

json_free(json);
json_mut_doc_free(doc);
```

### Nested Structures

```c
json_mut_doc *doc = json_mut_doc_new();

// Create nested object
json_mut_val *root = json_mut_obj(doc);
json_mut_val *user = json_mut_obj(doc);
json_mut_val *tags = json_mut_arr(doc);

json_mut_obj_add(user, "name", json_mut_str(doc, "Alice"));
json_mut_obj_add(user, "email", json_mut_str(doc, "alice@example.com"));

json_mut_arr_append(tags, json_mut_str(doc, "admin"));
json_mut_arr_append(tags, json_mut_str(doc, "active"));

json_mut_obj_add(root, "user", user);
json_mut_obj_add(root, "tags", tags);
json_mut_doc_set_root(doc, root);

char *json = json_mut_stringify_opts(doc, JSON_WRITE_PRETTY);
printf("%s\n", json);

json_free(json);
json_mut_doc_free(doc);
```

---

## File Operations

### Parse from File

```c
json_doc *doc = json_parse_file("data.json");
if (!doc) {
    json_err err = json_last_error();
    if (err.code == JSON_ERR_IO) {
        perror("Failed to read file");
    } else {
        fprintf(stderr, "Parse error: %s\n", err.msg);
    }
    return 1;
}

// Process document...

json_doc_free(doc);
```

### Write to File

```c
json_val *root = json_doc_root(doc);
int result = json_stringify_file(root, "output.json", JSON_WRITE_PRETTY);

if (result != 0) {
    perror("Failed to write file");
}
```

---

## Memory Management

### Custom Allocator

```c
void *my_malloc(size_t size) {
    void *ptr = malloc(size);
    printf("Allocated %zu bytes at %p\n", size, ptr);
    return ptr;
}

void *my_realloc(void *ptr, size_t size) {
    void *new_ptr = realloc(ptr, size);
    printf("Reallocated %p to %zu bytes at %p\n", ptr, size, new_ptr);
    return new_ptr;
}

void my_free(void *ptr) {
    printf("Freed %p\n", ptr);
    free(ptr);
}

int main(void) {
    json_allocator alloc = {my_malloc, my_realloc, my_free};
    json_set_allocator(&alloc);

    // All json-asm allocations now use custom allocator
    json_doc *doc = json_parse(json, len);
    // ...
}
```

### Arena Allocator Example

```c
#include <stdint.h>

typedef struct {
    uint8_t *base;
    size_t size;
    size_t used;
} arena_t;

arena_t arena;

void *arena_malloc(size_t size) {
    size_t aligned = (size + 7) & ~7;
    if (arena.used + aligned > arena.size) return NULL;
    void *ptr = arena.base + arena.used;
    arena.used += aligned;
    return ptr;
}

void *arena_realloc(void *ptr, size_t size) {
    // Simple implementation: allocate new, don't free old
    return arena_malloc(size);
}

void arena_free(void *ptr) {
    // No-op for arena allocator
    (void)ptr;
}

int main(void) {
    // Pre-allocate 1MB arena
    arena.base = malloc(1024 * 1024);
    arena.size = 1024 * 1024;
    arena.used = 0;

    json_allocator alloc = {arena_malloc, arena_realloc, arena_free};
    json_set_allocator(&alloc);

    // Parse many documents, reset arena between them
    for (int i = 0; i < 1000; i++) {
        arena.used = 0;  // Reset arena
        json_doc *doc = json_parse(json, len);
        // Process doc...
        // No need to call json_doc_free - arena will be reset
    }

    free(arena.base);
}
```

---

## Performance Tips

### Reuse Documents

```c
// Bad: Parse repeatedly
for (int i = 0; i < 1000; i++) {
    json_doc *doc = json_parse(json, len);
    process(doc);
    json_doc_free(doc);
}

// Better: Parse once if data doesn't change
json_doc *doc = json_parse(json, len);
for (int i = 0; i < 1000; i++) {
    process(doc);
}
json_doc_free(doc);
```

### Pre-size Buffers

```c
// Query size first, allocate once
size_t needed = json_stringify_buf(root, NULL, 0);
char *buffer = malloc(needed + 1);
json_stringify_buf(root, buffer, needed + 1);
```

### Avoid Repeated Key Lookups

```c
// Bad: Multiple lookups
printf("Name: %s\n", json_get_str(json_obj_get(root, "name")));
printf("Name length: %zu\n", strlen(json_get_str(json_obj_get(root, "name"))));

// Better: Cache the value
json_val *name = json_obj_get(root, "name");
const char *name_str = json_get_str(name);
printf("Name: %s\n", name_str);
printf("Name length: %zu\n", strlen(name_str));
```

---

## Complete Examples

### Config File Parser

```c
#include <json_asm.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    const char *host;
    int port;
    bool debug;
    int max_connections;
} config_t;

bool load_config(const char *path, config_t *config) {
    json_doc *doc = json_parse_file(path);
    if (!doc) {
        fprintf(stderr, "Failed to parse config: %s\n", json_last_error().msg);
        return false;
    }

    json_val *root = json_doc_root(doc);

    json_val *host = json_obj_get(root, "host");
    json_val *port = json_obj_get(root, "port");
    json_val *debug = json_obj_get(root, "debug");
    json_val *max_conn = json_obj_get(root, "max_connections");

    config->host = host ? strdup(json_get_str(host)) : "localhost";
    config->port = port ? (int)json_get_int(port) : 8080;
    config->debug = debug ? json_get_bool(debug) : false;
    config->max_connections = max_conn ? (int)json_get_int(max_conn) : 100;

    json_doc_free(doc);
    return true;
}

int main(void) {
    config_t config;
    if (load_config("config.json", &config)) {
        printf("Server: %s:%d\n", config.host, config.port);
        printf("Debug: %s\n", config.debug ? "enabled" : "disabled");
    }
    return 0;
}
```

### JSON API Response Handler

```c
#include <json_asm.h>
#include <stdio.h>

void handle_api_response(const char *json, size_t len) {
    json_doc *doc = json_parse(json, len);
    if (!doc) {
        fprintf(stderr, "Invalid JSON response\n");
        return;
    }

    json_val *root = json_doc_root(doc);

    // Check for error response
    json_val *error = json_obj_get(root, "error");
    if (error && !json_is_null(error)) {
        const char *msg = json_get_str(json_obj_get(error, "message"));
        int code = (int)json_get_int(json_obj_get(error, "code"));
        fprintf(stderr, "API Error %d: %s\n", code, msg ? msg : "Unknown");
        json_doc_free(doc);
        return;
    }

    // Process data
    json_val *data = json_obj_get(root, "data");
    if (data && json_is_arr(data)) {
        size_t iter = 0;
        json_val *item;
        while ((item = json_arr_iter(data, &iter)) != NULL) {
            json_val *id = json_obj_get(item, "id");
            json_val *name = json_obj_get(item, "name");
            printf("Item %lld: %s\n",
                   (long long)json_get_int(id),
                   json_get_str(name));
        }
    }

    json_doc_free(doc);
}
```

### JSON Transformer

```c
#include <json_asm.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// Transform all string values to uppercase
void transform_strings(json_val *val) {
    switch (json_get_type(val)) {
        case JSON_OBJECT: {
            size_t iter = 0;
            json_obj_entry entry;
            while ((entry = json_obj_iter(val, &iter)).key != NULL) {
                transform_strings(entry.val);
            }
            break;
        }
        case JSON_ARRAY: {
            size_t iter = 0;
            json_val *elem;
            while ((elem = json_arr_iter(val, &iter)) != NULL) {
                transform_strings(elem);
            }
            break;
        }
        case JSON_STRING:
            // Note: Actual mutation would require mutable document
            printf("Would uppercase: %s\n", json_get_str(val));
            break;
        default:
            break;
    }
}

int main(void) {
    const char *json = "{\"name\": \"alice\", \"items\": [\"foo\", \"bar\"]}";
    json_doc *doc = json_parse(json, strlen(json));

    transform_strings(json_doc_root(doc));

    json_doc_free(doc);
    return 0;
}
```
