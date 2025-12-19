/*
 * json-asm: Stringify tests
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

/* Helper to parse and stringify */
static char *roundtrip(const char *json) {
    json_doc *doc = json_parse(json, strlen(json));
    if (!doc) return NULL;
    char *result = json_stringify(json_doc_root(doc));
    json_doc_free(doc);
    return result;
}

/* ============================================================================
 * Basic Value Tests
 * ============================================================================ */

TEST(stringify_null) {
    char *s = roundtrip("null");
    assert(s != NULL);
    assert(strcmp(s, "null") == 0);
    free(s);
}

TEST(stringify_true) {
    char *s = roundtrip("true");
    assert(s != NULL);
    assert(strcmp(s, "true") == 0);
    free(s);
}

TEST(stringify_false) {
    char *s = roundtrip("false");
    assert(s != NULL);
    assert(strcmp(s, "false") == 0);
    free(s);
}

/* ============================================================================
 * Number Tests
 * ============================================================================ */

TEST(stringify_zero) {
    char *s = roundtrip("0");
    assert(s != NULL);
    assert(strcmp(s, "0") == 0);
    free(s);
}

TEST(stringify_positive_int) {
    char *s = roundtrip("42");
    assert(s != NULL);
    assert(strcmp(s, "42") == 0);
    free(s);
}

TEST(stringify_negative_int) {
    char *s = roundtrip("-123");
    assert(s != NULL);
    assert(strcmp(s, "-123") == 0);
    free(s);
}

TEST(stringify_float) {
    json_doc *doc = json_parse("3.14", 4);
    assert(doc != NULL);
    char *s = json_stringify(json_doc_root(doc));
    assert(s != NULL);
    /* Float might have more precision */
    assert(strncmp(s, "3.14", 4) == 0);
    free(s);
    json_doc_free(doc);
}

/* ============================================================================
 * String Tests
 * ============================================================================ */

TEST(stringify_empty_string) {
    char *s = roundtrip("\"\"");
    assert(s != NULL);
    assert(strcmp(s, "\"\"") == 0);
    free(s);
}

TEST(stringify_simple_string) {
    char *s = roundtrip("\"hello\"");
    assert(s != NULL);
    assert(strcmp(s, "\"hello\"") == 0);
    free(s);
}

TEST(stringify_string_with_escapes) {
    /* Parse a string with escapes, stringify should preserve them */
    json_doc *doc = json_parse("\"hello\\nworld\"", 14);
    assert(doc != NULL);
    char *s = json_stringify(json_doc_root(doc));
    assert(s != NULL);
    assert(strcmp(s, "\"hello\\nworld\"") == 0);
    free(s);
    json_doc_free(doc);
}

TEST(stringify_string_with_quote) {
    json_doc *doc = json_parse("\"say \\\"hi\\\"\"", 12);
    assert(doc != NULL);
    char *s = json_stringify(json_doc_root(doc));
    assert(s != NULL);
    assert(strcmp(s, "\"say \\\"hi\\\"\"") == 0);
    free(s);
    json_doc_free(doc);
}

/* ============================================================================
 * Array Tests
 * ============================================================================ */

TEST(stringify_empty_array) {
    char *s = roundtrip("[]");
    assert(s != NULL);
    assert(strcmp(s, "[]") == 0);
    free(s);
}

TEST(stringify_simple_array) {
    char *s = roundtrip("[1,2,3]");
    assert(s != NULL);
    assert(strcmp(s, "[1,2,3]") == 0);
    free(s);
}

TEST(stringify_mixed_array) {
    char *s = roundtrip("[1,\"two\",true,null]");
    assert(s != NULL);
    assert(strcmp(s, "[1,\"two\",true,null]") == 0);
    free(s);
}

TEST(stringify_nested_array) {
    char *s = roundtrip("[[1,2],[3,4]]");
    assert(s != NULL);
    assert(strcmp(s, "[[1,2],[3,4]]") == 0);
    free(s);
}

/* ============================================================================
 * Object Tests
 * ============================================================================ */

TEST(stringify_empty_object) {
    char *s = roundtrip("{}");
    assert(s != NULL);
    assert(strcmp(s, "{}") == 0);
    free(s);
}

TEST(stringify_simple_object) {
    json_doc *doc = json_parse("{\"a\":1}", 7);
    assert(doc != NULL);
    char *s = json_stringify(json_doc_root(doc));
    assert(s != NULL);
    assert(strcmp(s, "{\"a\":1}") == 0);
    free(s);
    json_doc_free(doc);
}

TEST(stringify_nested_object) {
    json_doc *doc = json_parse("{\"x\":{\"y\":1}}", 13);
    assert(doc != NULL);
    char *s = json_stringify(json_doc_root(doc));
    assert(s != NULL);
    assert(strcmp(s, "{\"x\":{\"y\":1}}") == 0);
    free(s);
    json_doc_free(doc);
}

/* ============================================================================
 * Pretty Print Tests
 * ============================================================================ */

TEST(stringify_pretty_object) {
    json_doc *doc = json_parse("{\"a\":1,\"b\":2}", 13);
    assert(doc != NULL);

    json_stringify_options opts = {
        .flags = JSON_STRINGIFY_PRETTY,
        .indent = 2,
        .newline = "\n"
    };

    char *s = json_stringify_opts(json_doc_root(doc), &opts);
    assert(s != NULL);
    /* Should contain newlines and indentation */
    assert(strstr(s, "\n") != NULL);
    assert(strstr(s, "  ") != NULL);

    free(s);
    json_doc_free(doc);
}

TEST(stringify_pretty_array) {
    json_doc *doc = json_parse("[1,2,3]", 7);
    assert(doc != NULL);

    json_stringify_options opts = {
        .flags = JSON_STRINGIFY_PRETTY,
        .indent = 4,
        .newline = "\n"
    };

    char *s = json_stringify_opts(json_doc_root(doc), &opts);
    assert(s != NULL);
    assert(strstr(s, "\n") != NULL);

    free(s);
    json_doc_free(doc);
}

/* ============================================================================
 * Buffer Stringify Tests
 * ============================================================================ */

TEST(stringify_to_buffer) {
    json_doc *doc = json_parse("{\"key\":\"value\"}", 15);
    assert(doc != NULL);

    char buf[100];
    size_t len = json_stringify_buf(json_doc_root(doc), buf, sizeof(buf));
    assert(len > 0);
    assert(len < sizeof(buf));
    assert(strcmp(buf, "{\"key\":\"value\"}") == 0);

    json_doc_free(doc);
}

TEST(stringify_buffer_too_small) {
    json_doc *doc = json_parse("{\"key\":\"value\"}", 15);
    assert(doc != NULL);

    char buf[5];
    size_t len = json_stringify_buf(json_doc_root(doc), buf, sizeof(buf));
    /* Should return required size even though buffer is too small */
    assert(len > sizeof(buf));

    json_doc_free(doc);
}

/* ============================================================================
 * Roundtrip Tests
 * ============================================================================ */

TEST(roundtrip_complex) {
    const char *json = "{\"name\":\"test\",\"values\":[1,2,3],\"nested\":{\"x\":true}}";
    json_doc *doc1 = json_parse(json, strlen(json));
    assert(doc1 != NULL);

    char *s = json_stringify(json_doc_root(doc1));
    assert(s != NULL);

    json_doc *doc2 = json_parse(s, strlen(s));
    assert(doc2 != NULL);

    /* Values should be equal */
    assert(json_equals(json_doc_root(doc1), json_doc_root(doc2)));

    free(s);
    json_doc_free(doc1);
    json_doc_free(doc2);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("Stringify tests:\n");

    /* Basic values */
    RUN(stringify_null);
    RUN(stringify_true);
    RUN(stringify_false);

    /* Numbers */
    RUN(stringify_zero);
    RUN(stringify_positive_int);
    RUN(stringify_negative_int);
    RUN(stringify_float);

    /* Strings */
    RUN(stringify_empty_string);
    RUN(stringify_simple_string);
    RUN(stringify_string_with_escapes);
    RUN(stringify_string_with_quote);

    /* Arrays */
    RUN(stringify_empty_array);
    RUN(stringify_simple_array);
    RUN(stringify_mixed_array);
    RUN(stringify_nested_array);

    /* Objects */
    RUN(stringify_empty_object);
    RUN(stringify_simple_object);
    RUN(stringify_nested_object);

    /* Pretty print */
    RUN(stringify_pretty_object);
    RUN(stringify_pretty_array);

    /* Buffer */
    RUN(stringify_to_buffer);
    RUN(stringify_buffer_too_small);

    /* Roundtrip */
    RUN(roundtrip_complex);

    printf("\nAll stringify tests passed!\n");
    return 0;
}
