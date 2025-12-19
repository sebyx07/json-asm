/*
 * json-asm: Parser tests
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
 * Basic Type Tests
 * ============================================================================ */

TEST(parse_null) {
    json_doc *doc = json_parse("null", 4);
    assert(doc != NULL);
    json_val *root = json_doc_root(doc);
    assert(json_is_null(root));
    assert(json_get_type(root) == JSON_NULL);
    json_doc_free(doc);
}

TEST(parse_true) {
    json_doc *doc = json_parse("true", 4);
    assert(doc != NULL);
    json_val *root = json_doc_root(doc);
    assert(json_is_true(root));
    assert(json_is_bool(root));
    assert(json_get_bool(root) == true);
    json_doc_free(doc);
}

TEST(parse_false) {
    json_doc *doc = json_parse("false", 5);
    assert(doc != NULL);
    json_val *root = json_doc_root(doc);
    assert(json_is_false(root));
    assert(json_is_bool(root));
    assert(json_get_bool(root) == false);
    json_doc_free(doc);
}

/* ============================================================================
 * Number Tests
 * ============================================================================ */

TEST(parse_zero) {
    json_doc *doc = json_parse("0", 1);
    assert(doc != NULL);
    json_val *root = json_doc_root(doc);
    assert(json_is_int(root));
    assert(json_get_int(root) == 0);
    json_doc_free(doc);
}

TEST(parse_positive_int) {
    json_doc *doc = json_parse("42", 2);
    assert(doc != NULL);
    json_val *root = json_doc_root(doc);
    assert(json_is_int(root));
    assert(json_get_int(root) == 42);
    json_doc_free(doc);
}

TEST(parse_negative_int) {
    json_doc *doc = json_parse("-123", 4);
    assert(doc != NULL);
    json_val *root = json_doc_root(doc);
    assert(json_is_int(root));
    assert(json_get_int(root) == -123);
    json_doc_free(doc);
}

TEST(parse_large_int) {
    json_doc *doc = json_parse("9223372036854775807", 19); /* INT64_MAX */
    assert(doc != NULL);
    json_val *root = json_doc_root(doc);
    assert(json_is_number(root));
    json_doc_free(doc);
}

TEST(parse_float) {
    json_doc *doc = json_parse("3.14159", 7);
    assert(doc != NULL);
    json_val *root = json_doc_root(doc);
    assert(json_is_float(root));
    double val = json_get_num(root);
    assert(val > 3.14 && val < 3.15);
    json_doc_free(doc);
}

TEST(parse_float_exponent) {
    json_doc *doc = json_parse("1.5e10", 6);
    assert(doc != NULL);
    json_val *root = json_doc_root(doc);
    assert(json_is_float(root));
    double val = json_get_num(root);
    assert(val > 1.4e10 && val < 1.6e10);
    json_doc_free(doc);
}

TEST(parse_negative_exponent) {
    json_doc *doc = json_parse("1.5e-3", 6);
    assert(doc != NULL);
    json_val *root = json_doc_root(doc);
    assert(json_is_float(root));
    double val = json_get_num(root);
    assert(val > 0.001 && val < 0.002);
    json_doc_free(doc);
}

/* ============================================================================
 * String Tests
 * ============================================================================ */

TEST(parse_empty_string) {
    json_doc *doc = json_parse("\"\"", 2);
    assert(doc != NULL);
    json_val *root = json_doc_root(doc);
    assert(json_is_string(root));
    assert(json_get_str_len(root) == 0);
    assert(strcmp(json_get_str(root), "") == 0);
    json_doc_free(doc);
}

TEST(parse_simple_string) {
    json_doc *doc = json_parse("\"hello\"", 7);
    assert(doc != NULL);
    json_val *root = json_doc_root(doc);
    assert(json_is_string(root));
    assert(json_get_str_len(root) == 5);
    assert(strcmp(json_get_str(root), "hello") == 0);
    json_doc_free(doc);
}

TEST(parse_short_string) {
    /* String <= 7 bytes should use short string optimization */
    json_doc *doc = json_parse("\"abc\"", 5);
    assert(doc != NULL);
    json_val *root = json_doc_root(doc);
    assert(json_is_string(root));
    assert(strcmp(json_get_str(root), "abc") == 0);
    json_doc_free(doc);
}

TEST(parse_long_string) {
    json_doc *doc = json_parse("\"this is a longer string\"", 25);
    assert(doc != NULL);
    json_val *root = json_doc_root(doc);
    assert(json_is_string(root));
    assert(strcmp(json_get_str(root), "this is a longer string") == 0);
    json_doc_free(doc);
}

TEST(parse_escaped_string) {
    json_doc *doc = json_parse("\"hello\\nworld\"", 14);
    assert(doc != NULL);
    json_val *root = json_doc_root(doc);
    assert(json_is_string(root));
    assert(strcmp(json_get_str(root), "hello\nworld") == 0);
    json_doc_free(doc);
}

TEST(parse_escaped_quote) {
    json_doc *doc = json_parse("\"say \\\"hello\\\"\"", 15);
    assert(doc != NULL);
    json_val *root = json_doc_root(doc);
    assert(json_is_string(root));
    assert(strcmp(json_get_str(root), "say \"hello\"") == 0);
    json_doc_free(doc);
}

TEST(parse_unicode_escape) {
    json_doc *doc = json_parse("\"\\u0041\"", 8); /* 'A' */
    assert(doc != NULL);
    json_val *root = json_doc_root(doc);
    assert(json_is_string(root));
    assert(strcmp(json_get_str(root), "A") == 0);
    json_doc_free(doc);
}

/* ============================================================================
 * Array Tests
 * ============================================================================ */

TEST(parse_empty_array) {
    json_doc *doc = json_parse("[]", 2);
    assert(doc != NULL);
    json_val *root = json_doc_root(doc);
    assert(json_is_array(root));
    assert(json_arr_size(root) == 0);
    json_doc_free(doc);
}

TEST(parse_simple_array) {
    json_doc *doc = json_parse("[1, 2, 3]", 9);
    assert(doc != NULL);
    json_val *root = json_doc_root(doc);
    assert(json_is_array(root));
    assert(json_arr_size(root) == 3);

    assert(json_get_int(json_arr_get(root, 0)) == 1);
    assert(json_get_int(json_arr_get(root, 1)) == 2);
    assert(json_get_int(json_arr_get(root, 2)) == 3);

    json_doc_free(doc);
}

TEST(parse_mixed_array) {
    json_doc *doc = json_parse("[1, \"two\", true, null]", 22);
    assert(doc != NULL);
    json_val *root = json_doc_root(doc);
    assert(json_is_array(root));
    assert(json_arr_size(root) == 4);

    assert(json_is_int(json_arr_get(root, 0)));
    assert(json_is_string(json_arr_get(root, 1)));
    assert(json_is_true(json_arr_get(root, 2)));
    assert(json_is_null(json_arr_get(root, 3)));

    json_doc_free(doc);
}

TEST(parse_nested_array) {
    json_doc *doc = json_parse("[[1, 2], [3, 4]]", 16);
    assert(doc != NULL);
    json_val *root = json_doc_root(doc);
    assert(json_is_array(root));
    assert(json_arr_size(root) == 2);

    json_val *inner1 = json_arr_get(root, 0);
    assert(json_is_array(inner1));
    assert(json_arr_size(inner1) == 2);

    json_doc_free(doc);
}

/* ============================================================================
 * Object Tests
 * ============================================================================ */

TEST(parse_empty_object) {
    json_doc *doc = json_parse("{}", 2);
    assert(doc != NULL);
    json_val *root = json_doc_root(doc);
    assert(json_is_object(root));
    assert(json_obj_size(root) == 0);
    json_doc_free(doc);
}

TEST(parse_simple_object) {
    json_doc *doc = json_parse("{\"name\": \"John\", \"age\": 30}", 27);
    assert(doc != NULL);
    json_val *root = json_doc_root(doc);
    assert(json_is_object(root));
    assert(json_obj_size(root) == 2);

    json_val *name = json_obj_get(root, "name");
    assert(name != NULL);
    assert(json_is_string(name));
    assert(strcmp(json_get_str(name), "John") == 0);

    json_val *age = json_obj_get(root, "age");
    assert(age != NULL);
    assert(json_is_int(age));
    assert(json_get_int(age) == 30);

    json_doc_free(doc);
}

TEST(parse_nested_object) {
    const char *json = "{\"person\": {\"name\": \"Alice\", \"age\": 25}}";
    json_doc *doc = json_parse(json, strlen(json));
    assert(doc != NULL);
    json_val *root = json_doc_root(doc);
    assert(json_is_object(root));

    json_val *person = json_obj_get(root, "person");
    assert(json_is_object(person));

    json_val *name = json_obj_get(person, "name");
    assert(strcmp(json_get_str(name), "Alice") == 0);

    json_doc_free(doc);
}

/* ============================================================================
 * Whitespace Tests
 * ============================================================================ */

TEST(parse_with_whitespace) {
    const char *json = "  {  \"key\"  :  \"value\"  }  ";
    json_doc *doc = json_parse(json, strlen(json));
    assert(doc != NULL);
    json_val *root = json_doc_root(doc);
    assert(json_is_object(root));
    json_doc_free(doc);
}

TEST(parse_with_newlines) {
    const char *json = "{\n  \"key\": \"value\"\n}";
    json_doc *doc = json_parse(json, strlen(json));
    assert(doc != NULL);
    json_val *root = json_doc_root(doc);
    assert(json_is_object(root));
    json_doc_free(doc);
}

/* ============================================================================
 * Error Tests
 * ============================================================================ */

TEST(error_empty_input) {
    json_doc *doc = json_parse("", 0);
    assert(doc == NULL);
    json_error_info err = json_get_error();
    assert(err.code == JSON_ERROR_SYNTAX);
}

TEST(error_invalid_token) {
    json_doc *doc = json_parse("undefined", 9);
    assert(doc == NULL);
    json_error_info err = json_get_error();
    assert(err.code == JSON_ERROR_SYNTAX);
}

TEST(error_unclosed_string) {
    json_doc *doc = json_parse("\"hello", 6);
    assert(doc == NULL);
    json_error_info err = json_get_error();
    assert(err.code == JSON_ERROR_STRING);
}

TEST(error_unclosed_array) {
    json_doc *doc = json_parse("[1, 2, 3", 8);
    assert(doc == NULL);
    json_error_info err = json_get_error();
    assert(err.code == JSON_ERROR_SYNTAX);
}

TEST(error_trailing_content) {
    json_doc *doc = json_parse("{}[]", 4);
    assert(doc == NULL);
    json_error_info err = json_get_error();
    assert(err.code == JSON_ERROR_SYNTAX);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("Parser tests:\n");

    /* Basic types */
    RUN(parse_null);
    RUN(parse_true);
    RUN(parse_false);

    /* Numbers */
    RUN(parse_zero);
    RUN(parse_positive_int);
    RUN(parse_negative_int);
    RUN(parse_large_int);
    RUN(parse_float);
    RUN(parse_float_exponent);
    RUN(parse_negative_exponent);

    /* Strings */
    RUN(parse_empty_string);
    RUN(parse_simple_string);
    RUN(parse_short_string);
    RUN(parse_long_string);
    RUN(parse_escaped_string);
    RUN(parse_escaped_quote);
    RUN(parse_unicode_escape);

    /* Arrays */
    RUN(parse_empty_array);
    RUN(parse_simple_array);
    RUN(parse_mixed_array);
    RUN(parse_nested_array);

    /* Objects */
    RUN(parse_empty_object);
    RUN(parse_simple_object);
    RUN(parse_nested_object);

    /* Whitespace */
    RUN(parse_with_whitespace);
    RUN(parse_with_newlines);

    /* Errors */
    RUN(error_empty_input);
    RUN(error_invalid_token);
    RUN(error_unclosed_string);
    RUN(error_unclosed_array);
    RUN(error_trailing_content);

    printf("\nAll parser tests passed!\n");
    return 0;
}
