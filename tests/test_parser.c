/*
 * Parser tests for xCDN-C.
 */

#include "xcdn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL [%s:%d]: %s\n", __FILE__, __LINE__, msg); \
        return; \
    } \
    tests_passed++; \
} while(0)

#define ASSERT_EQ_INT(a, b, msg) ASSERT((a) == (b), msg)
#define ASSERT_EQ_STR(a, b, msg) ASSERT(strcmp((a), (b)) == 0, msg)

/* ── Test: prolog directives ──────────────────────────────────────────── */

static void test_parse_prolog(void) {
    printf("  test_parse_prolog\n");
    const char *src =
        "$schema: \"https://example.com/schema\",\n"
        "$version: 2,\n"
        "\n"
        "{ answer: 42 }";

    xcdn_error_t err;
    xcdn_document_t *doc = xcdn_parse(src, &err);
    ASSERT(doc != NULL, "parse succeeded");
    ASSERT_EQ_INT((int)doc->prolog_len, 2, "2 directives");

    ASSERT_EQ_STR(doc->prolog[0].name, "schema", "directive name");
    ASSERT_EQ_INT(doc->prolog[0].value->type, XCDN_VAL_STRING, "schema is string");
    ASSERT_EQ_STR(xcdn_value_as_string(doc->prolog[0].value),
                  "https://example.com/schema", "schema value");

    ASSERT_EQ_STR(doc->prolog[1].name, "version", "version directive");
    ASSERT_EQ_INT(doc->prolog[1].value->type, XCDN_VAL_INT, "version is int");
    ASSERT_EQ_INT((int)xcdn_value_as_int(doc->prolog[1].value), 2, "version=2");

    ASSERT_EQ_INT((int)doc->values_len, 1, "1 value");
    xcdn_node_t *root = doc->values[0];
    ASSERT_EQ_INT(root->value->type, XCDN_VAL_OBJECT, "value is object");

    xcdn_node_t *answer = xcdn_object_get(root->value, "answer");
    ASSERT(answer != NULL, "answer key exists");
    ASSERT_EQ_INT(answer->value->type, XCDN_VAL_INT, "answer is int");
    ASSERT_EQ_INT((int)xcdn_value_as_int(answer->value), 42, "answer=42");

    xcdn_document_free(doc);
}

/* ── Test: implicit top-level object ──────────────────────────────────── */

static void test_parse_implicit_object(void) {
    printf("  test_parse_implicit_object\n");
    const char *src =
        "name: \"xcdn\",\n"
        "nested: { flag: true },\n";

    xcdn_error_t err;
    xcdn_document_t *doc = xcdn_parse(src, &err);
    ASSERT(doc != NULL, "parse succeeded");
    ASSERT_EQ_INT((int)doc->values_len, 1, "1 value");

    xcdn_node_t *root = doc->values[0];
    ASSERT_EQ_INT(root->value->type, XCDN_VAL_OBJECT, "implicit object");

    xcdn_node_t *name_node = xcdn_object_get(root->value, "name");
    ASSERT(name_node != NULL, "name exists");
    ASSERT_EQ_STR(xcdn_value_as_string(name_node->value), "xcdn", "name=xcdn");

    xcdn_node_t *nested = xcdn_object_get(root->value, "nested");
    ASSERT(nested != NULL, "nested exists");
    ASSERT_EQ_INT(nested->value->type, XCDN_VAL_OBJECT, "nested is object");

    xcdn_node_t *flag = xcdn_object_get(nested->value, "flag");
    ASSERT(flag != NULL, "flag exists");
    ASSERT_EQ_INT(flag->value->type, XCDN_VAL_BOOL, "flag is bool");
    ASSERT(xcdn_value_as_bool(flag->value) == true, "flag=true");

    xcdn_document_free(doc);
}

/* ── Test: annotations and tags ───────────────────────────────────────── */

static void test_parse_annotations_and_tags(void) {
    printf("  test_parse_annotations_and_tags\n");
    const char *src = "@mime(\"image/png\") #thumbnail b\"aGVsbG8=\"";

    xcdn_error_t err;
    xcdn_document_t *doc = xcdn_parse(src, &err);
    ASSERT(doc != NULL, "parse succeeded");
    ASSERT_EQ_INT((int)doc->values_len, 1, "1 value");

    xcdn_node_t *node = doc->values[0];
    ASSERT_EQ_INT((int)node->annotations_len, 1, "1 annotation");
    ASSERT_EQ_INT((int)node->tags_len, 1, "1 tag");

    ASSERT_EQ_STR(node->annotations[0].name, "mime", "annotation name");
    ASSERT_EQ_INT((int)node->annotations[0].args_len, 1, "1 annotation arg");
    ASSERT_EQ_STR(xcdn_value_as_string(node->annotations[0].args[0]),
                  "image/png", "mime arg");

    ASSERT_EQ_STR(node->tags[0].name, "thumbnail", "tag name");
    ASSERT_EQ_INT(node->value->type, XCDN_VAL_BYTES, "value is bytes");

    /* Verify the decoded bytes = "hello" */
    size_t blen = 0;
    const uint8_t *bdata = xcdn_value_as_bytes(node->value, &blen);
    ASSERT_EQ_INT((int)blen, 5, "5 bytes");
    ASSERT(memcmp(bdata, "hello", 5) == 0, "bytes = hello");

    xcdn_document_free(doc);
}

/* ── Test: stream of values ───────────────────────────────────────────── */

static void test_parse_stream(void) {
    printf("  test_parse_stream\n");
    const char *src = "{ a: 1 }\n42\n";

    xcdn_error_t err;
    xcdn_document_t *doc = xcdn_parse(src, &err);
    ASSERT(doc != NULL, "parse succeeded");
    ASSERT_EQ_INT((int)doc->values_len, 2, "2 values");

    ASSERT_EQ_INT(doc->values[0]->value->type, XCDN_VAL_OBJECT, "first is object");
    ASSERT_EQ_INT(doc->values[1]->value->type, XCDN_VAL_INT, "second is int");
    ASSERT_EQ_INT((int)xcdn_value_as_int(doc->values[1]->value), 42, "int=42");

    xcdn_document_free(doc);
}

/* ── Test: error on missing colon ─────────────────────────────────────── */

static void test_parse_missing_colon(void) {
    printf("  test_parse_missing_colon\n");
    const char *src = "{ a 1 }";

    xcdn_error_t err;
    xcdn_document_t *doc = xcdn_parse(src, &err);
    ASSERT(doc == NULL, "parse failed");
    ASSERT_EQ_INT(err.kind, XCDN_ERR_EXPECTED, "expected error");
}

/* ── Test: nested objects and arrays ──────────────────────────────────── */

static void test_parse_nested(void) {
    printf("  test_parse_nested\n");
    const char *src =
        "config: {\n"
        "  items: [1, 2, { nested: true }],\n"
        "  deep: { level2: { level3: \"found\" } },\n"
        "}";

    xcdn_error_t err;
    xcdn_document_t *doc = xcdn_parse(src, &err);
    ASSERT(doc != NULL, "parse succeeded");

    xcdn_node_t *config = xcdn_document_get_key(doc, "config");
    ASSERT(config != NULL, "config exists");
    ASSERT_EQ_INT(config->value->type, XCDN_VAL_OBJECT, "config is object");

    /* items array */
    xcdn_node_t *items = xcdn_object_get(config->value, "items");
    ASSERT(items != NULL, "items exists");
    ASSERT_EQ_INT(items->value->type, XCDN_VAL_ARRAY, "items is array");
    ASSERT_EQ_INT((int)xcdn_array_len(items->value), 3, "3 items");

    xcdn_node_t *item0 = xcdn_array_get(items->value, 0);
    ASSERT_EQ_INT((int)xcdn_value_as_int(item0->value), 1, "first item=1");

    xcdn_node_t *item2 = xcdn_array_get(items->value, 2);
    ASSERT_EQ_INT(item2->value->type, XCDN_VAL_OBJECT, "third item is object");

    /* deep path */
    xcdn_node_t *deep = xcdn_get_path(doc, "config.deep.level2.level3");
    ASSERT(deep != NULL, "deep path found");
    ASSERT_EQ_STR(xcdn_value_as_string(deep->value), "found", "deep value");

    xcdn_document_free(doc);
}

/* ── Test: all value types ────────────────────────────────────────────── */

static void test_parse_all_types(void) {
    printf("  test_parse_all_types\n");
    const char *src =
        "{\n"
        "  n: null,\n"
        "  b: true,\n"
        "  i: 42,\n"
        "  f: 3.14,\n"
        "  s: \"hello\",\n"
        "  d: d\"19.99\",\n"
        "  bytes: b\"aGVsbG8=\",\n"
        "  dt: t\"2025-01-15T10:30:00Z\",\n"
        "  dur: r\"PT30S\",\n"
        "  uuid: u\"550e8400-e29b-41d4-a716-446655440000\",\n"
        "  arr: [1, 2],\n"
        "  obj: { a: 1 },\n"
        "}";

    xcdn_error_t err;
    xcdn_document_t *doc = xcdn_parse(src, &err);
    ASSERT(doc != NULL, "parse succeeded");

    xcdn_node_t *root = doc->values[0];
    xcdn_value_t *obj = root->value;

    xcdn_node_t *n;

    n = xcdn_object_get(obj, "n");
    ASSERT(n && n->value->type == XCDN_VAL_NULL, "null type");

    n = xcdn_object_get(obj, "b");
    ASSERT(n && n->value->type == XCDN_VAL_BOOL, "bool type");
    ASSERT(xcdn_value_as_bool(n->value) == true, "bool=true");

    n = xcdn_object_get(obj, "i");
    ASSERT(n && n->value->type == XCDN_VAL_INT, "int type");
    ASSERT_EQ_INT((int)xcdn_value_as_int(n->value), 42, "int=42");

    n = xcdn_object_get(obj, "f");
    ASSERT(n && n->value->type == XCDN_VAL_FLOAT, "float type");

    n = xcdn_object_get(obj, "s");
    ASSERT(n && n->value->type == XCDN_VAL_STRING, "string type");
    ASSERT_EQ_STR(xcdn_value_as_string(n->value), "hello", "string=hello");

    n = xcdn_object_get(obj, "d");
    ASSERT(n && n->value->type == XCDN_VAL_DECIMAL, "decimal type");
    ASSERT_EQ_STR(xcdn_value_as_string(n->value), "19.99", "decimal=19.99");

    n = xcdn_object_get(obj, "bytes");
    ASSERT(n && n->value->type == XCDN_VAL_BYTES, "bytes type");

    n = xcdn_object_get(obj, "dt");
    ASSERT(n && n->value->type == XCDN_VAL_DATETIME, "datetime type");
    ASSERT_EQ_STR(xcdn_value_as_string(n->value), "2025-01-15T10:30:00Z",
                  "datetime value");

    n = xcdn_object_get(obj, "dur");
    ASSERT(n && n->value->type == XCDN_VAL_DURATION, "duration type");
    ASSERT_EQ_STR(xcdn_value_as_string(n->value), "PT30S", "duration=PT30S");

    n = xcdn_object_get(obj, "uuid");
    ASSERT(n && n->value->type == XCDN_VAL_UUID, "uuid type");
    ASSERT_EQ_STR(xcdn_value_as_string(n->value),
                  "550e8400-e29b-41d4-a716-446655440000", "uuid value");

    n = xcdn_object_get(obj, "arr");
    ASSERT(n && n->value->type == XCDN_VAL_ARRAY, "array type");
    ASSERT_EQ_INT((int)xcdn_array_len(n->value), 2, "array len=2");

    n = xcdn_object_get(obj, "obj");
    ASSERT(n && n->value->type == XCDN_VAL_OBJECT, "nested obj type");
    ASSERT_EQ_INT((int)xcdn_object_len(n->value), 1, "nested obj len=1");

    xcdn_document_free(doc);
}

/* ── Test: multiple tags and annotations ──────────────────────────────── */

static void test_parse_multiple_decorations(void) {
    printf("  test_parse_multiple_decorations\n");
    const char *src = "@size(100, 200) @visible #important #urgent \"task\"";

    xcdn_error_t err;
    xcdn_document_t *doc = xcdn_parse(src, &err);
    ASSERT(doc != NULL, "parse succeeded");

    xcdn_node_t *node = doc->values[0];
    ASSERT_EQ_INT((int)node->annotations_len, 2, "2 annotations");
    ASSERT_EQ_INT((int)node->tags_len, 2, "2 tags");

    /* @size(100, 200) */
    ASSERT_EQ_STR(node->annotations[0].name, "size", "first annotation=size");
    ASSERT_EQ_INT((int)node->annotations[0].args_len, 2, "size has 2 args");
    ASSERT_EQ_INT((int)xcdn_value_as_int(node->annotations[0].args[0]),
                  100, "size arg0=100");
    ASSERT_EQ_INT((int)xcdn_value_as_int(node->annotations[0].args[1]),
                  200, "size arg1=200");

    /* @visible (no args) */
    ASSERT_EQ_STR(node->annotations[1].name, "visible", "second annotation");
    ASSERT_EQ_INT((int)node->annotations[1].args_len, 0, "visible has 0 args");

    /* #important #urgent */
    ASSERT_EQ_STR(node->tags[0].name, "important", "tag1=important");
    ASSERT_EQ_STR(node->tags[1].name, "urgent", "tag2=urgent");

    /* Check accessor functions */
    ASSERT(xcdn_node_has_tag(node, "important"), "has_tag important");
    ASSERT(xcdn_node_has_tag(node, "urgent"), "has_tag urgent");
    ASSERT(!xcdn_node_has_tag(node, "nonexistent"), "no tag nonexistent");

    ASSERT(xcdn_node_has_annotation(node, "size"), "has_annotation size");
    ASSERT(xcdn_node_has_annotation(node, "visible"), "has_annotation visible");
    ASSERT(!xcdn_node_has_annotation(node, "none"), "no annotation none");

    const xcdn_annotation_t *size = xcdn_node_find_annotation(node, "size");
    ASSERT(size != NULL, "find_annotation size");
    ASSERT_EQ_INT((int)xcdn_annotation_arg_count(size), 2, "size arg count");

    xcdn_document_free(doc);
}

/* ── Test: empty document ─────────────────────────────────────────────── */

static void test_parse_empty_document(void) {
    printf("  test_parse_empty_document\n");
    const char *src = "";

    xcdn_error_t err;
    xcdn_document_t *doc = xcdn_parse(src, &err);
    ASSERT(doc != NULL, "parse succeeded");
    ASSERT_EQ_INT((int)doc->prolog_len, 0, "no prolog");
    ASSERT_EQ_INT((int)doc->values_len, 0, "no values");

    xcdn_document_free(doc);
}

/* ── Test: trailing commas ────────────────────────────────────────────── */

static void test_parse_trailing_commas(void) {
    printf("  test_parse_trailing_commas\n");
    const char *src = "{ a: 1, b: 2, }";

    xcdn_error_t err;
    xcdn_document_t *doc = xcdn_parse(src, &err);
    ASSERT(doc != NULL, "parse succeeded");
    ASSERT_EQ_INT((int)doc->values_len, 1, "1 value");

    xcdn_node_t *root = doc->values[0];
    ASSERT_EQ_INT((int)xcdn_object_len(root->value), 2, "2 entries");

    xcdn_document_free(doc);
}

/* ── Main ─────────────────────────────────────────────────────────────── */

int main(void) {
    printf("=== Parser Tests ===\n");

    test_parse_prolog();
    test_parse_implicit_object();
    test_parse_annotations_and_tags();
    test_parse_stream();
    test_parse_missing_colon();
    test_parse_nested();
    test_parse_all_types();
    test_parse_multiple_decorations();
    test_parse_empty_document();
    test_parse_trailing_commas();

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
