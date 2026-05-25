/*
 * test_basic.c - Tests for the built-in Integer BASIC interpreter
 *
 * Validates the BASIC interpreter by feeding input lines and checking
 * the display output captured via the PIA display callback.
 *
 * Compile with:
 *   cc -std=c17 -Wall -I../src -o test_basic test_basic.c \
 *      ../src/basic.c ../src/cpu.c ../src/memory.c ../src/pia.c \
 *      ../src/roms_builtin.c ../src/log.c
 *
 * Output format: TAP (Test Anything Protocol)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "basic.h"
#include "memory.h"
#include "pia.h"
#include "roms_builtin.h"
#include "log.h"

/* ---------- Test infrastructure ---------- */

#define OUTPUT_BUF_SIZE  8192
#define MAX_STEPS        100000

/* Display capture buffer */
static char output_buf[OUTPUT_BUF_SIZE];
static int  output_pos;

/* TAP counters */
static int test_num;
static int tests_failed;

/* ---------- Display callback ---------- */

static void display_capture(uint8_t ch)
{
    if (output_pos < OUTPUT_BUF_SIZE - 1) {
        /* The PIA callback receives 7-bit ASCII values */
        output_buf[output_pos++] = (char)ch;
        output_buf[output_pos] = '\0';
    }
}

/* ---------- Helper functions ---------- */

/*
 * Feed a line of text to the BASIC interpreter, character by character,
 * followed by a carriage return (0x0D).
 */
static void feed_line(const char *line)
{
    while (*line) {
        basic_input_char((uint8_t)*line);
        line++;
    }
    basic_input_char(0x0D);
}

/*
 * Call basic_step() up to max_steps times, allowing the interpreter
 * to process queued input and produce output.
 */
static void run_steps(int max_steps)
{
    for (int i = 0; i < max_steps; i++) {
        basic_step();
    }
}

/*
 * Reset the output capture buffer.
 */
static void clear_output(void)
{
    output_pos = 0;
    memset(output_buf, 0, sizeof(output_buf));
}

/*
 * Check if the captured output contains a given substring.
 */
static bool output_contains(const char *str)
{
    return strstr(output_buf, str) != NULL;
}

/*
 * Check that the captured output does NOT contain a given substring.
 */
static bool output_not_contains(const char *str)
{
    return strstr(output_buf, str) == NULL;
}

/* ---------- System initialization ---------- */

/*
 * Initialize the full emulator and start BASIC from cold.
 * Clears the output buffer after the banner so tests start fresh
 * when reset_output_after_banner is true.
 */
static void system_init(void)
{
    mem_init(32);
    pia_init();
    roms_load_builtin();
    basic_init();
    pia_set_display_callback(display_capture);

    clear_output();
    basic_cold_start();
    /* Process the cold start banner output */
    run_steps(100);
}

/* ---------- TAP output helpers ---------- */

static void tap_ok(const char *desc)
{
    test_num++;
    printf("ok %d - %s\n", test_num, desc);
}

static void tap_not_ok(const char *desc, const char *detail)
{
    test_num++;
    tests_failed++;
    printf("not ok %d - %s\n", test_num, desc);
    if (detail) {
        printf("  # %s\n", detail);
    }
    /* Dump partial output for debugging */
    printf("  # Output buffer (%d chars): \"%.200s\"\n", output_pos, output_buf);
}

/* ---------- Test cases ---------- */

/* Test 1: Cold start banner contains "BASIC" */
static void test_cold_start_banner(void)
{
    system_init();
    /* The banner should have been printed during cold start */
    if (output_contains("BASIC")) {
        tap_ok("cold start banner contains BASIC");
    } else {
        tap_not_ok("cold start banner contains BASIC",
                   "expected 'BASIC' in banner output");
    }
}

/* Test 2: PRINT number */
static void test_print_number(void)
{
    system_init();
    clear_output();

    feed_line("PRINT 42");
    run_steps(MAX_STEPS);

    if (output_contains("42")) {
        tap_ok("PRINT 42 outputs 42");
    } else {
        tap_not_ok("PRINT 42 outputs 42",
                   "expected '42' in output");
    }
}

/* Test 3: PRINT arithmetic (addition) */
static void test_print_addition(void)
{
    system_init();
    clear_output();

    feed_line("PRINT 2+3");
    run_steps(MAX_STEPS);

    if (output_contains("5")) {
        tap_ok("PRINT 2+3 outputs 5");
    } else {
        tap_not_ok("PRINT 2+3 outputs 5",
                   "expected '5' in output");
    }
}

/* Test 4: PRINT multiplication */
static void test_print_multiplication(void)
{
    system_init();
    clear_output();

    feed_line("PRINT 6*7");
    run_steps(MAX_STEPS);

    if (output_contains("42")) {
        tap_ok("PRINT 6*7 outputs 42");
    } else {
        tap_not_ok("PRINT 6*7 outputs 42",
                   "expected '42' in output");
    }
}

/* Test 5: PRINT division (integer) */
static void test_print_division(void)
{
    system_init();
    clear_output();

    feed_line("PRINT 100/3");
    run_steps(MAX_STEPS);

    if (output_contains("33")) {
        tap_ok("PRINT 100/3 outputs 33");
    } else {
        tap_not_ok("PRINT 100/3 outputs 33",
                   "expected '33' in output");
    }
}

/* Test 6: PRINT negative number */
static void test_print_negative(void)
{
    system_init();
    clear_output();

    feed_line("PRINT -5");
    run_steps(MAX_STEPS);

    if (output_contains("-5")) {
        tap_ok("PRINT -5 outputs -5");
    } else {
        tap_not_ok("PRINT -5 outputs -5",
                   "expected '-5' in output");
    }
}

/* Test 7: PRINT string literal */
static void test_print_string(void)
{
    system_init();
    clear_output();

    feed_line("PRINT \"HELLO\"");
    run_steps(MAX_STEPS);

    if (output_contains("HELLO")) {
        tap_ok("PRINT \"HELLO\" outputs HELLO");
    } else {
        tap_not_ok("PRINT \"HELLO\" outputs HELLO",
                   "expected 'HELLO' in output");
    }
}

/* Test 8: Variable assignment and PRINT */
static void test_variable_assignment(void)
{
    system_init();

    feed_line("LET A=10");
    run_steps(MAX_STEPS);
    clear_output();

    feed_line("PRINT A");
    run_steps(MAX_STEPS);

    if (output_contains("10")) {
        tap_ok("LET A=10 then PRINT A outputs 10");
    } else {
        tap_not_ok("LET A=10 then PRINT A outputs 10",
                   "expected '10' in output");
    }
}

/* Test 9: Multiple variables */
static void test_multiple_variables(void)
{
    system_init();

    feed_line("A=5");
    run_steps(MAX_STEPS);
    feed_line("B=3");
    run_steps(MAX_STEPS);
    clear_output();

    feed_line("PRINT A+B");
    run_steps(MAX_STEPS);

    if (output_contains("8")) {
        tap_ok("A=5, B=3, PRINT A+B outputs 8");
    } else {
        tap_not_ok("A=5, B=3, PRINT A+B outputs 8",
                   "expected '8' in output");
    }
}

/* Test 10: IF/THEN true condition */
static void test_if_then_true(void)
{
    system_init();

    feed_line("A=5");
    run_steps(MAX_STEPS);
    clear_output();

    feed_line("IF A=5 THEN PRINT \"YES\"");
    run_steps(MAX_STEPS);

    if (output_contains("YES")) {
        tap_ok("IF A=5 THEN PRINT YES (true condition)");
    } else {
        tap_not_ok("IF A=5 THEN PRINT YES (true condition)",
                   "expected 'YES' in output");
    }
}

/* Test 11: IF/THEN false condition
 * When the condition is false, PRINT should not execute.
 * We use a variable to verify: if the false branch ran, it would set B=99.
 * Then we check that B still has its default value (0). */
static void test_if_then_false(void)
{
    system_init();

    feed_line("A=3");
    run_steps(MAX_STEPS);
    feed_line("B=0");
    run_steps(MAX_STEPS);

    /* IF condition is false, so the THEN clause should not execute */
    feed_line("IF A=5 THEN B=99");
    run_steps(MAX_STEPS);
    clear_output();

    feed_line("PRINT B");
    run_steps(MAX_STEPS);

    /* B should still be 0 since the IF was false */
    if (output_contains("0") && output_not_contains("99")) {
        tap_ok("IF A=5 THEN B=99 (false condition, B remains 0)");
    } else {
        tap_not_ok("IF A=5 THEN B=99 (false condition, B remains 0)",
                   "expected '0' (not '99') in PRINT B output");
    }
}

/* Test 12: Line storage and RUN */
static void test_line_storage_and_run(void)
{
    system_init();

    feed_line("10 PRINT \"HELLO\"");
    run_steps(MAX_STEPS);
    clear_output();

    feed_line("RUN");
    run_steps(MAX_STEPS);

    if (output_contains("HELLO")) {
        tap_ok("store line 10 PRINT HELLO then RUN outputs HELLO");
    } else {
        tap_not_ok("store line 10 PRINT HELLO then RUN outputs HELLO",
                   "expected 'HELLO' in output after RUN");
    }
}

/* Test 13: FOR/NEXT loop */
static void test_for_next_loop(void)
{
    system_init();

    feed_line("10 FOR I=1 TO 3");
    run_steps(MAX_STEPS);
    feed_line("20 PRINT I");
    run_steps(MAX_STEPS);
    feed_line("30 NEXT I");
    run_steps(MAX_STEPS);
    clear_output();

    feed_line("RUN");
    run_steps(MAX_STEPS);

    bool has_1 = output_contains("1");
    bool has_2 = output_contains("2");
    bool has_3 = output_contains("3");

    if (has_1 && has_2 && has_3) {
        tap_ok("FOR I=1 TO 3 loop prints 1, 2, 3");
    } else {
        char detail[128];
        snprintf(detail, sizeof(detail),
                 "expected 1,2,3 in output; has_1=%d has_2=%d has_3=%d",
                 has_1, has_2, has_3);
        tap_not_ok("FOR I=1 TO 3 loop prints 1, 2, 3", detail);
    }
}

/* Test 14: GOTO skips lines */
static void test_goto(void)
{
    system_init();

    feed_line("10 PRINT \"A\"");
    run_steps(MAX_STEPS);
    feed_line("20 GOTO 40");
    run_steps(MAX_STEPS);
    feed_line("30 PRINT \"B\"");
    run_steps(MAX_STEPS);
    feed_line("40 PRINT \"C\"");
    run_steps(MAX_STEPS);
    clear_output();

    feed_line("RUN");
    run_steps(MAX_STEPS);

    bool has_a = output_contains("A");
    bool has_c = output_contains("C");
    /* Line 30 should be skipped: "B" should not appear alone.
     * Note: we cannot just check for "B" not being present since "B" could
     * appear as part of "BREAK" or other messages. Instead we verify A and C
     * are present and that B is not printed between them. */
    /* Simple approach: check that the output doesn't have a standalone B line */
    char *b_pos = strstr(output_buf, "B");
    bool b_between = false;
    if (b_pos != NULL) {
        /* Check if B appears after A and before C in a way that suggests it was printed */
        char *a_pos = strstr(output_buf, "A");
        char *c_pos = strstr(output_buf, "C");
        if (a_pos && c_pos && b_pos > a_pos && b_pos < c_pos) {
            b_between = true;
        }
    }

    if (has_a && has_c && !b_between) {
        tap_ok("GOTO 40 skips line 30 (A and C printed, B skipped)");
    } else {
        char detail[128];
        snprintf(detail, sizeof(detail),
                 "has_a=%d has_c=%d b_between=%d", has_a, has_c, b_between);
        tap_not_ok("GOTO 40 skips line 30 (A and C printed, B skipped)", detail);
    }
}

/* Test 15: GOSUB/RETURN */
static void test_gosub_return(void)
{
    system_init();

    feed_line("10 GOSUB 100");
    run_steps(MAX_STEPS);
    feed_line("20 END");
    run_steps(MAX_STEPS);
    feed_line("100 PRINT \"SUB\"");
    run_steps(MAX_STEPS);
    feed_line("110 RETURN");
    run_steps(MAX_STEPS);
    clear_output();

    feed_line("RUN");
    run_steps(MAX_STEPS);

    if (output_contains("SUB")) {
        tap_ok("GOSUB 100 / RETURN prints SUB");
    } else {
        tap_not_ok("GOSUB 100 / RETURN prints SUB",
                   "expected 'SUB' in output");
    }
}

/* Test 16: PEEK function */
static void test_peek_function(void)
{
    system_init();

    /* Write a known value to a specific memory address for PEEK to read */
    mem_write(0x0400, 0xAB);  /* 171 decimal */
    clear_output();

    feed_line("PRINT PEEK(1024)");
    run_steps(MAX_STEPS);

    if (output_contains("171")) {
        tap_ok("PRINT PEEK(1024) outputs 171 (value $AB at $0400)");
    } else {
        tap_not_ok("PRINT PEEK(1024) outputs 171 (value $AB at $0400)",
                   "expected '171' in output");
    }
}

/* Test 17: LIST command */
static void test_list_command(void)
{
    system_init();

    feed_line("10 PRINT \"X\"");
    run_steps(MAX_STEPS);
    clear_output();

    feed_line("LIST");
    run_steps(MAX_STEPS);

    if (output_contains("10")) {
        tap_ok("LIST shows line number 10");
    } else {
        tap_not_ok("LIST shows line number 10",
                   "expected '10' in LIST output");
    }
}

/* Test 18: NEW command clears program */
static void test_new_command(void)
{
    system_init();

    feed_line("10 PRINT \"X\"");
    run_steps(MAX_STEPS);
    feed_line("NEW");
    run_steps(MAX_STEPS);
    clear_output();

    feed_line("LIST");
    run_steps(MAX_STEPS);

    if (output_not_contains("PRINT")) {
        tap_ok("NEW clears program (LIST shows no PRINT)");
    } else {
        tap_not_ok("NEW clears program (LIST shows no PRINT)",
                   "expected no 'PRINT' in LIST output after NEW");
    }
}

/* Test 19: Syntax error */
static void test_syntax_error(void)
{
    system_init();
    clear_output();

    feed_line("PRINT 5+");
    run_steps(MAX_STEPS);

    if (output_contains("ERROR")) {
        tap_ok("PRINT 5+ produces ERROR");
    } else {
        tap_not_ok("PRINT 5+ produces ERROR",
                   "expected 'ERROR' in output for syntax error");
    }
}

/* Test 20: ABS function */
static void test_abs_function(void)
{
    system_init();
    clear_output();

    feed_line("PRINT ABS(-7)");
    run_steps(MAX_STEPS);

    if (output_contains("7")) {
        tap_ok("PRINT ABS(-7) outputs 7");
    } else {
        tap_not_ok("PRINT ABS(-7) outputs 7",
                   "expected '7' in output");
    }
}

/* Test 21: String variable */
static void test_string_variable(void)
{
    system_init();

    feed_line("A$=\"WORLD\"");
    run_steps(MAX_STEPS);
    clear_output();

    feed_line("PRINT A$");
    run_steps(MAX_STEPS);

    if (output_contains("WORLD")) {
        tap_ok("A$=\"WORLD\" then PRINT A$ outputs WORLD");
    } else {
        tap_not_ok("A$=\"WORLD\" then PRINT A$ outputs WORLD",
                   "expected 'WORLD' in output");
    }
}

/* Test 22: Comparison operator */
static void test_comparison(void)
{
    system_init();
    clear_output();

    feed_line("PRINT 5>3");
    run_steps(MAX_STEPS);

    if (output_contains("1")) {
        tap_ok("PRINT 5>3 outputs 1 (true)");
    } else {
        tap_not_ok("PRINT 5>3 outputs 1 (true)",
                   "expected '1' in output");
    }
}

/* ---------- Main ---------- */

int main(void)
{
    /* Suppress log output during tests */
    log_init(LOG_LEVEL_ERROR, NULL);

    /* TAP header */
    printf("TAP version 13\n");
    printf("1..22\n");

    test_num = 0;
    tests_failed = 0;

    /* Run all test cases */
    test_cold_start_banner();       /* 1 */
    test_print_number();            /* 2 */
    test_print_addition();          /* 3 */
    test_print_multiplication();    /* 4 */
    test_print_division();          /* 5 */
    test_print_negative();          /* 6 */
    test_print_string();            /* 7 */
    test_variable_assignment();     /* 8 */
    test_multiple_variables();      /* 9 */
    test_if_then_true();            /* 10 */
    test_if_then_false();           /* 11 */
    test_line_storage_and_run();    /* 12 */
    test_for_next_loop();           /* 13 */
    test_goto();                    /* 14 */
    test_gosub_return();            /* 15 */
    test_peek_function();           /* 16 */
    test_list_command();            /* 17 */
    test_new_command();             /* 18 */
    test_syntax_error();            /* 19 */
    test_abs_function();            /* 20 */
    test_string_variable();         /* 21 */
    test_comparison();              /* 22 */

    /* Summary */
    printf("# tests:  %d\n", test_num);
    printf("# passed: %d\n", test_num - tests_failed);
    printf("# failed: %d\n", tests_failed);

    log_shutdown();

    return (tests_failed > 0) ? 1 : 0;
}
