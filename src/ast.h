/*
 * xCDN - eXtensible Cognitive Data Notation (C implementation)
 * AST types for xCDN.
 *
 * The AST is intentionally decoupled from parsing/serialization so it can be
 * constructed or consumed programmatically.
 *
 * MIT License
 */

#ifndef XCDN_AST_H
#define XCDN_AST_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* ── Forward declarations ─────────────────────────────────────────────── */

typedef struct xcdn_node      xcdn_node_t;
typedef struct xcdn_value     xcdn_value_t;
typedef struct xcdn_tag       xcdn_tag_t;
typedef struct xcdn_annotation xcdn_annotation_t;
typedef struct xcdn_directive xcdn_directive_t;
typedef struct xcdn_document  xcdn_document_t;

/* ── Value types ──────────────────────────────────────────────────────── */

typedef enum {
    XCDN_VAL_NULL = 0,
    XCDN_VAL_BOOL,
    XCDN_VAL_INT,
    XCDN_VAL_FLOAT,
    XCDN_VAL_DECIMAL,     /* d"..." arbitrary-precision decimal (stored as string) */
    XCDN_VAL_STRING,
    XCDN_VAL_BYTES,       /* b"..." base64 decoded */
    XCDN_VAL_DATETIME,    /* t"..." RFC3339 (stored as string) */
    XCDN_VAL_DURATION,    /* r"..." ISO8601 (stored as string) */
    XCDN_VAL_UUID,        /* u"..." (stored as string) */
    XCDN_VAL_ARRAY,
    XCDN_VAL_OBJECT,
} xcdn_value_type_t;

/* ── Object entry (key-value pair in ordered map) ─────────────────────── */

typedef struct xcdn_object_entry {
    char        *key;
    xcdn_node_t *node;
} xcdn_object_entry_t;

/* ── The core value union ─────────────────────────────────────────────── */

struct xcdn_value {
    xcdn_value_type_t type;
    union {
        bool            boolean;
        int64_t         integer;
        double          floating;
        char           *string;      /* for STRING, DECIMAL, DATETIME, DURATION, UUID */
        struct {
            uint8_t    *data;
            size_t      len;
        } bytes;
        struct {
            xcdn_node_t **items;
            size_t        len;
            size_t        cap;
        } array;
        struct {
            xcdn_object_entry_t *entries;
            size_t               len;
            size_t               cap;
        } object;
    } data;
};

/* ── Tag ──────────────────────────────────────────────────────────────── */

struct xcdn_tag {
    char *name;
};

/* ── Annotation ───────────────────────────────────────────────────────── */

struct xcdn_annotation {
    char          *name;
    xcdn_value_t **args;      /* Array of value pointers */
    size_t         args_len;
    size_t         args_cap;
};

/* ── Node: a value enriched with optional #tags and @annotations ──────── */

struct xcdn_node {
    xcdn_tag_t        *tags;
    size_t              tags_len;
    size_t              tags_cap;
    xcdn_annotation_t  *annotations;
    size_t              annotations_len;
    size_t              annotations_cap;
    xcdn_value_t       *value;
};

/* ── Directive: a prolog directive, e.g. $schema: "..." ───────────────── */

struct xcdn_directive {
    char         *name;   /* without the leading '$' */
    xcdn_value_t *value;
};

/* ── Document: the whole xCDN document ────────────────────────────────── */

struct xcdn_document {
    xcdn_directive_t *prolog;
    size_t            prolog_len;
    size_t            prolog_cap;
    xcdn_node_t     **values;
    size_t            values_len;
    size_t            values_cap;
};

/* ═══════════════════════════════════════════════════════════════════════
 * Constructors
 * ═══════════════════════════════════════════════════════════════════════ */

/* Create an empty document. */
xcdn_document_t *xcdn_document_new(void);

/* Create a bare node wrapping a value. */
xcdn_node_t *xcdn_node_new(xcdn_value_t *value);

/* Value constructors. The library takes ownership of heap strings/bytes. */
xcdn_value_t *xcdn_value_null(void);
xcdn_value_t *xcdn_value_bool(bool v);
xcdn_value_t *xcdn_value_int(int64_t v);
xcdn_value_t *xcdn_value_float(double v);
xcdn_value_t *xcdn_value_decimal(const char *s);
xcdn_value_t *xcdn_value_string(const char *s);
xcdn_value_t *xcdn_value_string_owned(char *s);   /* takes ownership */
xcdn_value_t *xcdn_value_bytes(const uint8_t *data, size_t len);
xcdn_value_t *xcdn_value_bytes_owned(uint8_t *data, size_t len);
xcdn_value_t *xcdn_value_datetime(const char *s);
xcdn_value_t *xcdn_value_duration(const char *s);
xcdn_value_t *xcdn_value_uuid(const char *s);
xcdn_value_t *xcdn_value_array(void);
xcdn_value_t *xcdn_value_object(void);

/* ═══════════════════════════════════════════════════════════════════════
 * Mutators
 * ═══════════════════════════════════════════════════════════════════════ */

/* Append a node to a document's values. */
void xcdn_document_push_value(xcdn_document_t *doc, xcdn_node_t *node);

/* Append a directive to a document's prolog. */
void xcdn_document_push_directive(xcdn_document_t *doc, const char *name,
                                  xcdn_value_t *value);

/* Append a node to an array value. */
void xcdn_array_push(xcdn_value_t *arr, xcdn_node_t *node);

/* Insert/update a key-value pair in an object value. */
void xcdn_object_set(xcdn_value_t *obj, const char *key, xcdn_node_t *node);

/* Add a tag to a node. */
void xcdn_node_add_tag(xcdn_node_t *node, const char *name);

/* Add an annotation to a node. */
void xcdn_node_add_annotation(xcdn_node_t *node, const char *name);

/* Add an argument value to the last annotation on a node. */
void xcdn_annotation_push_arg(xcdn_annotation_t *ann, xcdn_value_t *val);

/* ═══════════════════════════════════════════════════════════════════════
 * Ergonomic Accessors — easy field/tag/annotation access
 * ═══════════════════════════════════════════════════════════════════════ */

/*
 * Get the i-th top-level node from a document.
 * Returns NULL if index is out of bounds.
 */
xcdn_node_t *xcdn_document_get(const xcdn_document_t *doc, size_t index);

/*
 * Look up a key in the document's first top-level object value.
 * Shorthand for: doc->values[0]->value->object["key"]
 * Returns the Node, or NULL if not found.
 */
xcdn_node_t *xcdn_document_get_key(const xcdn_document_t *doc, const char *key);

/*
 * Check if a key exists in the document's first top-level object.
 */
bool xcdn_document_has_key(const xcdn_document_t *doc, const char *key);

/*
 * Look up a key in an object value.
 * Returns the Node for that key, or NULL if not found.
 */
xcdn_node_t *xcdn_object_get(const xcdn_value_t *obj, const char *key);

/*
 * Check if a key exists in an object value.
 */
bool xcdn_object_has(const xcdn_value_t *obj, const char *key);

/*
 * Get the number of entries in an object.
 */
size_t xcdn_object_len(const xcdn_value_t *obj);

/*
 * Get the key at index i in an object.
 * Returns NULL if out of bounds.
 */
const char *xcdn_object_key_at(const xcdn_value_t *obj, size_t i);

/*
 * Get the node at index i in an object.
 * Returns NULL if out of bounds.
 */
xcdn_node_t *xcdn_object_node_at(const xcdn_value_t *obj, size_t i);

/*
 * Get the i-th element from an array value.
 * Returns the Node, or NULL if out of bounds.
 */
xcdn_node_t *xcdn_array_get(const xcdn_value_t *arr, size_t index);

/*
 * Get the length of an array value.
 */
size_t xcdn_array_len(const xcdn_value_t *arr);

/*
 * Shorthand: get the string from a value (for STRING, DECIMAL, DATETIME,
 * DURATION, UUID types). Returns NULL if value is not a string type.
 */
const char *xcdn_value_as_string(const xcdn_value_t *val);

/*
 * Shorthand: get the integer from a value. Returns 0 if not INT type.
 */
int64_t xcdn_value_as_int(const xcdn_value_t *val);

/*
 * Shorthand: get the float from a value. Returns 0.0 if not FLOAT type.
 */
double xcdn_value_as_float(const xcdn_value_t *val);

/*
 * Shorthand: get the boolean from a value. Returns false if not BOOL type.
 */
bool xcdn_value_as_bool(const xcdn_value_t *val);

/*
 * Shorthand: get bytes data and length. Returns NULL if not BYTES type.
 */
const uint8_t *xcdn_value_as_bytes(const xcdn_value_t *val, size_t *out_len);

/*
 * Deep-access a nested field by dot-separated path.
 * E.g. xcdn_get_path(doc, "config.host") navigates doc->config->host.
 * Returns the Node, or NULL if any segment is missing.
 */
xcdn_node_t *xcdn_get_path(const xcdn_document_t *doc, const char *path);

/*
 * Check if a node has a specific tag.
 */
bool xcdn_node_has_tag(const xcdn_node_t *node, const char *name);

/*
 * Get a tag by index. Returns NULL if out of bounds.
 */
const char *xcdn_node_tag_at(const xcdn_node_t *node, size_t i);

/*
 * Get the number of tags on a node.
 */
size_t xcdn_node_tag_count(const xcdn_node_t *node);

/*
 * Find an annotation by name on a node. Returns NULL if not found.
 */
const xcdn_annotation_t *xcdn_node_find_annotation(const xcdn_node_t *node,
                                                    const char *name);

/*
 * Check if a node has a specific annotation.
 */
bool xcdn_node_has_annotation(const xcdn_node_t *node, const char *name);

/*
 * Get the number of annotations on a node.
 */
size_t xcdn_node_annotation_count(const xcdn_node_t *node);

/*
 * Get an annotation's argument by index as a value.
 * Returns NULL if out of bounds.
 */
const xcdn_value_t *xcdn_annotation_arg(const xcdn_annotation_t *ann, size_t i);

/*
 * Get the number of arguments in an annotation.
 */
size_t xcdn_annotation_arg_count(const xcdn_annotation_t *ann);

/* ═══════════════════════════════════════════════════════════════════════
 * Destructors — recursive deep free
 * ═══════════════════════════════════════════════════════════════════════ */

void xcdn_document_free(xcdn_document_t *doc);
void xcdn_node_free(xcdn_node_t *node);
void xcdn_value_free(xcdn_value_t *val);

/* ═══════════════════════════════════════════════════════════════════════
 * Type name helper
 * ═══════════════════════════════════════════════════════════════════════ */

const char *xcdn_value_type_str(xcdn_value_type_t type);

#endif /* XCDN_AST_H */
