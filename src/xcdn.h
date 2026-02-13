/*
 * xCDN - eXtensible Cognitive Data Notation (C implementation)
 *
 * xcdn is a streaming-capable parser/serializer for
 * xCDN - eXtensible Cognitive Data Notation.
 *
 * The library exposes three public layers:
 *
 * - lexer:  tokenizes an xCDN document while tracking line/column.
 * - parser: produces a typed AST (xcdn_document_t) including optional prolog
 *           directives.
 * - ser:    pretty/compact serialization with strong typing (Decimal, UUID,
 *           DateTime, Duration, Bytes).
 *
 * Quick Start:
 *
 *   #include "xcdn.h"
 *
 *   const char *input =
 *       "$schema: \"https://gslf.github.io/xCDN/schemas/v1/meta.xcdn\",\n"
 *       "server_config: {\n"
 *       "  host: \"localhost\",\n"
 *       "  ports: [8080, 9090,],\n"
 *       "  timeout: r\"PT30S\",\n"
 *       "  max_cost: d\"19.99\",\n"
 *       "  admin: #user { id: u\"550e8400-e29b-41d4-a716-446655440000\","
 *       " role: \"superuser\" },\n"
 *       "  icon: @mime(\"image/png\") b\"aGVsbG8=\",\n"
 *       "}";
 *
 *   xcdn_error_t err;
 *   xcdn_document_t *doc = xcdn_parse(input, &err);
 *   if (!doc) {
 *       fprintf(stderr, "parse error: %s\n", err.message);
 *       return 1;
 *   }
 *
 *   char *text = xcdn_to_string_pretty(doc);
 *   printf("%s\n", text);
 *   free(text);
 *   xcdn_document_free(doc);
 *
 * MIT License
 */

#ifndef XCDN_H
#define XCDN_H

#include "error.h"
#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include "ser.h"

#define XCDN_VERSION "0.1.0"

#endif /* XCDN_H */
