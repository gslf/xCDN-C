/*
 * xCDN - eXtensible Cognitive Data Notation (C implementation)
 * Error types with spans and pretty display.
 *
 * MIT License
 */

#include "error.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

xcdn_span_t xcdn_span_start(void) {
    xcdn_span_t s = {0, 1, 1};
    return s;
}

xcdn_span_t xcdn_span_new(size_t offset, size_t line, size_t column) {
    xcdn_span_t s = {offset, line, column};
    return s;
}

xcdn_error_t xcdn_error_none(void) {
    xcdn_error_t e;
    memset(&e, 0, sizeof(e));
    e.kind = XCDN_ERR_NONE;
    return e;
}

xcdn_error_t xcdn_error_new(xcdn_error_kind_t kind, xcdn_span_t span,
                            const char *fmt, ...) {
    xcdn_error_t e;
    memset(&e, 0, sizeof(e));
    e.kind = kind;
    e.span = span;
    if (fmt) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(e.message, sizeof(e.message), fmt, ap);
        va_end(ap);
    }
    return e;
}

int xcdn_error_is_set(const xcdn_error_t *err) {
    return err && err->kind != XCDN_ERR_NONE;
}

const char *xcdn_error_kind_str(xcdn_error_kind_t kind) {
    switch (kind) {
        case XCDN_ERR_NONE:             return "no error";
        case XCDN_ERR_EOF:              return "unexpected end of input";
        case XCDN_ERR_INVALID_TOKEN:    return "invalid token";
        case XCDN_ERR_EXPECTED:         return "unexpected token";
        case XCDN_ERR_INVALID_ESCAPE:   return "invalid escape sequence";
        case XCDN_ERR_INVALID_NUMBER:   return "invalid number literal";
        case XCDN_ERR_INVALID_DECIMAL:  return "invalid decimal literal";
        case XCDN_ERR_INVALID_DATETIME: return "invalid RFC3339 datetime";
        case XCDN_ERR_INVALID_DURATION: return "invalid ISO8601 duration";
        case XCDN_ERR_INVALID_UUID:     return "invalid UUID";
        case XCDN_ERR_INVALID_BASE64:   return "invalid base64 encoding";
        case XCDN_ERR_MESSAGE:          return "error";
        case XCDN_ERR_OUT_OF_MEMORY:    return "out of memory";
        default:                        return "unknown error";
    }
}
