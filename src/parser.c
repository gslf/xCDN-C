/*
 * xCDN - eXtensible Cognitive Data Notation (C implementation)
 * Recursive-descent parser for xCDN.
 *
 * MIT License
 */

#include "parser.h"
#include "lexer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ── Base64 decoder ───────────────────────────────────────────────────── */

static const unsigned char b64_table[256] = {
    ['A']=0,['B']=1,['C']=2,['D']=3,['E']=4,['F']=5,['G']=6,['H']=7,
    ['I']=8,['J']=9,['K']=10,['L']=11,['M']=12,['N']=13,['O']=14,['P']=15,
    ['Q']=16,['R']=17,['S']=18,['T']=19,['U']=20,['V']=21,['W']=22,['X']=23,
    ['Y']=24,['Z']=25,
    ['a']=26,['b']=27,['c']=28,['d']=29,['e']=30,['f']=31,['g']=32,['h']=33,
    ['i']=34,['j']=35,['k']=36,['l']=37,['m']=38,['n']=39,['o']=40,['p']=41,
    ['q']=42,['r']=43,['s']=44,['t']=45,['u']=46,['v']=47,['w']=48,['x']=49,
    ['y']=50,['z']=51,
    ['0']=52,['1']=53,['2']=54,['3']=55,['4']=56,['5']=57,['6']=58,['7']=59,
    ['8']=60,['9']=61,
    ['+']=62,['/']=63,
    ['-']=62,['_']=63, /* URL-safe variants */
};

static int is_b64_char(unsigned char c) {
    return isalnum(c) || c == '+' || c == '/' || c == '-' || c == '_' || c == '=';
}

static uint8_t *decode_base64(const char *input, size_t in_len, size_t *out_len) {
    /* Strip padding */
    while (in_len > 0 && input[in_len - 1] == '=') in_len--;

    size_t max_out = (in_len * 3) / 4 + 3;
    uint8_t *out = (uint8_t *)malloc(max_out);
    if (!out) return NULL;

    size_t o = 0;
    uint32_t accum = 0;
    int bits = 0;

    for (size_t i = 0; i < in_len; i++) {
        unsigned char c = (unsigned char)input[i];
        if (c == '=' || c == ' ' || c == '\n' || c == '\r') continue;
        if (!is_b64_char(c)) {
            free(out);
            return NULL;
        }
        accum = (accum << 6) | b64_table[c];
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out[o++] = (uint8_t)((accum >> bits) & 0xFF);
        }
    }

    *out_len = o;
    return out;
}

/* ── UUID validation ──────────────────────────────────────────────────── */

static int validate_uuid(const char *s) {
    /* Expected format: 8-4-4-4-12 hex chars with dashes */
    size_t len = strlen(s);
    if (len != 36) return 0;
    static const int dash_pos[] = {8, 13, 18, 23};
    for (int i = 0; i < 4; i++) {
        if (s[dash_pos[i]] != '-') return 0;
    }
    for (size_t i = 0; i < len; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) continue;
        if (!isxdigit((unsigned char)s[i])) return 0;
    }
    return 1;
}

/* ── Helper to get current span from lexer ────────────────────────────── */

static xcdn_span_t parser_span(const xcdn_lexer_t *lex) {
    return xcdn_span_new(lex->idx, lex->line, lex->col);
}

/* ── Parser state ─────────────────────────────────────────────────────── */

typedef struct {
    xcdn_lexer_t  lex;
    xcdn_token_t  look;
    int           has_look;
    xcdn_error_t  err;
} parser_t;

static void parser_init(parser_t *p, const char *src, size_t src_len) {
    xcdn_lexer_init(&p->lex, src, src_len);
    memset(&p->look, 0, sizeof(p->look));
    p->has_look = 0;
    p->err = xcdn_error_none();
}

static xcdn_token_t parser_bump(parser_t *p) {
    if (p->has_look) {
        xcdn_token_t t = p->look;
        p->has_look = 0;
        memset(&p->look, 0, sizeof(p->look));
        return t;
    }
    return xcdn_lexer_next(&p->lex, &p->err);
}

static xcdn_token_type_t parser_peek_type(parser_t *p) {
    if (!p->has_look) {
        p->look = xcdn_lexer_next(&p->lex, &p->err);
        p->has_look = 1;
    }
    return p->look.type;
}

static xcdn_token_t parser_expect(parser_t *p, xcdn_token_type_t kind,
                                  const char *expected) {
    xcdn_token_t t = parser_bump(p);
    if (t.type != kind) {
        p->err = xcdn_error_new(XCDN_ERR_EXPECTED, t.span,
                                "expected %s, found %s", expected,
                                xcdn_token_type_str(t.type));
        xcdn_token_free(&t);
        xcdn_token_t empty;
        memset(&empty, 0, sizeof(empty));
        return empty;
    }
    return t;
}

/* ── Forward declarations ─────────────────────────────────────────────── */

static xcdn_node_t  *parse_node(parser_t *p);
static xcdn_value_t *parse_value(parser_t *p);
static xcdn_value_t *parse_object(parser_t *p);
static xcdn_value_t *parse_array(parser_t *p);

/* ── Parse helpers ────────────────────────────────────────────────────── */

static char *parse_ident_string(parser_t *p) {
    xcdn_token_t t = parser_bump(p);
    if (t.type == XCDN_TOK_IDENT) {
        return t.data.string_val.str; /* Caller owns the string */
    }
    p->err = xcdn_error_new(XCDN_ERR_EXPECTED, t.span,
                            "expected identifier, found %s",
                            xcdn_token_type_str(t.type));
    xcdn_token_free(&t);
    return NULL;
}

static char *parse_key(parser_t *p) {
    xcdn_token_t t = parser_bump(p);
    if (t.type == XCDN_TOK_IDENT || t.type == XCDN_TOK_STRING) {
        return t.data.string_val.str; /* Caller owns the string */
    }
    p->err = xcdn_error_new(XCDN_ERR_EXPECTED, t.span,
                            "expected object key, found %s",
                            xcdn_token_type_str(t.type));
    xcdn_token_free(&t);
    return NULL;
}

/* ── Parse value ──────────────────────────────────────────────────────── */

static xcdn_value_t *parse_value(parser_t *p) {
    xcdn_token_t t = parser_bump(p);
    if (xcdn_error_is_set(&p->err)) {
        xcdn_token_free(&t);
        return NULL;
    }

    xcdn_value_t *val = NULL;

    switch (t.type) {
        case XCDN_TOK_LBRACE:
            val = parse_object(p);
            break;

        case XCDN_TOK_LBRACKET:
            val = parse_array(p);
            break;

        case XCDN_TOK_STRING:
        case XCDN_TOK_TRIPLE_STRING:
            val = xcdn_value_string_owned(t.data.string_val.str);
            t.data.string_val.str = NULL; /* ownership transferred */
            break;

        case XCDN_TOK_TRUE:
            val = xcdn_value_bool(true);
            break;

        case XCDN_TOK_FALSE:
            val = xcdn_value_bool(false);
            break;

        case XCDN_TOK_NULL:
            val = xcdn_value_null();
            break;

        case XCDN_TOK_INT:
            val = xcdn_value_int(t.data.int_val);
            break;

        case XCDN_TOK_FLOAT:
            val = xcdn_value_float(t.data.float_val);
            break;

        case XCDN_TOK_D_QUOTED:
            /* Store decimal as string; validation is lenient */
            val = xcdn_value_decimal(t.data.string_val.str);
            xcdn_token_free(&t);
            break;

        case XCDN_TOK_B_QUOTED: {
            size_t decoded_len = 0;
            uint8_t *decoded = decode_base64(t.data.string_val.str,
                                             t.data.string_val.len,
                                             &decoded_len);
            if (!decoded) {
                p->err = xcdn_error_new(XCDN_ERR_INVALID_BASE64, t.span,
                                        "invalid base64: %s",
                                        t.data.string_val.str);
                xcdn_token_free(&t);
                return NULL;
            }
            val = xcdn_value_bytes_owned(decoded, decoded_len);
            xcdn_token_free(&t);
            break;
        }

        case XCDN_TOK_U_QUOTED:
            if (!validate_uuid(t.data.string_val.str)) {
                p->err = xcdn_error_new(XCDN_ERR_INVALID_UUID, t.span,
                                        "invalid UUID: %s",
                                        t.data.string_val.str);
                xcdn_token_free(&t);
                return NULL;
            }
            val = xcdn_value_uuid(t.data.string_val.str);
            xcdn_token_free(&t);
            break;

        case XCDN_TOK_T_QUOTED:
            /* Store datetime as string; validation is lenient */
            val = xcdn_value_datetime(t.data.string_val.str);
            xcdn_token_free(&t);
            break;

        case XCDN_TOK_R_QUOTED:
            /* Store duration as string; basic validation */
            val = xcdn_value_duration(t.data.string_val.str);
            xcdn_token_free(&t);
            break;

        default:
            p->err = xcdn_error_new(XCDN_ERR_EXPECTED, t.span,
                                    "expected value, found %s",
                                    xcdn_token_type_str(t.type));
            xcdn_token_free(&t);
            return NULL;
    }

    return val;
}

/* ── Parse object ─────────────────────────────────────────────────────── */

static xcdn_value_t *parse_object(parser_t *p) {
    xcdn_value_t *obj = xcdn_value_object();

    for (;;) {
        if (xcdn_error_is_set(&p->err)) {
            xcdn_value_free(obj);
            return NULL;
        }

        if (parser_peek_type(p) == XCDN_TOK_RBRACE) {
            parser_bump(p);
            break;
        }

        char *key = parse_key(p);
        if (!key || xcdn_error_is_set(&p->err)) {
            free(key);
            xcdn_value_free(obj);
            return NULL;
        }

        parser_expect(p, XCDN_TOK_COLON, ":");
        if (xcdn_error_is_set(&p->err)) {
            free(key);
            xcdn_value_free(obj);
            return NULL;
        }

        xcdn_node_t *node = parse_node(p);
        if (!node || xcdn_error_is_set(&p->err)) {
            free(key);
            xcdn_node_free(node);
            xcdn_value_free(obj);
            return NULL;
        }

        xcdn_object_set(obj, key, node);
        free(key);

        /* Optional comma */
        if (parser_peek_type(p) == XCDN_TOK_COMMA) {
            xcdn_token_t comma = parser_bump(p);
            xcdn_token_free(&comma);
        }
    }

    return obj;
}

/* ── Parse array ──────────────────────────────────────────────────────── */

static xcdn_value_t *parse_array(parser_t *p) {
    xcdn_value_t *arr = xcdn_value_array();

    for (;;) {
        if (xcdn_error_is_set(&p->err)) {
            xcdn_value_free(arr);
            return NULL;
        }

        if (parser_peek_type(p) == XCDN_TOK_RBRACKET) {
            parser_bump(p);
            break;
        }

        xcdn_node_t *node = parse_node(p);
        if (!node || xcdn_error_is_set(&p->err)) {
            xcdn_node_free(node);
            xcdn_value_free(arr);
            return NULL;
        }

        xcdn_array_push(arr, node);

        /* Optional comma */
        if (parser_peek_type(p) == XCDN_TOK_COMMA) {
            xcdn_token_t comma = parser_bump(p);
            xcdn_token_free(&comma);
        }
    }

    return arr;
}

/* ── Parse node (value with decorations) ──────────────────────────────── */

static xcdn_node_t *parse_node(parser_t *p) {
    xcdn_node_t *node = xcdn_node_new(NULL);
    if (!node) {
        p->err = xcdn_error_new(XCDN_ERR_OUT_OF_MEMORY, parser_span(&p->lex),
                                "out of memory");
        return NULL;
    }

    /* Gather decorations: @annotations and #tags */
    for (;;) {
        if (xcdn_error_is_set(&p->err)) {
            xcdn_node_free(node);
            return NULL;
        }

        xcdn_token_type_t pk = parser_peek_type(p);

        if (pk == XCDN_TOK_AT) {
            parser_bump(p); /* consume @ */
            char *name = parse_ident_string(p);
            if (!name || xcdn_error_is_set(&p->err)) {
                free(name);
                xcdn_node_free(node);
                return NULL;
            }

            xcdn_node_add_annotation(node, name);
            xcdn_annotation_t *ann =
                &node->annotations[node->annotations_len - 1];
            free(name);

            /* Optional arg list: (arg1, arg2, ...) */
            if (parser_peek_type(p) == XCDN_TOK_LPAREN) {
                parser_bump(p); /* consume ( */
                if (parser_peek_type(p) == XCDN_TOK_RPAREN) {
                    parser_bump(p); /* consume ) */
                } else {
                    for (;;) {
                        xcdn_value_t *v = parse_value(p);
                        if (!v || xcdn_error_is_set(&p->err)) {
                            xcdn_value_free(v);
                            xcdn_node_free(node);
                            return NULL;
                        }
                        xcdn_annotation_push_arg(ann, v);

                        xcdn_token_type_t next = parser_peek_type(p);
                        if (next == XCDN_TOK_COMMA) {
                            parser_bump(p);
                            continue;
                        } else if (next == XCDN_TOK_RPAREN) {
                            parser_bump(p);
                            break;
                        } else {
                            xcdn_token_t bad = parser_bump(p);
                            p->err = xcdn_error_new(XCDN_ERR_EXPECTED, bad.span,
                                "expected \",\" or \")\", found %s",
                                xcdn_token_type_str(bad.type));
                            xcdn_token_free(&bad);
                            xcdn_node_free(node);
                            return NULL;
                        }
                    }
                }
            }
        } else if (pk == XCDN_TOK_HASH) {
            parser_bump(p); /* consume # */
            char *name = parse_ident_string(p);
            if (!name || xcdn_error_is_set(&p->err)) {
                free(name);
                xcdn_node_free(node);
                return NULL;
            }
            xcdn_node_add_tag(node, name);
            free(name);
        } else {
            break;
        }
    }

    xcdn_value_t *val = parse_value(p);
    if (!val || xcdn_error_is_set(&p->err)) {
        xcdn_value_free(val);
        xcdn_node_free(node);
        return NULL;
    }

    node->value = val;
    return node;
}

/* ── Parse document ───────────────────────────────────────────────────── */

static xcdn_document_t *parse_document(parser_t *p) {
    xcdn_document_t *doc = xcdn_document_new();
    if (!doc) {
        p->err = xcdn_error_new(XCDN_ERR_OUT_OF_MEMORY, parser_span(&p->lex),
                                "out of memory");
        return NULL;
    }

    /* Optional prolog: sequence of $ident : value separated by commas */
    while (parser_peek_type(p) == XCDN_TOK_DOLLAR) {
        if (xcdn_error_is_set(&p->err)) {
            xcdn_document_free(doc);
            return NULL;
        }

        parser_bump(p); /* consume $ */
        char *name = parse_ident_string(p);
        if (!name || xcdn_error_is_set(&p->err)) {
            free(name);
            xcdn_document_free(doc);
            return NULL;
        }

        parser_expect(p, XCDN_TOK_COLON, ":");
        if (xcdn_error_is_set(&p->err)) {
            free(name);
            xcdn_document_free(doc);
            return NULL;
        }

        xcdn_node_t *value_node = parse_node(p);
        if (!value_node || xcdn_error_is_set(&p->err)) {
            free(name);
            xcdn_node_free(value_node);
            xcdn_document_free(doc);
            return NULL;
        }

        xcdn_document_push_directive(doc, name, value_node->value);
        /* Transfer ownership: detach value from node before freeing node shell */
        value_node->value = NULL;
        xcdn_node_free(value_node);
        free(name);

        /* Optional comma */
        if (parser_peek_type(p) == XCDN_TOK_COMMA) {
            xcdn_token_t comma = parser_bump(p);
            xcdn_token_free(&comma);
        }
    }

    /* After prolog: either implicit object, stream, or EOF */
    xcdn_token_type_t pk = parser_peek_type(p);

    if (pk == XCDN_TOK_IDENT || pk == XCDN_TOK_STRING) {
        /*
         * Detect implicit top-level object: starts with key followed by ':'
         * We need to look two tokens ahead. Consume the key, check if ':' follows.
         */
        xcdn_token_t key_tok = parser_bump(p);
        xcdn_token_type_t after_key = parser_peek_type(p);

        if (after_key == XCDN_TOK_COLON) {
            /* Implicit object */
            parser_bump(p); /* consume : */

            xcdn_value_t *obj = xcdn_value_object();
            char *first_key = key_tok.data.string_val.str;
            key_tok.data.string_val.str = NULL;

            xcdn_node_t *first_node = parse_node(p);
            if (!first_node || xcdn_error_is_set(&p->err)) {
                free(first_key);
                xcdn_node_free(first_node);
                xcdn_value_free(obj);
                xcdn_document_free(doc);
                return NULL;
            }
            xcdn_object_set(obj, first_key, first_node);
            free(first_key);

            /* Subsequent entries until EOF */
            for (;;) {
                if (xcdn_error_is_set(&p->err)) {
                    xcdn_value_free(obj);
                    xcdn_document_free(doc);
                    return NULL;
                }

                pk = parser_peek_type(p);
                if (pk == XCDN_TOK_COMMA) {
                    xcdn_token_t comma = parser_bump(p);
                    xcdn_token_free(&comma);
                } else if (pk == XCDN_TOK_IDENT || pk == XCDN_TOK_STRING) {
                    char *key = parse_key(p);
                    if (!key || xcdn_error_is_set(&p->err)) {
                        free(key);
                        xcdn_value_free(obj);
                        xcdn_document_free(doc);
                        return NULL;
                    }
                    parser_expect(p, XCDN_TOK_COLON, ":");
                    if (xcdn_error_is_set(&p->err)) {
                        free(key);
                        xcdn_value_free(obj);
                        xcdn_document_free(doc);
                        return NULL;
                    }
                    xcdn_node_t *n = parse_node(p);
                    if (!n || xcdn_error_is_set(&p->err)) {
                        free(key);
                        xcdn_node_free(n);
                        xcdn_value_free(obj);
                        xcdn_document_free(doc);
                        return NULL;
                    }
                    xcdn_object_set(obj, key, n);
                    free(key);
                } else if (pk == XCDN_TOK_EOF) {
                    break;
                } else {
                    xcdn_token_t bad = parser_bump(p);
                    p->err = xcdn_error_new(XCDN_ERR_EXPECTED, bad.span,
                        "expected object key, found %s",
                        xcdn_token_type_str(bad.type));
                    xcdn_token_free(&bad);
                    xcdn_value_free(obj);
                    xcdn_document_free(doc);
                    return NULL;
                }
            }

            xcdn_node_t *obj_node = xcdn_node_new(obj);
            xcdn_document_push_value(doc, obj_node);
        } else {
            /*
             * Not an implicit object — the first token was a value start
             * (e.g., an identifier used as a boolean/null, or a string value
             * that happens to be the first element of a stream).
             * Put the key token back by constructing a node from it.
             *
             * Actually, identifiers that aren't keywords can't be standalone
             * values, so this case means we have an error or the identifier is
             * truly a bare string. Let's handle it robustly: if the token was
             * a STRING, wrap it. Otherwise error.
             */
            if (key_tok.type == XCDN_TOK_STRING) {
                xcdn_value_t *sv = xcdn_value_string_owned(key_tok.data.string_val.str);
                key_tok.data.string_val.str = NULL;
                xcdn_node_t *sn = xcdn_node_new(sv);
                xcdn_document_push_value(doc, sn);
            } else {
                /* An ident not followed by : in top-level is an error */
                p->err = xcdn_error_new(XCDN_ERR_EXPECTED, key_tok.span,
                    "expected ':' after top-level key '%s'",
                    key_tok.data.string_val.str ? key_tok.data.string_val.str : "?");
                xcdn_token_free(&key_tok);
                xcdn_document_free(doc);
                return NULL;
            }

            /* Continue parsing more values */
            while (parser_peek_type(p) != XCDN_TOK_EOF) {
                if (xcdn_error_is_set(&p->err)) {
                    xcdn_document_free(doc);
                    return NULL;
                }
                xcdn_node_t *n = parse_node(p);
                if (!n || xcdn_error_is_set(&p->err)) {
                    xcdn_node_free(n);
                    xcdn_document_free(doc);
                    return NULL;
                }
                xcdn_document_push_value(doc, n);
            }
        }
    } else if (pk == XCDN_TOK_EOF) {
        /* Empty document after prolog */
        return doc;
    } else {
        /* Stream of values */
        xcdn_node_t *first = parse_node(p);
        if (!first || xcdn_error_is_set(&p->err)) {
            xcdn_node_free(first);
            xcdn_document_free(doc);
            return NULL;
        }
        xcdn_document_push_value(doc, first);

        while (parser_peek_type(p) != XCDN_TOK_EOF) {
            if (xcdn_error_is_set(&p->err)) {
                xcdn_document_free(doc);
                return NULL;
            }
            xcdn_node_t *n = parse_node(p);
            if (!n || xcdn_error_is_set(&p->err)) {
                xcdn_node_free(n);
                xcdn_document_free(doc);
                return NULL;
            }
            xcdn_document_push_value(doc, n);
        }
    }

    return doc;
}

/* ── Public API ───────────────────────────────────────────────────────── */

xcdn_document_t *xcdn_parse_str(const char *src, size_t src_len,
                                xcdn_error_t *err) {
    parser_t p;
    parser_init(&p, src, src_len);
    xcdn_document_t *doc = parse_document(&p);
    if (xcdn_error_is_set(&p.err)) {
        if (err) *err = p.err;
        xcdn_document_free(doc);
        return NULL;
    }
    if (err) *err = xcdn_error_none();
    return doc;
}

xcdn_document_t *xcdn_parse(const char *src, xcdn_error_t *err) {
    return xcdn_parse_str(src, strlen(src), err);
}
