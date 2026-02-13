/*
 * Lexer tests for xCDN-C.
 */

#include "xcdn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL [%s:%d]: %s\n", __FILE__, __LINE__, msg); \
        return; \
    } \
    tests_passed++; \
} while(0)

#define ASSERT_EQ_INT(a, b, msg) ASSERT((a) == (b), msg)
#define ASSERT_EQ_STR(a, b, msg) ASSERT(strcmp((a), (b)) == 0, msg)

/* ── Test: basic symbols ──────────────────────────────────────────────── */

static void test_lex_basic_symbols(void) {
    printf("  test_lex_basic_symbols\n");
    const char *src = "{ } [ ] ( ) : , $ # @";
    xcdn_lexer_t lex;
    xcdn_error_t err;
    xcdn_lexer_init(&lex, src, strlen(src));

    xcdn_token_type_t expected[] = {
        XCDN_TOK_LBRACE, XCDN_TOK_RBRACE,
        XCDN_TOK_LBRACKET, XCDN_TOK_RBRACKET,
        XCDN_TOK_LPAREN, XCDN_TOK_RPAREN,
        XCDN_TOK_COLON, XCDN_TOK_COMMA,
        XCDN_TOK_DOLLAR, XCDN_TOK_HASH, XCDN_TOK_AT,
        XCDN_TOK_EOF,
    };

    for (int i = 0; i < 12; i++) {
        xcdn_token_t tok = xcdn_lexer_next(&lex, &err);
        ASSERT(!xcdn_error_is_set(&err), "no error expected");
        ASSERT_EQ_INT(tok.type, expected[i], "token type matches");
        xcdn_token_free(&tok);
    }
}

/* ── Test: identifiers and keywords ───────────────────────────────────── */

static void test_lex_ident_and_keywords(void) {
    printf("  test_lex_ident_and_keywords\n");
    const char *src = "true false null ident_1 another-ident";
    xcdn_lexer_t lex;
    xcdn_error_t err;
    xcdn_lexer_init(&lex, src, strlen(src));

    xcdn_token_t t1 = xcdn_lexer_next(&lex, &err);
    ASSERT_EQ_INT(t1.type, XCDN_TOK_TRUE, "true keyword");
    xcdn_token_free(&t1);

    xcdn_token_t t2 = xcdn_lexer_next(&lex, &err);
    ASSERT_EQ_INT(t2.type, XCDN_TOK_FALSE, "false keyword");
    xcdn_token_free(&t2);

    xcdn_token_t t3 = xcdn_lexer_next(&lex, &err);
    ASSERT_EQ_INT(t3.type, XCDN_TOK_NULL, "null keyword");
    xcdn_token_free(&t3);

    xcdn_token_t t4 = xcdn_lexer_next(&lex, &err);
    ASSERT_EQ_INT(t4.type, XCDN_TOK_IDENT, "ident type");
    ASSERT_EQ_STR(t4.data.string_val.str, "ident_1", "ident value");
    xcdn_token_free(&t4);

    xcdn_token_t t5 = xcdn_lexer_next(&lex, &err);
    ASSERT_EQ_INT(t5.type, XCDN_TOK_IDENT, "ident type");
    ASSERT_EQ_STR(t5.data.string_val.str, "another-ident", "ident with dash");
    xcdn_token_free(&t5);
}

/* ── Test: numbers ────────────────────────────────────────────────────── */

static void test_lex_numbers(void) {
    printf("  test_lex_numbers\n");
    const char *src = "0 -42 3.14 1e10 -2.5E-3 +7";
    xcdn_lexer_t lex;
    xcdn_error_t err;
    xcdn_lexer_init(&lex, src, strlen(src));

    xcdn_token_t t;

    t = xcdn_lexer_next(&lex, &err);
    ASSERT_EQ_INT(t.type, XCDN_TOK_INT, "0 is int");
    ASSERT_EQ_INT((int)t.data.int_val, 0, "0 value");
    xcdn_token_free(&t);

    t = xcdn_lexer_next(&lex, &err);
    ASSERT_EQ_INT(t.type, XCDN_TOK_INT, "-42 is int");
    ASSERT_EQ_INT((int)t.data.int_val, -42, "-42 value");
    xcdn_token_free(&t);

    t = xcdn_lexer_next(&lex, &err);
    ASSERT_EQ_INT(t.type, XCDN_TOK_FLOAT, "3.14 is float");
    ASSERT(fabs(t.data.float_val - 3.14) < 0.001, "3.14 value");
    xcdn_token_free(&t);

    t = xcdn_lexer_next(&lex, &err);
    ASSERT_EQ_INT(t.type, XCDN_TOK_FLOAT, "1e10 is float");
    xcdn_token_free(&t);

    t = xcdn_lexer_next(&lex, &err);
    ASSERT_EQ_INT(t.type, XCDN_TOK_FLOAT, "-2.5E-3 is float");
    xcdn_token_free(&t);

    t = xcdn_lexer_next(&lex, &err);
    ASSERT_EQ_INT(t.type, XCDN_TOK_INT, "+7 is int");
    ASSERT_EQ_INT((int)t.data.int_val, 7, "+7 value");
    xcdn_token_free(&t);
}

/* ── Test: strings ────────────────────────────────────────────────────── */

static void test_lex_strings(void) {
    printf("  test_lex_strings\n");
    const char *src = "\"hi\\n\" \"\"\"multi\nline\"\"\"";
    xcdn_lexer_t lex;
    xcdn_error_t err;
    xcdn_lexer_init(&lex, src, strlen(src));

    xcdn_token_t t1 = xcdn_lexer_next(&lex, &err);
    ASSERT_EQ_INT(t1.type, XCDN_TOK_STRING, "string type");
    ASSERT_EQ_STR(t1.data.string_val.str, "hi\\n", "string with escape");
    xcdn_token_free(&t1);

    xcdn_token_t t2 = xcdn_lexer_next(&lex, &err);
    ASSERT_EQ_INT(t2.type, XCDN_TOK_TRIPLE_STRING, "triple string type");
    ASSERT(strstr(t2.data.string_val.str, "multi\nline") != NULL,
           "triple string contains newline");
    xcdn_token_free(&t2);
}

/* ── Test: typed strings ──────────────────────────────────────────────── */

static void test_lex_typed_strings(void) {
    printf("  test_lex_typed_strings\n");
    const char *src =
        "d\"19.99\" b\"aGVsbG8=\" "
        "u\"550e8400-e29b-41d4-a716-446655440000\" "
        "t\"2020-01-01T00:00:00Z\" r\"PT30S\"";
    xcdn_lexer_t lex;
    xcdn_error_t err;
    xcdn_lexer_init(&lex, src, strlen(src));

    xcdn_token_t t;
    t = xcdn_lexer_next(&lex, &err);
    ASSERT_EQ_INT(t.type, XCDN_TOK_D_QUOTED, "decimal typed string");
    ASSERT_EQ_STR(t.data.string_val.str, "19.99", "decimal value");
    xcdn_token_free(&t);

    t = xcdn_lexer_next(&lex, &err);
    ASSERT_EQ_INT(t.type, XCDN_TOK_B_QUOTED, "bytes typed string");
    xcdn_token_free(&t);

    t = xcdn_lexer_next(&lex, &err);
    ASSERT_EQ_INT(t.type, XCDN_TOK_U_QUOTED, "uuid typed string");
    xcdn_token_free(&t);

    t = xcdn_lexer_next(&lex, &err);
    ASSERT_EQ_INT(t.type, XCDN_TOK_T_QUOTED, "datetime typed string");
    xcdn_token_free(&t);

    t = xcdn_lexer_next(&lex, &err);
    ASSERT_EQ_INT(t.type, XCDN_TOK_R_QUOTED, "duration typed string");
    xcdn_token_free(&t);
}

/* ── Test: comments are skipped ───────────────────────────────────────── */

static void test_lex_comments_skipped(void) {
    printf("  test_lex_comments_skipped\n");
    const char *src = "// cmt\n/* block */ ident // tail\n";
    xcdn_lexer_t lex;
    xcdn_error_t err;
    xcdn_lexer_init(&lex, src, strlen(src));

    xcdn_token_t t = xcdn_lexer_next(&lex, &err);
    ASSERT_EQ_INT(t.type, XCDN_TOK_IDENT, "ident after comments");
    ASSERT_EQ_STR(t.data.string_val.str, "ident", "ident value");
    xcdn_token_free(&t);

    t = xcdn_lexer_next(&lex, &err);
    ASSERT_EQ_INT(t.type, XCDN_TOK_EOF, "EOF after comments");
    xcdn_token_free(&t);
}

/* ── Test: invalid number error ───────────────────────────────────────── */

static void test_lex_invalid_number(void) {
    printf("  test_lex_invalid_number\n");
    const char *src = "-e";
    xcdn_lexer_t lex;
    xcdn_error_t err;
    xcdn_lexer_init(&lex, src, strlen(src));

    xcdn_lexer_next(&lex, &err);
    ASSERT(xcdn_error_is_set(&err), "error expected for -e");
    ASSERT_EQ_INT(err.kind, XCDN_ERR_INVALID_NUMBER, "invalid number error kind");
}

/* ── Test: line/column tracking ───────────────────────────────────────── */

static void test_lex_position_tracking(void) {
    printf("  test_lex_position_tracking\n");
    const char *src = "{\n  name\n}";
    xcdn_lexer_t lex;
    xcdn_error_t err;
    xcdn_lexer_init(&lex, src, strlen(src));

    xcdn_token_t t;
    t = xcdn_lexer_next(&lex, &err);
    ASSERT_EQ_INT(t.type, XCDN_TOK_LBRACE, "lbrace");
    ASSERT_EQ_INT((int)t.span.line, 1, "line 1");
    ASSERT_EQ_INT((int)t.span.column, 1, "col 1");
    xcdn_token_free(&t);

    t = xcdn_lexer_next(&lex, &err);
    ASSERT_EQ_INT(t.type, XCDN_TOK_IDENT, "name ident");
    ASSERT_EQ_INT((int)t.span.line, 2, "line 2");
    ASSERT_EQ_INT((int)t.span.column, 3, "col 3");
    xcdn_token_free(&t);

    t = xcdn_lexer_next(&lex, &err);
    ASSERT_EQ_INT(t.type, XCDN_TOK_RBRACE, "rbrace");
    ASSERT_EQ_INT((int)t.span.line, 3, "line 3");
    xcdn_token_free(&t);
}

/* ── Test: string escape sequences ────────────────────────────────────── */

static void test_lex_string_escapes(void) {
    printf("  test_lex_string_escapes\n");
    const char *src = "\"hello \\\"world\\\"\" \"tab\\there\" \"newline\\nend\"";
    xcdn_lexer_t lex;
    xcdn_error_t err;
    xcdn_lexer_init(&lex, src, strlen(src));

    xcdn_token_t t;
    t = xcdn_lexer_next(&lex, &err);
    ASSERT(!xcdn_error_is_set(&err), "no error");
    ASSERT_EQ_INT(t.type, XCDN_TOK_STRING, "string type");
    ASSERT(strstr(t.data.string_val.str, "hello \"world\"") != NULL,
           "escaped quotes in string");
    xcdn_token_free(&t);

    t = xcdn_lexer_next(&lex, &err);
    ASSERT_EQ_STR(t.data.string_val.str, "tab\\there", "tab escape preserved");
    xcdn_token_free(&t);

    t = xcdn_lexer_next(&lex, &err);
    ASSERT_EQ_STR(t.data.string_val.str, "newline\\nend", "newline escape preserved");
    xcdn_token_free(&t);
}

/* ── Test: unicode escape ─────────────────────────────────────────────── */

static void test_lex_unicode_escape(void) {
    printf("  test_lex_unicode_escape\n");
    const char *src = "\"\\u0041\"";
    xcdn_lexer_t lex;
    xcdn_error_t err;
    xcdn_lexer_init(&lex, src, strlen(src));

    xcdn_token_t t = xcdn_lexer_next(&lex, &err);
    ASSERT(!xcdn_error_is_set(&err), "no error");
    ASSERT_EQ_INT(t.type, XCDN_TOK_STRING, "string type");
    ASSERT_EQ_STR(t.data.string_val.str, "\\u0041", "unicode escape preserved");
    xcdn_token_free(&t);
}

/* ── Main ─────────────────────────────────────────────────────────────── */

int main(void) {
    printf("=== Lexer Tests ===\n");

    test_lex_basic_symbols();
    test_lex_ident_and_keywords();
    test_lex_numbers();
    test_lex_strings();
    test_lex_typed_strings();
    test_lex_comments_skipped();
    test_lex_invalid_number();
    test_lex_position_tracking();
    test_lex_string_escapes();
    test_lex_unicode_escape();

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
