/*
 * test_cpu.c - Standalone unit tests for the MOS 6502 CPU emulation core
 *
 * Compiles with:
 *   cc -std=c17 -Wall -I../src -o test_cpu test_cpu.c ../src/cpu.c
 *
 * Output format: TAP (Test Anything Protocol)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpu.h"

/* ========================================================================== */
/* Test memory - flat 64KB array, no I/O dispatch                             */
/* ========================================================================== */

static uint8_t test_mem[65536];

static uint8_t bus_read(uint16_t addr) {
    return test_mem[addr];
}

static void bus_write(uint16_t addr, uint8_t val) {
    test_mem[addr] = val;
}

/* ========================================================================== */
/* Test infrastructure                                                         */
/* ========================================================================== */

static int test_num = 1;
static int failures = 0;

#define ASSERT_EQ(expected, actual, msg) do { \
    if ((expected) != (actual)) { \
        printf("not ok %d - %s (expected 0x%02X, got 0x%02X)\n", \
               test_num, msg, (unsigned)(expected), (unsigned)(actual)); \
        failures++; \
    } else { \
        printf("ok %d - %s\n", test_num, msg); \
    } \
    test_num++; \
} while(0)

#define ASSERT_EQ16(expected, actual, msg) do { \
    if ((expected) != (actual)) { \
        printf("not ok %d - %s (expected 0x%04X, got 0x%04X)\n", \
               test_num, msg, (unsigned)(expected), (unsigned)(actual)); \
        failures++; \
    } else { \
        printf("ok %d - %s\n", test_num, msg); \
    } \
    test_num++; \
} while(0)

#define ASSERT_FLAG(cpu, flag, val, msg) do { \
    int _actual = ((cpu)->status & (flag)) ? 1 : 0; \
    int _expected = (val) ? 1 : 0; \
    if (_expected != _actual) { \
        printf("not ok %d - %s (expected %d, got %d)\n", \
               test_num, msg, _expected, _actual); \
        failures++; \
    } else { \
        printf("ok %d - %s\n", test_num, msg); \
    } \
    test_num++; \
} while(0)

static cpu_state cpu;

/* Reset test memory and CPU, placing code at the given start address.
 * Sets the reset vector to point to start_addr. */
static void setup(uint16_t start_addr) {
    memset(test_mem, 0, sizeof(test_mem));
    /* Set reset vector */
    test_mem[0xFFFC] = start_addr & 0xFF;
    test_mem[0xFFFD] = start_addr >> 8;
    cpu_init(&cpu, bus_read, bus_write);
    cpu_reset(&cpu);
}

/* Place code bytes starting at address and set PC there directly
 * (avoids needing reset vector for each sub-test). */
static void place_code(uint16_t addr, const uint8_t *code, size_t len) {
    memcpy(&test_mem[addr], code, len);
    cpu.pc = addr;
}

/* ========================================================================== */
/* Test categories                                                             */
/* ========================================================================== */

/* -------------------------------------------------------------------------- */
/* (a) Load/Store tests                                                        */
/* -------------------------------------------------------------------------- */

static void test_load_store(void) {
    uint8_t cycles;

    /* LDA immediate - loads correct value */
    setup(0x0200);
    uint8_t code1[] = { 0xA9, 0x42 }; /* LDA #$42 */
    place_code(0x0200, code1, sizeof(code1));
    cycles = cpu_step(&cpu);
    ASSERT_EQ(0x42, cpu.a, "LDA immediate loads correct value");
    ASSERT_EQ(2, cycles, "LDA immediate takes 2 cycles");

    /* LDA immediate - sets zero flag */
    setup(0x0200);
    uint8_t code2[] = { 0xA9, 0x00 }; /* LDA #$00 */
    place_code(0x0200, code2, sizeof(code2));
    cpu_step(&cpu);
    ASSERT_FLAG(&cpu, CPU_FLAG_Z, 1, "LDA immediate sets zero flag");

    /* LDA immediate - sets negative flag */
    setup(0x0200);
    uint8_t code3[] = { 0xA9, 0x80 }; /* LDA #$80 */
    place_code(0x0200, code3, sizeof(code3));
    cpu_step(&cpu);
    ASSERT_FLAG(&cpu, CPU_FLAG_N, 1, "LDA immediate sets negative flag");

    /* LDA zero page */
    setup(0x0200);
    test_mem[0x0010] = 0x77;
    uint8_t code4[] = { 0xA5, 0x10 }; /* LDA $10 */
    place_code(0x0200, code4, sizeof(code4));
    cycles = cpu_step(&cpu);
    ASSERT_EQ(0x77, cpu.a, "LDA zero page loads correct value");
    ASSERT_EQ(3, cycles, "LDA zero page takes 3 cycles");

    /* LDA absolute */
    setup(0x0200);
    test_mem[0x1234] = 0xAB;
    uint8_t code5[] = { 0xAD, 0x34, 0x12 }; /* LDA $1234 */
    place_code(0x0200, code5, sizeof(code5));
    cycles = cpu_step(&cpu);
    ASSERT_EQ(0xAB, cpu.a, "LDA absolute loads correct value");
    ASSERT_EQ(4, cycles, "LDA absolute takes 4 cycles");

    /* LDX immediate */
    setup(0x0200);
    uint8_t code6[] = { 0xA2, 0x55 }; /* LDX #$55 */
    place_code(0x0200, code6, sizeof(code6));
    cpu_step(&cpu);
    ASSERT_EQ(0x55, cpu.x, "LDX immediate loads correct value");

    /* LDY immediate */
    setup(0x0200);
    uint8_t code7[] = { 0xA0, 0xCC }; /* LDY #$CC */
    place_code(0x0200, code7, sizeof(code7));
    cpu_step(&cpu);
    ASSERT_EQ(0xCC, cpu.y, "LDY immediate loads correct value");

    /* STA zero page */
    setup(0x0200);
    cpu.a = 0x99;
    uint8_t code8[] = { 0x85, 0x30 }; /* STA $30 */
    place_code(0x0200, code8, sizeof(code8));
    cycles = cpu_step(&cpu);
    ASSERT_EQ(0x99, test_mem[0x30], "STA zero page stores correct value");
    ASSERT_EQ(3, cycles, "STA zero page takes 3 cycles");

    /* STX zero page */
    setup(0x0200);
    cpu.x = 0xBB;
    uint8_t code9[] = { 0x86, 0x40 }; /* STX $40 */
    place_code(0x0200, code9, sizeof(code9));
    cpu_step(&cpu);
    ASSERT_EQ(0xBB, test_mem[0x40], "STX zero page stores correct value");

    /* STY zero page */
    setup(0x0200);
    cpu.y = 0xDD;
    uint8_t code10[] = { 0x84, 0x50 }; /* STY $50 */
    place_code(0x0200, code10, sizeof(code10));
    cpu_step(&cpu);
    ASSERT_EQ(0xDD, test_mem[0x50], "STY zero page stores correct value");

    /* LDA absolute,X with page crossing: base $10FE + X=$02 = $1100 */
    setup(0x0200);
    cpu.x = 0x02;
    test_mem[0x1100] = 0x33;
    uint8_t code11[] = { 0xBD, 0xFE, 0x10 }; /* LDA $10FE,X */
    place_code(0x0200, code11, sizeof(code11));
    cycles = cpu_step(&cpu);
    ASSERT_EQ(0x33, cpu.a, "LDA absolute,X with page crossing loads correct value");
    ASSERT_EQ(5, cycles, "LDA absolute,X with page crossing takes 5 cycles");
}

/* -------------------------------------------------------------------------- */
/* (b) Arithmetic tests - ADC, SBC                                             */
/* -------------------------------------------------------------------------- */

static void test_arithmetic(void) {
    uint8_t cycles;

    /* ADC immediate - simple addition no carry */
    setup(0x0200);
    cpu.a = 0x10;
    cpu.status &= ~CPU_FLAG_C; /* Clear carry */
    cpu.status &= ~CPU_FLAG_D; /* Clear decimal */
    uint8_t code1[] = { 0x69, 0x20 }; /* ADC #$20 */
    place_code(0x0200, code1, sizeof(code1));
    cycles = cpu_step(&cpu);
    ASSERT_EQ(0x30, cpu.a, "ADC immediate simple addition");
    ASSERT_EQ(2, cycles, "ADC immediate takes 2 cycles");
    ASSERT_FLAG(&cpu, CPU_FLAG_C, 0, "ADC no carry out");

    /* ADC with carry in */
    setup(0x0200);
    cpu.a = 0x10;
    cpu.status |= CPU_FLAG_C;  /* Set carry */
    cpu.status &= ~CPU_FLAG_D;
    uint8_t code2[] = { 0x69, 0x20 }; /* ADC #$20 */
    place_code(0x0200, code2, sizeof(code2));
    cpu_step(&cpu);
    ASSERT_EQ(0x31, cpu.a, "ADC with carry in adds extra 1");

    /* ADC overflow: 0x50 + 0x50 = 0xA0 (positive + positive = negative) */
    setup(0x0200);
    cpu.a = 0x50;
    cpu.status &= ~(CPU_FLAG_C | CPU_FLAG_D);
    uint8_t code3[] = { 0x69, 0x50 }; /* ADC #$50 */
    place_code(0x0200, code3, sizeof(code3));
    cpu_step(&cpu);
    ASSERT_EQ(0xA0, cpu.a, "ADC overflow result");
    ASSERT_FLAG(&cpu, CPU_FLAG_V, 1, "ADC sets overflow when positive + positive = negative");
    ASSERT_FLAG(&cpu, CPU_FLAG_N, 1, "ADC sets negative flag on overflow");

    /* ADC carry out: 0xFF + 0x01 = 0x00 with carry */
    setup(0x0200);
    cpu.a = 0xFF;
    cpu.status &= ~(CPU_FLAG_C | CPU_FLAG_D);
    uint8_t code4[] = { 0x69, 0x01 }; /* ADC #$01 */
    place_code(0x0200, code4, sizeof(code4));
    cpu_step(&cpu);
    ASSERT_EQ(0x00, cpu.a, "ADC with carry out result is 0");
    ASSERT_FLAG(&cpu, CPU_FLAG_C, 1, "ADC sets carry on overflow past 0xFF");
    ASSERT_FLAG(&cpu, CPU_FLAG_Z, 1, "ADC sets zero flag when result is 0");

    /* SBC immediate: 0x50 - 0x10 = 0x40 (carry set = no borrow) */
    setup(0x0200);
    cpu.a = 0x50;
    cpu.status |= CPU_FLAG_C;  /* No borrow */
    cpu.status &= ~CPU_FLAG_D;
    uint8_t code5[] = { 0xE9, 0x10 }; /* SBC #$10 */
    place_code(0x0200, code5, sizeof(code5));
    cycles = cpu_step(&cpu);
    ASSERT_EQ(0x40, cpu.a, "SBC immediate simple subtraction");
    ASSERT_EQ(2, cycles, "SBC immediate takes 2 cycles");
    ASSERT_FLAG(&cpu, CPU_FLAG_C, 1, "SBC no borrow out (carry remains set)");

    /* SBC with borrow: 0x50 - 0x60 = 0xF0 (unsigned underflow) */
    setup(0x0200);
    cpu.a = 0x50;
    cpu.status |= CPU_FLAG_C;
    cpu.status &= ~CPU_FLAG_D;
    uint8_t code6[] = { 0xE9, 0x60 }; /* SBC #$60 */
    place_code(0x0200, code6, sizeof(code6));
    cpu_step(&cpu);
    ASSERT_EQ(0xF0, cpu.a, "SBC with borrow produces correct result");
    ASSERT_FLAG(&cpu, CPU_FLAG_C, 0, "SBC clears carry on borrow");
    ASSERT_FLAG(&cpu, CPU_FLAG_N, 1, "SBC sets negative flag");

    /* SBC overflow: 0x80 - 0x01 = 0x7F (negative - positive = positive) */
    setup(0x0200);
    cpu.a = 0x80;
    cpu.status |= CPU_FLAG_C;
    cpu.status &= ~CPU_FLAG_D;
    uint8_t code7[] = { 0xE9, 0x01 }; /* SBC #$01 */
    place_code(0x0200, code7, sizeof(code7));
    cpu_step(&cpu);
    ASSERT_EQ(0x7F, cpu.a, "SBC overflow result");
    ASSERT_FLAG(&cpu, CPU_FLAG_V, 1, "SBC sets overflow when negative - positive = positive");
}

/* -------------------------------------------------------------------------- */
/* (c) BCD tests                                                               */
/* -------------------------------------------------------------------------- */

static void test_bcd(void) {
    /* ADC BCD: 0x15 + 0x27 = 0x42 */
    setup(0x0200);
    cpu.a = 0x15;
    cpu.status |= CPU_FLAG_D;
    cpu.status &= ~CPU_FLAG_C;
    uint8_t code1[] = { 0x69, 0x27 }; /* ADC #$27 */
    place_code(0x0200, code1, sizeof(code1));
    cpu_step(&cpu);
    ASSERT_EQ(0x42, cpu.a, "ADC BCD 15 + 27 = 42");
    ASSERT_FLAG(&cpu, CPU_FLAG_C, 0, "ADC BCD no carry on 15+27");

    /* ADC BCD: 0x99 + 0x01 = 0x00 with carry (99 + 1 = 100) */
    setup(0x0200);
    cpu.a = 0x99;
    cpu.status |= CPU_FLAG_D;
    cpu.status &= ~CPU_FLAG_C;
    uint8_t code2[] = { 0x69, 0x01 }; /* ADC #$01 */
    place_code(0x0200, code2, sizeof(code2));
    cpu_step(&cpu);
    ASSERT_EQ(0x00, cpu.a, "ADC BCD 99 + 01 = 00 (with carry)");
    ASSERT_FLAG(&cpu, CPU_FLAG_C, 1, "ADC BCD sets carry on 99+01");

    /* SBC BCD: 0x46 - 0x12 = 0x34 */
    setup(0x0200);
    cpu.a = 0x46;
    cpu.status |= CPU_FLAG_D;
    cpu.status |= CPU_FLAG_C; /* No borrow */
    uint8_t code3[] = { 0xE9, 0x12 }; /* SBC #$12 */
    place_code(0x0200, code3, sizeof(code3));
    cpu_step(&cpu);
    ASSERT_EQ(0x34, cpu.a, "SBC BCD 46 - 12 = 34");
    ASSERT_FLAG(&cpu, CPU_FLAG_C, 1, "SBC BCD no borrow on 46-12");

    /* SBC BCD: 0x10 - 0x20 (borrow needed) */
    setup(0x0200);
    cpu.a = 0x10;
    cpu.status |= CPU_FLAG_D;
    cpu.status |= CPU_FLAG_C;
    uint8_t code4[] = { 0xE9, 0x20 }; /* SBC #$20 */
    place_code(0x0200, code4, sizeof(code4));
    cpu_step(&cpu);
    ASSERT_EQ(0x90, cpu.a, "SBC BCD 10 - 20 = 90 (with borrow)");
    ASSERT_FLAG(&cpu, CPU_FLAG_C, 0, "SBC BCD sets borrow on 10-20");
}

/* -------------------------------------------------------------------------- */
/* (d) Logic tests                                                             */
/* -------------------------------------------------------------------------- */

static void test_logic(void) {
    /* AND immediate */
    setup(0x0200);
    cpu.a = 0xFF;
    uint8_t code1[] = { 0x29, 0x0F }; /* AND #$0F */
    place_code(0x0200, code1, sizeof(code1));
    cpu_step(&cpu);
    ASSERT_EQ(0x0F, cpu.a, "AND immediate masks correctly");
    ASSERT_FLAG(&cpu, CPU_FLAG_Z, 0, "AND result non-zero clears Z");

    /* AND results in zero */
    setup(0x0200);
    cpu.a = 0xF0;
    uint8_t code2[] = { 0x29, 0x0F }; /* AND #$0F */
    place_code(0x0200, code2, sizeof(code2));
    cpu_step(&cpu);
    ASSERT_EQ(0x00, cpu.a, "AND produces zero");
    ASSERT_FLAG(&cpu, CPU_FLAG_Z, 1, "AND sets Z when result is zero");

    /* ORA immediate */
    setup(0x0200);
    cpu.a = 0x0F;
    uint8_t code3[] = { 0x09, 0xF0 }; /* ORA #$F0 */
    place_code(0x0200, code3, sizeof(code3));
    cpu_step(&cpu);
    ASSERT_EQ(0xFF, cpu.a, "ORA immediate combines bits");
    ASSERT_FLAG(&cpu, CPU_FLAG_N, 1, "ORA sets N when bit 7 set");

    /* EOR immediate */
    setup(0x0200);
    cpu.a = 0xAA;
    uint8_t code4[] = { 0x49, 0xFF }; /* EOR #$FF */
    place_code(0x0200, code4, sizeof(code4));
    cpu_step(&cpu);
    ASSERT_EQ(0x55, cpu.a, "EOR inverts bits with 0xFF mask");
}

/* -------------------------------------------------------------------------- */
/* (e) Shift/Rotate tests                                                      */
/* -------------------------------------------------------------------------- */

static void test_shifts(void) {
    uint8_t cycles;

    /* ASL accumulator */
    setup(0x0200);
    cpu.a = 0x81;
    uint8_t code1[] = { 0x0A }; /* ASL A */
    place_code(0x0200, code1, sizeof(code1));
    cycles = cpu_step(&cpu);
    ASSERT_EQ(0x02, cpu.a, "ASL A shifts left");
    ASSERT_FLAG(&cpu, CPU_FLAG_C, 1, "ASL A shifts bit 7 into carry");
    ASSERT_EQ(2, cycles, "ASL A takes 2 cycles");

    /* LSR accumulator */
    setup(0x0200);
    cpu.a = 0x03;
    uint8_t code2[] = { 0x4A }; /* LSR A */
    place_code(0x0200, code2, sizeof(code2));
    cpu_step(&cpu);
    ASSERT_EQ(0x01, cpu.a, "LSR A shifts right");
    ASSERT_FLAG(&cpu, CPU_FLAG_C, 1, "LSR A shifts bit 0 into carry");
    ASSERT_FLAG(&cpu, CPU_FLAG_N, 0, "LSR A always clears N");

    /* ROL accumulator with carry set */
    setup(0x0200);
    cpu.a = 0x80;
    cpu.status |= CPU_FLAG_C;
    uint8_t code3[] = { 0x2A }; /* ROL A */
    place_code(0x0200, code3, sizeof(code3));
    cpu_step(&cpu);
    ASSERT_EQ(0x01, cpu.a, "ROL A rotates carry into bit 0");
    ASSERT_FLAG(&cpu, CPU_FLAG_C, 1, "ROL A rotates bit 7 into carry");

    /* ROR accumulator with carry set - critical edge case */
    setup(0x0200);
    cpu.a = 0x01;
    cpu.status |= CPU_FLAG_C;
    uint8_t code4[] = { 0x6A }; /* ROR A */
    place_code(0x0200, code4, sizeof(code4));
    cpu_step(&cpu);
    ASSERT_EQ(0x80, cpu.a, "ROR A rotates carry into bit 7");
    ASSERT_FLAG(&cpu, CPU_FLAG_C, 1, "ROR A rotates bit 0 into carry");
    ASSERT_FLAG(&cpu, CPU_FLAG_N, 1, "ROR A with carry sets N flag");

    /* ROR accumulator with carry clear */
    setup(0x0200);
    cpu.a = 0x02;
    cpu.status &= ~CPU_FLAG_C;
    uint8_t code5[] = { 0x6A }; /* ROR A */
    place_code(0x0200, code5, sizeof(code5));
    cpu_step(&cpu);
    ASSERT_EQ(0x01, cpu.a, "ROR A without carry clears bit 7");
    ASSERT_FLAG(&cpu, CPU_FLAG_C, 0, "ROR A bit 0 was 0, carry clear");
}

/* -------------------------------------------------------------------------- */
/* (f) Branch tests                                                            */
/* -------------------------------------------------------------------------- */

static void test_branches(void) {
    uint8_t cycles;

    /* BEQ taken (Z=1) */
    setup(0x0200);
    cpu.status |= CPU_FLAG_Z;
    uint8_t code1[] = { 0xF0, 0x05 }; /* BEQ +5 */
    place_code(0x0200, code1, sizeof(code1));
    cycles = cpu_step(&cpu);
    ASSERT_EQ16(0x0207, cpu.pc, "BEQ taken branches forward");
    ASSERT_EQ(3, cycles, "BEQ taken same page takes 3 cycles");

    /* BEQ not taken (Z=0) */
    setup(0x0200);
    cpu.status &= ~CPU_FLAG_Z;
    uint8_t code2[] = { 0xF0, 0x05 }; /* BEQ +5 */
    place_code(0x0200, code2, sizeof(code2));
    cycles = cpu_step(&cpu);
    ASSERT_EQ16(0x0202, cpu.pc, "BEQ not taken falls through");
    ASSERT_EQ(2, cycles, "BEQ not taken takes 2 cycles");

    /* BNE taken (Z=0) */
    setup(0x0200);
    cpu.status &= ~CPU_FLAG_Z;
    uint8_t code3[] = { 0xD0, 0x10 }; /* BNE +16 */
    place_code(0x0200, code3, sizeof(code3));
    cpu_step(&cpu);
    ASSERT_EQ16(0x0212, cpu.pc, "BNE taken branches forward");

    /* BCS taken (C=1) */
    setup(0x0200);
    cpu.status |= CPU_FLAG_C;
    uint8_t code4[] = { 0xB0, 0x03 }; /* BCS +3 */
    place_code(0x0200, code4, sizeof(code4));
    cpu_step(&cpu);
    ASSERT_EQ16(0x0205, cpu.pc, "BCS taken when carry set");

    /* BCC taken (C=0) */
    setup(0x0200);
    cpu.status &= ~CPU_FLAG_C;
    uint8_t code5[] = { 0x90, 0x03 }; /* BCC +3 */
    place_code(0x0200, code5, sizeof(code5));
    cpu_step(&cpu);
    ASSERT_EQ16(0x0205, cpu.pc, "BCC taken when carry clear");

    /* BMI taken (N=1) */
    setup(0x0200);
    cpu.status |= CPU_FLAG_N;
    uint8_t code6[] = { 0x30, 0x04 }; /* BMI +4 */
    place_code(0x0200, code6, sizeof(code6));
    cpu_step(&cpu);
    ASSERT_EQ16(0x0206, cpu.pc, "BMI taken when negative set");

    /* BPL taken (N=0) */
    setup(0x0200);
    cpu.status &= ~CPU_FLAG_N;
    uint8_t code7[] = { 0x10, 0x04 }; /* BPL +4 */
    place_code(0x0200, code7, sizeof(code7));
    cpu_step(&cpu);
    ASSERT_EQ16(0x0206, cpu.pc, "BPL taken when negative clear");

    /* BVS taken (V=1) */
    setup(0x0200);
    cpu.status |= CPU_FLAG_V;
    uint8_t code8[] = { 0x70, 0x02 }; /* BVS +2 */
    place_code(0x0200, code8, sizeof(code8));
    cpu_step(&cpu);
    ASSERT_EQ16(0x0204, cpu.pc, "BVS taken when overflow set");

    /* BVC taken (V=0) */
    setup(0x0200);
    cpu.status &= ~CPU_FLAG_V;
    uint8_t code9[] = { 0x50, 0x02 }; /* BVC +2 */
    place_code(0x0200, code9, sizeof(code9));
    cpu_step(&cpu);
    ASSERT_EQ16(0x0204, cpu.pc, "BVC taken when overflow clear");

    /* Branch backward */
    setup(0x0200);
    cpu.status |= CPU_FLAG_Z;
    uint8_t code10[] = { 0xF0, 0xFC }; /* BEQ -4 (offset = -4 signed) */
    place_code(0x0210, code10, sizeof(code10));
    cpu_step(&cpu);
    ASSERT_EQ16(0x020E, cpu.pc, "BEQ backward branch");

    /* Branch with page crossing penalty */
    setup(0x0200);
    cpu.status |= CPU_FLAG_Z;
    uint8_t code11[] = { 0xF0, 0x70 }; /* BEQ +112 (crosses from 0x02xx to 0x03xx) */
    place_code(0x02F0, code11, sizeof(code11));
    cycles = cpu_step(&cpu);
    ASSERT_EQ16(0x0362, cpu.pc, "BEQ page-crossing branches to correct addr");
    ASSERT_EQ(4, cycles, "BEQ page crossing takes 4 cycles");
}

/* -------------------------------------------------------------------------- */
/* (g) Jump tests                                                              */
/* -------------------------------------------------------------------------- */

static void test_jumps(void) {
    uint8_t cycles;

    /* JMP absolute */
    setup(0x0200);
    uint8_t code1[] = { 0x4C, 0x00, 0x10 }; /* JMP $1000 */
    place_code(0x0200, code1, sizeof(code1));
    cycles = cpu_step(&cpu);
    ASSERT_EQ16(0x1000, cpu.pc, "JMP absolute jumps to target");
    ASSERT_EQ(3, cycles, "JMP absolute takes 3 cycles");

    /* JMP indirect */
    setup(0x0200);
    test_mem[0x3000] = 0x78;
    test_mem[0x3001] = 0x56;
    uint8_t code2[] = { 0x6C, 0x00, 0x30 }; /* JMP ($3000) */
    place_code(0x0200, code2, sizeof(code2));
    cycles = cpu_step(&cpu);
    ASSERT_EQ16(0x5678, cpu.pc, "JMP indirect reads target from pointer");
    ASSERT_EQ(5, cycles, "JMP indirect takes 5 cycles");

    /* JMP indirect page boundary bug: pointer at $30FF wraps within page */
    setup(0x0200);
    test_mem[0x30FF] = 0x40;  /* Low byte at $30FF */
    test_mem[0x3000] = 0x80;  /* High byte wraps to $3000 (not $3100) */
    test_mem[0x3100] = 0x99;  /* This should NOT be read */
    uint8_t code3[] = { 0x6C, 0xFF, 0x30 }; /* JMP ($30FF) */
    place_code(0x0200, code3, sizeof(code3));
    cpu_step(&cpu);
    ASSERT_EQ16(0x8040, cpu.pc, "JMP indirect page boundary bug wraps within page");

    /* JSR/RTS */
    setup(0x0200);
    /* JSR $0300 at $0200, then at $0300 place RTS */
    uint8_t code_jsr[] = { 0x20, 0x00, 0x03 }; /* JSR $0300 */
    uint8_t code_rts[] = { 0x60 };              /* RTS */
    place_code(0x0200, code_jsr, sizeof(code_jsr));
    memcpy(&test_mem[0x0300], code_rts, sizeof(code_rts));
    cycles = cpu_step(&cpu); /* Execute JSR */
    ASSERT_EQ16(0x0300, cpu.pc, "JSR jumps to subroutine address");
    ASSERT_EQ(6, cycles, "JSR takes 6 cycles");

    /* Now execute RTS */
    cycles = cpu_step(&cpu);
    ASSERT_EQ16(0x0203, cpu.pc, "RTS returns to instruction after JSR");
    ASSERT_EQ(6, cycles, "RTS takes 6 cycles");
}

/* -------------------------------------------------------------------------- */
/* (h) Stack tests                                                             */
/* -------------------------------------------------------------------------- */

static void test_stack(void) {
    uint8_t cycles;

    /* PHA / PLA */
    setup(0x0200);
    cpu.a = 0x42;
    uint8_t code1[] = { 0x48 }; /* PHA */
    place_code(0x0200, code1, sizeof(code1));
    cycles = cpu_step(&cpu);
    ASSERT_EQ(0xFC, cpu.sp, "PHA decrements SP");
    ASSERT_EQ(0x42, test_mem[0x01FD], "PHA pushes A onto stack");
    ASSERT_EQ(3, cycles, "PHA takes 3 cycles");

    /* PLA */
    cpu.a = 0x00;
    uint8_t code2[] = { 0x68 }; /* PLA */
    place_code(0x0201, code2, sizeof(code2));
    cycles = cpu_step(&cpu);
    ASSERT_EQ(0x42, cpu.a, "PLA pulls correct value");
    ASSERT_EQ(0xFD, cpu.sp, "PLA increments SP");
    ASSERT_EQ(4, cycles, "PLA takes 4 cycles");

    /* PHP pushes status with B and U set */
    setup(0x0200);
    cpu.status = CPU_FLAG_C | CPU_FLAG_Z | CPU_FLAG_U; /* C, Z, U */
    uint8_t code3[] = { 0x08 }; /* PHP */
    place_code(0x0200, code3, sizeof(code3));
    cpu_step(&cpu);
    uint8_t pushed = test_mem[0x01FD];
    ASSERT_EQ(CPU_FLAG_C | CPU_FLAG_Z | CPU_FLAG_B | CPU_FLAG_U, pushed,
              "PHP pushes status with B and U flags set");

    /* PLP clears B, sets U */
    setup(0x0200);
    test_mem[0x01FE] = CPU_FLAG_C | CPU_FLAG_B | CPU_FLAG_N; /* Simulate pushed value */
    cpu.sp = 0xFD;
    uint8_t code4[] = { 0x28 }; /* PLP */
    place_code(0x0200, code4, sizeof(code4));
    cpu_step(&cpu);
    /* PLP clears B and always sets U */
    ASSERT_EQ((CPU_FLAG_C | CPU_FLAG_N | CPU_FLAG_U) & ~CPU_FLAG_B, cpu.status,
              "PLP restores flags, clears B, sets U");
}

/* -------------------------------------------------------------------------- */
/* (i) Register transfer tests                                                 */
/* -------------------------------------------------------------------------- */

static void test_transfers(void) {
    /* TAX */
    setup(0x0200);
    cpu.a = 0x7F;
    uint8_t code1[] = { 0xAA }; /* TAX */
    place_code(0x0200, code1, sizeof(code1));
    cpu_step(&cpu);
    ASSERT_EQ(0x7F, cpu.x, "TAX transfers A to X");
    ASSERT_FLAG(&cpu, CPU_FLAG_N, 0, "TAX positive value clears N");

    /* TAY */
    setup(0x0200);
    cpu.a = 0x80;
    uint8_t code2[] = { 0xA8 }; /* TAY */
    place_code(0x0200, code2, sizeof(code2));
    cpu_step(&cpu);
    ASSERT_EQ(0x80, cpu.y, "TAY transfers A to Y");
    ASSERT_FLAG(&cpu, CPU_FLAG_N, 1, "TAY negative value sets N");

    /* TXA */
    setup(0x0200);
    cpu.x = 0x00;
    uint8_t code3[] = { 0x8A }; /* TXA */
    place_code(0x0200, code3, sizeof(code3));
    cpu_step(&cpu);
    ASSERT_EQ(0x00, cpu.a, "TXA transfers X to A");
    ASSERT_FLAG(&cpu, CPU_FLAG_Z, 1, "TXA zero value sets Z");

    /* TYA */
    setup(0x0200);
    cpu.y = 0xAB;
    uint8_t code4[] = { 0x98 }; /* TYA */
    place_code(0x0200, code4, sizeof(code4));
    cpu_step(&cpu);
    ASSERT_EQ(0xAB, cpu.a, "TYA transfers Y to A");

    /* TSX */
    setup(0x0200);
    cpu.sp = 0xFD;
    uint8_t code5[] = { 0xBA }; /* TSX */
    place_code(0x0200, code5, sizeof(code5));
    cpu_step(&cpu);
    ASSERT_EQ(0xFD, cpu.x, "TSX transfers SP to X");

    /* TXS - does NOT affect flags */
    setup(0x0200);
    cpu.x = 0xFF;
    cpu.status &= ~(CPU_FLAG_N | CPU_FLAG_Z);
    uint8_t code6[] = { 0x9A }; /* TXS */
    place_code(0x0200, code6, sizeof(code6));
    cpu_step(&cpu);
    ASSERT_EQ(0xFF, cpu.sp, "TXS transfers X to SP");
    ASSERT_FLAG(&cpu, CPU_FLAG_N, 0, "TXS does not affect N flag");
    ASSERT_FLAG(&cpu, CPU_FLAG_Z, 0, "TXS does not affect Z flag");
}

/* -------------------------------------------------------------------------- */
/* (j) Compare tests                                                           */
/* -------------------------------------------------------------------------- */

static void test_compares(void) {
    /* CMP equal */
    setup(0x0200);
    cpu.a = 0x42;
    uint8_t code1[] = { 0xC9, 0x42 }; /* CMP #$42 */
    place_code(0x0200, code1, sizeof(code1));
    cpu_step(&cpu);
    ASSERT_FLAG(&cpu, CPU_FLAG_Z, 1, "CMP equal sets Z");
    ASSERT_FLAG(&cpu, CPU_FLAG_C, 1, "CMP equal sets C (A >= M)");
    ASSERT_FLAG(&cpu, CPU_FLAG_N, 0, "CMP equal clears N");

    /* CMP greater */
    setup(0x0200);
    cpu.a = 0x50;
    uint8_t code2[] = { 0xC9, 0x30 }; /* CMP #$30 */
    place_code(0x0200, code2, sizeof(code2));
    cpu_step(&cpu);
    ASSERT_FLAG(&cpu, CPU_FLAG_Z, 0, "CMP greater clears Z");
    ASSERT_FLAG(&cpu, CPU_FLAG_C, 1, "CMP greater sets C");

    /* CMP less */
    setup(0x0200);
    cpu.a = 0x10;
    uint8_t code3[] = { 0xC9, 0x20 }; /* CMP #$20 */
    place_code(0x0200, code3, sizeof(code3));
    cpu_step(&cpu);
    ASSERT_FLAG(&cpu, CPU_FLAG_Z, 0, "CMP less clears Z");
    ASSERT_FLAG(&cpu, CPU_FLAG_C, 0, "CMP less clears C");
    ASSERT_FLAG(&cpu, CPU_FLAG_N, 1, "CMP less sets N (result $F0)");

    /* CPX */
    setup(0x0200);
    cpu.x = 0x80;
    uint8_t code4[] = { 0xE0, 0x80 }; /* CPX #$80 */
    place_code(0x0200, code4, sizeof(code4));
    cpu_step(&cpu);
    ASSERT_FLAG(&cpu, CPU_FLAG_Z, 1, "CPX equal sets Z");
    ASSERT_FLAG(&cpu, CPU_FLAG_C, 1, "CPX equal sets C");

    /* CPY */
    setup(0x0200);
    cpu.y = 0x10;
    uint8_t code5[] = { 0xC0, 0x20 }; /* CPY #$20 */
    place_code(0x0200, code5, sizeof(code5));
    cpu_step(&cpu);
    ASSERT_FLAG(&cpu, CPU_FLAG_C, 0, "CPY less clears C");
}

/* -------------------------------------------------------------------------- */
/* (k) Increment/Decrement tests                                               */
/* -------------------------------------------------------------------------- */

static void test_inc_dec(void) {
    uint8_t cycles;

    /* INC zero page */
    setup(0x0200);
    test_mem[0x10] = 0x41;
    uint8_t code1[] = { 0xE6, 0x10 }; /* INC $10 */
    place_code(0x0200, code1, sizeof(code1));
    cycles = cpu_step(&cpu);
    ASSERT_EQ(0x42, test_mem[0x10], "INC zero page increments");
    ASSERT_EQ(5, cycles, "INC zero page takes 5 cycles");

    /* INC wraps from 0xFF to 0x00 */
    setup(0x0200);
    test_mem[0x10] = 0xFF;
    uint8_t code2[] = { 0xE6, 0x10 }; /* INC $10 */
    place_code(0x0200, code2, sizeof(code2));
    cpu_step(&cpu);
    ASSERT_EQ(0x00, test_mem[0x10], "INC wraps 0xFF to 0x00");
    ASSERT_FLAG(&cpu, CPU_FLAG_Z, 1, "INC to zero sets Z");

    /* DEC zero page */
    setup(0x0200);
    test_mem[0x10] = 0x01;
    uint8_t code3[] = { 0xC6, 0x10 }; /* DEC $10 */
    place_code(0x0200, code3, sizeof(code3));
    cycles = cpu_step(&cpu);
    ASSERT_EQ(0x00, test_mem[0x10], "DEC zero page decrements to 0");
    ASSERT_FLAG(&cpu, CPU_FLAG_Z, 1, "DEC to zero sets Z");
    ASSERT_EQ(5, cycles, "DEC zero page takes 5 cycles");

    /* DEC wraps from 0x00 to 0xFF */
    setup(0x0200);
    test_mem[0x10] = 0x00;
    uint8_t code4[] = { 0xC6, 0x10 }; /* DEC $10 */
    place_code(0x0200, code4, sizeof(code4));
    cpu_step(&cpu);
    ASSERT_EQ(0xFF, test_mem[0x10], "DEC wraps 0x00 to 0xFF");
    ASSERT_FLAG(&cpu, CPU_FLAG_N, 1, "DEC to 0xFF sets N");

    /* INX */
    setup(0x0200);
    cpu.x = 0x7F;
    uint8_t code5[] = { 0xE8 }; /* INX */
    place_code(0x0200, code5, sizeof(code5));
    cpu_step(&cpu);
    ASSERT_EQ(0x80, cpu.x, "INX increments X");
    ASSERT_FLAG(&cpu, CPU_FLAG_N, 1, "INX to 0x80 sets N");

    /* INY */
    setup(0x0200);
    cpu.y = 0xFF;
    uint8_t code6[] = { 0xC8 }; /* INY */
    place_code(0x0200, code6, sizeof(code6));
    cpu_step(&cpu);
    ASSERT_EQ(0x00, cpu.y, "INY wraps 0xFF to 0x00");
    ASSERT_FLAG(&cpu, CPU_FLAG_Z, 1, "INY wrap to zero sets Z");

    /* DEX */
    setup(0x0200);
    cpu.x = 0x00;
    uint8_t code7[] = { 0xCA }; /* DEX */
    place_code(0x0200, code7, sizeof(code7));
    cpu_step(&cpu);
    ASSERT_EQ(0xFF, cpu.x, "DEX wraps 0x00 to 0xFF");

    /* DEY */
    setup(0x0200);
    cpu.y = 0x01;
    uint8_t code8[] = { 0x88 }; /* DEY */
    place_code(0x0200, code8, sizeof(code8));
    cpu_step(&cpu);
    ASSERT_EQ(0x00, cpu.y, "DEY decrements Y to 0");
    ASSERT_FLAG(&cpu, CPU_FLAG_Z, 1, "DEY to zero sets Z");
}

/* -------------------------------------------------------------------------- */
/* (l) Flag tests                                                              */
/* -------------------------------------------------------------------------- */

static void test_flags(void) {
    /* SEC */
    setup(0x0200);
    cpu.status &= ~CPU_FLAG_C;
    uint8_t code1[] = { 0x38 }; /* SEC */
    place_code(0x0200, code1, sizeof(code1));
    cpu_step(&cpu);
    ASSERT_FLAG(&cpu, CPU_FLAG_C, 1, "SEC sets carry");

    /* CLC */
    setup(0x0200);
    cpu.status |= CPU_FLAG_C;
    uint8_t code2[] = { 0x18 }; /* CLC */
    place_code(0x0200, code2, sizeof(code2));
    cpu_step(&cpu);
    ASSERT_FLAG(&cpu, CPU_FLAG_C, 0, "CLC clears carry");

    /* SEI */
    setup(0x0200);
    cpu.status &= ~CPU_FLAG_I;
    uint8_t code3[] = { 0x78 }; /* SEI */
    place_code(0x0200, code3, sizeof(code3));
    cpu_step(&cpu);
    ASSERT_FLAG(&cpu, CPU_FLAG_I, 1, "SEI sets interrupt disable");

    /* CLI */
    setup(0x0200);
    cpu.status |= CPU_FLAG_I;
    uint8_t code4[] = { 0x58 }; /* CLI */
    place_code(0x0200, code4, sizeof(code4));
    cpu_step(&cpu);
    ASSERT_FLAG(&cpu, CPU_FLAG_I, 0, "CLI clears interrupt disable");

    /* SED */
    setup(0x0200);
    cpu.status &= ~CPU_FLAG_D;
    uint8_t code5[] = { 0xF8 }; /* SED */
    place_code(0x0200, code5, sizeof(code5));
    cpu_step(&cpu);
    ASSERT_FLAG(&cpu, CPU_FLAG_D, 1, "SED sets decimal mode");

    /* CLD */
    setup(0x0200);
    cpu.status |= CPU_FLAG_D;
    uint8_t code6[] = { 0xD8 }; /* CLD */
    place_code(0x0200, code6, sizeof(code6));
    cpu_step(&cpu);
    ASSERT_FLAG(&cpu, CPU_FLAG_D, 0, "CLD clears decimal mode");

    /* CLV */
    setup(0x0200);
    cpu.status |= CPU_FLAG_V;
    uint8_t code7[] = { 0xB8 }; /* CLV */
    place_code(0x0200, code7, sizeof(code7));
    cpu_step(&cpu);
    ASSERT_FLAG(&cpu, CPU_FLAG_V, 0, "CLV clears overflow");
}

/* -------------------------------------------------------------------------- */
/* (m) Interrupt tests                                                         */
/* -------------------------------------------------------------------------- */

static void test_interrupts(void) {
    uint8_t cycles;

    /* BRK pushes PC+2 and status with B flag set */
    setup(0x0200);
    /* Set IRQ vector to $0400 */
    test_mem[0xFFFE] = 0x00;
    test_mem[0xFFFF] = 0x04;
    cpu.status = CPU_FLAG_U | CPU_FLAG_C; /* Some flags set */
    cpu.sp = 0xFD;
    uint8_t code1[] = { 0x00, 0xFF }; /* BRK (with padding byte) */
    place_code(0x0200, code1, sizeof(code1));
    cycles = cpu_step(&cpu);

    ASSERT_EQ16(0x0400, cpu.pc, "BRK jumps to IRQ vector");
    ASSERT_EQ(7, cycles, "BRK takes 7 cycles");
    ASSERT_EQ(0xFA, cpu.sp, "BRK pushes 3 bytes (PC_hi, PC_lo, status)");

    /* Check pushed PC is $0202 (BRK address + 2) */
    uint8_t pushed_pcl = test_mem[0x01FC];
    uint8_t pushed_pch = test_mem[0x01FD];
    uint16_t pushed_pc = (uint16_t)(pushed_pcl | (pushed_pch << 8));
    ASSERT_EQ16(0x0202, pushed_pc, "BRK pushes PC+2 onto stack");

    /* Check pushed status has B flag set */
    uint8_t pushed_status = test_mem[0x01FB];
    ASSERT_EQ(1, (pushed_status & CPU_FLAG_B) ? 1 : 0,
              "BRK pushes status with B flag set");
    ASSERT_EQ(1, (pushed_status & CPU_FLAG_U) ? 1 : 0,
              "BRK pushes status with U flag set");

    /* Verify I flag is set after BRK */
    ASSERT_FLAG(&cpu, CPU_FLAG_I, 1, "BRK sets I flag");

    /* IRQ (via cpu_irq) - pushes status without B flag */
    setup(0x0200);
    test_mem[0xFFFE] = 0x00;
    test_mem[0xFFFF] = 0x05;
    cpu.status = CPU_FLAG_U | CPU_FLAG_C; /* I flag clear = interrupts enabled */
    cpu.sp = 0xFD;
    cpu.pc = 0x0200;
    cpu_irq(&cpu);
    cycles = cpu_step(&cpu);

    ASSERT_EQ16(0x0500, cpu.pc, "IRQ jumps to IRQ vector");
    ASSERT_EQ(7, cycles, "IRQ takes 7 cycles");

    /* Check pushed status does NOT have B flag */
    uint8_t irq_status = test_mem[0x01FB];
    ASSERT_EQ(0, (irq_status & CPU_FLAG_B) ? 1 : 0,
              "IRQ pushes status without B flag (distinguishes from BRK)");
}

/* ========================================================================== */
/* Main                                                                        */
/* ========================================================================== */

int main(void) {
    test_load_store();
    test_arithmetic();
    test_bcd();
    test_logic();
    test_shifts();
    test_branches();
    test_jumps();
    test_stack();
    test_transfers();
    test_compares();
    test_inc_dec();
    test_flags();
    test_interrupts();

    /* Print TAP plan */
    int total = test_num - 1;
    printf("1..%d\n", total);

    if (failures == 0) {
        printf("# All %d tests passed\n", total);
    } else {
        printf("# %d of %d tests FAILED\n", failures, total);
    }

    return failures == 0 ? 0 : 1;
}
