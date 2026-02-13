/*
 * xCDN - eXtensible Cognitive Data Notation (C implementation)
 * Tokenizer for xCDN.
 *
 * MIT License
 */

#include "lexer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>

/* ── Internal helpers ─────────────────────────────────────────────────── */

static int is_ident_start(unsigned char b) {
    return (b >= 'A' && b <= 'Z') || (b >= 'a' && b <= 'z') || b == '_';
}

static int is_ident_part(unsigned char b) {
    return is_ident_start(b) || (b >= '0' && b <= '9') || b == '-';
}

static int lex_peek(const xcdn_lexer_t *lex) {
    if (lex->idx >= lex->src_len) return -1;
    return (unsigned char)lex->src[lex->idx];
}

static int lex_peek_at(const xcdn_lexer_t *lex, size_t offset) {
    size_t pos = lex->idx + offset;
    if (pos >= lex->src_len) return -1;
    return (unsigned char)lex->src[pos];
}

static int lex_bump(xcdn_lexer_t *lex) {
    if (lex->idx >= lex->src_len) return -1;
    unsigned char b = (unsigned char)lex->src[lex->idx];
    lex->idx++;
    if (b == '\n') {
        lex->line++;
        lex->col = 1;
    } else {
        lex->col++;
    }
    return b;
}

static xcdn_span_t lex_span(const xcdn_lexer_t *lex) {
    return xcdn_span_new(lex->idx, lex->line, lex->col);
}

static void skip_ws_and_comments(xcdn_lexer_t *lex) {
    for (;;) {
        /* Skip whitespace */
        while (lex->idx < lex->src_len) {
            unsigned char b = (unsigned char)lex->src[lex->idx];
            if (b == ' ' || b == '\t' || b == '\r' || b == '\n') {
                lex_bump(lex);
            } else {
                break;
            }
        }

        if (lex->idx >= lex->src_len) return;

        unsigned char b = (unsigned char)lex->src[lex->idx];
        if (b == '/' && lex->idx + 1 < lex->src_len) {
            unsigned char b2 = (unsigned char)lex->src[lex->idx + 1];
            if (b2 == '/') {
                /* Line comment */
                lex_bump(lex);
                lex_bump(lex);
                while (lex->idx < lex->src_len) {
                    int c = lex_bump(lex);
                    if (c == '\n') break;
                }
                continue;
            } else if (b2 == '*') {
                /* Block comment */
                lex_bump(lex);
                lex_bump(lex);
                while (lex->idx < lex->src_len) {
                    int c = lex_bump(lex);
                    if (c == '*' && lex_peek(lex) == '/') {
                        lex_bump(lex);
                        break;
                    }
                }
                continue;
            }
        }
        break;
    }
}

/* ── String builder ───────────────────────────────────────────────────── */

typedef struct {
    char  *buf;
    size_t len;
    size_t cap;
} strbuf_t;

static void strbuf_init(strbuf_t *sb) {
    sb->buf = NULL;
    sb->len = 0;
    sb->cap = 0;
}

static void strbuf_push(strbuf_t *sb, char c) {
    if (sb->len >= sb->cap) {
        size_t new_cap = (sb->cap == 0) ? 32 : sb->cap * 2;
        char *new_buf = (char *)realloc(sb->buf, new_cap);
        if (!new_buf) return;
        sb->buf = new_buf;
        sb->cap = new_cap;
    }
    sb->buf[sb->len++] = c;
}

static char *strbuf_finish(strbuf_t *sb, size_t *out_len) {
    strbuf_push(sb, '\0');
    if (out_len) *out_len = sb->len - 1; /* exclude NUL */
    return sb->buf;
}

static void strbuf_free(strbuf_t *sb) {
    free(sb->buf);
    sb->buf = NULL;
    sb->len = 0;
    sb->cap = 0;
}

/* ── Read string (normal or triple-quoted) ────────────────────────────── */

static char *read_string(xcdn_lexer_t *lex, int triple, size_t *out_len,
                         xcdn_error_t *err) {
    strbuf_t sb;
    strbuf_init(&sb);
    xcdn_span_t start_span = lex_span(lex);

    if (triple) {
        /* Consume the three opening quotes """ */
        lex_bump(lex);
        lex_bump(lex);
        lex_bump(lex);
        for (;;) {
            if (lex_peek(lex) == '"' && lex_peek_at(lex, 1) == '"' &&
                lex_peek_at(lex, 2) == '"') {
                lex_bump(lex);
                lex_bump(lex);
                lex_bump(lex);
                break;
            }
            int b = lex_bump(lex);
            if (b < 0) {
                strbuf_free(&sb);
                *err = xcdn_error_new(XCDN_ERR_EOF, start_span,
                                      "unterminated triple-quoted string");
                return NULL;
            }
            strbuf_push(&sb, (char)b);
        }
    } else {
        /* Consume opening quote */
        int q = lex_bump(lex);
        if (q != '"') {
            strbuf_free(&sb);
            *err = xcdn_error_new(XCDN_ERR_EXPECTED, start_span,
                                  "expected '\"', found '%c'", (char)q);
            return NULL;
        }
        for (;;) {
            int b = lex_bump(lex);
            if (b < 0) {
                strbuf_free(&sb);
                *err = xcdn_error_new(XCDN_ERR_EOF, start_span,
                                      "unterminated string");
                return NULL;
            }
            if (b == '"') break;
            if (b == '\\') {
                int e = lex_bump(lex);
                if (e < 0) {
                    strbuf_free(&sb);
                    *err = xcdn_error_new(XCDN_ERR_INVALID_ESCAPE, start_span,
                                          "incomplete escape at end of input");
                    return NULL;
                }
                switch (e) {
                    case '"':  strbuf_push(&sb, '"'); break;
                    case '\\': strbuf_push(&sb, '\\'); break;
                    case '/':  strbuf_push(&sb, '\\'); strbuf_push(&sb, '/'); break;
                    case 'b':  strbuf_push(&sb, '\\'); strbuf_push(&sb, 'b'); break;
                    case 'f':  strbuf_push(&sb, '\\'); strbuf_push(&sb, 'f'); break;
                    case 'n':  strbuf_push(&sb, '\\'); strbuf_push(&sb, 'n'); break;
                    case 'r':  strbuf_push(&sb, '\\'); strbuf_push(&sb, 'r'); break;
                    case 't':  strbuf_push(&sb, '\\'); strbuf_push(&sb, 't'); break;
                    case 'u': {
                        strbuf_push(&sb, '\\');
                        strbuf_push(&sb, 'u');
                        for (int i = 0; i < 4; i++) {
                            int h = lex_bump(lex);
                            if (h < 0 || !isxdigit((unsigned char)h)) {
                                strbuf_free(&sb);
                                *err = xcdn_error_new(XCDN_ERR_INVALID_ESCAPE,
                                    start_span, "invalid \\uXXXX escape");
                                return NULL;
                            }
                            strbuf_push(&sb, (char)h);
                        }
                        break;
                    }
                    default:
                        strbuf_free(&sb);
                        *err = xcdn_error_new(XCDN_ERR_INVALID_ESCAPE,
                            start_span, "unknown escape '\\%c'", (char)e);
                        return NULL;
                }
            } else {
                strbuf_push(&sb, (char)b);
            }
        }
    }

    return strbuf_finish(&sb, out_len);
}

/* ── Read identifier ──────────────────────────────────────────────────── */

static char *read_ident(xcdn_lexer_t *lex, size_t *out_len) {
    size_t start = lex->idx;
    lex_bump(lex);
    while (lex->idx < lex->src_len && is_ident_part((unsigned char)lex->src[lex->idx])) {
        lex_bump(lex);
    }
    size_t len = lex->idx - start;
    char *s = (char *)malloc(len + 1);
    if (s) {
        memcpy(s, lex->src + start, len);
        s[len] = '\0';
    }
    if (out_len) *out_len = len;
    return s;
}

/* ── Read number ──────────────────────────────────────────────────────── */

static void read_number(xcdn_lexer_t *lex, int64_t *out_int, double *out_float,
                        int *is_float_out, xcdn_error_t *err) {
    size_t start = lex->idx;
    int has_dot = 0, has_exp = 0, has_digit = 0;

    /* Optional sign */
    int p = lex_peek(lex);
    if (p == '+' || p == '-') lex_bump(lex);

    for (;;) {
        p = lex_peek(lex);
        if (p < 0) break;
        if (p >= '0' && p <= '9') {
            has_digit = 1;
            lex_bump(lex);
        } else if (p == '.' && !has_dot && !has_exp) {
            has_dot = 1;
            lex_bump(lex);
        } else if ((p == 'e' || p == 'E') && !has_exp) {
            has_exp = 1;
            lex_bump(lex);
            int sign = lex_peek(lex);
            if (sign == '+' || sign == '-') lex_bump(lex);
        } else {
            break;
        }
    }

    if (!has_digit) {
        *err = xcdn_error_new(XCDN_ERR_INVALID_NUMBER, lex_span(lex),
                              "no digits in number");
        return;
    }

    /* Extract the substring */
    size_t len = lex->idx - start;
    char tmp[128];
    if (len >= sizeof(tmp)) len = sizeof(tmp) - 1;
    memcpy(tmp, lex->src + start, len);
    tmp[len] = '\0';

    *is_float_out = has_dot || has_exp;
    if (*is_float_out) {
        char *endp = NULL;
        errno = 0;
        *out_float = strtod(tmp, &endp);
        if (errno == ERANGE || endp == tmp) {
            *err = xcdn_error_new(XCDN_ERR_INVALID_NUMBER, lex_span(lex),
                                  "invalid float: %s", tmp);
        }
    } else {
        char *endp = NULL;
        errno = 0;
        long long v = strtoll(tmp, &endp, 10);
        if (errno == ERANGE || endp == tmp) {
            *err = xcdn_error_new(XCDN_ERR_INVALID_NUMBER, lex_span(lex),
                                  "invalid integer: %s", tmp);
        }
        *out_int = (int64_t)v;
    }
}

/* ── Public API ───────────────────────────────────────────────────────── */

void xcdn_lexer_init(xcdn_lexer_t *lex, const char *src, size_t src_len) {
    lex->src = src;
    lex->src_len = src_len;
    lex->idx = 0;
    lex->line = 1;
    lex->col = 1;
}

xcdn_token_t xcdn_lexer_next(xcdn_lexer_t *lex, xcdn_error_t *err) {
    xcdn_token_t tok;
    memset(&tok, 0, sizeof(tok));
    *err = xcdn_error_none();

    skip_ws_and_comments(lex);
    xcdn_span_t start = lex_span(lex);

    int b = lex_peek(lex);
    if (b < 0) {
        tok.type = XCDN_TOK_EOF;
        tok.span = start;
        return tok;
    }

    /* Triple-quoted string """...""" */
    if (b == '"' && lex_peek_at(lex, 1) == '"' && lex_peek_at(lex, 2) == '"') {
        size_t slen = 0;
        char *s = read_string(lex, 1, &slen, err);
        if (xcdn_error_is_set(err)) {
            tok.type = XCDN_TOK_EOF;
            return tok;
        }
        tok.type = XCDN_TOK_TRIPLE_STRING;
        tok.span = start;
        tok.data.string_val.str = s;
        tok.data.string_val.len = slen;
        return tok;
    }

    /* Single-char tokens */
    switch (b) {
        case '{': lex_bump(lex); tok.type = XCDN_TOK_LBRACE;   tok.span = start; return tok;
        case '}': lex_bump(lex); tok.type = XCDN_TOK_RBRACE;   tok.span = start; return tok;
        case '[': lex_bump(lex); tok.type = XCDN_TOK_LBRACKET; tok.span = start; return tok;
        case ']': lex_bump(lex); tok.type = XCDN_TOK_RBRACKET; tok.span = start; return tok;
        case '(': lex_bump(lex); tok.type = XCDN_TOK_LPAREN;   tok.span = start; return tok;
        case ')': lex_bump(lex); tok.type = XCDN_TOK_RPAREN;   tok.span = start; return tok;
        case ':': lex_bump(lex); tok.type = XCDN_TOK_COLON;    tok.span = start; return tok;
        case ',': lex_bump(lex); tok.type = XCDN_TOK_COMMA;    tok.span = start; return tok;
        case '$': lex_bump(lex); tok.type = XCDN_TOK_DOLLAR;   tok.span = start; return tok;
        case '#': lex_bump(lex); tok.type = XCDN_TOK_HASH;     tok.span = start; return tok;
        case '@': lex_bump(lex); tok.type = XCDN_TOK_AT;       tok.span = start; return tok;
        default: break;
    }

    /* Quoted string */
    if (b == '"') {
        size_t slen = 0;
        char *s = read_string(lex, 0, &slen, err);
        if (xcdn_error_is_set(err)) {
            tok.type = XCDN_TOK_EOF;
            return tok;
        }
        tok.type = XCDN_TOK_STRING;
        tok.span = start;
        tok.data.string_val.str = s;
        tok.data.string_val.len = slen;
        return tok;
    }

    /* Numbers */
    if (b == '.' || b == '-' || b == '+' || (b >= '0' && b <= '9')) {
        int64_t iv = 0;
        double fv = 0.0;
        int is_float = 0;
        read_number(lex, &iv, &fv, &is_float, err);
        if (xcdn_error_is_set(err)) {
            tok.type = XCDN_TOK_EOF;
            return tok;
        }
        tok.span = start;
        if (is_float) {
            tok.type = XCDN_TOK_FLOAT;
            tok.data.float_val = fv;
        } else {
            tok.type = XCDN_TOK_INT;
            tok.data.int_val = iv;
        }
        return tok;
    }

    /* Typed strings: d"...", b"...", u"...", t"...", r"..." */
    if ((b == 'd' || b == 'b' || b == 'u' || b == 't' || b == 'r') &&
        lex_peek_at(lex, 1) == '"') {
        int ch = b;
        lex_bump(lex); /* consume type char */
        size_t slen = 0;
        char *s = read_string(lex, 0, &slen, err);
        if (xcdn_error_is_set(err)) {
            tok.type = XCDN_TOK_EOF;
            return tok;
        }
        tok.span = start;
        tok.data.string_val.str = s;
        tok.data.string_val.len = slen;
        switch (ch) {
            case 'd': tok.type = XCDN_TOK_D_QUOTED; break;
            case 'b': tok.type = XCDN_TOK_B_QUOTED; break;
            case 'u': tok.type = XCDN_TOK_U_QUOTED; break;
            case 't': tok.type = XCDN_TOK_T_QUOTED; break;
            case 'r': tok.type = XCDN_TOK_R_QUOTED; break;
            default: break;
        }
        return tok;
    }

    /* Identifiers and keywords */
    if (is_ident_start((unsigned char)b)) {
        size_t slen = 0;
        char *s = read_ident(lex, &slen);
        tok.span = start;
        if (strcmp(s, "true") == 0) {
            tok.type = XCDN_TOK_TRUE;
            free(s);
        } else if (strcmp(s, "false") == 0) {
            tok.type = XCDN_TOK_FALSE;
            free(s);
        } else if (strcmp(s, "null") == 0) {
            tok.type = XCDN_TOK_NULL;
            free(s);
        } else {
            tok.type = XCDN_TOK_IDENT;
            tok.data.string_val.str = s;
            tok.data.string_val.len = slen;
        }
        return tok;
    }

    /* Unknown token */
    *err = xcdn_error_new(XCDN_ERR_INVALID_TOKEN, start,
                          "unexpected character '%c' (0x%02x)", (char)b, b);
    tok.type = XCDN_TOK_EOF;
    return tok;
}

void xcdn_token_free(xcdn_token_t *tok) {
    if (!tok) return;
    switch (tok->type) {
        case XCDN_TOK_IDENT:
        case XCDN_TOK_STRING:
        case XCDN_TOK_TRIPLE_STRING:
        case XCDN_TOK_D_QUOTED:
        case XCDN_TOK_B_QUOTED:
        case XCDN_TOK_U_QUOTED:
        case XCDN_TOK_T_QUOTED:
        case XCDN_TOK_R_QUOTED:
            free(tok->data.string_val.str);
            tok->data.string_val.str = NULL;
            break;
        default:
            break;
    }
}

const char *xcdn_token_type_str(xcdn_token_type_t type) {
    switch (type) {
        case XCDN_TOK_LBRACE:       return "{";
        case XCDN_TOK_RBRACE:       return "}";
        case XCDN_TOK_LBRACKET:     return "[";
        case XCDN_TOK_RBRACKET:     return "]";
        case XCDN_TOK_LPAREN:       return "(";
        case XCDN_TOK_RPAREN:       return ")";
        case XCDN_TOK_COLON:        return ":";
        case XCDN_TOK_COMMA:        return ",";
        case XCDN_TOK_DOLLAR:       return "$";
        case XCDN_TOK_HASH:         return "#";
        case XCDN_TOK_AT:           return "@";
        case XCDN_TOK_TRUE:         return "true";
        case XCDN_TOK_FALSE:        return "false";
        case XCDN_TOK_NULL:         return "null";
        case XCDN_TOK_IDENT:        return "identifier";
        case XCDN_TOK_INT:          return "integer";
        case XCDN_TOK_FLOAT:        return "float";
        case XCDN_TOK_STRING:       return "string";
        case XCDN_TOK_TRIPLE_STRING:return "\"\"\"string\"\"\"";
        case XCDN_TOK_D_QUOTED:     return "d\"...\"";
        case XCDN_TOK_B_QUOTED:     return "b\"...\"";
        case XCDN_TOK_U_QUOTED:     return "u\"...\"";
        case XCDN_TOK_T_QUOTED:     return "t\"...\"";
        case XCDN_TOK_R_QUOTED:     return "r\"...\"";
        case XCDN_TOK_EOF:          return "EOF";
        default:                    return "unknown";
    }
}
