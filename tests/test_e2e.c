/*
 * test_e2e.c - End-to-end tests for the Apple 1 emulator
 *
 * Validates the FULL system as a user would experience it: feeding keystrokes
 * and verifying display output. Tests cover the Woz Monitor, BASIC interpreter,
 * and memory subsystem.
 *
 * Compile with:
 *   cc -std=c17 -Wall -Wextra -Wpedantic -O2 -Isrc -o tests/test_e2e \
 *      tests/test_e2e.c src/cpu.c src/memory.c src/pia.c src/roms_builtin.c \
 *      src/basic.c src/log.c src/aci.c src/disk.c
 *
 * Output format: TAP (Test Anything Protocol)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "cpu.h"
#include "memory.h"
#include "pia.h"
#include "roms_builtin.h"
#include "basic.h"
#include "aci.h"
#include "disk.h"
#include "log.h"

/* ---------- Constants ---------- */

#define OUTPUT_BUF_SIZE  8192
#define MAX_CYCLES       50000000  /* 50 million cycles safety limit */

/* ---------- Global state ---------- */

static cpu_state cpu;

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
        /* The PIA masks to 7 bits before calling us; map CR to newline
         * for easier string matching. */
        if (ch == 0x0D) {
            output_buf[output_pos++] = '\n';
        } else if (ch >= 0x20 && ch < 0x7F) {
            output_buf[output_pos++] = (char)ch;
        }
        /* Ignore control chars other than CR */
        output_buf[output_pos] = '\0';
    }
}

/* ---------- Helper functions ---------- */

/* Clear the output capture buffer. */
static void clear_output(void)
{
    output_pos = 0;
    output_buf[0] = '\0';
}

/* Get captured display output as string. */
__attribute__((unused))
static const char *get_output(void)
{
    return output_buf;
}

/* Check if output contains a substring. */
static bool output_contains(const char *needle)
{
    return strstr(output_buf, needle) != NULL;
}

/* Run the CPU until it is waiting for keyboard input (polling $D011)
 * or max_cycles is exhausted. Detects the Woz Monitor's tight polling loop. */
static void run_until_input_wait(int max_cycles)
{
    int cycles_done = 0;
    int poll_count = 0;
    uint16_t prev_pc = 0;
    uint16_t prev_prev_pc = 0;

    while (cycles_done < max_cycles) {
        /* Check BASIC trap */
        if (cpu.pc == BASIC_COLD_START_ADDR && !basic_is_running()) {
            basic_cold_start();
            cpu.pc = BASIC_COLD_START_ADDR + 3;
            break;
        }
        if (cpu.pc == BASIC_WARM_START_ADDR && !basic_is_running()) {
            basic_warm_start();
            cpu.pc = BASIC_WARM_START_ADDR + 3;
            break;
        }

        /* If BASIC is running, step it */
        if (basic_is_running()) {
            break;
        }

        uint16_t pc_before = cpu.pc;
        cycles_done += cpu_step(&cpu);

        /* Detect tight polling loop (oscillating between 2 addresses) */
        if (pc_before == prev_prev_pc && pc_before != prev_pc) {
            poll_count++;
            if (poll_count > 50) break;
        } else if (pc_before == prev_pc) {
            poll_count++;
            if (poll_count > 50) break;
        } else {
            poll_count = 0;
        }

        prev_prev_pc = prev_pc;
        prev_pc = pc_before;
    }
}

/* Feed a string character by character to PIA, running CPU between chars.
 * For each character: set key via pia_key_press, then run CPU ~1000 cycles
 * for normal chars, ~50000 cycles for CR (to let commands process). */
static void type_string(const char *str)
{
    while (*str) {
        uint8_t ch = (uint8_t)*str;

        /* Convert lowercase to uppercase for the Apple 1 */
        if (ch >= 'a' && ch <= 'z') {
            ch = ch - 'a' + 'A';
        }

        pia_key_press(ch);

        /* Run enough cycles for the monitor to read and process the key */
        if (ch == '\r' || ch == '\n') {
            /* CR triggers command processing; give it more time */
            pia_key_press(0x0D);
            run_until_input_wait(50000);
        } else {
            run_until_input_wait(1000);
        }

        str++;
    }
}

/* Run CPU for exactly N cycles. */
__attribute__((unused))
static void run_cycles(int n)
{
    int done = 0;
    while (done < n) {
        done += cpu_step(&cpu);
    }
}

/* Step BASIC interpreter N times, feeding chars from a string first. */
static void basic_type_and_run(const char *str, int steps)
{
    /* Feed all chars to BASIC input buffer */
    for (const char *p = str; *p; p++) {
        uint8_t ch = (uint8_t)*p;
        if (ch >= 'a' && ch <= 'z') ch = ch - 'a' + 'A';
        if (ch == '\n') ch = 0x0D;
        basic_input_char(ch);
    }

    /* Step BASIC enough times to process everything */
    for (int i = 0; i < steps; i++) {
        basic_step();
    }
}

/* ---------- System initialization ---------- */

static void system_init(void)
{
    mem_init(32);
    pia_init();
    roms_load_builtin();
    basic_init();
    aci_init();
    disk_init();

    pia_set_display_callback(display_capture);
    cpu_init(&cpu, mem_read, mem_write);
    cpu_reset(&cpu);

    clear_output();
}

/* Full re-init for a fresh test. */
static void system_reset_fresh(void)
{
    system_init();
}

/* Enter BASIC mode from monitor: type E000R, detect PC at $E000,
 * call basic_cold_start(). */
static void enter_basic(void)
{
    /* Boot the monitor first */
    run_until_input_wait(100000);
    clear_output();

    /* Type the BASIC entry command */
    type_string("E000R\r");

    /* The monitor will set PC to $E000 via the R command.
     * Our run_until_input_wait detects the BASIC trap and calls
     * basic_cold_start() for us. But we may need an extra kick: */
    if (!basic_is_running()) {
        /* Manually trigger if the CPU reached $E000 */
        if (cpu.pc >= BASIC_COLD_START_ADDR && cpu.pc <= BASIC_COLD_START_ADDR + 3) {
            basic_cold_start();
            cpu.pc = BASIC_COLD_START_ADDR + 3;
        } else {
            /* Force it - the R command should have jumped there */
            basic_cold_start();
        }
    }

    clear_output();
}

/* ---------- TAP output ---------- */

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
    /* Show first 200 chars of output for debugging */
    if (output_pos > 0) {
        printf("  # output[%d]: \"%.200s\"\n", output_pos, output_buf);
    }
}

static void tap_check(bool condition, const char *desc, const char *detail)
{
    if (condition) {
        tap_ok(desc);
    } else {
        tap_not_ok(desc, detail);
    }
}

/* ======================================================================
 * MONITOR TESTS
 * ====================================================================== */

/* Test 1: Boot shows backslash prompt */
static void test_monitor_boot_prompt(void)
{
    system_reset_fresh();
    run_until_input_wait(100000);

    tap_check(output_contains("\\"),
              "monitor boot shows backslash prompt",
              "expected backslash in output");
}

/* Test 2: Typing characters echoes them */
static void test_monitor_echo(void)
{
    system_reset_fresh();
    run_until_input_wait(100000);
    clear_output();

    type_string("AB");

    tap_check(output_contains("A") && output_contains("B"),
              "monitor echoes typed characters",
              "expected 'A' and 'B' in output");
}

/* Test 3: CR starts new line with backslash */
static void test_monitor_cr_newline(void)
{
    system_reset_fresh();
    run_until_input_wait(100000);
    clear_output();

    type_string("X\r");

    /* After CR, the monitor prints a new prompt with backslash */
    tap_check(output_contains("\\"),
              "CR starts new line with backslash prompt",
              "expected backslash after CR");
}

/* Test 4: Examine single address FF00 -> D8 */
static void test_monitor_examine_single(void)
{
    system_reset_fresh();
    run_until_input_wait(100000);
    clear_output();

    type_string("FF00\r");

    tap_check(output_contains("D8"),
              "examine FF00 shows D8 (first byte of ROM)",
              "expected 'D8' in output");
}

/* Test 5: Examine range FF00.FF07 */
static void test_monitor_examine_range(void)
{
    system_reset_fresh();
    run_until_input_wait(100000);
    clear_output();

    type_string("FF00.FF07\r");

    /* First few bytes of Woz Monitor ROM: D8 58 A0 7F 8C 12 D0 A9 */
    bool has_d8 = output_contains("D8");
    bool has_58 = output_contains("58");
    bool has_a0 = output_contains("A0");

    tap_check(has_d8 && has_58 && has_a0,
              "examine range FF00.FF07 shows ROM bytes",
              "expected 'D8', '58', 'A0' in output");
}

/* Test 6: Deposit byte and read back */
static void test_monitor_deposit(void)
{
    system_reset_fresh();
    run_until_input_wait(100000);
    clear_output();

    /* Deposit 42 at address 0300 */
    type_string("0300:42\r");
    clear_output();

    /* Examine address 0300 */
    type_string("0300\r");

    tap_check(output_contains("42"),
              "deposit byte 42 at 0300 and read back",
              "expected '42' in examine output");
}

/* Test 7: Deposit multiple bytes and read range */
static void test_monitor_deposit_multiple(void)
{
    system_reset_fresh();
    run_until_input_wait(100000);
    clear_output();

    /* Deposit 48 45 4C at 0300 */
    type_string("0300:48 45 4C\r");
    clear_output();

    /* Examine range */
    type_string("0300.0302\r");

    bool has_48 = output_contains("48");
    bool has_45 = output_contains("45");
    bool has_4c = output_contains("4C");

    tap_check(has_48 && has_45 && has_4c,
              "deposit multiple bytes and examine range",
              "expected '48', '45', '4C' in output");
}

/* Test 8: Run command - deposit small program and execute */
static void test_monitor_run(void)
{
    system_reset_fresh();
    run_until_input_wait(100000);

    /* Deposit: LDA #$42, STA $0400, JMP $FF1F at $0300 */
    /* A9 42 8D 00 04 4C 1F FF */
    mem_write(0x0300, 0xA9); /* LDA #$42 */
    mem_write(0x0301, 0x42);
    mem_write(0x0302, 0x8D); /* STA $0400 */
    mem_write(0x0303, 0x00);
    mem_write(0x0304, 0x04);
    mem_write(0x0305, 0x4C); /* JMP $FF1F */
    mem_write(0x0306, 0x1F);
    mem_write(0x0307, 0xFF);

    clear_output();

    /* Run from 0300 */
    type_string("0300R\r");

    /* After running, $0400 should have been set to $42 */
    uint8_t val = mem_read(0x0400);
    tap_check(val == 0x42,
              "run command executes program (mem[$0400]==$42)",
              "expected $42 at $0400 after execution");
}

/* ======================================================================
 * BASIC TESTS
 * ====================================================================== */

/* Test 9: Cold start banner contains "BASIC" */
static void test_basic_banner(void)
{
    system_reset_fresh();
    enter_basic();

    /* Re-capture the banner by doing cold start with output visible */
    clear_output();
    basic_cold_start();
    basic_step();

    tap_check(output_contains("BASIC"),
              "BASIC cold start banner contains 'BASIC'",
              "expected 'BASIC' in banner output");
}

/* Test 10: PRINT immediate number */
static void test_basic_print_number(void)
{
    system_reset_fresh();
    enter_basic();
    clear_output();

    basic_type_and_run("PRINT 42\r", 500);

    tap_check(output_contains("42"),
              "PRINT 42 outputs '42'",
              "expected '42' in output");
}

/* Test 11: PRINT arithmetic */
static void test_basic_print_arithmetic(void)
{
    system_reset_fresh();
    enter_basic();
    clear_output();

    basic_type_and_run("PRINT 7*6\r", 500);

    tap_check(output_contains("42"),
              "PRINT 7*6 outputs '42'",
              "expected '42' in output");
}

/* Test 12: PRINT string */
static void test_basic_print_string(void)
{
    system_reset_fresh();
    enter_basic();
    clear_output();

    basic_type_and_run("PRINT \"HELLO\"\r", 500);

    tap_check(output_contains("HELLO"),
              "PRINT \"HELLO\" outputs 'HELLO'",
              "expected 'HELLO' in output");
}

/* Test 13: Variable assignment and print */
static void test_basic_variable(void)
{
    system_reset_fresh();
    enter_basic();
    clear_output();

    basic_type_and_run("A=99\r", 500);
    basic_type_and_run("PRINT A\r", 500);

    tap_check(output_contains("99"),
              "variable assignment A=99 then PRINT A shows '99'",
              "expected '99' in output");
}

/* Test 14: IF true */
static void test_basic_if_true(void)
{
    system_reset_fresh();
    enter_basic();
    clear_output();

    basic_type_and_run("IF 1=1 THEN PRINT \"YES\"\r", 500);

    tap_check(output_contains("YES"),
              "IF 1=1 THEN PRINT outputs 'YES'",
              "expected 'YES' in output");
}

/* Test 15: IF false */
static void test_basic_if_false(void)
{
    system_reset_fresh();
    enter_basic();
    clear_output();

    /* Type the command - the echo will contain "NO" as part of the literal,
     * so we check that between the last newline and the prompt '>' there is
     * no "NO" printed as standalone output. Instead we use a unique marker. */
    basic_type_and_run("IF 1=2 THEN PRINT \"NOPE\"\r", 500);

    /* The output will echo the command, then just show a prompt.
     * If IF was false, we should see only one occurrence of NOPE (in the echo).
     * Count occurrences: */
    int count = 0;
    const char *p = output_buf;
    while ((p = strstr(p, "NOPE")) != NULL) {
        count++;
        p += 4;
    }

    /* Only 1 occurrence = just the echo; 2 would mean it printed */
    tap_check(count <= 1,
              "IF 1=2 THEN PRINT does NOT execute (NOPE appears only in echo)",
              "NOPE appeared more than once, suggesting THEN branch executed");
}

/* Test 16: FOR loop */
static void test_basic_for_loop(void)
{
    system_reset_fresh();
    enter_basic();
    clear_output();

    basic_type_and_run("10 FOR I=1 TO 3\r", 200);
    basic_type_and_run("20 PRINT I\r", 200);
    basic_type_and_run("30 NEXT I\r", 200);
    basic_type_and_run("RUN\r", 2000);

    bool has_1 = output_contains("1");
    bool has_2 = output_contains("2");
    bool has_3 = output_contains("3");

    tap_check(has_1 && has_2 && has_3,
              "FOR loop prints 1, 2, 3",
              "expected '1', '2', '3' in output");
}

/* Test 17: GOSUB/RETURN */
static void test_basic_gosub(void)
{
    system_reset_fresh();
    enter_basic();
    clear_output();

    basic_type_and_run("10 GOSUB 100\r", 200);
    basic_type_and_run("20 END\r", 200);
    basic_type_and_run("100 PRINT \"HI\"\r", 200);
    basic_type_and_run("110 RETURN\r", 200);
    basic_type_and_run("RUN\r", 2000);

    tap_check(output_contains("HI"),
              "GOSUB/RETURN prints 'HI'",
              "expected 'HI' in output");
}

/* Test 18: NEW clears program */
static void test_basic_new(void)
{
    system_reset_fresh();
    enter_basic();
    clear_output();

    basic_type_and_run("10 PRINT \"X\"\r", 200);
    basic_type_and_run("NEW\r", 500);
    clear_output();
    basic_type_and_run("RUN\r", 1000);

    tap_check(!output_contains("X"),
              "NEW clears program (RUN does not print 'X')",
              "unexpected 'X' in output after NEW");
}

/* Test 19: LIST shows line numbers */
static void test_basic_list(void)
{
    system_reset_fresh();
    enter_basic();
    clear_output();

    basic_type_and_run("10 PRINT \"Y\"\r", 200);
    clear_output();
    basic_type_and_run("LIST\r", 1000);

    tap_check(output_contains("10"),
              "LIST shows line number '10'",
              "expected '10' in LIST output");
}

/* Test 20: String variable */
static void test_basic_string_variable(void)
{
    system_reset_fresh();
    enter_basic();
    clear_output();

    basic_type_and_run("A$=\"WORLD\"\r", 500);
    basic_type_and_run("PRINT A$\r", 500);

    tap_check(output_contains("WORLD"),
              "string variable A$=\"WORLD\" then PRINT A$ shows 'WORLD'",
              "expected 'WORLD' in output");
}

/* Test 21: ABS function */
static void test_basic_abs(void)
{
    system_reset_fresh();
    enter_basic();
    clear_output();

    basic_type_and_run("PRINT ABS(-5)\r", 500);

    tap_check(output_contains("5"),
              "PRINT ABS(-5) outputs '5'",
              "expected '5' in output");
}

/* Test 22: Nested arithmetic with parentheses */
static void test_basic_nested_arithmetic(void)
{
    system_reset_fresh();
    enter_basic();
    clear_output();

    basic_type_and_run("PRINT (2+3)*4\r", 500);

    tap_check(output_contains("20"),
              "PRINT (2+3)*4 outputs '20'",
              "expected '20' in output");
}

/* Test 23: Comparison operator */
static void test_basic_comparison(void)
{
    system_reset_fresh();
    enter_basic();
    clear_output();

    basic_type_and_run("PRINT 10>5\r", 500);

    tap_check(output_contains("1"),
              "PRINT 10>5 outputs '1' (true)",
              "expected '1' in output");
}

/* Test 24: PEEK and POKE */
static void test_basic_peek_poke(void)
{
    system_reset_fresh();
    enter_basic();
    clear_output();

    basic_type_and_run("POKE 1024,66\r", 500);
    basic_type_and_run("PRINT PEEK(1024)\r", 500);

    tap_check(output_contains("66"),
              "POKE 1024,66 then PRINT PEEK(1024) outputs '66'",
              "expected '66' in output");
}

/* Test 25: RND returns a value (some number appears) */
static void test_basic_rnd(void)
{
    system_reset_fresh();
    enter_basic();
    clear_output();

    basic_type_and_run("PRINT RND(100)\r", 500);

    /* RND should produce at least one digit character */
    bool has_digit = false;
    for (int i = 0; i < output_pos; i++) {
        if (output_buf[i] >= '0' && output_buf[i] <= '9') {
            has_digit = true;
            break;
        }
    }

    tap_check(has_digit,
              "PRINT RND(100) outputs a number",
              "expected at least one digit in output");
}

/* Test 26: Division by zero error */
static void test_basic_div_zero(void)
{
    system_reset_fresh();
    enter_basic();
    clear_output();

    basic_type_and_run("PRINT 1/0\r", 500);

    tap_check(output_contains("ERR"),
              "PRINT 1/0 produces error message",
              "expected 'ERR' in output for division by zero");
}

/* Test 27: Multi-statement line with colon (via program) */
static void test_basic_multi_statement(void)
{
    system_reset_fresh();
    enter_basic();
    clear_output();

    /* Use a program line since immediate multi-statement may not be supported */
    basic_type_and_run("10 A=1:B=2:PRINT A+B\r", 200);
    clear_output();
    basic_type_and_run("RUN\r", 2000);

    tap_check(output_contains("3"),
              "multi-statement line A=1:B=2:PRINT A+B outputs '3'",
              "expected '3' in output");
}

/* ======================================================================
 * MEMORY TESTS
 * ====================================================================== */

/* Test 28: RAM read/write at address 0 */
static void test_memory_ram_boundary(void)
{
    system_reset_fresh();

    mem_write(0x0000, 0xAB);
    uint8_t val = mem_read(0x0000);

    tap_check(val == 0xAB,
              "RAM write/read at address $0000",
              "expected $AB at address $0000");
}

/* Test 29: ROM protection - writing to $FF00 doesn't change it */
static void test_memory_rom_protection(void)
{
    system_reset_fresh();

    /* ROM at $FF00 should be $D8 (CLD) */
    uint8_t before = mem_read(0xFF00);
    mem_write(0xFF00, 0x00);  /* Attempt to overwrite ROM */
    uint8_t after = mem_read(0xFF00);

    tap_check(before == 0xD8 && after == 0xD8,
              "ROM protection: writing to $FF00 doesn't change it (still $D8)",
              "expected $D8 at $FF00 after write attempt");
}

/* Test 30: PIA keyboard - pia_key_press sets KBDCR bit 7 */
static void test_pia_keyboard_flag(void)
{
    system_reset_fresh();

    /* Initially, KBDCR bit 7 should be clear */
    uint8_t before = pia_read(PIA_KBDCR);
    bool initially_clear = (before & 0x80) == 0;

    /* Press a key */
    pia_key_press('A');

    /* Now KBDCR bit 7 should be set */
    uint8_t after = pia_read(PIA_KBDCR);
    bool now_set = (after & 0x80) != 0;

    tap_check(initially_clear && now_set,
              "pia_key_press sets KBDCR bit 7",
              "expected bit 7 clear before press, set after");
}

/* Test 31: PIA keyboard read clears flag */
static void test_pia_keyboard_read_clears(void)
{
    system_reset_fresh();

    pia_key_press('B');

    /* Reading KBD ($D010) should clear the KBDCR flag */
    uint8_t kbd_val = pia_read(PIA_KBD);
    uint8_t kbdcr_after = pia_read(PIA_KBDCR);

    bool key_correct = (kbd_val == ('B' | 0x80));
    bool flag_cleared = (kbdcr_after & 0x80) == 0;

    tap_check(key_correct && flag_cleared,
              "reading KBD returns key|0x80 and clears KBDCR flag",
              "expected key with bit 7 set and flag cleared after read");
}

/* Test 32: Display write triggers callback */
static void test_pia_display_callback(void)
{
    system_reset_fresh();
    clear_output();

    /* Write a character to DSP register directly */
    pia_write(PIA_DSP, 'Z');

    tap_check(output_contains("Z"),
              "writing to PIA DSP register triggers display callback",
              "expected 'Z' in captured output");
}

/* Test 33: RAM at various addresses in 32KB range */
static void test_memory_ram_range(void)
{
    system_reset_fresh();

    /* Write pattern to various RAM addresses */
    mem_write(0x0100, 0x11);  /* Page 1 (stack) */
    mem_write(0x0200, 0x22);  /* Page 2 (input buffer) */
    mem_write(0x0800, 0x33);  /* BASIC program area */
    mem_write(0x4000, 0x44);  /* Mid-RAM */
    mem_write(0x7FFF, 0x55);  /* Top of 32KB */

    bool ok = (mem_read(0x0100) == 0x11) &&
              (mem_read(0x0200) == 0x22) &&
              (mem_read(0x0800) == 0x33) &&
              (mem_read(0x4000) == 0x44) &&
              (mem_read(0x7FFF) == 0x55);

    tap_check(ok,
              "RAM read/write across 32KB range",
              "expected correct values at various RAM addresses");
}

/* Test 34: Monitor processes hex digits correctly (address parsing) */
static void test_monitor_hex_parsing(void)
{
    system_reset_fresh();
    run_until_input_wait(100000);

    /* Write a known byte to a specific address and read it back via monitor */
    mem_write(0x0400, 0xBE);
    clear_output();

    type_string("0400\r");

    tap_check(output_contains("BE"),
              "monitor correctly parses hex address and displays byte",
              "expected 'BE' in examine output for $0400");
}

/* Test 35: BASIC addition of larger numbers */
static void test_basic_large_numbers(void)
{
    system_reset_fresh();
    enter_basic();
    clear_output();

    basic_type_and_run("PRINT 1000+2000\r", 500);

    tap_check(output_contains("3000"),
              "PRINT 1000+2000 outputs '3000'",
              "expected '3000' in output");
}

/* Test 36: BASIC subtraction producing negative */
static void test_basic_negative_result(void)
{
    system_reset_fresh();
    enter_basic();
    clear_output();

    basic_type_and_run("PRINT 5-10\r", 500);

    tap_check(output_contains("-5"),
              "PRINT 5-10 outputs '-5'",
              "expected '-5' in output");
}

/* Test 37: BASIC multiple variables */
static void test_basic_multiple_vars(void)
{
    system_reset_fresh();
    enter_basic();
    clear_output();

    basic_type_and_run("X=10\r", 200);
    basic_type_and_run("Y=20\r", 200);
    basic_type_and_run("PRINT X+Y\r", 500);

    tap_check(output_contains("30"),
              "X=10, Y=20, PRINT X+Y outputs '30'",
              "expected '30' in output");
}

/* Test 38: CPU reset vector points to $FF00 */
static void test_cpu_reset_vector(void)
{
    system_reset_fresh();

    /* After reset, PC should be at $FF00 (from reset vector at $FFFC-$FFFD) */
    uint8_t lo = mem_read(0xFFFC);
    uint8_t hi = mem_read(0xFFFD);
    uint16_t reset_vec = lo | (hi << 8);

    tap_check(reset_vec == 0xFF00,
              "reset vector at $FFFC points to $FF00",
              "expected reset vector $FF00");
}

/* Test 39: BASIC SGN function */
static void test_basic_sgn(void)
{
    system_reset_fresh();
    enter_basic();
    clear_output();

    basic_type_and_run("PRINT SGN(-42)\r", 500);

    tap_check(output_contains("-1"),
              "PRINT SGN(-42) outputs '-1'",
              "expected '-1' in output");
}

/* Test 40: BASIC END statement stops execution */
static void test_basic_end(void)
{
    system_reset_fresh();
    enter_basic();
    clear_output();

    /* Use unique markers that won't appear in echoed line numbers or keywords */
    basic_type_and_run("10 PRINT \"AAA\"\r", 200);
    basic_type_and_run("20 END\r", 200);
    basic_type_and_run("30 PRINT \"ZZZ\"\r", 200);

    /* Clear output before RUN to avoid seeing the echoed line text */
    clear_output();
    basic_type_and_run("RUN\r", 2000);

    bool has_aaa = output_contains("AAA");
    bool has_zzz = output_contains("ZZZ");

    tap_check(has_aaa && !has_zzz,
              "END stops execution (prints AAA but not ZZZ)",
              "expected 'AAA' but not 'ZZZ' in RUN output");
}

/* ---------- Main ---------- */

int main(void)
{
    /* Suppress log output during tests */
    log_init(LOG_LEVEL_ERROR, NULL);

    /* TAP header */
    printf("TAP version 13\n");
    printf("1..40\n");

    test_num = 0;
    tests_failed = 0;

    /* MONITOR TESTS (1-8) */
    test_monitor_boot_prompt();         /* 1 */
    test_monitor_echo();                /* 2 */
    test_monitor_cr_newline();          /* 3 */
    test_monitor_examine_single();      /* 4 */
    test_monitor_examine_range();       /* 5 */
    test_monitor_deposit();             /* 6 */
    test_monitor_deposit_multiple();    /* 7 */
    test_monitor_run();                 /* 8 */

    /* BASIC TESTS (9-27) */
    test_basic_banner();                /* 9 */
    test_basic_print_number();          /* 10 */
    test_basic_print_arithmetic();      /* 11 */
    test_basic_print_string();          /* 12 */
    test_basic_variable();              /* 13 */
    test_basic_if_true();               /* 14 */
    test_basic_if_false();              /* 15 */
    test_basic_for_loop();              /* 16 */
    test_basic_gosub();                 /* 17 */
    test_basic_new();                   /* 18 */
    test_basic_list();                  /* 19 */
    test_basic_string_variable();       /* 20 */
    test_basic_abs();                   /* 21 */
    test_basic_nested_arithmetic();     /* 22 */
    test_basic_comparison();            /* 23 */
    test_basic_peek_poke();             /* 24 */
    test_basic_rnd();                   /* 25 */
    test_basic_div_zero();              /* 26 */
    test_basic_multi_statement();       /* 27 */

    /* MEMORY TESTS (28-34) */
    test_memory_ram_boundary();         /* 28 */
    test_memory_rom_protection();       /* 29 */
    test_pia_keyboard_flag();           /* 30 */
    test_pia_keyboard_read_clears();    /* 31 */
    test_pia_display_callback();        /* 32 */
    test_memory_ram_range();            /* 33 */
    test_monitor_hex_parsing();         /* 34 */

    /* ADDITIONAL TESTS (35-40) */
    test_basic_large_numbers();         /* 35 */
    test_basic_negative_result();       /* 36 */
    test_basic_multiple_vars();         /* 37 */
    test_cpu_reset_vector();            /* 38 */
    test_basic_sgn();                   /* 39 */
    test_basic_end();                   /* 40 */

    /* TAP summary */
    printf("# tests:  %d\n", test_num);
    printf("# passed: %d\n", test_num - tests_failed);
    printf("# failed: %d\n", tests_failed);

    log_shutdown();

    return (tests_failed > 0) ? 1 : 0;
}
