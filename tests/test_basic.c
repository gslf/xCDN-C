/*
 * Basic integration tests for xCDN-C.
 *
 * Full roundtrip: parse -> serialize -> reparse -> verify.
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

/* ── Test: full roundtrip with all features ───────────────────────────── */

static void test_full_roundtrip(void) {
    printf("  test_full_roundtrip\n");
    const char *input =
        "$schema: \"https://gslf.github.io/xCDN/schemas/v1/meta.xcdn\",\n"
        "\n"
        "server_config: {\n"
        "  host: \"localhost\",\n"
        "  // Unquoted keys & trailing commas? Yes.\n"
        "  ports: [8080, 9090,],\n"
        "\n"
        "  // Native Decimals & ISO8601 Duration\n"
        "  timeout: r\"PT30S\",\n"
        "  max_cost: d\"19.99\",\n"
        "  // Semantic Tagging\n"
        "  admin: #user {\n"
        "    id: u\"550e8400-e29b-41d4-a716-446655440000\",\n"
        "    role: \"superuser\"\n"
        "  },\n"
        "\n"
        "  // Binary data handling\n"
        "  icon: @mime(\"image/png\") b\"aGVsbG8=\",\n"
        "}";

    xcdn_error_t err;
    xcdn_document_t *doc = xcdn_parse(input, &err);
    ASSERT(doc != NULL, "parse succeeded");
    ASSERT(doc->values_len > 0, "has values");

    /* Verify prolog */
    ASSERT_EQ_INT((int)doc->prolog_len, 1, "1 prolog directive");
    ASSERT_EQ_STR(doc->prolog[0].name, "schema", "prolog name=schema");

    /* Verify structure */
    xcdn_node_t *sc = xcdn_document_get_key(doc, "server_config");
    ASSERT(sc != NULL, "server_config exists");
    ASSERT_EQ_INT(sc->value->type, XCDN_VAL_OBJECT, "server_config is object");

    /* host */
    xcdn_node_t *host = xcdn_object_get(sc->value, "host");
    ASSERT(host != NULL, "host exists");
    ASSERT_EQ_STR(xcdn_value_as_string(host->value), "localhost", "host=localhost");

    /* ports */
    xcdn_node_t *ports = xcdn_object_get(sc->value, "ports");
    ASSERT(ports != NULL, "ports exists");
    ASSERT_EQ_INT((int)xcdn_array_len(ports->value), 2, "2 ports");
    ASSERT_EQ_INT((int)xcdn_value_as_int(xcdn_array_get(ports->value, 0)->value),
                  8080, "port0=8080");
    ASSERT_EQ_INT((int)xcdn_value_as_int(xcdn_array_get(ports->value, 1)->value),
                  9090, "port1=9090");

    /* timeout (duration) */
    xcdn_node_t *timeout = xcdn_object_get(sc->value, "timeout");
    ASSERT(timeout != NULL, "timeout exists");
    ASSERT_EQ_INT(timeout->value->type, XCDN_VAL_DURATION, "timeout is duration");
    ASSERT_EQ_STR(xcdn_value_as_string(timeout->value), "PT30S", "timeout=PT30S");

    /* max_cost (decimal) */
    xcdn_node_t *cost = xcdn_object_get(sc->value, "max_cost");
    ASSERT(cost != NULL, "max_cost exists");
    ASSERT_EQ_INT(cost->value->type, XCDN_VAL_DECIMAL, "cost is decimal");
    ASSERT_EQ_STR(xcdn_value_as_string(cost->value), "19.99", "cost=19.99");

    /* admin (with #user tag) */
    xcdn_node_t *admin = xcdn_object_get(sc->value, "admin");
    ASSERT(admin != NULL, "admin exists");
    ASSERT(xcdn_node_has_tag(admin, "user"), "admin has #user tag");
    ASSERT_EQ_INT(admin->value->type, XCDN_VAL_OBJECT, "admin is object");

    xcdn_node_t *admin_id = xcdn_object_get(admin->value, "id");
    ASSERT(admin_id != NULL, "admin.id exists");
    ASSERT_EQ_INT(admin_id->value->type, XCDN_VAL_UUID, "id is uuid");
    ASSERT_EQ_STR(xcdn_value_as_string(admin_id->value),
                  "550e8400-e29b-41d4-a716-446655440000", "uuid value");

    xcdn_node_t *role = xcdn_object_get(admin->value, "role");
    ASSERT(role != NULL, "admin.role exists");
    ASSERT_EQ_STR(xcdn_value_as_string(role->value), "superuser", "role=superuser");

    /* icon (with @mime annotation) */
    xcdn_node_t *icon = xcdn_object_get(sc->value, "icon");
    ASSERT(icon != NULL, "icon exists");
    ASSERT(xcdn_node_has_annotation(icon, "mime"), "icon has @mime annotation");
    ASSERT_EQ_INT(icon->value->type, XCDN_VAL_BYTES, "icon is bytes");

    const xcdn_annotation_t *mime = xcdn_node_find_annotation(icon, "mime");
    ASSERT(mime != NULL, "mime annotation found");
    ASSERT_EQ_INT((int)xcdn_annotation_arg_count(mime), 1, "mime has 1 arg");
    ASSERT_EQ_STR(xcdn_value_as_string(xcdn_annotation_arg(mime, 0)),
                  "image/png", "mime=image/png");

    size_t blen;
    const uint8_t *bdata = xcdn_value_as_bytes(icon->value, &blen);
    ASSERT_EQ_INT((int)blen, 5, "icon is 5 bytes");
    ASSERT(memcmp(bdata, "hello", 5) == 0, "icon bytes = hello");

    /* Serialize and verify roundtrip */
    char *text = xcdn_to_string_pretty(doc);
    ASSERT(text != NULL, "serialization ok");
    ASSERT(strstr(text, "server_config") != NULL, "output contains server_config");

    /* Re-parse serialized output */
    xcdn_document_t *doc2 = xcdn_parse(text, &err);
    ASSERT(doc2 != NULL, "reparse succeeded");
    ASSERT(doc2->values_len > 0, "reparse has values");

    /* Verify key data survived roundtrip */
    xcdn_node_t *sc2 = xcdn_document_get_key(doc2, "server_config");
    ASSERT(sc2 != NULL, "server_config survives roundtrip");

    xcdn_node_t *host2 = xcdn_object_get(sc2->value, "host");
    ASSERT(host2 != NULL, "host survives roundtrip");
    ASSERT_EQ_STR(xcdn_value_as_string(host2->value), "localhost",
                  "host=localhost roundtrip");

    free(text);
    xcdn_document_free(doc);
    xcdn_document_free(doc2);
}

/* ── Test: programmatic construction ──────────────────────────────────── */

static void test_programmatic_construction(void) {
    printf("  test_programmatic_construction\n");

    xcdn_document_t *doc = xcdn_document_new();
    ASSERT(doc != NULL, "doc created");

    /* Build: { name: "Alice", age: 30 } */
    xcdn_value_t *obj = xcdn_value_object();
    xcdn_object_set(obj, "name", xcdn_node_new(xcdn_value_string("Alice")));
    xcdn_object_set(obj, "age", xcdn_node_new(xcdn_value_int(30)));

    xcdn_node_t *root = xcdn_node_new(obj);
    xcdn_node_add_tag(root, "person");
    xcdn_document_push_value(doc, root);

    /* Serialize */
    char *text = xcdn_to_string_pretty(doc);
    ASSERT(text != NULL, "serialized ok");
    ASSERT(strstr(text, "#person") != NULL, "tag in output");
    ASSERT(strstr(text, "\"Alice\"") != NULL, "name in output");
    ASSERT(strstr(text, "30") != NULL, "age in output");

    /* Re-parse */
    xcdn_error_t err;
    xcdn_document_t *doc2 = xcdn_parse(text, &err);
    ASSERT(doc2 != NULL, "reparse ok");

    free(text);
    xcdn_document_free(doc);
    xcdn_document_free(doc2);
}

/* ── Test: document path access ───────────────────────────────────────── */

static void test_path_access(void) {
    printf("  test_path_access\n");
    const char *src =
        "config: {\n"
        "  db: {\n"
        "    host: \"localhost\",\n"
        "    port: 5432,\n"
        "  },\n"
        "  cache: {\n"
        "    ttl: r\"PT5M\",\n"
        "  },\n"
        "}";

    xcdn_error_t err;
    xcdn_document_t *doc = xcdn_parse(src, &err);
    ASSERT(doc != NULL, "parse succeeded");

    /* Test xcdn_get_path */
    xcdn_node_t *host = xcdn_get_path(doc, "config.db.host");
    ASSERT(host != NULL, "path config.db.host found");
    ASSERT_EQ_STR(xcdn_value_as_string(host->value), "localhost", "host=localhost");

    xcdn_node_t *port = xcdn_get_path(doc, "config.db.port");
    ASSERT(port != NULL, "path config.db.port found");
    ASSERT_EQ_INT((int)xcdn_value_as_int(port->value), 5432, "port=5432");

    xcdn_node_t *ttl = xcdn_get_path(doc, "config.cache.ttl");
    ASSERT(ttl != NULL, "path config.cache.ttl found");
    ASSERT_EQ_STR(xcdn_value_as_string(ttl->value), "PT5M", "ttl=PT5M");

    /* Non-existent path */
    xcdn_node_t *missing = xcdn_get_path(doc, "config.db.nonexistent");
    ASSERT(missing == NULL, "missing path returns NULL");

    xcdn_node_t *too_deep = xcdn_get_path(doc, "config.db.host.x");
    ASSERT(too_deep == NULL, "too deep path returns NULL");

    xcdn_document_free(doc);
}

/* ── Test: object iteration ───────────────────────────────────────────── */

static void test_object_iteration(void) {
    printf("  test_object_iteration\n");
    const char *src = "{ a: 1, b: 2, c: 3 }";

    xcdn_error_t err;
    xcdn_document_t *doc = xcdn_parse(src, &err);
    ASSERT(doc != NULL, "parse succeeded");

    xcdn_value_t *obj = doc->values[0]->value;
    ASSERT_EQ_INT((int)xcdn_object_len(obj), 3, "3 entries");

    /* Verify order preserved */
    ASSERT_EQ_STR(xcdn_object_key_at(obj, 0), "a", "key0=a");
    ASSERT_EQ_STR(xcdn_object_key_at(obj, 1), "b", "key1=b");
    ASSERT_EQ_STR(xcdn_object_key_at(obj, 2), "c", "key2=c");

    ASSERT_EQ_INT((int)xcdn_value_as_int(xcdn_object_node_at(obj, 0)->value),
                  1, "val0=1");
    ASSERT_EQ_INT((int)xcdn_value_as_int(xcdn_object_node_at(obj, 1)->value),
                  2, "val1=2");
    ASSERT_EQ_INT((int)xcdn_value_as_int(xcdn_object_node_at(obj, 2)->value),
                  3, "val2=3");

    /* Has/get checks */
    ASSERT(xcdn_object_has(obj, "a"), "has a");
    ASSERT(xcdn_object_has(obj, "b"), "has b");
    ASSERT(!xcdn_object_has(obj, "d"), "no d");

    xcdn_document_free(doc);
}

/* ── Main ─────────────────────────────────────────────────────────────── */

int main(void) {
    printf("=== Basic Integration Tests ===\n");

    test_full_roundtrip();
    test_programmatic_construction();
    test_path_access();
    test_object_iteration();

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
