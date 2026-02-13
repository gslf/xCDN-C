/*
 * Example: dict-like operations on xCDN documents.
 *
 * Demonstrates checking key existence, iterating over keys,
 * accessing array items, and using tags/annotations.
 */

#include "xcdn.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    const char *input =
        "config: {\n"
        "  name: \"demo\",\n"
        "  version: \"1.0.0\",\n"
        "  ids: [1, 2, 3],\n"
        "  admin: #user @role(\"superuser\") {\n"
        "    id: u\"550e8400-e29b-41d4-a716-446655440000\",\n"
        "    email: \"admin@example.com\"\n"
        "  },\n"
        "  nested: {\n"
        "    deep: {\n"
        "      value: \"found it!\"\n"
        "    }\n"
        "  }\n"
        "}";

    xcdn_error_t err;
    xcdn_document_t *doc = xcdn_parse(input, &err);
    if (!doc) {
        fprintf(stderr, "Parse error: %s\n", err.message);
        return 1;
    }

    xcdn_node_t *config = xcdn_document_get_key(doc, "config");
    if (!config) {
        fprintf(stderr, "No config key found\n");
        xcdn_document_free(doc);
        return 1;
    }

    xcdn_value_t *obj = config->value;

    /* Check key existence */
    if (xcdn_object_has(obj, "name")) {
        printf("'name' exists in config\n");
    }

    /* Iterate over keys */
    printf("\nConfig keys:\n");
    size_t len = xcdn_object_len(obj);
    for (size_t i = 0; i < len; i++) {
        printf("  - %s\n", xcdn_object_key_at(obj, i));
    }

    /* Missing key returns NULL */
    xcdn_node_t *missing = xcdn_object_get(obj, "missing_key");
    printf("\nMissing key: %s\n", missing ? "found" : "NULL (not found)");

    /* Array iteration */
    xcdn_node_t *ids = xcdn_object_get(obj, "ids");
    if (ids) {
        size_t arr_len = xcdn_array_len(ids->value);
        printf("\nArray length: %zu\n", arr_len);
        printf("Array items:\n");
        for (size_t i = 0; i < arr_len; i++) {
            xcdn_node_t *item = xcdn_array_get(ids->value, i);
            printf("  [%zu] = %lld\n", i,
                   (long long)xcdn_value_as_int(item->value));
        }
    }

    /* Tags and annotations on the admin node */
    xcdn_node_t *admin = xcdn_object_get(obj, "admin");
    if (admin) {
        printf("\nAdmin node:\n");

        /* Tags */
        printf("  Tags (%zu):\n", xcdn_node_tag_count(admin));
        for (size_t i = 0; i < xcdn_node_tag_count(admin); i++) {
            printf("    #%s\n", xcdn_node_tag_at(admin, i));
        }

        /* Check specific tag */
        printf("  Has #user tag: %s\n",
               xcdn_node_has_tag(admin, "user") ? "yes" : "no");

        /* Annotations */
        printf("  Annotations (%zu):\n", xcdn_node_annotation_count(admin));
        const xcdn_annotation_t *role = xcdn_node_find_annotation(admin, "role");
        if (role) {
            printf("    @%s with %zu args\n",
                   role->name, xcdn_annotation_arg_count(role));
            if (xcdn_annotation_arg_count(role) > 0) {
                printf("    First arg: %s\n",
                       xcdn_value_as_string(xcdn_annotation_arg(role, 0)));
            }
        }
    }

    xcdn_document_free(doc);
    return 0;
}
