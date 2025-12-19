/*
 * json-asm: Arena allocator for cache-efficient value storage
 */

#include "internal.h"

#define ARENA_INITIAL_SIZE  (64 * 1024)     /* 64 KB initial arena */
#define ARENA_GROWTH_FACTOR 2
#define STRING_INITIAL_SIZE (16 * 1024)     /* 16 KB initial string storage */
#define ARENA_ALIGNMENT     64              /* Cache line alignment */

/* Align size up to boundary */
static inline size_t align_up(size_t size, size_t align) {
    return (size + align - 1) & ~(align - 1);
}

/* Aligned memory allocation */
static void *aligned_alloc_impl(size_t alignment, size_t size) {
#if defined(_WIN32)
    return _aligned_malloc(size, alignment);
#elif defined(__APPLE__) || defined(__ANDROID__)
    void *ptr = NULL;
    if (posix_memalign(&ptr, alignment, size) != 0) {
        return NULL;
    }
    return ptr;
#else
    return aligned_alloc(alignment, align_up(size, alignment));
#endif
}

static void aligned_free_impl(void *ptr) {
#if defined(_WIN32)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

struct json_doc *arena_create(size_t initial_size) {
    if (initial_size == 0) {
        initial_size = ARENA_INITIAL_SIZE;
    }
    initial_size = align_up(initial_size, ARENA_ALIGNMENT);

    struct json_doc *doc = calloc(1, sizeof(struct json_doc));
    if (!doc) return NULL;

    doc->arena = aligned_alloc_impl(ARENA_ALIGNMENT, initial_size);
    if (!doc->arena) {
        free(doc);
        return NULL;
    }

    doc->arena_size = initial_size;
    doc->arena_used = 0;

    doc->strings = malloc(STRING_INITIAL_SIZE);
    if (!doc->strings) {
        aligned_free_impl(doc->arena);
        free(doc);
        return NULL;
    }

    doc->strings_size = STRING_INITIAL_SIZE;
    doc->strings_used = 0;
    doc->root = NULL;
    doc->value_count = 0;
    doc->cpu_features = g_cpu_features;

    return doc;
}

void arena_destroy(struct json_doc *doc) {
    if (!doc) return;

    if (doc->arena) {
        aligned_free_impl(doc->arena);
    }
    if (doc->strings) {
        free(doc->strings);
    }
    free(doc);
}

static bool arena_grow(struct json_doc *doc, size_t needed) {
    size_t new_size = doc->arena_size * ARENA_GROWTH_FACTOR;
    while (new_size < doc->arena_used + needed) {
        new_size *= ARENA_GROWTH_FACTOR;
    }
    new_size = align_up(new_size, ARENA_ALIGNMENT);

    uint8_t *new_arena = aligned_alloc_impl(ARENA_ALIGNMENT, new_size);
    if (!new_arena) return false;

    memcpy(new_arena, doc->arena, doc->arena_used);
    aligned_free_impl(doc->arena);

    doc->arena = new_arena;
    doc->arena_size = new_size;
    return true;
}

static bool strings_grow(struct json_doc *doc, size_t needed) {
    size_t new_size = doc->strings_size * ARENA_GROWTH_FACTOR;
    while (new_size < doc->strings_used + needed) {
        new_size *= ARENA_GROWTH_FACTOR;
    }

    uint8_t *new_strings = realloc(doc->strings, new_size);
    if (!new_strings) return false;

    doc->strings = new_strings;
    doc->strings_size = new_size;
    return true;
}

struct json_val *arena_alloc_val(struct json_doc *doc) {
    size_t needed = sizeof(struct json_val);

    if (doc->arena_used + needed > doc->arena_size) {
        if (!arena_grow(doc, needed)) {
            return NULL;
        }
    }

    struct json_val *val = (struct json_val *)(doc->arena + doc->arena_used);
    doc->arena_used += needed;
    doc->value_count++;

    /* Zero-initialize */
    memset(val, 0, sizeof(*val));
    return val;
}

char *arena_alloc_string(struct json_doc *doc, size_t len) {
    /* Include null terminator */
    size_t needed = len + 1;

    if (doc->strings_used + needed > doc->strings_size) {
        if (!strings_grow(doc, needed)) {
            return NULL;
        }
    }

    char *str = (char *)(doc->strings + doc->strings_used);
    doc->strings_used += needed;
    return str;
}
