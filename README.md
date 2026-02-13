# > xCDN_ (C)

A complete C11 library to parse, serialize and manipulate **> xCDN_ — eXtensible Cognitive Data Notation**.

> **What is > xCDN_?**
> xCDN_ is a human-first, machine-optimized data notation with native types, tags and annotations.
> It supports comments, trailing commas, unquoted keys and multi-line strings.
> You can read more about this notation in the [> xCDN_ repository](https://github.com/gslf/xCDN).

## Features

- Full streaming document model (one or more top-level values)
- Optional **prolog** (`$schema: "..."`, ...)
- Objects, arrays and scalars
- Native types: `Decimal` (`d"..."`), `UUID` (`u"..."`), `DateTime` (`t"..."` RFC3339),
  `Duration` (`r"..."` ISO8601), `Bytes` (`b"..."` Base64)
- `#tags` and `@annotations(args?)` that decorate any value
- Comments: `//` and `/* ... */`
- Trailing commas and unquoted keys
- Pretty or compact serialization
- **Zero external dependencies** — pure C11, only the standard library
- Ergonomic accessor API: `xcdn_get_path()`, `xcdn_object_get()`, `xcdn_node_has_tag()`, etc.

## Example

```xcdn
$schema: "https://gslf.github.io/xCDN/schemas/v1/meta.xcdn",

server_config: {
  host: "localhost",
  // Unquoted keys & trailing commas? Yes.
  ports: [8080, 9090,],

  // Native Decimals & ISO8601 Duration
  timeout: r"PT30S",
  max_cost: d"19.99",

  // Semantic Tagging
  admin: #user {
    id: u"550e8400-e29b-41d4-a716-446655440000",
    role: "superuser"
  },

  // Binary data handling
  icon: @mime("image/png") b"iVBORw0KGgoAAAANSUhEUgAAAAUA...",
}
```

## Usage

### Parsing and serializing

```c
#include "xcdn.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    const char *input = "name: \"xCDN\", version: 1";

    xcdn_error_t err;
    xcdn_document_t *doc = xcdn_parse(input, &err);
    if (!doc) {
        fprintf(stderr, "Error: %s at line %zu\n", err.message, err.span.line);
        return 1;
    }

    char *text = xcdn_to_string_pretty(doc);
    printf("%s\n", text);

    free(text);
    xcdn_document_free(doc);
    return 0;
}
```

### Accessing fields

```c
xcdn_document_t *doc = xcdn_parse(input, &err);

/* Direct key lookup on the root object */
xcdn_node_t *name = xcdn_document_get_key(doc, "name");
printf("Name: %s\n", xcdn_value_as_string(name->value));

/* Nested access */
xcdn_node_t *host = xcdn_object_get(config->value, "host");
printf("Host: %s\n", xcdn_value_as_string(host->value));

/* Deep path access (dot-separated) */
xcdn_node_t *deep = xcdn_get_path(doc, "config.db.host");
if (deep) printf("DB Host: %s\n", xcdn_value_as_string(deep->value));

/* Array access */
xcdn_node_t *first = xcdn_array_get(arr_value, 0);
printf("First: %lld\n", (long long)xcdn_value_as_int(first->value));
```

### Tags and annotations

```c
/* Check if a node has a tag */
if (xcdn_node_has_tag(node, "user")) {
    printf("This is a user!\n");
}

/* Find annotation and access arguments */
const xcdn_annotation_t *mime = xcdn_node_find_annotation(node, "mime");
if (mime) {
    const char *type = xcdn_value_as_string(xcdn_annotation_arg(mime, 0));
    printf("MIME type: %s\n", type);
}

/* Iterate tags */
for (size_t i = 0; i < xcdn_node_tag_count(node); i++) {
    printf("Tag: #%s\n", xcdn_node_tag_at(node, i));
}
```

### Object iteration

```c
xcdn_value_t *obj = node->value;
size_t len = xcdn_object_len(obj);
for (size_t i = 0; i < len; i++) {
    const char *key = xcdn_object_key_at(obj, i);
    xcdn_node_t *val = xcdn_object_node_at(obj, i);
    printf("  %s: type=%s\n", key, xcdn_value_type_str(val->value->type));
}
```

### Programmatic construction

```c
xcdn_document_t *doc = xcdn_document_new();

xcdn_value_t *obj = xcdn_value_object();
xcdn_object_set(obj, "name", xcdn_node_new(xcdn_value_string("Alice")));
xcdn_object_set(obj, "age", xcdn_node_new(xcdn_value_int(30)));

xcdn_node_t *root = xcdn_node_new(obj);
xcdn_node_add_tag(root, "person");
xcdn_document_push_value(doc, root);

char *text = xcdn_to_string_pretty(doc);
printf("%s\n", text);

free(text);
xcdn_document_free(doc);
```

## Building

### With CMake

```bash
mkdir build && cd build
cmake ..
cmake --build .
ctest
```

### With Make

```bash
make all
make test
```

### Manual compilation

```bash
gcc -std=c11 -Isrc src/*.c examples/roundtrip.c -o roundtrip
```

## API Reference

### Parsing

| Function | Description |
|---|---|
| `xcdn_parse(src, &err)` | Parse a NUL-terminated string |
| `xcdn_parse_str(src, len, &err)` | Parse a string with explicit length |

### Serialization

| Function | Description |
|---|---|
| `xcdn_to_string_pretty(doc)` | Pretty-print (indent=2, trailing commas) |
| `xcdn_to_string_compact(doc)` | Compact (no whitespace) |
| `xcdn_to_string_with_format(doc, fmt)` | Custom format options |

### Value Constructors

| Function | Description |
|---|---|
| `xcdn_value_null()` | Null value |
| `xcdn_value_bool(v)` | Boolean value |
| `xcdn_value_int(v)` | 64-bit integer |
| `xcdn_value_float(v)` | Double-precision float |
| `xcdn_value_string(s)` | String (copies) |
| `xcdn_value_decimal(s)` | Arbitrary-precision decimal |
| `xcdn_value_bytes(data, len)` | Binary data (copies) |
| `xcdn_value_datetime(s)` | RFC3339 datetime |
| `xcdn_value_duration(s)` | ISO8601 duration |
| `xcdn_value_uuid(s)` | UUID |
| `xcdn_value_array()` | Empty array |
| `xcdn_value_object()` | Empty object |

### Accessors

| Function | Description |
|---|---|
| `xcdn_document_get_key(doc, key)` | Lookup key in root object |
| `xcdn_get_path(doc, "a.b.c")` | Deep path access |
| `xcdn_object_get(obj, key)` | Lookup key in object |
| `xcdn_object_has(obj, key)` | Check key existence |
| `xcdn_object_len(obj)` | Number of entries |
| `xcdn_object_key_at(obj, i)` | Key at index |
| `xcdn_array_get(arr, i)` | Element at index |
| `xcdn_array_len(arr)` | Array length |
| `xcdn_value_as_string(val)` | Extract string |
| `xcdn_value_as_int(val)` | Extract int64 |
| `xcdn_value_as_float(val)` | Extract double |
| `xcdn_value_as_bool(val)` | Extract bool |
| `xcdn_value_as_bytes(val, &len)` | Extract byte data |

### Tags & Annotations

| Function | Description |
|---|---|
| `xcdn_node_has_tag(node, name)` | Check tag existence |
| `xcdn_node_tag_at(node, i)` | Tag name at index |
| `xcdn_node_tag_count(node)` | Number of tags |
| `xcdn_node_has_annotation(node, name)` | Check annotation existence |
| `xcdn_node_find_annotation(node, name)` | Find annotation by name |
| `xcdn_annotation_arg(ann, i)` | Annotation argument at index |
| `xcdn_annotation_arg_count(ann)` | Number of arguments |

### Memory Management

| Function | Description |
|---|---|
| `xcdn_document_free(doc)` | Free document and all children |
| `xcdn_node_free(node)` | Free node and its value |
| `xcdn_value_free(val)` | Free a value |

All `free` functions are recursive and NULL-safe.

## Testing

```bash
# With CMake
cd build && ctest --output-on-failure

# With Make
make test
```

## License

MIT, see [LICENSE](LICENSE).

---

#### This is a :/# GSLF project.
