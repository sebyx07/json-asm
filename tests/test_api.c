/*
 * json-asm: API tests
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "json_asm.h"

#define TEST(name) static void test_##name(void)
#define RUN(name) do { \
    printf("  " #name "... "); \
    test_##name(); \
    printf("OK\n"); \
} while(0)

/* ============================================================================
 * Initialization Tests
 * ============================================================================ */

TEST(version) {
    const char *ver = json_version();
    assert(ver != NULL);
    assert(strlen(ver) > 0);
    printf("(v%s) ", ver);
}

TEST(cpu_features) {
    uint32_t features = json_get_cpu_features();
    /* On x86-64, should have at least SSE4.2 */
    /* On ARM64, should have at least NEON */
    printf("(features=0x%x) ", features);
}

/* ============================================================================
 * Type Name Tests
 * ============================================================================ */

TEST(type_names) {
    assert(strcmp(json_type_name(JSON_NULL), "null") == 0);
    assert(strcmp(json_type_name(JSON_TRUE), "true") == 0);
    assert(strcmp(json_type_name(JSON_FALSE), "false") == 0);
    assert(strcmp(json_type_name(JSON_INT), "integer") == 0);
    assert(strcmp(json_type_name(JSON_FLOAT), "float") == 0);
    assert(strcmp(json_type_name(JSON_STRING), "string") == 0);
    assert(strcmp(json_type_name(JSON_ARRAY), "array") == 0);
    assert(strcmp(json_type_name(JSON_OBJECT), "object") == 0);
}

TEST(error_strings) {
    assert(strcmp(json_error_string(JSON_OK), "No error") == 0);
    assert(strcmp(json_error_string(JSON_ERROR_MEMORY), "Memory allocation failed") == 0);
    assert(strcmp(json_error_string(JSON_ERROR_SYNTAX), "Invalid JSON syntax") == 0);
}

/* ============================================================================
 * Document Tests
 * ============================================================================ */

TEST(doc_memory) {
    json_doc *doc = json_parse("{\"key\":\"value\"}", 15);
    assert(doc != NULL);

    size_t mem = json_doc_memory(doc);
    assert(mem > 0);
    printf("(mem=%zu) ", mem);

    json_doc_free(doc);
}

TEST(doc_count) {
    json_doc *doc = json_parse("[1,2,3]", 7);
    assert(doc != NULL);

    size_t count = json_doc_count(doc);
    assert(count >= 4); /* Array + 3 numbers */

    json_doc_free(doc);
}

/* ============================================================================
 * Object Iteration Tests
 * ============================================================================ */

TEST(obj_iteration) {
    json_doc *doc = json_parse("{\"a\":1,\"b\":2,\"c\":3}", 19);
    assert(doc != NULL);
    json_val *obj = json_doc_root(doc);

    int count = 0;
    json_obj_foreach(obj, key) {
        assert(json_obj_key(key) != NULL);
        assert(json_obj_key_len(key) == 1);
        count++;
    }
    assert(count == 3);

    json_doc_free(doc);
}

TEST(obj_has) {
    json_doc *doc = json_parse("{\"exists\":true}", 15);
    assert(doc != NULL);
    json_val *obj = json_doc_root(doc);

    assert(json_obj_has(obj, "exists") == true);
    assert(json_obj_has(obj, "missing") == false);

    json_doc_free(doc);
}

/* ============================================================================
 * Array Iteration Tests
 * ============================================================================ */

TEST(arr_iteration) {
    json_doc *doc = json_parse("[1,2,3,4,5]", 11);
    assert(doc != NULL);
    json_val *arr = json_doc_root(doc);

    int count = 0;
    int64_t sum = 0;
    json_arr_foreach(arr, elem) {
        sum += json_get_int(elem);
        count++;
    }
    assert(count == 5);
    assert(sum == 15);

    json_doc_free(doc);
}

TEST(arr_get) {
    json_doc *doc = json_parse("[10,20,30]", 10);
    assert(doc != NULL);
    json_val *arr = json_doc_root(doc);

    assert(json_get_int(json_arr_get(arr, 0)) == 10);
    assert(json_get_int(json_arr_get(arr, 1)) == 20);
    assert(json_get_int(json_arr_get(arr, 2)) == 30);
    assert(json_arr_get(arr, 3) == NULL); /* Out of bounds */
    assert(json_arr_get(arr, 100) == NULL);

    json_doc_free(doc);
}

/* ============================================================================
 * Equality Tests
 * ============================================================================ */

TEST(equals_primitives) {
    json_doc *d1 = json_parse("42", 2);
    json_doc *d2 = json_parse("42", 2);
    json_doc *d3 = json_parse("43", 2);

    assert(json_equals(json_doc_root(d1), json_doc_root(d2)));
    assert(!json_equals(json_doc_root(d1), json_doc_root(d3)));

    json_doc_free(d1);
    json_doc_free(d2);
    json_doc_free(d3);
}

TEST(equals_strings) {
    json_doc *d1 = json_parse("\"hello\"", 7);
    json_doc *d2 = json_parse("\"hello\"", 7);
    json_doc *d3 = json_parse("\"world\"", 7);

    assert(json_equals(json_doc_root(d1), json_doc_root(d2)));
    assert(!json_equals(json_doc_root(d1), json_doc_root(d3)));

    json_doc_free(d1);
    json_doc_free(d2);
    json_doc_free(d3);
}

TEST(equals_arrays) {
    json_doc *d1 = json_parse("[1,2,3]", 7);
    json_doc *d2 = json_parse("[1,2,3]", 7);
    json_doc *d3 = json_parse("[1,2,4]", 7);
    json_doc *d4 = json_parse("[1,2]", 5);

    assert(json_equals(json_doc_root(d1), json_doc_root(d2)));
    assert(!json_equals(json_doc_root(d1), json_doc_root(d3)));
    assert(!json_equals(json_doc_root(d1), json_doc_root(d4)));

    json_doc_free(d1);
    json_doc_free(d2);
    json_doc_free(d3);
    json_doc_free(d4);
}

TEST(equals_objects) {
    json_doc *d1 = json_parse("{\"a\":1,\"b\":2}", 13);
    json_doc *d2 = json_parse("{\"a\":1,\"b\":2}", 13);
    json_doc *d3 = json_parse("{\"a\":1,\"b\":3}", 13);
    json_doc *d4 = json_parse("{\"a\":1}", 7);

    assert(json_equals(json_doc_root(d1), json_doc_root(d2)));
    assert(!json_equals(json_doc_root(d1), json_doc_root(d3)));
    assert(!json_equals(json_doc_root(d1), json_doc_root(d4)));

    json_doc_free(d1);
    json_doc_free(d2);
    json_doc_free(d3);
    json_doc_free(d4);
}

/* ============================================================================
 * Clone Tests
 * ============================================================================ */

TEST(clone) {
    json_doc *orig = json_parse("{\"a\":[1,2,3],\"b\":\"hello\"}", 25);
    assert(orig != NULL);

    json_doc *copy = json_clone(json_doc_root(orig));
    assert(copy != NULL);

    assert(json_equals(json_doc_root(orig), json_doc_root(copy)));

    json_doc_free(orig);
    json_doc_free(copy);
}

/* ============================================================================
 * NULL Safety Tests
 * ============================================================================ */

TEST(null_safety) {
    /* All functions should handle NULL gracefully */
    assert(json_doc_root(NULL) == NULL);
    assert(json_get_type(NULL) == JSON_NULL);
    assert(json_is_null(NULL) == false);
    assert(json_get_bool(NULL) == false);
    assert(json_get_int(NULL) == 0);
    assert(json_get_num(NULL) == 0.0);
    assert(json_get_str(NULL) == NULL);
    assert(json_get_str_len(NULL) == 0);
    assert(json_obj_get(NULL, "key") == NULL);
    assert(json_arr_get(NULL, 0) == NULL);
    assert(json_obj_size(NULL) == 0);
    assert(json_arr_size(NULL) == 0);
    assert(json_stringify(NULL) == NULL);
    assert(json_equals(NULL, NULL) == true);
    assert(json_clone(NULL) == NULL);

    /* Free should not crash on NULL */
    json_doc_free(NULL);
}

/* ============================================================================
 * Type Coercion Tests
 * ============================================================================ */

TEST(type_coercion) {
    /* Getting int from float */
    json_doc *d1 = json_parse("3.7", 3);
    assert(json_get_int(json_doc_root(d1)) == 3);
    json_doc_free(d1);

    /* Getting float from int */
    json_doc *d2 = json_parse("42", 2);
    assert(json_get_num(json_doc_root(d2)) == 42.0);
    json_doc_free(d2);

    /* Getting int from non-number returns 0 */
    json_doc *d3 = json_parse("\"hello\"", 7);
    assert(json_get_int(json_doc_root(d3)) == 0);
    json_doc_free(d3);
}

/* ============================================================================
 * Container Type Tests
 * ============================================================================ */

TEST(is_container) {
    json_doc *d1 = json_parse("[]", 2);
    json_doc *d2 = json_parse("{}", 2);
    json_doc *d3 = json_parse("42", 2);

    assert(json_is_container(json_doc_root(d1)) == true);
    assert(json_is_container(json_doc_root(d2)) == true);
    assert(json_is_container(json_doc_root(d3)) == false);

    json_doc_free(d1);
    json_doc_free(d2);
    json_doc_free(d3);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("API tests:\n");

    /* Initialization */
    RUN(version);
    RUN(cpu_features);

    /* Type names */
    RUN(type_names);
    RUN(error_strings);

    /* Document */
    RUN(doc_memory);
    RUN(doc_count);

    /* Object iteration */
    RUN(obj_iteration);
    RUN(obj_has);

    /* Array iteration */
    RUN(arr_iteration);
    RUN(arr_get);

    /* Equality */
    RUN(equals_primitives);
    RUN(equals_strings);
    RUN(equals_arrays);
    RUN(equals_objects);

    /* Clone */
    RUN(clone);

    /* NULL safety */
    RUN(null_safety);

    /* Type coercion */
    RUN(type_coercion);

    /* Container */
    RUN(is_container);

    printf("\nAll API tests passed!\n");
    return 0;
}
