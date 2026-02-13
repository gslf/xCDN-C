/*
 * Serialization tests for xCDN-C.
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

/* ── Test: roundtrip pretty and compact ───────────────────────────────── */

static void test_serialize_roundtrip_pretty_and_compact(void) {
    printf("  test_serialize_roundtrip_pretty_and_compact\n");
    const char *src =
        "$schema: \"https://gslf.github.io/xCDN/schemas/v1/meta.xcdn\",\n"
        "\n"
        "config: {\n"
        "  host: \"localhost\",\n"
        "  ports: [8080, 9090,],\n"
        "  timeout: r\"PT30S\",\n"
        "  cost: d\"19.99\",\n"
        "  admin: #user { id: u\"550e8400-e29b-41d4-a716-446655440000\","
        " role: \"super\" },\n"
        "  icon: @mime(\"image/png\") b\"aGVsbG8=\",\n"
        "}";

    xcdn_error_t err;
    xcdn_document_t *doc = xcdn_parse(src, &err);
    ASSERT(doc != NULL, "parse succeeded");

    char *pretty = xcdn_to_string_pretty(doc);
    char *compact = xcdn_to_string_compact(doc);
    ASSERT(pretty != NULL, "pretty not null");
    ASSERT(compact != NULL, "compact not null");

    ASSERT(strstr(pretty, "config") != NULL, "pretty contains config");
    ASSERT(strstr(compact, "config") != NULL, "compact contains config");

    /* Pretty should have more newlines than compact */
    int pretty_nl = 0, compact_nl = 0;
    for (char *p = pretty; *p; p++) if (*p == '\n') pretty_nl++;
    for (char *p = compact; *p; p++) if (*p == '\n') compact_nl++;
    ASSERT(pretty_nl > compact_nl, "pretty has more newlines");

    free(pretty);
    free(compact);
    xcdn_document_free(doc);
}

/* ── Test: trailing commas option ─────────────────────────────────────── */

static void test_serialize_trailing_commas(void) {
    printf("  test_serialize_trailing_commas\n");
    const char *src = "{ a: 1, b: 2, }";

    xcdn_error_t err;
    xcdn_document_t *doc = xcdn_parse(src, &err);
    ASSERT(doc != NULL, "parse succeeded");

    xcdn_format_t fmt = {true, 2, false};  /* no trailing commas */
    char *s = xcdn_to_string_with_format(doc, fmt);
    ASSERT(s != NULL, "serialized ok");
    ASSERT(strstr(s, "a: ") != NULL, "contains a:");

    /* With trailing_commas=false, the last entry before } should not have comma */
    /* Look for the pattern "2\n}" which means no trailing comma */
    ASSERT(strstr(s, "2\n}") != NULL, "no trailing comma before }");

    free(s);
    xcdn_document_free(doc);
}

/* ── Test: string escaping ────────────────────────────────────────────── */

static void test_serialize_string_escapes(void) {
    printf("  test_serialize_string_escapes\n");
    const char *src =
        "{ a: \"line\\n\", b: \"quote: \\\"\", c: \"slash: \\\\\","
        " d: \"control: \\u0001\" }";

    xcdn_error_t err;
    xcdn_document_t *doc = xcdn_parse(src, &err);
    ASSERT(doc != NULL, "parse succeeded");

    char *s = xcdn_to_string_pretty(doc);
    ASSERT(s != NULL, "serialized ok");
    ASSERT(strstr(s, "\\n") != NULL, "contains \\n");
    ASSERT(strstr(s, "\\\"") != NULL, "contains \\\"");
    ASSERT(strstr(s, "\\\\") != NULL, "contains \\\\");

    free(s);
    xcdn_document_free(doc);
}

/* ── Test: all types serialize ────────────────────────────────────────── */

static void test_serialize_all_types(void) {
    printf("  test_serialize_all_types\n");
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
        "}";

    xcdn_error_t err;
    xcdn_document_t *doc = xcdn_parse(src, &err);
    ASSERT(doc != NULL, "parse succeeded");

    char *s = xcdn_to_string_pretty(doc);
    ASSERT(s != NULL, "serialized ok");

    ASSERT(strstr(s, "null") != NULL, "contains null");
    ASSERT(strstr(s, "true") != NULL, "contains true");
    ASSERT(strstr(s, "42") != NULL, "contains 42");
    ASSERT(strstr(s, "3.14") != NULL, "contains 3.14");
    ASSERT(strstr(s, "\"hello\"") != NULL, "contains hello");
    ASSERT(strstr(s, "d\"19.99\"") != NULL, "contains decimal");
    ASSERT(strstr(s, "b\"") != NULL, "contains bytes");
    ASSERT(strstr(s, "t\"2025-01-15T10:30:00Z\"") != NULL, "contains datetime");
    ASSERT(strstr(s, "r\"PT30S\"") != NULL, "contains duration");
    ASSERT(strstr(s, "u\"550e8400") != NULL, "contains uuid");

    free(s);
    xcdn_document_free(doc);
}

/* ── Test: roundtrip reparse ──────────────────────────────────────────── */

static void test_serialize_roundtrip_reparse(void) {
    printf("  test_serialize_roundtrip_reparse\n");
    const char *src =
        "config: {\n"
        "  name: \"demo\",\n"
        "  ids: [1, 2, 3,],\n"
        "  timeout: r\"PT30S\",\n"
        "}";

    xcdn_error_t err;
    xcdn_document_t *doc1 = xcdn_parse(src, &err);
    ASSERT(doc1 != NULL, "first parse ok");

    char *text = xcdn_to_string_pretty(doc1);
    ASSERT(text != NULL, "serialize ok");

    /* Re-parse the serialized output */
    xcdn_document_t *doc2 = xcdn_parse(text, &err);
    ASSERT(doc2 != NULL, "reparse ok");

    /* Verify structure is preserved */
    xcdn_node_t *config = xcdn_document_get_key(doc2, "config");
    ASSERT(config != NULL, "config still exists");

    xcdn_node_t *name = xcdn_object_get(config->value, "name");
    ASSERT(name != NULL, "name still exists");
    ASSERT_EQ_INT(name->value->type, XCDN_VAL_STRING, "name still string");
    ASSERT(strcmp(xcdn_value_as_string(name->value), "demo") == 0, "name=demo");

    xcdn_node_t *ids = xcdn_object_get(config->value, "ids");
    ASSERT(ids != NULL, "ids still exists");
    ASSERT_EQ_INT((int)xcdn_array_len(ids->value), 3, "3 ids");

    free(text);
    xcdn_document_free(doc1);
    xcdn_document_free(doc2);
}

/* ── Test: compact format ─────────────────────────────────────────────── */

static void test_serialize_compact(void) {
    printf("  test_serialize_compact\n");
    const char *src = "{ a: 1, b: [2, 3] }";

    xcdn_error_t err;
    xcdn_document_t *doc = xcdn_parse(src, &err);
    ASSERT(doc != NULL, "parse succeeded");

    char *s = xcdn_to_string_compact(doc);
    ASSERT(s != NULL, "serialized ok");

    /* Compact should have no newlines */
    ASSERT(strchr(s, '\n') == NULL, "no newlines in compact");

    /* Should still contain the data */
    ASSERT(strstr(s, "a:") != NULL || strstr(s, "a: ") != NULL, "contains a");

    free(s);
    xcdn_document_free(doc);
}

/* ── Test: annotations and tags are preserved ─────────────────────────── */

static void test_serialize_decorations(void) {
    printf("  test_serialize_decorations\n");
    const char *src = "@mime(\"image/png\") #thumbnail b\"aGVsbG8=\"";

    xcdn_error_t err;
    xcdn_document_t *doc = xcdn_parse(src, &err);
    ASSERT(doc != NULL, "parse succeeded");

    char *s = xcdn_to_string_pretty(doc);
    ASSERT(s != NULL, "serialized ok");
    ASSERT(strstr(s, "@mime(\"image/png\")") != NULL, "annotation preserved");
    ASSERT(strstr(s, "#thumbnail") != NULL, "tag preserved");
    ASSERT(strstr(s, "b\"") != NULL, "bytes preserved");

    free(s);
    xcdn_document_free(doc);
}

/* ── Test: prolog serialization ───────────────────────────────────────── */

static void test_serialize_prolog(void) {
    printf("  test_serialize_prolog\n");
    const char *src =
        "$schema: \"https://example.com\",\n"
        "$version: 2,\n"
        "{ a: 1 }";

    xcdn_error_t err;
    xcdn_document_t *doc = xcdn_parse(src, &err);
    ASSERT(doc != NULL, "parse succeeded");

    char *s = xcdn_to_string_pretty(doc);
    ASSERT(s != NULL, "serialized ok");
    ASSERT(strstr(s, "$schema: ") != NULL, "prolog schema");
    ASSERT(strstr(s, "$version: ") != NULL, "prolog version");

    free(s);
    xcdn_document_free(doc);
}

/* ── Main ─────────────────────────────────────────────────────────────── */

int main(void) {
    printf("=== Serializer Tests ===\n");

    test_serialize_roundtrip_pretty_and_compact();
    test_serialize_trailing_commas();
    test_serialize_string_escapes();
    test_serialize_all_types();
    test_serialize_roundtrip_reparse();
    test_serialize_compact();
    test_serialize_decorations();
    test_serialize_prolog();

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
