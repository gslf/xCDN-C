/*
 * xCDN - eXtensible Cognitive Data Notation (C implementation)
 * Recursive-descent parser for xCDN.
 *
 * It implements the published grammar shape:
 * - Optional prolog: $ident : value entries separated by commas
 * - One or more values (streaming)
 * - Values: object, array, string/number/bool/null, and typed literals
 * - Decorations: zero or more @annotation(args?) and #tag preceding any value
 *
 * MIT License
 */

#ifndef XCDN_PARSER_H
#define XCDN_PARSER_H

#include "ast.h"
#include "error.h"
#include <stddef.h>

/*
 * Parse a full xCDN document from a string.
 * Returns a heap-allocated Document, or NULL on error.
 * On error, *err is populated with details.
 */
xcdn_document_t *xcdn_parse_str(const char *src, size_t src_len,
                                xcdn_error_t *err);

/*
 * Parse a full xCDN document from a NUL-terminated string.
 * Convenience wrapper over xcdn_parse_str.
 */
xcdn_document_t *xcdn_parse(const char *src, xcdn_error_t *err);

#endif /* XCDN_PARSER_H */
