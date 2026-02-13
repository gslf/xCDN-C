/*
 * xCDN - eXtensible Cognitive Data Notation (C implementation)
 * AST types for xCDN.
 *
 * MIT License
 */

#include "ast.h"
#include <stdlib.h>
#include <string.h>

/* ── Internal helpers ─────────────────────────────────────────────────── */

static char *xcdn_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *dup = (char *)malloc(len + 1);
    if (dup) memcpy(dup, s, len + 1);
    return dup;
}

#define INITIAL_CAP 4

static void grow_ptr_array(void **ptr, size_t *cap, size_t elem_size) {
    size_t new_cap = (*cap == 0) ? INITIAL_CAP : (*cap * 2);
    void *new_ptr = realloc(*ptr, new_cap * elem_size);
    if (new_ptr) {
        *ptr = new_ptr;
        *cap = new_cap;
    }
}

/* ── Document ─────────────────────────────────────────────────────────── */

xcdn_document_t *xcdn_document_new(void) {
    xcdn_document_t *doc = (xcdn_document_t *)calloc(1, sizeof(xcdn_document_t));
    return doc;
}

void xcdn_document_push_value(xcdn_document_t *doc, xcdn_node_t *node) {
    if (!doc || !node) return;
    if (doc->values_len >= doc->values_cap) {
        grow_ptr_array((void **)&doc->values, &doc->values_cap,
                       sizeof(xcdn_node_t *));
    }
    doc->values[doc->values_len++] = node;
}

void xcdn_document_push_directive(xcdn_document_t *doc, const char *name,
                                  xcdn_value_t *value) {
    if (!doc) return;
    if (doc->prolog_len >= doc->prolog_cap) {
        grow_ptr_array((void **)&doc->prolog, &doc->prolog_cap,
                       sizeof(xcdn_directive_t));
    }
    xcdn_directive_t *d = &doc->prolog[doc->prolog_len++];
    d->name = xcdn_strdup(name);
    d->value = value;
}

xcdn_node_t *xcdn_document_get(const xcdn_document_t *doc, size_t index) {
    if (!doc || index >= doc->values_len) return NULL;
    return doc->values[index];
}

xcdn_node_t *xcdn_document_get_key(const xcdn_document_t *doc, const char *key) {
    if (!doc || doc->values_len == 0) return NULL;
    xcdn_node_t *first = doc->values[0];
    if (!first || !first->value || first->value->type != XCDN_VAL_OBJECT)
        return NULL;
    return xcdn_object_get(first->value, key);
}

bool xcdn_document_has_key(const xcdn_document_t *doc, const char *key) {
    return xcdn_document_get_key(doc, key) != NULL;
}

/* ── Node ─────────────────────────────────────────────────────────────── */

xcdn_node_t *xcdn_node_new(xcdn_value_t *value) {
    xcdn_node_t *node = (xcdn_node_t *)calloc(1, sizeof(xcdn_node_t));
    if (node) node->value = value;
    return node;
}

void xcdn_node_add_tag(xcdn_node_t *node, const char *name) {
    if (!node) return;
    if (node->tags_len >= node->tags_cap) {
        grow_ptr_array((void **)&node->tags, &node->tags_cap,
                       sizeof(xcdn_tag_t));
    }
    node->tags[node->tags_len].name = xcdn_strdup(name);
    node->tags_len++;
}

void xcdn_node_add_annotation(xcdn_node_t *node, const char *name) {
    if (!node) return;
    if (node->annotations_len >= node->annotations_cap) {
        grow_ptr_array((void **)&node->annotations, &node->annotations_cap,
                       sizeof(xcdn_annotation_t));
    }
    xcdn_annotation_t *ann = &node->annotations[node->annotations_len];
    memset(ann, 0, sizeof(*ann));
    ann->name = xcdn_strdup(name);
    node->annotations_len++;
}

void xcdn_annotation_push_arg(xcdn_annotation_t *ann, xcdn_value_t *val) {
    if (!ann || !val) return;
    if (ann->args_len >= ann->args_cap) {
        grow_ptr_array((void **)&ann->args, &ann->args_cap,
                       sizeof(xcdn_value_t *));
    }
    ann->args[ann->args_len++] = val;
}

/* ── Value constructors ───────────────────────────────────────────────── */

static xcdn_value_t *alloc_value(xcdn_value_type_t type) {
    xcdn_value_t *v = (xcdn_value_t *)calloc(1, sizeof(xcdn_value_t));
    if (v) v->type = type;
    return v;
}

xcdn_value_t *xcdn_value_null(void) {
    return alloc_value(XCDN_VAL_NULL);
}

xcdn_value_t *xcdn_value_bool(bool v) {
    xcdn_value_t *val = alloc_value(XCDN_VAL_BOOL);
    if (val) val->data.boolean = v;
    return val;
}

xcdn_value_t *xcdn_value_int(int64_t v) {
    xcdn_value_t *val = alloc_value(XCDN_VAL_INT);
    if (val) val->data.integer = v;
    return val;
}

xcdn_value_t *xcdn_value_float(double v) {
    xcdn_value_t *val = alloc_value(XCDN_VAL_FLOAT);
    if (val) val->data.floating = v;
    return val;
}

xcdn_value_t *xcdn_value_decimal(const char *s) {
    xcdn_value_t *val = alloc_value(XCDN_VAL_DECIMAL);
    if (val) val->data.string = xcdn_strdup(s);
    return val;
}

xcdn_value_t *xcdn_value_string(const char *s) {
    xcdn_value_t *val = alloc_value(XCDN_VAL_STRING);
    if (val) val->data.string = xcdn_strdup(s);
    return val;
}

xcdn_value_t *xcdn_value_string_owned(char *s) {
    xcdn_value_t *val = alloc_value(XCDN_VAL_STRING);
    if (val) val->data.string = s;
    return val;
}

xcdn_value_t *xcdn_value_bytes(const uint8_t *data, size_t len) {
    xcdn_value_t *val = alloc_value(XCDN_VAL_BYTES);
    if (val) {
        val->data.bytes.data = (uint8_t *)malloc(len);
        if (val->data.bytes.data) {
            memcpy(val->data.bytes.data, data, len);
            val->data.bytes.len = len;
        }
    }
    return val;
}

xcdn_value_t *xcdn_value_bytes_owned(uint8_t *data, size_t len) {
    xcdn_value_t *val = alloc_value(XCDN_VAL_BYTES);
    if (val) {
        val->data.bytes.data = data;
        val->data.bytes.len = len;
    }
    return val;
}

xcdn_value_t *xcdn_value_datetime(const char *s) {
    xcdn_value_t *val = alloc_value(XCDN_VAL_DATETIME);
    if (val) val->data.string = xcdn_strdup(s);
    return val;
}

xcdn_value_t *xcdn_value_duration(const char *s) {
    xcdn_value_t *val = alloc_value(XCDN_VAL_DURATION);
    if (val) val->data.string = xcdn_strdup(s);
    return val;
}

xcdn_value_t *xcdn_value_uuid(const char *s) {
    xcdn_value_t *val = alloc_value(XCDN_VAL_UUID);
    if (val) val->data.string = xcdn_strdup(s);
    return val;
}

xcdn_value_t *xcdn_value_array(void) {
    return alloc_value(XCDN_VAL_ARRAY);
}

xcdn_value_t *xcdn_value_object(void) {
    return alloc_value(XCDN_VAL_OBJECT);
}

/* ── Array operations ─────────────────────────────────────────────────── */

void xcdn_array_push(xcdn_value_t *arr, xcdn_node_t *node) {
    if (!arr || arr->type != XCDN_VAL_ARRAY || !node) return;
    if (arr->data.array.len >= arr->data.array.cap) {
        grow_ptr_array((void **)&arr->data.array.items, &arr->data.array.cap,
                       sizeof(xcdn_node_t *));
    }
    arr->data.array.items[arr->data.array.len++] = node;
}

xcdn_node_t *xcdn_array_get(const xcdn_value_t *arr, size_t index) {
    if (!arr || arr->type != XCDN_VAL_ARRAY) return NULL;
    if (index >= arr->data.array.len) return NULL;
    return arr->data.array.items[index];
}

size_t xcdn_array_len(const xcdn_value_t *arr) {
    if (!arr || arr->type != XCDN_VAL_ARRAY) return 0;
    return arr->data.array.len;
}

/* ── Object operations ────────────────────────────────────────────────── */

void xcdn_object_set(xcdn_value_t *obj, const char *key, xcdn_node_t *node) {
    if (!obj || obj->type != XCDN_VAL_OBJECT || !key || !node) return;

    /* Check if key already exists and update */
    for (size_t i = 0; i < obj->data.object.len; i++) {
        if (strcmp(obj->data.object.entries[i].key, key) == 0) {
            xcdn_node_free(obj->data.object.entries[i].node);
            obj->data.object.entries[i].node = node;
            return;
        }
    }

    /* Insert new entry */
    if (obj->data.object.len >= obj->data.object.cap) {
        grow_ptr_array((void **)&obj->data.object.entries, &obj->data.object.cap,
                       sizeof(xcdn_object_entry_t));
    }
    xcdn_object_entry_t *e = &obj->data.object.entries[obj->data.object.len++];
    e->key = xcdn_strdup(key);
    e->node = node;
}

xcdn_node_t *xcdn_object_get(const xcdn_value_t *obj, const char *key) {
    if (!obj || obj->type != XCDN_VAL_OBJECT || !key) return NULL;
    for (size_t i = 0; i < obj->data.object.len; i++) {
        if (strcmp(obj->data.object.entries[i].key, key) == 0)
            return obj->data.object.entries[i].node;
    }
    return NULL;
}

bool xcdn_object_has(const xcdn_value_t *obj, const char *key) {
    return xcdn_object_get(obj, key) != NULL;
}

size_t xcdn_object_len(const xcdn_value_t *obj) {
    if (!obj || obj->type != XCDN_VAL_OBJECT) return 0;
    return obj->data.object.len;
}

const char *xcdn_object_key_at(const xcdn_value_t *obj, size_t i) {
    if (!obj || obj->type != XCDN_VAL_OBJECT || i >= obj->data.object.len)
        return NULL;
    return obj->data.object.entries[i].key;
}

xcdn_node_t *xcdn_object_node_at(const xcdn_value_t *obj, size_t i) {
    if (!obj || obj->type != XCDN_VAL_OBJECT || i >= obj->data.object.len)
        return NULL;
    return obj->data.object.entries[i].node;
}

/* ── Value accessors ──────────────────────────────────────────────────── */

const char *xcdn_value_as_string(const xcdn_value_t *val) {
    if (!val) return NULL;
    switch (val->type) {
        case XCDN_VAL_STRING:
        case XCDN_VAL_DECIMAL:
        case XCDN_VAL_DATETIME:
        case XCDN_VAL_DURATION:
        case XCDN_VAL_UUID:
            return val->data.string;
        default:
            return NULL;
    }
}

int64_t xcdn_value_as_int(const xcdn_value_t *val) {
    if (!val || val->type != XCDN_VAL_INT) return 0;
    return val->data.integer;
}

double xcdn_value_as_float(const xcdn_value_t *val) {
    if (!val || val->type != XCDN_VAL_FLOAT) return 0.0;
    return val->data.floating;
}

bool xcdn_value_as_bool(const xcdn_value_t *val) {
    if (!val || val->type != XCDN_VAL_BOOL) return false;
    return val->data.boolean;
}

const uint8_t *xcdn_value_as_bytes(const xcdn_value_t *val, size_t *out_len) {
    if (!val || val->type != XCDN_VAL_BYTES) {
        if (out_len) *out_len = 0;
        return NULL;
    }
    if (out_len) *out_len = val->data.bytes.len;
    return val->data.bytes.data;
}

/* ── Deep path access ─────────────────────────────────────────────────── */

xcdn_node_t *xcdn_get_path(const xcdn_document_t *doc, const char *path) {
    if (!doc || !path || doc->values_len == 0) return NULL;

    xcdn_node_t *current = doc->values[0];
    if (!current) return NULL;

    /* Copy path so we can tokenize it */
    char *buf = xcdn_strdup(path);
    if (!buf) return NULL;

    char *saveptr = NULL;
    char *segment = strtok_r(buf, ".", &saveptr);

    while (segment) {
        if (!current || !current->value ||
            current->value->type != XCDN_VAL_OBJECT) {
            free(buf);
            return NULL;
        }
        current = xcdn_object_get(current->value, segment);
        segment = strtok_r(NULL, ".", &saveptr);
    }

    free(buf);
    return current;
}

/* ── Tag/Annotation accessors ─────────────────────────────────────────── */

bool xcdn_node_has_tag(const xcdn_node_t *node, const char *name) {
    if (!node || !name) return false;
    for (size_t i = 0; i < node->tags_len; i++) {
        if (strcmp(node->tags[i].name, name) == 0) return true;
    }
    return false;
}

const char *xcdn_node_tag_at(const xcdn_node_t *node, size_t i) {
    if (!node || i >= node->tags_len) return NULL;
    return node->tags[i].name;
}

size_t xcdn_node_tag_count(const xcdn_node_t *node) {
    return node ? node->tags_len : 0;
}

const xcdn_annotation_t *xcdn_node_find_annotation(const xcdn_node_t *node,
                                                    const char *name) {
    if (!node || !name) return NULL;
    for (size_t i = 0; i < node->annotations_len; i++) {
        if (strcmp(node->annotations[i].name, name) == 0)
            return &node->annotations[i];
    }
    return NULL;
}

bool xcdn_node_has_annotation(const xcdn_node_t *node, const char *name) {
    return xcdn_node_find_annotation(node, name) != NULL;
}

size_t xcdn_node_annotation_count(const xcdn_node_t *node) {
    return node ? node->annotations_len : 0;
}

const xcdn_value_t *xcdn_annotation_arg(const xcdn_annotation_t *ann, size_t i) {
    if (!ann || i >= ann->args_len) return NULL;
    return ann->args[i];
}

size_t xcdn_annotation_arg_count(const xcdn_annotation_t *ann) {
    return ann ? ann->args_len : 0;
}

/* ── Destructors ──────────────────────────────────────────────────────── */

void xcdn_value_free(xcdn_value_t *val) {
    if (!val) return;
    switch (val->type) {
        case XCDN_VAL_STRING:
        case XCDN_VAL_DECIMAL:
        case XCDN_VAL_DATETIME:
        case XCDN_VAL_DURATION:
        case XCDN_VAL_UUID:
            free(val->data.string);
            break;
        case XCDN_VAL_BYTES:
            free(val->data.bytes.data);
            break;
        case XCDN_VAL_ARRAY:
            for (size_t i = 0; i < val->data.array.len; i++)
                xcdn_node_free(val->data.array.items[i]);
            free(val->data.array.items);
            break;
        case XCDN_VAL_OBJECT:
            for (size_t i = 0; i < val->data.object.len; i++) {
                free(val->data.object.entries[i].key);
                xcdn_node_free(val->data.object.entries[i].node);
            }
            free(val->data.object.entries);
            break;
        default:
            break;
    }
    free(val);
}

static void free_annotation(xcdn_annotation_t *ann) {
    free(ann->name);
    for (size_t i = 0; i < ann->args_len; i++)
        xcdn_value_free(ann->args[i]);
    free(ann->args);
}

void xcdn_node_free(xcdn_node_t *node) {
    if (!node) return;
    for (size_t i = 0; i < node->tags_len; i++)
        free(node->tags[i].name);
    free(node->tags);
    for (size_t i = 0; i < node->annotations_len; i++)
        free_annotation(&node->annotations[i]);
    free(node->annotations);
    xcdn_value_free(node->value);
    free(node);
}

void xcdn_document_free(xcdn_document_t *doc) {
    if (!doc) return;
    for (size_t i = 0; i < doc->prolog_len; i++) {
        free(doc->prolog[i].name);
        xcdn_value_free(doc->prolog[i].value);
    }
    free(doc->prolog);
    for (size_t i = 0; i < doc->values_len; i++)
        xcdn_node_free(doc->values[i]);
    free(doc->values);
    free(doc);
}

/* ── Type name ────────────────────────────────────────────────────────── */

const char *xcdn_value_type_str(xcdn_value_type_t type) {
    switch (type) {
        case XCDN_VAL_NULL:     return "null";
        case XCDN_VAL_BOOL:     return "bool";
        case XCDN_VAL_INT:      return "int";
        case XCDN_VAL_FLOAT:    return "float";
        case XCDN_VAL_DECIMAL:  return "decimal";
        case XCDN_VAL_STRING:   return "string";
        case XCDN_VAL_BYTES:    return "bytes";
        case XCDN_VAL_DATETIME: return "datetime";
        case XCDN_VAL_DURATION: return "duration";
        case XCDN_VAL_UUID:     return "uuid";
        case XCDN_VAL_ARRAY:    return "array";
        case XCDN_VAL_OBJECT:   return "object";
        default:                return "unknown";
    }
}
