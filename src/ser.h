/*
 * xCDN - eXtensible Cognitive Data Notation (C implementation)
 * Serializer for xCDN.
 *
 * Provides pretty and compact string encoders.
 *
 * MIT License
 */

#ifndef XCDN_SER_H
#define XCDN_SER_H

#include "ast.h"
#include <stddef.h>
#include <stdbool.h>

/* Formatting options for serialization. */
typedef struct {
    bool   pretty;          /* Pretty-print with indentation and newlines. */
    int    indent;          /* Indentation width (spaces). Default: 2. */
    bool   trailing_commas; /* Emit trailing commas where allowed. Default: true. */
} xcdn_format_t;

/* Returns the default format (pretty=true, indent=2, trailing_commas=true). */
xcdn_format_t xcdn_format_default(void);

/* Returns a compact format (no whitespace, no trailing commas). */
xcdn_format_t xcdn_format_compact(void);

/*
 * Serialize a Document to a heap-allocated string using the default format.
 * Caller must free() the returned string.
 * Returns NULL on error.
 */
char *xcdn_to_string_pretty(const xcdn_document_t *doc);

/*
 * Serialize a Document to a compact string.
 * Caller must free() the returned string.
 * Returns NULL on error.
 */
char *xcdn_to_string_compact(const xcdn_document_t *doc);

/*
 * Serialize a Document with custom formatting options.
 * Caller must free() the returned string.
 * Returns NULL on error.
 */
char *xcdn_to_string_with_format(const xcdn_document_t *doc,
                                 xcdn_format_t fmt);

#endif /* XCDN_SER_H */
