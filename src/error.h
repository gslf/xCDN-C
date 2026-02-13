/*
 * xCDN - eXtensible Cognitive Data Notation (C implementation)
 * Error types with spans and pretty display.
 *
 * MIT License
 */

#ifndef XCDN_ERROR_H
#define XCDN_ERROR_H

#include <stddef.h>

/* Location information for diagnostics. */
typedef struct {
    size_t offset;
    size_t line;
    size_t column;
} xcdn_span_t;

/* Error kinds. */
typedef enum {
    XCDN_ERR_NONE = 0,
    XCDN_ERR_EOF,
    XCDN_ERR_INVALID_TOKEN,
    XCDN_ERR_EXPECTED,
    XCDN_ERR_INVALID_ESCAPE,
    XCDN_ERR_INVALID_NUMBER,
    XCDN_ERR_INVALID_DECIMAL,
    XCDN_ERR_INVALID_DATETIME,
    XCDN_ERR_INVALID_DURATION,
    XCDN_ERR_INVALID_UUID,
    XCDN_ERR_INVALID_BASE64,
    XCDN_ERR_MESSAGE,
    XCDN_ERR_OUT_OF_MEMORY,
} xcdn_error_kind_t;

/* Full error with position. */
typedef struct {
    xcdn_error_kind_t kind;
    xcdn_span_t       span;
    char              message[256];
} xcdn_error_t;

/* Construct a starting span (line 1, col 1). */
xcdn_span_t xcdn_span_start(void);

/* Construct a span with explicit values. */
xcdn_span_t xcdn_span_new(size_t offset, size_t line, size_t column);

/* Construct a "no error" error. */
xcdn_error_t xcdn_error_none(void);

/* Construct an error with kind, span, and printf-style message. */
xcdn_error_t xcdn_error_new(xcdn_error_kind_t kind, xcdn_span_t span,
                            const char *fmt, ...);

/* Check if an error is set. */
int xcdn_error_is_set(const xcdn_error_t *err);

/* Get a human-readable string for an error kind. */
const char *xcdn_error_kind_str(xcdn_error_kind_t kind);

#endif /* XCDN_ERROR_H */
