/*
 * Example: roundtrip parse and serialize an xCDN document.
 */

#include "xcdn.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    const char *input =
        "$schema: \"https://gslf.github.io/xCDN/schemas/v1/meta.xcdn\",\n"
        "\n"
        "config: {\n"
        "  name: \"demo\",\n"
        "  ids: [1, 2, 3,],\n"
        "  timeout: r\"PT30S\",\n"
        "  id: u\"550e8400-e29b-41d4-a716-446655440000\",\n"
        "  created_at: t\"2025-12-07T10:00:00Z\",\n"
        "  payload: b\"aGVsbG8=\",\n"
        "}";

    /* Parse */
    xcdn_error_t err;
    xcdn_document_t *doc = xcdn_parse(input, &err);
    if (!doc) {
        fprintf(stderr, "Parse error: %s at line %zu, col %zu\n",
                err.message, err.span.line, err.span.column);
        return 1;
    }

    /* Pretty-print */
    char *text = xcdn_to_string_pretty(doc);
    if (text) {
        printf("=== Pretty ===\n%s\n", text);
        free(text);
    }

    /* Compact */
    char *compact = xcdn_to_string_compact(doc);
    if (compact) {
        printf("\n=== Compact ===\n%s\n", compact);
        free(compact);
    }

    xcdn_document_free(doc);
    return 0;
}
