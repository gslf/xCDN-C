/*
 * xCDN - eXtensible Cognitive Data Notation (C implementation)
 * Serializer for xCDN.
 *
 * MIT License
 */

#include "ser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <inttypes.h>

/* ── String buffer ────────────────────────────────────────────────────── */

typedef struct {
    char  *buf;
    size_t len;
    size_t cap;
} sbuf_t;

static void sbuf_init(sbuf_t *sb) {
    sb->buf = NULL;
    sb->len = 0;
    sb->cap = 0;
}

static void sbuf_ensure(sbuf_t *sb, size_t extra) {
    size_t need = sb->len + extra;
    if (need <= sb->cap) return;
    size_t new_cap = (sb->cap == 0) ? 256 : sb->cap;
    while (new_cap < need) new_cap *= 2;
    char *new_buf = (char *)realloc(sb->buf, new_cap);
    if (!new_buf) return;
    sb->buf = new_buf;
    sb->cap = new_cap;
}

static void sbuf_push_char(sbuf_t *sb, char c) {
    sbuf_ensure(sb, 1);
    sb->buf[sb->len++] = c;
}

static void sbuf_push_str(sbuf_t *sb, const char *s) {
    size_t slen = strlen(s);
    sbuf_ensure(sb, slen);
    memcpy(sb->buf + sb->len, s, slen);
    sb->len += slen;
}

static void sbuf_push_fmt(sbuf_t *sb, const char *fmt, ...) {
    char tmp[128];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n > 0) {
        sbuf_ensure(sb, (size_t)n);
        memcpy(sb->buf + sb->len, tmp, (size_t)n);
        sb->len += (size_t)n;
    }
}

static char *sbuf_finish(sbuf_t *sb) {
    sbuf_push_char(sb, '\0');
    return sb->buf;
}

/* ── Base64 encoder ───────────────────────────────────────────────────── */

static const char b64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void b64_encode(sbuf_t *sb, const uint8_t *data, size_t len) {
    size_t i = 0;
    while (i + 2 < len) {
        uint32_t n = ((uint32_t)data[i] << 16) |
                     ((uint32_t)data[i+1] << 8) |
                     (uint32_t)data[i+2];
        sbuf_push_char(sb, b64_chars[(n >> 18) & 0x3F]);
        sbuf_push_char(sb, b64_chars[(n >> 12) & 0x3F]);
        sbuf_push_char(sb, b64_chars[(n >> 6) & 0x3F]);
        sbuf_push_char(sb, b64_chars[n & 0x3F]);
        i += 3;
    }
    if (i < len) {
        uint32_t n = (uint32_t)data[i] << 16;
        if (i + 1 < len) n |= (uint32_t)data[i+1] << 8;
        sbuf_push_char(sb, b64_chars[(n >> 18) & 0x3F]);
        sbuf_push_char(sb, b64_chars[(n >> 12) & 0x3F]);
        if (i + 1 < len) {
            sbuf_push_char(sb, b64_chars[(n >> 6) & 0x3F]);
        } else {
            sbuf_push_char(sb, '=');
        }
        sbuf_push_char(sb, '=');
    }
}

/* ── Helpers ──────────────────────────────────────────────────────────── */

static int is_simple_ident(const char *s) {
    if (!s || !*s) return 0;
    unsigned char c = (unsigned char)s[0];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'))
        return 0;
    for (size_t i = 1; s[i]; i++) {
        c = (unsigned char)s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-'))
            return 0;
    }
    return 1;
}

static void write_indent(sbuf_t *sb, int depth, int space) {
    int n = depth * space;
    for (int i = 0; i < n; i++) sbuf_push_char(sb, ' ');
}

static void write_escaped_string(sbuf_t *sb, const char *s) {
    sbuf_push_char(sb, '"');
    if (s) {
        for (size_t i = 0; s[i]; i++) {
            unsigned char ch = (unsigned char)s[i];
            switch (ch) {
                case '\\': sbuf_push_str(sb, "\\\\"); break;
                case '"':  sbuf_push_str(sb, "\\\""); break;
                case '\n': sbuf_push_str(sb, "\\n"); break;
                case '\r': sbuf_push_str(sb, "\\r"); break;
                case '\t': sbuf_push_str(sb, "\\t"); break;
                default:
                    if (ch < 32) {
                        sbuf_push_fmt(sb, "\\u%04X", ch);
                    } else {
                        sbuf_push_char(sb, (char)ch);
                    }
                    break;
            }
        }
    }
    sbuf_push_char(sb, '"');
}

static void write_key(sbuf_t *sb, const char *k) {
    if (is_simple_ident(k)) {
        sbuf_push_str(sb, k);
    } else {
        write_escaped_string(sb, k);
    }
}

/* ── Forward declarations ─────────────────────────────────────────────── */

static void write_node(sbuf_t *sb, const xcdn_node_t *node,
                       xcdn_format_t fmt, int depth);
static void write_value(sbuf_t *sb, const xcdn_value_t *val,
                        xcdn_format_t fmt, int depth);

/* ── Write annotation ─────────────────────────────────────────────────── */

static void write_annotation(sbuf_t *sb, const xcdn_annotation_t *a) {
    sbuf_push_char(sb, '@');
    sbuf_push_str(sb, a->name);
    if (a->args_len > 0) {
        sbuf_push_char(sb, '(');
        xcdn_format_t compact = xcdn_format_compact();
        for (size_t i = 0; i < a->args_len; i++) {
            if (i > 0) sbuf_push_str(sb, ", ");
            write_value(sb, a->args[i], compact, 0);
        }
        sbuf_push_char(sb, ')');
    }
}

static void write_tag(sbuf_t *sb, const xcdn_tag_t *t) {
    sbuf_push_char(sb, '#');
    sbuf_push_str(sb, t->name);
}

/* ── Write value ──────────────────────────────────────────────────────── */

static void write_value(sbuf_t *sb, const xcdn_value_t *val,
                        xcdn_format_t fmt, int depth) {
    if (!val) { sbuf_push_str(sb, "null"); return; }

    switch (val->type) {
        case XCDN_VAL_NULL:
            sbuf_push_str(sb, "null");
            break;

        case XCDN_VAL_BOOL:
            sbuf_push_str(sb, val->data.boolean ? "true" : "false");
            break;

        case XCDN_VAL_INT:
            sbuf_push_fmt(sb, "%" PRId64, val->data.integer);
            break;

        case XCDN_VAL_FLOAT: {
            char tmp[64];
            snprintf(tmp, sizeof(tmp), "%g", val->data.floating);
            sbuf_push_str(sb, tmp);
            break;
        }

        case XCDN_VAL_DECIMAL:
            sbuf_push_str(sb, "d\"");
            sbuf_push_str(sb, val->data.string ? val->data.string : "");
            sbuf_push_char(sb, '"');
            break;

        case XCDN_VAL_STRING:
            write_escaped_string(sb, val->data.string);
            break;

        case XCDN_VAL_BYTES:
            sbuf_push_str(sb, "b\"");
            b64_encode(sb, val->data.bytes.data, val->data.bytes.len);
            sbuf_push_char(sb, '"');
            break;

        case XCDN_VAL_DATETIME:
            sbuf_push_str(sb, "t\"");
            sbuf_push_str(sb, val->data.string ? val->data.string : "");
            sbuf_push_char(sb, '"');
            break;

        case XCDN_VAL_DURATION:
            sbuf_push_str(sb, "r\"");
            sbuf_push_str(sb, val->data.string ? val->data.string : "");
            sbuf_push_char(sb, '"');
            break;

        case XCDN_VAL_UUID:
            sbuf_push_str(sb, "u\"");
            sbuf_push_str(sb, val->data.string ? val->data.string : "");
            sbuf_push_char(sb, '"');
            break;

        case XCDN_VAL_ARRAY: {
            sbuf_push_char(sb, '[');
            size_t len = val->data.array.len;
            if (fmt.pretty && len > 0) sbuf_push_char(sb, '\n');
            for (size_t i = 0; i < len; i++) {
                if (fmt.pretty) write_indent(sb, depth + 1, fmt.indent);
                write_node(sb, val->data.array.items[i], fmt, depth + 1);
                if (i + 1 < len || fmt.trailing_commas)
                    sbuf_push_char(sb, ',');
                if (fmt.pretty) sbuf_push_char(sb, '\n');
            }
            if (fmt.pretty && len > 0) write_indent(sb, depth, fmt.indent);
            sbuf_push_char(sb, ']');
            break;
        }

        case XCDN_VAL_OBJECT: {
            sbuf_push_char(sb, '{');
            size_t len = val->data.object.len;
            if (fmt.pretty && len > 0) sbuf_push_char(sb, '\n');
            for (size_t i = 0; i < len; i++) {
                if (fmt.pretty) write_indent(sb, depth + 1, fmt.indent);
                write_key(sb, val->data.object.entries[i].key);
                sbuf_push_str(sb, ": ");
                write_node(sb, val->data.object.entries[i].node, fmt, depth + 1);
                if (i + 1 < len || fmt.trailing_commas)
                    sbuf_push_char(sb, ',');
                if (fmt.pretty) sbuf_push_char(sb, '\n');
            }
            if (fmt.pretty && len > 0) write_indent(sb, depth, fmt.indent);
            sbuf_push_char(sb, '}');
            break;
        }
    }
}

/* ── Write node ───────────────────────────────────────────────────────── */

static void write_node(sbuf_t *sb, const xcdn_node_t *node,
                       xcdn_format_t fmt, int depth) {
    if (!node) return;
    for (size_t i = 0; i < node->annotations_len; i++) {
        write_annotation(sb, &node->annotations[i]);
        sbuf_push_char(sb, ' ');
    }
    for (size_t i = 0; i < node->tags_len; i++) {
        write_tag(sb, &node->tags[i]);
        sbuf_push_char(sb, ' ');
    }
    write_value(sb, node->value, fmt, depth);
}

/* ── Public API ───────────────────────────────────────────────────────── */

xcdn_format_t xcdn_format_default(void) {
    xcdn_format_t f = {true, 2, true};
    return f;
}

xcdn_format_t xcdn_format_compact(void) {
    xcdn_format_t f = {false, 0, false};
    return f;
}

char *xcdn_to_string_with_format(const xcdn_document_t *doc,
                                 xcdn_format_t fmt) {
    if (!doc) return NULL;

    sbuf_t sb;
    sbuf_init(&sb);

    int first_dir = 1;
    for (size_t i = 0; i < doc->prolog_len; i++) {
        if (!first_dir && fmt.pretty) sbuf_push_char(&sb, '\n');
        sbuf_push_char(&sb, '$');
        sbuf_push_str(&sb, doc->prolog[i].name);
        sbuf_push_str(&sb, ": ");
        write_value(&sb, doc->prolog[i].value, fmt, 0);
        if (fmt.trailing_commas) sbuf_push_char(&sb, ',');
        sbuf_push_char(&sb, '\n');
        first_dir = 0;
    }

    for (size_t i = 0; i < doc->values_len; i++) {
        if (i > 0 && fmt.pretty) sbuf_push_char(&sb, '\n');
        write_node(&sb, doc->values[i], fmt, 0);
        if (i + 1 < doc->values_len && fmt.pretty)
            sbuf_push_char(&sb, '\n');
    }

    return sbuf_finish(&sb);
}

char *xcdn_to_string_pretty(const xcdn_document_t *doc) {
    return xcdn_to_string_with_format(doc, xcdn_format_default());
}

char *xcdn_to_string_compact(const xcdn_document_t *doc) {
    return xcdn_to_string_with_format(doc, xcdn_format_compact());
}
