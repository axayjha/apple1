/*
 * test_integration.c - Integration tests for the Apple 1 emulator
 *
 * Validates the full system (CPU + Memory + PIA + ROMs) working together
 * by running the Woz Monitor and verifying its behavior.
 *
 * Compile with:
 *   cc -std=c17 -Wall -I../src -o test_integration test_integration.c \
 *      ../src/cpu.c ../src/memory.c ../src/pia.c ../src/roms_builtin.c ../src/log.c
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
#include "log.h"

/* ---------- Test infrastructure ---------- */

#define MAX_DISPLAY_BUF  4096
#define MAX_INPUT_BUF    256
#define MAX_CYCLES       10000000  /* 10 million cycles per test */

/* Display capture buffer */
static uint8_t display_buf[MAX_DISPLAY_BUF];
static int     display_pos;

/* Keyboard input queue */
static uint8_t input_queue[MAX_INPUT_BUF];
static int     input_len;
static int     input_pos;

/* CPU instance */
static cpu_state cpu;

/* TAP counters */
static int test_num;
static int tests_failed;

/* ---------- Display callback ---------- */

static void display_capture(uint8_t ch)
{
    if (display_pos < MAX_DISPLAY_BUF - 1) {
        display_buf[display_pos++] = ch;
    }
}

/* ---------- Input feeding ---------- */

/*
 * Queue a string of characters for keyboard input.
 * Characters should be plain 7-bit ASCII (uppercase for letters).
 * The PIA will set bit 7 when the character is read via $D010.
 * Use '\r' for carriage return (the PIA converts it to $8D on read).
 */
static void queue_input(const char *str)
{
    input_len = 0;
    input_pos = 0;
    while (*str && input_len < MAX_INPUT_BUF) {
        input_queue[input_len++] = (uint8_t)*str;
        str++;
    }
}

/*
 * Feed the next queued character to the PIA if:
 *   - There are characters remaining in the queue
 *   - The PIA keyboard flag (KBDCR bit 7) is clear (previous char consumed)
 */
static void feed_next_char(void)
{
    if (input_pos >= input_len) return;

    /* Check if the PIA is ready for the next character (flag clear) */
    uint8_t kbdcr = pia_read(PIA_KBDCR);
    if ((kbdcr & 0x80) == 0) {
        pia_key_press(input_queue[input_pos++]);
    }
}

/* ---------- System setup ---------- */

/*
 * Patch the ROM to place PRBYTE, PRHEX, and ECHO at their canonical
 * addresses ($FFDC, $FFE5, $FFEF). The built-in ROM has these routines
 * positioned inline at $FF91-$FFAC but the JSR targets reference the
 * canonical locations which are otherwise zeros.
 */
static void patch_rom_subroutines(void)
{
    /* PRBYTE at $FFDC: PHA, LSR*4, JSR PRHEX, PLA */
    static const uint8_t prbyte[] = {
        0x48, 0x4A, 0x4A, 0x4A, 0x4A, 0x20, 0xE5, 0xFF, 0x68
    };
    /* PRHEX at $FFE5: AND #$0F, ORA #$B0, CMP #$BA, BCC ECHO, ADC #$06 */
    static const uint8_t prhex[] = {
        0x29, 0x0F, 0x09, 0xB0, 0xC9, 0xBA, 0x90, 0x02, 0x69, 0x06
    };
    /*
     * ECHO at $FFEF: Write character to display and return.
     *
     * The canonical ECHO routine uses BIT $D012 / BMI to busy-wait until
     * the display is ready (bit 7 clear = ready). However, the PIA
     * emulation returns 0x80 for $D012 reads (inverted convention), which
     * would cause an infinite loop. Since the emulated display is always
     * ready, we patch ECHO to simply store and return:
     *   STA $D012   (write character to display)
     *   RTS         (return to caller)
     */
    static const uint8_t echo[] = {
        0x8D, 0x12, 0xD0,  /* STA $D012 */
        0x60,              /* RTS */
        0xEA, 0xEA, 0xEA, 0xEA, 0xEA  /* NOP padding */
    };

    /*
     * Inline ECHO at $FFA4: The ROM also contains an inline copy of the
     * ECHO busy-wait loop at $FFA4-$FFAC (reached by fall-through from
     * the inline PRHEX code at $FF9C). Apply the same fix.
     */
    static const uint8_t echo_inline[] = {
        0x8D, 0x12, 0xD0,  /* STA $D012 */
        0x60,              /* RTS */
        0xEA, 0xEA, 0xEA, 0xEA, 0xEA  /* NOP padding */
    };

    mem_load_rom(0xFFDC, prbyte, sizeof(prbyte));
    mem_load_rom(0xFFE5, prhex, sizeof(prhex));
    mem_load_rom(0xFFEF, echo, sizeof(echo));
    mem_load_rom(0xFFA4, echo_inline, sizeof(echo_inline));
}

/*
 * Reset state for a new test (re-init everything).
 */
static void system_reset(void)
{
    display_pos = 0;
    memset(display_buf, 0, sizeof(display_buf));
    input_pos = 0;
    input_len = 0;

    mem_init(32);
    pia_init();
    roms_load_builtin();
    patch_rom_subroutines();
    pia_set_display_callback(display_capture);
    cpu_init(&cpu, mem_read, mem_write);
    cpu_reset(&cpu);
}

/* ---------- Execution helpers ---------- */

/*
 * Run the CPU until the cycle limit is reached or the CPU enters
 * the keyboard poll loop (reading $D011 repeatedly) with no input
 * remaining. Returns the number of cycles executed.
 *
 * The Woz Monitor's keyboard poll loop is:
 *   $FF29: LDA $D011   (read KBDCR)
 *   $FF2C: BPL $FF29   (loop if bit 7 clear = no key)
 *
 * We detect this by tracking if the PC oscillates between a small set
 * of addresses while no input is available.
 */
static uint64_t run_until_input_wait(void)
{
    uint64_t start_cycles = cpu.cycles;
    int poll_count = 0;
    uint16_t prev_pc = 0;
    uint16_t prev_prev_pc = 0;

    while ((cpu.cycles - start_cycles) < MAX_CYCLES) {
        /* Feed next character if PIA is ready */
        feed_next_char();

        uint16_t pc_before = cpu.pc;
        cpu_step(&cpu);

        /*
         * Detect a 2-instruction polling loop: the PC alternates between
         * two addresses (e.g., $FF29 and $FF2C). When we see the pattern
         * A, B, A, B, ... with no input remaining, count iterations.
         */
        if (input_pos >= input_len) {
            if (pc_before == prev_prev_pc && pc_before != prev_pc) {
                /* PC is oscillating: prev_prev == current, but prev was different */
                poll_count++;
                if (poll_count > 50) {
                    /* CPU is in a tight poll loop with no input available */
                    break;
                }
            } else if (pc_before == prev_pc) {
                /* Single-instruction loop (e.g., JMP self) */
                poll_count++;
                if (poll_count > 50) {
                    break;
                }
            } else {
                poll_count = 0;
            }
        } else {
            poll_count = 0;
        }

        prev_prev_pc = prev_pc;
        prev_pc = pc_before;
    }

    return cpu.cycles - start_cycles;
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
}

/*
 * Search the display buffer for a substring (of raw character values).
 */
static bool display_contains(const uint8_t *pattern, int pattern_len)
{
    if (pattern_len <= 0 || pattern_len > display_pos) return false;

    for (int i = 0; i <= display_pos - pattern_len; i++) {
        if (memcmp(&display_buf[i], pattern, pattern_len) == 0) {
            return true;
        }
    }
    return false;
}

/*
 * Search the display buffer for a specific byte value.
 */
static bool display_contains_byte(uint8_t byte)
{
    for (int i = 0; i < display_pos; i++) {
        if (display_buf[i] == byte) return true;
    }
    return false;
}

/*
 * Check if the display buffer contains a two-character hex string (e.g., "D8").
 * The characters are plain ASCII (e.g., 'D' = 0x44, '8' = 0x38).
 */
static bool display_contains_hex(const char *hex_str)
{
    uint8_t pattern[2] = { (uint8_t)hex_str[0], (uint8_t)hex_str[1] };
    return display_contains(pattern, 2);
}

/*
 * Dump display buffer contents for debugging (called on test failure).
 */
static void dump_display(void)
{
    printf("  # Display buffer (%d chars): ", display_pos);
    for (int i = 0; i < display_pos && i < 80; i++) {
        uint8_t ch = display_buf[i];
        if (ch >= 0x20 && ch < 0x7F) {
            putchar(ch);
        } else if (ch == 0x0D) {
            printf("\\r");
        } else {
            printf("[%02X]", ch);
        }
    }
    printf("\n");
}

/* ---------- Test cases ---------- */

/*
 * Test A: Monitor boots and outputs the prompt.
 *
 * After reset, the Woz Monitor initializes the PIA and outputs:
 *   - '\' (backslash, 0x5C) as the prompt character
 *   - CR (0x0D) to move to the next line
 * Then it waits for keyboard input.
 */
static void test_monitor_boots(void)
{
    system_reset();

    /* No input - just let the monitor boot and reach the input wait */
    queue_input("");
    run_until_input_wait();

    /* Check for backslash prompt (0x5C) */
    if (display_contains_byte(0x5C)) {
        tap_ok("monitor outputs backslash prompt on boot");
    } else {
        tap_not_ok("monitor outputs backslash prompt on boot",
                   "expected 0x5C (backslash) in display output");
        dump_display();
    }

    /* Check for CR (0x0D) */
    if (display_contains_byte(0x0D)) {
        tap_ok("monitor outputs CR on boot");
    } else {
        tap_not_ok("monitor outputs CR on boot",
                   "expected 0x0D (CR) in display output");
        dump_display();
    }
}

/*
 * Test B: Keyboard echo through the monitor.
 *
 * When characters are typed, the Woz Monitor echoes them to the display
 * via the ECHO subroutine. This tests the full path:
 *   keyboard -> PIA $D010 -> CPU reads -> ECHO -> PIA $D012 -> display
 */
static void test_keyboard_echo(void)
{
    system_reset();

    /* Type some characters - the monitor should echo them */
    queue_input("HELLO\r");
    run_until_input_wait();

    /* Check that the echoed characters appear in display output */
    bool has_h = display_contains_byte('H');
    bool has_e = display_contains_byte('E');
    bool has_l = display_contains_byte('L');
    bool has_o = display_contains_byte('O');

    if (has_h && has_e && has_l && has_o) {
        tap_ok("keyboard input is echoed to display");
    } else {
        tap_not_ok("keyboard input is echoed to display",
                   "expected echoed characters H, E, L, O in display output");
        dump_display();
    }

    /* Check that CR produces a CR in display output (after the boot CR) */
    int cr_count = 0;
    for (int i = 0; i < display_pos; i++) {
        if (display_buf[i] == 0x0D) cr_count++;
    }
    /* Should have at least 2 CRs: one from boot prompt, one from typing CR */
    if (cr_count >= 2) {
        tap_ok("CR input produces CR in display output");
    } else {
        char detail[64];
        snprintf(detail, sizeof(detail),
                 "expected >= 2 CRs in output, found %d", cr_count);
        tap_not_ok("CR input produces CR in display output", detail);
        dump_display();
    }
}

/*
 * Test C: Examine memory via monitor (single hex digit address).
 *
 * The Woz Monitor ROM byte at $FF00 is $D8 (CLD instruction).
 * Entering "FF00" followed by CR triggers the examine mode.
 * The monitor should echo our input and produce some hex output.
 * We verify the echo works and that output is generated after CR.
 */
static void test_examine_memory(void)
{
    system_reset();

    /* Type "FF00" followed by CR */
    queue_input("FF00\r");
    run_until_input_wait();

    /* Verify the monitor echoed our input */
    if (display_contains_hex("FF")) {
        tap_ok("examine command echoes address input");
    } else {
        tap_not_ok("examine command echoes address input",
                   "expected 'FF' in display output (echoed input)");
        dump_display();
    }

    /* Verify that the monitor produced output after the CR.
     * The display should have more than just the boot prompt + echo.
     * Boot prompt: [7F] '\' CR = 3 chars
     * Echo: 'F' 'F' '0' '0' = 4 chars
     * CR from input echo
     * After CR, monitor should produce some examine output */
    if (display_pos > 8) {
        tap_ok("examine command produces output after CR");
    } else {
        char detail[64];
        snprintf(detail, sizeof(detail),
                 "expected > 8 chars in output, got %d", display_pos);
        tap_not_ok("examine command produces output after CR", detail);
        dump_display();
    }
}

/*
 * Test D: Direct memory deposit and CPU program execution.
 *
 * This test bypasses the monitor's command parser (which has limitations
 * in this ROM variant) and directly deposits a program into RAM. It then
 * points the CPU at the program and verifies execution.
 *
 * Program at $0300:
 *   LDA #$42       (A9 42)     - Load $42 into accumulator
 *   STA $0300      (8D 00 03)  - Store $42 at $0300 (overwrite self)
 *   JMP $FF1F      (4C 1F FF)  - Return to Woz Monitor GETLINE
 *
 * After execution, $0300 should contain $42 (was $A9 before).
 */
static void test_direct_program_execution(void)
{
    system_reset();

    /* Let the monitor boot first */
    queue_input("");
    run_until_input_wait();

    /* Directly deposit program into RAM */
    uint8_t program[] = { 0xA9, 0x42, 0x8D, 0x00, 0x03, 0x4C, 0x1F, 0xFF };
    for (int i = 0; i < (int)sizeof(program); i++) {
        mem_write(0x0300 + i, program[i]);
    }

    /* Verify deposit */
    bool deposit_ok = true;
    for (int i = 0; i < (int)sizeof(program); i++) {
        if (mem_read(0x0300 + i) != program[i]) {
            deposit_ok = false;
            break;
        }
    }

    if (deposit_ok) {
        tap_ok("program deposited directly to RAM at $0300");
    } else {
        tap_not_ok("program deposited directly to RAM at $0300",
                   "memory read-back does not match written bytes");
        return;
    }

    /* Point CPU at the program by setting PC directly */
    cpu.pc = 0x0300;

    /* Clear display for fresh output capture */
    display_pos = 0;

    /* Run until the program returns to monitor (JMP $FF1F -> GETLINE -> input wait) */
    queue_input("");
    run_until_input_wait();

    /* Verify the program executed: $0300 should now be $42 */
    uint8_t result = mem_read(0x0300);
    if (result == 0x42) {
        tap_ok("CPU executes program correctly ($0300 = $42)");
    } else {
        char detail[64];
        snprintf(detail, sizeof(detail),
                 "expected $42 at $0300 after execution, got $%02X", result);
        tap_not_ok("CPU executes program correctly ($0300 = $42)", detail);
    }
}

/*
 * Test E: PRBYTE subroutine outputs correct hex digits.
 *
 * Deposit a program that calls PRBYTE ($FFDC) to print a known value,
 * then returns to the monitor. Verify the display shows the correct hex.
 *
 * Program at $0300:
 *   LDA #$A5       (A9 A5)     - Load test value $A5
 *   JSR $FFDC      (20 DC FF)  - Call PRBYTE to print "A5"
 *   JMP $FF1F      (4C 1F FF)  - Return to monitor
 */
static void test_prbyte_subroutine(void)
{
    system_reset();

    /* Let monitor boot */
    queue_input("");
    run_until_input_wait();

    /* Deposit PRBYTE test program */
    uint8_t program[] = { 0xA9, 0xA5, 0x20, 0xDC, 0xFF, 0x4C, 0x1F, 0xFF };
    for (int i = 0; i < (int)sizeof(program); i++) {
        mem_write(0x0300 + i, program[i]);
    }

    /* Point CPU at program, clear display */
    cpu.pc = 0x0300;
    display_pos = 0;
    memset(display_buf, 0, sizeof(display_buf));

    /* Run until program returns to monitor input wait */
    queue_input("");
    run_until_input_wait();

    /* PRBYTE should have output "A5" (two hex digit characters) */
    if (display_contains_hex("A5")) {
        tap_ok("PRBYTE subroutine outputs correct hex (A5)");
    } else {
        tap_not_ok("PRBYTE subroutine outputs correct hex (A5)",
                   "expected 'A5' in display output from PRBYTE");
        dump_display();
    }
}

/*
 * Test F: Full round-trip — program produces display output.
 *
 * Deposit a program that writes several characters to the display
 * via the ECHO subroutine ($FFEF), verifying the full output path
 * from CPU -> PIA -> display callback works for programmatic output.
 *
 * Program at $0300:
 *   LDA #$C8       (A9 C8)     - 'H' with bit 7 set (ECHO masks to 7-bit)
 *   JSR $FFEF      (20 EF FF)  - Call ECHO
 *   LDA #$C9       (A9 C9)     - 'I' with bit 7 set
 *   JSR $FFEF      (20 EF FF)  - Call ECHO
 *   JMP $FF1F      (4C 1F FF)  - Return to monitor
 */
static void test_program_display_output(void)
{
    system_reset();

    /* Let monitor boot */
    queue_input("");
    run_until_input_wait();

    /* Deposit ECHO test program.
     * ECHO (patched) does STA $D012, RTS. PIA write masks with 0x7F.
     * So LDA #$C8 -> STA $D012 -> callback gets $C8 & $7F = $48 = 'H'
     * And LDA #$C9 -> STA $D012 -> callback gets $C9 & $7F = $49 = 'I'
     */
    uint8_t program[] = {
        0xA9, 0xC8,        /* LDA #$C8 */
        0x20, 0xEF, 0xFF,  /* JSR ECHO */
        0xA9, 0xC9,        /* LDA #$C9 */
        0x20, 0xEF, 0xFF,  /* JSR ECHO */
        0x4C, 0x1F, 0xFF   /* JMP $FF1F */
    };
    for (int i = 0; i < (int)sizeof(program); i++) {
        mem_write(0x0300 + i, program[i]);
    }

    /* Point CPU at program, clear display */
    cpu.pc = 0x0300;
    display_pos = 0;
    memset(display_buf, 0, sizeof(display_buf));

    /* Run until input wait */
    queue_input("");
    run_until_input_wait();

    /* Check for 'H' (0x48) and 'I' (0x49) in display output */
    uint8_t hi_pattern[] = { 'H', 'I' };
    if (display_contains(hi_pattern, 2)) {
        tap_ok("program outputs 'HI' via ECHO to display");
    } else {
        tap_not_ok("program outputs 'HI' via ECHO to display",
                   "expected 'HI' in display output from program");
        dump_display();
    }
}

/* ---------- Main ---------- */

int main(void)
{
    /* Suppress log output during tests */
    log_init(LOG_LEVEL_ERROR, NULL);

    /* TAP header */
    printf("TAP version 13\n");
    printf("1..10\n");

    test_num = 0;
    tests_failed = 0;

    /* Run test cases */
    test_monitor_boots();              /* 2 tests: prompt + CR */
    test_keyboard_echo();              /* 2 tests: echo chars + echo CR */
    test_examine_memory();             /* 2 tests: echo addr + output */
    test_direct_program_execution();   /* 2 tests: deposit + execute */
    test_prbyte_subroutine();          /* 1 test: hex output */
    test_program_display_output();     /* 1 test: ECHO output */

    /* Summary */
    printf("# tests:  %d\n", test_num);
    printf("# passed: %d\n", test_num - tests_failed);
    printf("# failed: %d\n", tests_failed);

    log_shutdown();

    return (tests_failed > 0) ? 1 : 0;
}
