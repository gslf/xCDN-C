/*
 * xCDN - eXtensible Cognitive Data Notation (C implementation)
 * Tokenizer for xCDN.
 *
 * The lexer is designed for speed and clear error reporting:
 * - Ignores whitespace and comments
 * - Tracks line/column per token
 * - Recognizes typed string literals: d"...", b"...", u"...", t"...", r"..."
 * - Supports double-quoted strings and triple-quoted multi-line strings
 *
 * MIT License
 */

#ifndef XCDN_LEXER_H
#define XCDN_LEXER_H

#include "error.h"
#include <stddef.h>
#include <stdint.h>

/* Token types. */
typedef enum {
    XCDN_TOK_LBRACE = 0,
    XCDN_TOK_RBRACE,
    XCDN_TOK_LBRACKET,
    XCDN_TOK_RBRACKET,
    XCDN_TOK_LPAREN,
    XCDN_TOK_RPAREN,
    XCDN_TOK_COLON,
    XCDN_TOK_COMMA,
    XCDN_TOK_DOLLAR,
    XCDN_TOK_HASH,
    XCDN_TOK_AT,
    XCDN_TOK_TRUE,
    XCDN_TOK_FALSE,
    XCDN_TOK_NULL,
    XCDN_TOK_IDENT,
    XCDN_TOK_INT,
    XCDN_TOK_FLOAT,
    XCDN_TOK_STRING,
    XCDN_TOK_TRIPLE_STRING,
    XCDN_TOK_D_QUOTED,   /* decimal d"..." */
    XCDN_TOK_B_QUOTED,   /* bytes   b"..." */
    XCDN_TOK_U_QUOTED,   /* uuid    u"..." */
    XCDN_TOK_T_QUOTED,   /* datetime t"..." */
    XCDN_TOK_R_QUOTED,   /* duration r"..." */
    XCDN_TOK_EOF,
} xcdn_token_type_t;

/* A token with its type, value, and source position. */
typedef struct {
    xcdn_token_type_t type;
    xcdn_span_t       span;
    /* Value data: depends on token type */
    union {
        int64_t int_val;
        double  float_val;
        struct {
            char  *str;
            size_t len;
        } string_val;
    } data;
} xcdn_token_t;

/* Lexer state. */
typedef struct {
    const char *src;
    size_t      src_len;
    size_t      idx;
    size_t      line;
    size_t      col;
} xcdn_lexer_t;

/* Initialize a lexer for the given source string. */
void xcdn_lexer_init(xcdn_lexer_t *lex, const char *src, size_t src_len);

/* Read and return the next token. Returns an error if invalid. */
xcdn_token_t xcdn_lexer_next(xcdn_lexer_t *lex, xcdn_error_t *err);

/* Free any heap data owned by a token (string values). */
void xcdn_token_free(xcdn_token_t *tok);

/* Get a human-readable name for a token type. */
const char *xcdn_token_type_str(xcdn_token_type_t type);

#endif /* XCDN_LEXER_H */
