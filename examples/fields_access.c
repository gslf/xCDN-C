/*
 * Example: accessing fields in an xCDN document.
 *
 * Demonstrates the ergonomic accessor API for navigating nested structures.
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

    /*
     * Method 1: Step-by-step access using xcdn_object_get
     */
    xcdn_node_t *config = xcdn_document_get_key(doc, "config");
    if (config) {
        xcdn_node_t *name = xcdn_object_get(config->value, "name");
        if (name) {
            printf("Name: %s\n", xcdn_value_as_string(name->value));
        }

        xcdn_node_t *version = xcdn_object_get(config->value, "version");
        if (version) {
            printf("Version: %s\n", xcdn_value_as_string(version->value));
        }

        /* Array access */
        xcdn_node_t *ids = xcdn_object_get(config->value, "ids");
        if (ids) {
            xcdn_node_t *first = xcdn_array_get(ids->value, 0);
            if (first) {
                printf("First ID: %lld\n",
                       (long long)xcdn_value_as_int(first->value));
            }
        }
    }

    /*
     * Method 2: Deep path access with xcdn_get_path (dot-separated)
     */
    xcdn_node_t *deep = xcdn_get_path(doc, "config.nested.deep.value");
    if (deep) {
        printf("Deep value: %s\n", xcdn_value_as_string(deep->value));
    }

    xcdn_document_free(doc);
    return 0;
}
