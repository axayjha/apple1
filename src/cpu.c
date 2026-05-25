/*
 * MOS 6502 CPU Emulation Core
 *
 * Full implementation of all 56 official instructions (151 opcode variants)
 * with correct cycle timing, BCD mode, and interrupt handling.
 * Designed to pass the Klaus Dormann 6502 functional test suite.
 */

#include "cpu.h"
#include <string.h>

/* -------------------------------------------------------------------------- */
/* Internal helpers                                                            */
/* -------------------------------------------------------------------------- */

static inline uint8_t cpu_read(cpu_state *cpu, uint16_t addr) {
    return cpu->bus_read(addr);
}

static inline void cpu_write(cpu_state *cpu, uint16_t addr, uint8_t val) {
    cpu->bus_write(addr, val);
}

static inline uint16_t cpu_read16(cpu_state *cpu, uint16_t addr) {
    uint8_t lo = cpu_read(cpu, addr);
    uint8_t hi = cpu_read(cpu, addr + 1);
    return (uint16_t)(lo | (hi << 8));
}

/* Read 16-bit value with the 6502 page-boundary bug (used by JMP indirect).
 * If the low byte of the pointer is 0xFF, the high byte wraps within the page. */
static inline uint16_t cpu_read16_bug(cpu_state *cpu, uint16_t addr) {
    uint8_t lo = cpu_read(cpu, addr);
    uint16_t hi_addr = (addr & 0xFF00) | ((addr + 1) & 0x00FF);
    uint8_t hi = cpu_read(cpu, hi_addr);
    return (uint16_t)(lo | (hi << 8));
}

static inline void cpu_push(cpu_state *cpu, uint8_t val) {
    cpu_write(cpu, 0x0100 | cpu->sp, val);
    cpu->sp--;
}

static inline uint8_t cpu_pull(cpu_state *cpu) {
    cpu->sp++;
    return cpu_read(cpu, 0x0100 | cpu->sp);
}

static inline void cpu_push16(cpu_state *cpu, uint16_t val) {
    cpu_push(cpu, (uint8_t)(val >> 8));
    cpu_push(cpu, (uint8_t)(val & 0xFF));
}

static inline uint16_t cpu_pull16(cpu_state *cpu) {
    uint8_t lo = cpu_pull(cpu);
    uint8_t hi = cpu_pull(cpu);
    return (uint16_t)(lo | (hi << 8));
}

/* Flag manipulation */
static inline void cpu_set_flag(cpu_state *cpu, uint8_t flag, bool val) {
    if (val)
        cpu->status |= flag;
    else
        cpu->status &= ~flag;
}

static inline bool cpu_get_flag(cpu_state *cpu, uint8_t flag) {
    return (cpu->status & flag) != 0;
}

static inline void cpu_update_nz(cpu_state *cpu, uint8_t val) {
    cpu_set_flag(cpu, CPU_FLAG_Z, val == 0);
    cpu_set_flag(cpu, CPU_FLAG_N, (val & 0x80) != 0);
}

/* -------------------------------------------------------------------------- */
/* Addressing mode helpers - return the effective address                      */
/* -------------------------------------------------------------------------- */

static inline uint16_t addr_imm(cpu_state *cpu) {
    return cpu->pc++;
}

static inline uint16_t addr_zp(cpu_state *cpu) {
    return cpu_read(cpu, cpu->pc++);
}

static inline uint16_t addr_zpx(cpu_state *cpu) {
    return (cpu_read(cpu, cpu->pc++) + cpu->x) & 0xFF;
}

static inline uint16_t addr_zpy(cpu_state *cpu) {
    return (cpu_read(cpu, cpu->pc++) + cpu->y) & 0xFF;
}

static inline uint16_t addr_abs(cpu_state *cpu) {
    uint16_t addr = cpu_read16(cpu, cpu->pc);
    cpu->pc += 2;
    return addr;
}

static inline uint16_t addr_abx(cpu_state *cpu, bool page_penalty) {
    uint16_t base = cpu_read16(cpu, cpu->pc);
    cpu->pc += 2;
    uint16_t addr = base + cpu->x;
    if (page_penalty && ((base & 0xFF00) != (addr & 0xFF00)))
        cpu->step_cycles++;
    return addr;
}

static inline uint16_t addr_aby(cpu_state *cpu, bool page_penalty) {
    uint16_t base = cpu_read16(cpu, cpu->pc);
    cpu->pc += 2;
    uint16_t addr = base + cpu->y;
    if (page_penalty && ((base & 0xFF00) != (addr & 0xFF00)))
        cpu->step_cycles++;
    return addr;
}

static inline uint16_t addr_ind(cpu_state *cpu) {
    uint16_t ptr = cpu_read16(cpu, cpu->pc);
    cpu->pc += 2;
    return cpu_read16_bug(cpu, ptr);
}

/* Indexed Indirect (X) - (zp,X) */
static inline uint16_t addr_izx(cpu_state *cpu) {
    uint8_t zp = cpu_read(cpu, cpu->pc++);
    uint8_t ptr = (zp + cpu->x) & 0xFF;
    uint8_t lo = cpu_read(cpu, ptr);
    uint8_t hi = cpu_read(cpu, (ptr + 1) & 0xFF);
    return (uint16_t)(lo | (hi << 8));
}

/* Indirect Indexed (Y) - (zp),Y */
static inline uint16_t addr_izy(cpu_state *cpu, bool page_penalty) {
    uint8_t zp = cpu_read(cpu, cpu->pc++);
    uint8_t lo = cpu_read(cpu, zp);
    uint8_t hi = cpu_read(cpu, (zp + 1) & 0xFF);
    uint16_t base = (uint16_t)(lo | (hi << 8));
    uint16_t addr = base + cpu->y;
    if (page_penalty && ((base & 0xFF00) != (addr & 0xFF00)))
        cpu->step_cycles++;
    return addr;
}

static inline uint16_t addr_rel(cpu_state *cpu) {
    int8_t offset = (int8_t)cpu_read(cpu, cpu->pc++);
    return cpu->pc + offset;
}

/* -------------------------------------------------------------------------- */
/* Instruction implementations                                                 */
/* -------------------------------------------------------------------------- */

/* ADC - Add with Carry */
static inline void do_adc(cpu_state *cpu, uint8_t val) {
    uint8_t a = cpu->a;
    uint8_t c = cpu_get_flag(cpu, CPU_FLAG_C) ? 1 : 0;

    if (cpu_get_flag(cpu, CPU_FLAG_D)) {
        /* BCD mode */
        uint8_t al = (a & 0x0F) + (val & 0x0F) + c;
        if (al > 9) al += 6;
        uint8_t ah = (a >> 4) + (val >> 4) + (al > 0x0F ? 1 : 0);

        /* Zero flag is set based on binary result */
        uint16_t bin = (uint16_t)a + (uint16_t)val + (uint16_t)c;
        cpu_set_flag(cpu, CPU_FLAG_Z, (bin & 0xFF) == 0);

        /* Negative flag and overflow based on BCD high nibble */
        cpu_set_flag(cpu, CPU_FLAG_N, (ah & 0x08) != 0);
        cpu_set_flag(cpu, CPU_FLAG_V, ((~(a ^ val) & (a ^ (ah << 4))) & 0x80) != 0);

        if (ah > 9) ah += 6;
        cpu_set_flag(cpu, CPU_FLAG_C, ah > 0x0F);
        cpu->a = ((ah & 0x0F) << 4) | (al & 0x0F);
    } else {
        uint16_t sum = (uint16_t)a + (uint16_t)val + (uint16_t)c;
        cpu->a = (uint8_t)sum;
        cpu_set_flag(cpu, CPU_FLAG_C, sum > 0xFF);
        cpu_set_flag(cpu, CPU_FLAG_V, ((~(a ^ val) & (a ^ cpu->a)) & 0x80) != 0);
        cpu_update_nz(cpu, cpu->a);
    }
}

/* SBC - Subtract with Carry */
static inline void do_sbc(cpu_state *cpu, uint8_t val) {
    uint8_t a = cpu->a;
    uint8_t c = cpu_get_flag(cpu, CPU_FLAG_C) ? 1 : 0;

    if (cpu_get_flag(cpu, CPU_FLAG_D)) {
        /* BCD mode */
        int16_t al = (a & 0x0F) - (val & 0x0F) + c - 1;
        int16_t ah;
        if (al < 0) {
            al = ((al - 6) & 0x0F);
            ah = (a >> 4) - (val >> 4) - 1;
        } else {
            ah = (a >> 4) - (val >> 4);
        }

        /* Flags based on binary result */
        uint16_t bin = (uint16_t)a - (uint16_t)val + (uint16_t)c - 1;
        cpu_set_flag(cpu, CPU_FLAG_Z, (bin & 0xFF) == 0);
        cpu_set_flag(cpu, CPU_FLAG_N, (bin & 0x80) != 0);
        cpu_set_flag(cpu, CPU_FLAG_V, (((a ^ val) & (a ^ (uint8_t)bin)) & 0x80) != 0);
        cpu_set_flag(cpu, CPU_FLAG_C, bin < 0x100);

        if (ah < 0) ah -= 6;
        cpu->a = ((ah & 0x0F) << 4) | (al & 0x0F);
    } else {
        uint16_t diff = (uint16_t)a - (uint16_t)val + (uint16_t)c - 1;
        cpu->a = (uint8_t)diff;
        cpu_set_flag(cpu, CPU_FLAG_C, diff < 0x100);
        cpu_set_flag(cpu, CPU_FLAG_V, (((a ^ val) & (a ^ cpu->a)) & 0x80) != 0);
        cpu_update_nz(cpu, cpu->a);
    }
}

/* Compare helper */
static inline void do_cmp(cpu_state *cpu, uint8_t reg, uint8_t val) {
    uint16_t result = (uint16_t)reg - (uint16_t)val;
    cpu_set_flag(cpu, CPU_FLAG_C, reg >= val);
    cpu_update_nz(cpu, (uint8_t)result);
}

/* Branch helper */
static inline void do_branch(cpu_state *cpu, bool condition) {
    uint16_t target = addr_rel(cpu);
    if (condition) {
        cpu->step_cycles++;
        if ((cpu->pc & 0xFF00) != (target & 0xFF00))
            cpu->step_cycles++;
        cpu->pc = target;
    }
}

/* -------------------------------------------------------------------------- */
/* Opcode handlers                                                             */
/* Each handler is a function taking cpu_state* matching the dispatch table    */
/* -------------------------------------------------------------------------- */

/* BRK - Force Interrupt */
static void op_00(cpu_state *cpu) {
    cpu->pc++; /* BRK pushes PC+2 (skips padding byte) */
    cpu_push16(cpu, cpu->pc);
    cpu_push(cpu, cpu->status | CPU_FLAG_B | CPU_FLAG_U);
    cpu_set_flag(cpu, CPU_FLAG_I, true);
    cpu->pc = cpu_read16(cpu, CPU_VEC_IRQ);
    cpu->step_cycles = 7;
}

/* ORA (zp,X) */
static void op_01(cpu_state *cpu) {
    uint16_t addr = addr_izx(cpu);
    cpu->a |= cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles = 6;
}

/* ORA zp */
static void op_05(cpu_state *cpu) {
    uint16_t addr = addr_zp(cpu);
    cpu->a |= cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles = 3;
}

/* ASL zp */
static void op_06(cpu_state *cpu) {
    uint16_t addr = addr_zp(cpu);
    uint8_t val = cpu_read(cpu, addr);
    cpu_set_flag(cpu, CPU_FLAG_C, (val & 0x80) != 0);
    val <<= 1;
    cpu_write(cpu, addr, val);
    cpu_update_nz(cpu, val);
    cpu->step_cycles = 5;
}

/* PHP - Push Processor Status */
static void op_08(cpu_state *cpu) {
    cpu_push(cpu, cpu->status | CPU_FLAG_B | CPU_FLAG_U);
    cpu->step_cycles = 3;
}

/* ORA #imm */
static void op_09(cpu_state *cpu) {
    cpu->a |= cpu_read(cpu, addr_imm(cpu));
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles = 2;
}

/* ASL A */
static void op_0a(cpu_state *cpu) {
    cpu_set_flag(cpu, CPU_FLAG_C, (cpu->a & 0x80) != 0);
    cpu->a <<= 1;
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles = 2;
}

/* ORA abs */
static void op_0d(cpu_state *cpu) {
    uint16_t addr = addr_abs(cpu);
    cpu->a |= cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles = 4;
}

/* ASL abs */
static void op_0e(cpu_state *cpu) {
    uint16_t addr = addr_abs(cpu);
    uint8_t val = cpu_read(cpu, addr);
    cpu_set_flag(cpu, CPU_FLAG_C, (val & 0x80) != 0);
    val <<= 1;
    cpu_write(cpu, addr, val);
    cpu_update_nz(cpu, val);
    cpu->step_cycles = 6;
}

/* BPL rel */
static void op_10(cpu_state *cpu) {
    cpu->step_cycles = 2;
    do_branch(cpu, !cpu_get_flag(cpu, CPU_FLAG_N));
}

/* ORA (zp),Y */
static void op_11(cpu_state *cpu) {
    uint16_t addr = addr_izy(cpu, true);
    cpu->a |= cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles += 5;
}

/* ORA zp,X */
static void op_15(cpu_state *cpu) {
    uint16_t addr = addr_zpx(cpu);
    cpu->a |= cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles = 4;
}

/* ASL zp,X */
static void op_16(cpu_state *cpu) {
    uint16_t addr = addr_zpx(cpu);
    uint8_t val = cpu_read(cpu, addr);
    cpu_set_flag(cpu, CPU_FLAG_C, (val & 0x80) != 0);
    val <<= 1;
    cpu_write(cpu, addr, val);
    cpu_update_nz(cpu, val);
    cpu->step_cycles = 6;
}

/* CLC */
static void op_18(cpu_state *cpu) {
    cpu_set_flag(cpu, CPU_FLAG_C, false);
    cpu->step_cycles = 2;
}

/* ORA abs,Y */
static void op_19(cpu_state *cpu) {
    uint16_t addr = addr_aby(cpu, true);
    cpu->a |= cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles += 4;
}

/* ORA abs,X */
static void op_1d(cpu_state *cpu) {
    uint16_t addr = addr_abx(cpu, true);
    cpu->a |= cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles += 4;
}

/* ASL abs,X */
static void op_1e(cpu_state *cpu) {
    uint16_t addr = addr_abx(cpu, false);
    uint8_t val = cpu_read(cpu, addr);
    cpu_set_flag(cpu, CPU_FLAG_C, (val & 0x80) != 0);
    val <<= 1;
    cpu_write(cpu, addr, val);
    cpu_update_nz(cpu, val);
    cpu->step_cycles = 7;
}

/* JSR abs */
static void op_20(cpu_state *cpu) {
    uint16_t addr = addr_abs(cpu);
    cpu_push16(cpu, cpu->pc - 1);
    cpu->pc = addr;
    cpu->step_cycles = 6;
}

/* AND (zp,X) */
static void op_21(cpu_state *cpu) {
    uint16_t addr = addr_izx(cpu);
    cpu->a &= cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles = 6;
}

/* BIT zp */
static void op_24(cpu_state *cpu) {
    uint16_t addr = addr_zp(cpu);
    uint8_t val = cpu_read(cpu, addr);
    cpu_set_flag(cpu, CPU_FLAG_Z, (cpu->a & val) == 0);
    cpu_set_flag(cpu, CPU_FLAG_N, (val & 0x80) != 0);
    cpu_set_flag(cpu, CPU_FLAG_V, (val & 0x40) != 0);
    cpu->step_cycles = 3;
}

/* AND zp */
static void op_25(cpu_state *cpu) {
    uint16_t addr = addr_zp(cpu);
    cpu->a &= cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles = 3;
}

/* ROL zp */
static void op_26(cpu_state *cpu) {
    uint16_t addr = addr_zp(cpu);
    uint8_t val = cpu_read(cpu, addr);
    uint8_t old_c = cpu_get_flag(cpu, CPU_FLAG_C) ? 1 : 0;
    cpu_set_flag(cpu, CPU_FLAG_C, (val & 0x80) != 0);
    val = (val << 1) | old_c;
    cpu_write(cpu, addr, val);
    cpu_update_nz(cpu, val);
    cpu->step_cycles = 5;
}

/* PLP - Pull Processor Status */
static void op_28(cpu_state *cpu) {
    cpu->status = (cpu_pull(cpu) & ~CPU_FLAG_B) | CPU_FLAG_U;
    cpu->step_cycles = 4;
}

/* AND #imm */
static void op_29(cpu_state *cpu) {
    cpu->a &= cpu_read(cpu, addr_imm(cpu));
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles = 2;
}

/* ROL A */
static void op_2a(cpu_state *cpu) {
    uint8_t old_c = cpu_get_flag(cpu, CPU_FLAG_C) ? 1 : 0;
    cpu_set_flag(cpu, CPU_FLAG_C, (cpu->a & 0x80) != 0);
    cpu->a = (cpu->a << 1) | old_c;
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles = 2;
}

/* BIT abs */
static void op_2c(cpu_state *cpu) {
    uint16_t addr = addr_abs(cpu);
    uint8_t val = cpu_read(cpu, addr);
    cpu_set_flag(cpu, CPU_FLAG_Z, (cpu->a & val) == 0);
    cpu_set_flag(cpu, CPU_FLAG_N, (val & 0x80) != 0);
    cpu_set_flag(cpu, CPU_FLAG_V, (val & 0x40) != 0);
    cpu->step_cycles = 4;
}

/* AND abs */
static void op_2d(cpu_state *cpu) {
    uint16_t addr = addr_abs(cpu);
    cpu->a &= cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles = 4;
}

/* ROL abs */
static void op_2e(cpu_state *cpu) {
    uint16_t addr = addr_abs(cpu);
    uint8_t val = cpu_read(cpu, addr);
    uint8_t old_c = cpu_get_flag(cpu, CPU_FLAG_C) ? 1 : 0;
    cpu_set_flag(cpu, CPU_FLAG_C, (val & 0x80) != 0);
    val = (val << 1) | old_c;
    cpu_write(cpu, addr, val);
    cpu_update_nz(cpu, val);
    cpu->step_cycles = 6;
}

/* BMI rel */
static void op_30(cpu_state *cpu) {
    cpu->step_cycles = 2;
    do_branch(cpu, cpu_get_flag(cpu, CPU_FLAG_N));
}

/* AND (zp),Y */
static void op_31(cpu_state *cpu) {
    uint16_t addr = addr_izy(cpu, true);
    cpu->a &= cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles += 5;
}

/* AND zp,X */
static void op_35(cpu_state *cpu) {
    uint16_t addr = addr_zpx(cpu);
    cpu->a &= cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles = 4;
}

/* ROL zp,X */
static void op_36(cpu_state *cpu) {
    uint16_t addr = addr_zpx(cpu);
    uint8_t val = cpu_read(cpu, addr);
    uint8_t old_c = cpu_get_flag(cpu, CPU_FLAG_C) ? 1 : 0;
    cpu_set_flag(cpu, CPU_FLAG_C, (val & 0x80) != 0);
    val = (val << 1) | old_c;
    cpu_write(cpu, addr, val);
    cpu_update_nz(cpu, val);
    cpu->step_cycles = 6;
}

/* SEC */
static void op_38(cpu_state *cpu) {
    cpu_set_flag(cpu, CPU_FLAG_C, true);
    cpu->step_cycles = 2;
}

/* AND abs,Y */
static void op_39(cpu_state *cpu) {
    uint16_t addr = addr_aby(cpu, true);
    cpu->a &= cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles += 4;
}

/* AND abs,X */
static void op_3d(cpu_state *cpu) {
    uint16_t addr = addr_abx(cpu, true);
    cpu->a &= cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles += 4;
}

/* ROL abs,X */
static void op_3e(cpu_state *cpu) {
    uint16_t addr = addr_abx(cpu, false);
    uint8_t val = cpu_read(cpu, addr);
    uint8_t old_c = cpu_get_flag(cpu, CPU_FLAG_C) ? 1 : 0;
    cpu_set_flag(cpu, CPU_FLAG_C, (val & 0x80) != 0);
    val = (val << 1) | old_c;
    cpu_write(cpu, addr, val);
    cpu_update_nz(cpu, val);
    cpu->step_cycles = 7;
}

/* RTI - Return from Interrupt */
static void op_40(cpu_state *cpu) {
    cpu->status = (cpu_pull(cpu) & ~CPU_FLAG_B) | CPU_FLAG_U;
    cpu->pc = cpu_pull16(cpu);
    cpu->step_cycles = 6;
}

/* EOR (zp,X) */
static void op_41(cpu_state *cpu) {
    uint16_t addr = addr_izx(cpu);
    cpu->a ^= cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles = 6;
}

/* EOR zp */
static void op_45(cpu_state *cpu) {
    uint16_t addr = addr_zp(cpu);
    cpu->a ^= cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles = 3;
}

/* LSR zp */
static void op_46(cpu_state *cpu) {
    uint16_t addr = addr_zp(cpu);
    uint8_t val = cpu_read(cpu, addr);
    cpu_set_flag(cpu, CPU_FLAG_C, (val & 0x01) != 0);
    val >>= 1;
    cpu_write(cpu, addr, val);
    cpu_update_nz(cpu, val);
    cpu->step_cycles = 5;
}

/* PHA - Push Accumulator */
static void op_48(cpu_state *cpu) {
    cpu_push(cpu, cpu->a);
    cpu->step_cycles = 3;
}

/* EOR #imm */
static void op_49(cpu_state *cpu) {
    cpu->a ^= cpu_read(cpu, addr_imm(cpu));
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles = 2;
}

/* LSR A */
static void op_4a(cpu_state *cpu) {
    cpu_set_flag(cpu, CPU_FLAG_C, (cpu->a & 0x01) != 0);
    cpu->a >>= 1;
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles = 2;
}

/* JMP abs */
static void op_4c(cpu_state *cpu) {
    cpu->pc = addr_abs(cpu);
    cpu->step_cycles = 3;
}

/* EOR abs */
static void op_4d(cpu_state *cpu) {
    uint16_t addr = addr_abs(cpu);
    cpu->a ^= cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles = 4;
}

/* LSR abs */
static void op_4e(cpu_state *cpu) {
    uint16_t addr = addr_abs(cpu);
    uint8_t val = cpu_read(cpu, addr);
    cpu_set_flag(cpu, CPU_FLAG_C, (val & 0x01) != 0);
    val >>= 1;
    cpu_write(cpu, addr, val);
    cpu_update_nz(cpu, val);
    cpu->step_cycles = 6;
}

/* BVC rel */
static void op_50(cpu_state *cpu) {
    cpu->step_cycles = 2;
    do_branch(cpu, !cpu_get_flag(cpu, CPU_FLAG_V));
}

/* EOR (zp),Y */
static void op_51(cpu_state *cpu) {
    uint16_t addr = addr_izy(cpu, true);
    cpu->a ^= cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles += 5;
}

/* EOR zp,X */
static void op_55(cpu_state *cpu) {
    uint16_t addr = addr_zpx(cpu);
    cpu->a ^= cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles = 4;
}

/* LSR zp,X */
static void op_56(cpu_state *cpu) {
    uint16_t addr = addr_zpx(cpu);
    uint8_t val = cpu_read(cpu, addr);
    cpu_set_flag(cpu, CPU_FLAG_C, (val & 0x01) != 0);
    val >>= 1;
    cpu_write(cpu, addr, val);
    cpu_update_nz(cpu, val);
    cpu->step_cycles = 6;
}

/* CLI */
static void op_58(cpu_state *cpu) {
    cpu_set_flag(cpu, CPU_FLAG_I, false);
    cpu->step_cycles = 2;
}

/* EOR abs,Y */
static void op_59(cpu_state *cpu) {
    uint16_t addr = addr_aby(cpu, true);
    cpu->a ^= cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles += 4;
}

/* EOR abs,X */
static void op_5d(cpu_state *cpu) {
    uint16_t addr = addr_abx(cpu, true);
    cpu->a ^= cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles += 4;
}

/* LSR abs,X */
static void op_5e(cpu_state *cpu) {
    uint16_t addr = addr_abx(cpu, false);
    uint8_t val = cpu_read(cpu, addr);
    cpu_set_flag(cpu, CPU_FLAG_C, (val & 0x01) != 0);
    val >>= 1;
    cpu_write(cpu, addr, val);
    cpu_update_nz(cpu, val);
    cpu->step_cycles = 7;
}

/* RTS - Return from Subroutine */
static void op_60(cpu_state *cpu) {
    cpu->pc = cpu_pull16(cpu) + 1;
    cpu->step_cycles = 6;
}

/* ADC (zp,X) */
static void op_61(cpu_state *cpu) {
    uint16_t addr = addr_izx(cpu);
    do_adc(cpu, cpu_read(cpu, addr));
    cpu->step_cycles = 6;
}

/* ADC zp */
static void op_65(cpu_state *cpu) {
    uint16_t addr = addr_zp(cpu);
    do_adc(cpu, cpu_read(cpu, addr));
    cpu->step_cycles = 3;
}

/* ROR zp */
static void op_66(cpu_state *cpu) {
    uint16_t addr = addr_zp(cpu);
    uint8_t val = cpu_read(cpu, addr);
    uint8_t old_c = cpu_get_flag(cpu, CPU_FLAG_C) ? 0x80 : 0;
    cpu_set_flag(cpu, CPU_FLAG_C, (val & 0x01) != 0);
    val = (val >> 1) | old_c;
    cpu_write(cpu, addr, val);
    cpu_update_nz(cpu, val);
    cpu->step_cycles = 5;
}

/* PLA - Pull Accumulator */
static void op_68(cpu_state *cpu) {
    cpu->a = cpu_pull(cpu);
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles = 4;
}

/* ADC #imm */
static void op_69(cpu_state *cpu) {
    do_adc(cpu, cpu_read(cpu, addr_imm(cpu)));
    cpu->step_cycles = 2;
}

/* ROR A */
static void op_6a(cpu_state *cpu) {
    uint8_t old_c = cpu_get_flag(cpu, CPU_FLAG_C) ? 0x80 : 0;
    cpu_set_flag(cpu, CPU_FLAG_C, (cpu->a & 0x01) != 0);
    cpu->a = (cpu->a >> 1) | old_c;
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles = 2;
}

/* JMP (indirect) */
static void op_6c(cpu_state *cpu) {
    cpu->pc = addr_ind(cpu);
    cpu->step_cycles = 5;
}

/* ADC abs */
static void op_6d(cpu_state *cpu) {
    uint16_t addr = addr_abs(cpu);
    do_adc(cpu, cpu_read(cpu, addr));
    cpu->step_cycles = 4;
}

/* ROR abs */
static void op_6e(cpu_state *cpu) {
    uint16_t addr = addr_abs(cpu);
    uint8_t val = cpu_read(cpu, addr);
    uint8_t old_c = cpu_get_flag(cpu, CPU_FLAG_C) ? 0x80 : 0;
    cpu_set_flag(cpu, CPU_FLAG_C, (val & 0x01) != 0);
    val = (val >> 1) | old_c;
    cpu_write(cpu, addr, val);
    cpu_update_nz(cpu, val);
    cpu->step_cycles = 6;
}

/* BVS rel */
static void op_70(cpu_state *cpu) {
    cpu->step_cycles = 2;
    do_branch(cpu, cpu_get_flag(cpu, CPU_FLAG_V));
}

/* ADC (zp),Y */
static void op_71(cpu_state *cpu) {
    uint16_t addr = addr_izy(cpu, true);
    do_adc(cpu, cpu_read(cpu, addr));
    cpu->step_cycles += 5;
}

/* ADC zp,X */
static void op_75(cpu_state *cpu) {
    uint16_t addr = addr_zpx(cpu);
    do_adc(cpu, cpu_read(cpu, addr));
    cpu->step_cycles = 4;
}

/* ROR zp,X */
static void op_76(cpu_state *cpu) {
    uint16_t addr = addr_zpx(cpu);
    uint8_t val = cpu_read(cpu, addr);
    uint8_t old_c = cpu_get_flag(cpu, CPU_FLAG_C) ? 0x80 : 0;
    cpu_set_flag(cpu, CPU_FLAG_C, (val & 0x01) != 0);
    val = (val >> 1) | old_c;
    cpu_write(cpu, addr, val);
    cpu_update_nz(cpu, val);
    cpu->step_cycles = 6;
}

/* SEI */
static void op_78(cpu_state *cpu) {
    cpu_set_flag(cpu, CPU_FLAG_I, true);
    cpu->step_cycles = 2;
}

/* ADC abs,Y */
static void op_79(cpu_state *cpu) {
    uint16_t addr = addr_aby(cpu, true);
    do_adc(cpu, cpu_read(cpu, addr));
    cpu->step_cycles += 4;
}

/* ADC abs,X */
static void op_7d(cpu_state *cpu) {
    uint16_t addr = addr_abx(cpu, true);
    do_adc(cpu, cpu_read(cpu, addr));
    cpu->step_cycles += 4;
}

/* ROR abs,X */
static void op_7e(cpu_state *cpu) {
    uint16_t addr = addr_abx(cpu, false);
    uint8_t val = cpu_read(cpu, addr);
    uint8_t old_c = cpu_get_flag(cpu, CPU_FLAG_C) ? 0x80 : 0;
    cpu_set_flag(cpu, CPU_FLAG_C, (val & 0x01) != 0);
    val = (val >> 1) | old_c;
    cpu_write(cpu, addr, val);
    cpu_update_nz(cpu, val);
    cpu->step_cycles = 7;
}

/* STA (zp,X) */
static void op_81(cpu_state *cpu) {
    uint16_t addr = addr_izx(cpu);
    cpu_write(cpu, addr, cpu->a);
    cpu->step_cycles = 6;
}

/* STY zp */
static void op_84(cpu_state *cpu) {
    uint16_t addr = addr_zp(cpu);
    cpu_write(cpu, addr, cpu->y);
    cpu->step_cycles = 3;
}

/* STA zp */
static void op_85(cpu_state *cpu) {
    uint16_t addr = addr_zp(cpu);
    cpu_write(cpu, addr, cpu->a);
    cpu->step_cycles = 3;
}

/* STX zp */
static void op_86(cpu_state *cpu) {
    uint16_t addr = addr_zp(cpu);
    cpu_write(cpu, addr, cpu->x);
    cpu->step_cycles = 3;
}

/* DEY */
static void op_88(cpu_state *cpu) {
    cpu->y--;
    cpu_update_nz(cpu, cpu->y);
    cpu->step_cycles = 2;
}

/* TXA */
static void op_8a(cpu_state *cpu) {
    cpu->a = cpu->x;
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles = 2;
}

/* STY abs */
static void op_8c(cpu_state *cpu) {
    uint16_t addr = addr_abs(cpu);
    cpu_write(cpu, addr, cpu->y);
    cpu->step_cycles = 4;
}

/* STA abs */
static void op_8d(cpu_state *cpu) {
    uint16_t addr = addr_abs(cpu);
    cpu_write(cpu, addr, cpu->a);
    cpu->step_cycles = 4;
}

/* STX abs */
static void op_8e(cpu_state *cpu) {
    uint16_t addr = addr_abs(cpu);
    cpu_write(cpu, addr, cpu->x);
    cpu->step_cycles = 4;
}

/* BCC rel */
static void op_90(cpu_state *cpu) {
    cpu->step_cycles = 2;
    do_branch(cpu, !cpu_get_flag(cpu, CPU_FLAG_C));
}

/* STA (zp),Y */
static void op_91(cpu_state *cpu) {
    uint16_t addr = addr_izy(cpu, false);
    cpu_write(cpu, addr, cpu->a);
    cpu->step_cycles = 6;
}

/* STY zp,X */
static void op_94(cpu_state *cpu) {
    uint16_t addr = addr_zpx(cpu);
    cpu_write(cpu, addr, cpu->y);
    cpu->step_cycles = 4;
}

/* STA zp,X */
static void op_95(cpu_state *cpu) {
    uint16_t addr = addr_zpx(cpu);
    cpu_write(cpu, addr, cpu->a);
    cpu->step_cycles = 4;
}

/* STX zp,Y */
static void op_96(cpu_state *cpu) {
    uint16_t addr = addr_zpy(cpu);
    cpu_write(cpu, addr, cpu->x);
    cpu->step_cycles = 4;
}

/* TYA */
static void op_98(cpu_state *cpu) {
    cpu->a = cpu->y;
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles = 2;
}

/* STA abs,Y */
static void op_99(cpu_state *cpu) {
    uint16_t addr = addr_aby(cpu, false);
    cpu_write(cpu, addr, cpu->a);
    cpu->step_cycles = 5;
}

/* TXS */
static void op_9a(cpu_state *cpu) {
    cpu->sp = cpu->x;
    cpu->step_cycles = 2;
}

/* STA abs,X */
static void op_9d(cpu_state *cpu) {
    uint16_t addr = addr_abx(cpu, false);
    cpu_write(cpu, addr, cpu->a);
    cpu->step_cycles = 5;
}

/* LDY #imm */
static void op_a0(cpu_state *cpu) {
    cpu->y = cpu_read(cpu, addr_imm(cpu));
    cpu_update_nz(cpu, cpu->y);
    cpu->step_cycles = 2;
}

/* LDA (zp,X) */
static void op_a1(cpu_state *cpu) {
    uint16_t addr = addr_izx(cpu);
    cpu->a = cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles = 6;
}

/* LDX #imm */
static void op_a2(cpu_state *cpu) {
    cpu->x = cpu_read(cpu, addr_imm(cpu));
    cpu_update_nz(cpu, cpu->x);
    cpu->step_cycles = 2;
}

/* LDY zp */
static void op_a4(cpu_state *cpu) {
    uint16_t addr = addr_zp(cpu);
    cpu->y = cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->y);
    cpu->step_cycles = 3;
}

/* LDA zp */
static void op_a5(cpu_state *cpu) {
    uint16_t addr = addr_zp(cpu);
    cpu->a = cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles = 3;
}

/* LDX zp */
static void op_a6(cpu_state *cpu) {
    uint16_t addr = addr_zp(cpu);
    cpu->x = cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->x);
    cpu->step_cycles = 3;
}

/* TAY */
static void op_a8(cpu_state *cpu) {
    cpu->y = cpu->a;
    cpu_update_nz(cpu, cpu->y);
    cpu->step_cycles = 2;
}

/* LDA #imm */
static void op_a9(cpu_state *cpu) {
    cpu->a = cpu_read(cpu, addr_imm(cpu));
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles = 2;
}

/* TAX */
static void op_aa(cpu_state *cpu) {
    cpu->x = cpu->a;
    cpu_update_nz(cpu, cpu->x);
    cpu->step_cycles = 2;
}

/* LDY abs */
static void op_ac(cpu_state *cpu) {
    uint16_t addr = addr_abs(cpu);
    cpu->y = cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->y);
    cpu->step_cycles = 4;
}

/* LDA abs */
static void op_ad(cpu_state *cpu) {
    uint16_t addr = addr_abs(cpu);
    cpu->a = cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles = 4;
}

/* LDX abs */
static void op_ae(cpu_state *cpu) {
    uint16_t addr = addr_abs(cpu);
    cpu->x = cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->x);
    cpu->step_cycles = 4;
}

/* BCS rel */
static void op_b0(cpu_state *cpu) {
    cpu->step_cycles = 2;
    do_branch(cpu, cpu_get_flag(cpu, CPU_FLAG_C));
}

/* LDA (zp),Y */
static void op_b1(cpu_state *cpu) {
    uint16_t addr = addr_izy(cpu, true);
    cpu->a = cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles += 5;
}

/* LDY zp,X */
static void op_b4(cpu_state *cpu) {
    uint16_t addr = addr_zpx(cpu);
    cpu->y = cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->y);
    cpu->step_cycles = 4;
}

/* LDA zp,X */
static void op_b5(cpu_state *cpu) {
    uint16_t addr = addr_zpx(cpu);
    cpu->a = cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles = 4;
}

/* LDX zp,Y */
static void op_b6(cpu_state *cpu) {
    uint16_t addr = addr_zpy(cpu);
    cpu->x = cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->x);
    cpu->step_cycles = 4;
}

/* CLV */
static void op_b8(cpu_state *cpu) {
    cpu_set_flag(cpu, CPU_FLAG_V, false);
    cpu->step_cycles = 2;
}

/* LDA abs,Y */
static void op_b9(cpu_state *cpu) {
    uint16_t addr = addr_aby(cpu, true);
    cpu->a = cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles += 4;
}

/* TSX */
static void op_ba(cpu_state *cpu) {
    cpu->x = cpu->sp;
    cpu_update_nz(cpu, cpu->x);
    cpu->step_cycles = 2;
}

/* LDY abs,X */
static void op_bc(cpu_state *cpu) {
    uint16_t addr = addr_abx(cpu, true);
    cpu->y = cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->y);
    cpu->step_cycles += 4;
}

/* LDA abs,X */
static void op_bd(cpu_state *cpu) {
    uint16_t addr = addr_abx(cpu, true);
    cpu->a = cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->a);
    cpu->step_cycles += 4;
}

/* LDX abs,Y */
static void op_be(cpu_state *cpu) {
    uint16_t addr = addr_aby(cpu, true);
    cpu->x = cpu_read(cpu, addr);
    cpu_update_nz(cpu, cpu->x);
    cpu->step_cycles += 4;
}

/* CPY #imm */
static void op_c0(cpu_state *cpu) {
    do_cmp(cpu, cpu->y, cpu_read(cpu, addr_imm(cpu)));
    cpu->step_cycles = 2;
}

/* CMP (zp,X) */
static void op_c1(cpu_state *cpu) {
    uint16_t addr = addr_izx(cpu);
    do_cmp(cpu, cpu->a, cpu_read(cpu, addr));
    cpu->step_cycles = 6;
}

/* CPY zp */
static void op_c4(cpu_state *cpu) {
    uint16_t addr = addr_zp(cpu);
    do_cmp(cpu, cpu->y, cpu_read(cpu, addr));
    cpu->step_cycles = 3;
}

/* CMP zp */
static void op_c5(cpu_state *cpu) {
    uint16_t addr = addr_zp(cpu);
    do_cmp(cpu, cpu->a, cpu_read(cpu, addr));
    cpu->step_cycles = 3;
}

/* DEC zp */
static void op_c6(cpu_state *cpu) {
    uint16_t addr = addr_zp(cpu);
    uint8_t val = cpu_read(cpu, addr) - 1;
    cpu_write(cpu, addr, val);
    cpu_update_nz(cpu, val);
    cpu->step_cycles = 5;
}

/* INY */
static void op_c8(cpu_state *cpu) {
    cpu->y++;
    cpu_update_nz(cpu, cpu->y);
    cpu->step_cycles = 2;
}

/* CMP #imm */
static void op_c9(cpu_state *cpu) {
    do_cmp(cpu, cpu->a, cpu_read(cpu, addr_imm(cpu)));
    cpu->step_cycles = 2;
}

/* DEX */
static void op_ca(cpu_state *cpu) {
    cpu->x--;
    cpu_update_nz(cpu, cpu->x);
    cpu->step_cycles = 2;
}

/* CPY abs */
static void op_cc(cpu_state *cpu) {
    uint16_t addr = addr_abs(cpu);
    do_cmp(cpu, cpu->y, cpu_read(cpu, addr));
    cpu->step_cycles = 4;
}

/* CMP abs */
static void op_cd(cpu_state *cpu) {
    uint16_t addr = addr_abs(cpu);
    do_cmp(cpu, cpu->a, cpu_read(cpu, addr));
    cpu->step_cycles = 4;
}

/* DEC abs */
static void op_ce(cpu_state *cpu) {
    uint16_t addr = addr_abs(cpu);
    uint8_t val = cpu_read(cpu, addr) - 1;
    cpu_write(cpu, addr, val);
    cpu_update_nz(cpu, val);
    cpu->step_cycles = 6;
}

/* BNE rel */
static void op_d0(cpu_state *cpu) {
    cpu->step_cycles = 2;
    do_branch(cpu, !cpu_get_flag(cpu, CPU_FLAG_Z));
}

/* CMP (zp),Y */
static void op_d1(cpu_state *cpu) {
    uint16_t addr = addr_izy(cpu, true);
    do_cmp(cpu, cpu->a, cpu_read(cpu, addr));
    cpu->step_cycles += 5;
}

/* CMP zp,X */
static void op_d5(cpu_state *cpu) {
    uint16_t addr = addr_zpx(cpu);
    do_cmp(cpu, cpu->a, cpu_read(cpu, addr));
    cpu->step_cycles = 4;
}

/* DEC zp,X */
static void op_d6(cpu_state *cpu) {
    uint16_t addr = addr_zpx(cpu);
    uint8_t val = cpu_read(cpu, addr) - 1;
    cpu_write(cpu, addr, val);
    cpu_update_nz(cpu, val);
    cpu->step_cycles = 6;
}

/* CLD */
static void op_d8(cpu_state *cpu) {
    cpu_set_flag(cpu, CPU_FLAG_D, false);
    cpu->step_cycles = 2;
}

/* CMP abs,Y */
static void op_d9(cpu_state *cpu) {
    uint16_t addr = addr_aby(cpu, true);
    do_cmp(cpu, cpu->a, cpu_read(cpu, addr));
    cpu->step_cycles += 4;
}

/* CMP abs,X */
static void op_dd(cpu_state *cpu) {
    uint16_t addr = addr_abx(cpu, true);
    do_cmp(cpu, cpu->a, cpu_read(cpu, addr));
    cpu->step_cycles += 4;
}

/* DEC abs,X */
static void op_de(cpu_state *cpu) {
    uint16_t addr = addr_abx(cpu, false);
    uint8_t val = cpu_read(cpu, addr) - 1;
    cpu_write(cpu, addr, val);
    cpu_update_nz(cpu, val);
    cpu->step_cycles = 7;
}

/* CPX #imm */
static void op_e0(cpu_state *cpu) {
    do_cmp(cpu, cpu->x, cpu_read(cpu, addr_imm(cpu)));
    cpu->step_cycles = 2;
}

/* SBC (zp,X) */
static void op_e1(cpu_state *cpu) {
    uint16_t addr = addr_izx(cpu);
    do_sbc(cpu, cpu_read(cpu, addr));
    cpu->step_cycles = 6;
}

/* CPX zp */
static void op_e4(cpu_state *cpu) {
    uint16_t addr = addr_zp(cpu);
    do_cmp(cpu, cpu->x, cpu_read(cpu, addr));
    cpu->step_cycles = 3;
}

/* SBC zp */
static void op_e5(cpu_state *cpu) {
    uint16_t addr = addr_zp(cpu);
    do_sbc(cpu, cpu_read(cpu, addr));
    cpu->step_cycles = 3;
}

/* INC zp */
static void op_e6(cpu_state *cpu) {
    uint16_t addr = addr_zp(cpu);
    uint8_t val = cpu_read(cpu, addr) + 1;
    cpu_write(cpu, addr, val);
    cpu_update_nz(cpu, val);
    cpu->step_cycles = 5;
}

/* INX */
static void op_e8(cpu_state *cpu) {
    cpu->x++;
    cpu_update_nz(cpu, cpu->x);
    cpu->step_cycles = 2;
}

/* SBC #imm */
static void op_e9(cpu_state *cpu) {
    do_sbc(cpu, cpu_read(cpu, addr_imm(cpu)));
    cpu->step_cycles = 2;
}

/* NOP */
static void op_ea(cpu_state *cpu) {
    cpu->step_cycles = 2;
}

/* CPX abs */
static void op_ec(cpu_state *cpu) {
    uint16_t addr = addr_abs(cpu);
    do_cmp(cpu, cpu->x, cpu_read(cpu, addr));
    cpu->step_cycles = 4;
}

/* SBC abs */
static void op_ed(cpu_state *cpu) {
    uint16_t addr = addr_abs(cpu);
    do_sbc(cpu, cpu_read(cpu, addr));
    cpu->step_cycles = 4;
}

/* INC abs */
static void op_ee(cpu_state *cpu) {
    uint16_t addr = addr_abs(cpu);
    uint8_t val = cpu_read(cpu, addr) + 1;
    cpu_write(cpu, addr, val);
    cpu_update_nz(cpu, val);
    cpu->step_cycles = 6;
}

/* BEQ rel */
static void op_f0(cpu_state *cpu) {
    cpu->step_cycles = 2;
    do_branch(cpu, cpu_get_flag(cpu, CPU_FLAG_Z));
}

/* SBC (zp),Y */
static void op_f1(cpu_state *cpu) {
    uint16_t addr = addr_izy(cpu, true);
    do_sbc(cpu, cpu_read(cpu, addr));
    cpu->step_cycles += 5;
}

/* SBC zp,X */
static void op_f5(cpu_state *cpu) {
    uint16_t addr = addr_zpx(cpu);
    do_sbc(cpu, cpu_read(cpu, addr));
    cpu->step_cycles = 4;
}

/* INC zp,X */
static void op_f6(cpu_state *cpu) {
    uint16_t addr = addr_zpx(cpu);
    uint8_t val = cpu_read(cpu, addr) + 1;
    cpu_write(cpu, addr, val);
    cpu_update_nz(cpu, val);
    cpu->step_cycles = 6;
}

/* SED */
static void op_f8(cpu_state *cpu) {
    cpu_set_flag(cpu, CPU_FLAG_D, true);
    cpu->step_cycles = 2;
}

/* SBC abs,Y */
static void op_f9(cpu_state *cpu) {
    uint16_t addr = addr_aby(cpu, true);
    do_sbc(cpu, cpu_read(cpu, addr));
    cpu->step_cycles += 4;
}

/* SBC abs,X */
static void op_fd(cpu_state *cpu) {
    uint16_t addr = addr_abx(cpu, true);
    do_sbc(cpu, cpu_read(cpu, addr));
    cpu->step_cycles += 4;
}

/* INC abs,X */
static void op_fe(cpu_state *cpu) {
    uint16_t addr = addr_abx(cpu, false);
    uint8_t val = cpu_read(cpu, addr) + 1;
    cpu_write(cpu, addr, val);
    cpu_update_nz(cpu, val);
    cpu->step_cycles = 7;
}

/* NOP for illegal/undefined opcodes - consume 2 cycles, do nothing */
static void op_nop(cpu_state *cpu) {
    cpu->step_cycles = 2;
}

/* -------------------------------------------------------------------------- */
/* Dispatch table initialization                                               */
/* -------------------------------------------------------------------------- */

static void build_optable(cpu_state *cpu) {
    /* Fill all 256 entries with NOP handler for illegal opcodes */
    for (int i = 0; i < 256; i++) {
        cpu->optable[i] = op_nop;
    }

    /* Map all official opcodes */
    cpu->optable[0x00] = op_00;  /* BRK */
    cpu->optable[0x01] = op_01;  /* ORA (zp,X) */
    cpu->optable[0x05] = op_05;  /* ORA zp */
    cpu->optable[0x06] = op_06;  /* ASL zp */
    cpu->optable[0x08] = op_08;  /* PHP */
    cpu->optable[0x09] = op_09;  /* ORA #imm */
    cpu->optable[0x0A] = op_0a;  /* ASL A */
    cpu->optable[0x0D] = op_0d;  /* ORA abs */
    cpu->optable[0x0E] = op_0e;  /* ASL abs */
    cpu->optable[0x10] = op_10;  /* BPL */
    cpu->optable[0x11] = op_11;  /* ORA (zp),Y */
    cpu->optable[0x15] = op_15;  /* ORA zp,X */
    cpu->optable[0x16] = op_16;  /* ASL zp,X */
    cpu->optable[0x18] = op_18;  /* CLC */
    cpu->optable[0x19] = op_19;  /* ORA abs,Y */
    cpu->optable[0x1D] = op_1d;  /* ORA abs,X */
    cpu->optable[0x1E] = op_1e;  /* ASL abs,X */
    cpu->optable[0x20] = op_20;  /* JSR abs */
    cpu->optable[0x21] = op_21;  /* AND (zp,X) */
    cpu->optable[0x24] = op_24;  /* BIT zp */
    cpu->optable[0x25] = op_25;  /* AND zp */
    cpu->optable[0x26] = op_26;  /* ROL zp */
    cpu->optable[0x28] = op_28;  /* PLP */
    cpu->optable[0x29] = op_29;  /* AND #imm */
    cpu->optable[0x2A] = op_2a;  /* ROL A */
    cpu->optable[0x2C] = op_2c;  /* BIT abs */
    cpu->optable[0x2D] = op_2d;  /* AND abs */
    cpu->optable[0x2E] = op_2e;  /* ROL abs */
    cpu->optable[0x30] = op_30;  /* BMI */
    cpu->optable[0x31] = op_31;  /* AND (zp),Y */
    cpu->optable[0x35] = op_35;  /* AND zp,X */
    cpu->optable[0x36] = op_36;  /* ROL zp,X */
    cpu->optable[0x38] = op_38;  /* SEC */
    cpu->optable[0x39] = op_39;  /* AND abs,Y */
    cpu->optable[0x3D] = op_3d;  /* AND abs,X */
    cpu->optable[0x3E] = op_3e;  /* ROL abs,X */
    cpu->optable[0x40] = op_40;  /* RTI */
    cpu->optable[0x41] = op_41;  /* EOR (zp,X) */
    cpu->optable[0x45] = op_45;  /* EOR zp */
    cpu->optable[0x46] = op_46;  /* LSR zp */
    cpu->optable[0x48] = op_48;  /* PHA */
    cpu->optable[0x49] = op_49;  /* EOR #imm */
    cpu->optable[0x4A] = op_4a;  /* LSR A */
    cpu->optable[0x4C] = op_4c;  /* JMP abs */
    cpu->optable[0x4D] = op_4d;  /* EOR abs */
    cpu->optable[0x4E] = op_4e;  /* LSR abs */
    cpu->optable[0x50] = op_50;  /* BVC */
    cpu->optable[0x51] = op_51;  /* EOR (zp),Y */
    cpu->optable[0x55] = op_55;  /* EOR zp,X */
    cpu->optable[0x56] = op_56;  /* LSR zp,X */
    cpu->optable[0x58] = op_58;  /* CLI */
    cpu->optable[0x59] = op_59;  /* EOR abs,Y */
    cpu->optable[0x5D] = op_5d;  /* EOR abs,X */
    cpu->optable[0x5E] = op_5e;  /* LSR abs,X */
    cpu->optable[0x60] = op_60;  /* RTS */
    cpu->optable[0x61] = op_61;  /* ADC (zp,X) */
    cpu->optable[0x65] = op_65;  /* ADC zp */
    cpu->optable[0x66] = op_66;  /* ROR zp */
    cpu->optable[0x68] = op_68;  /* PLA */
    cpu->optable[0x69] = op_69;  /* ADC #imm */
    cpu->optable[0x6A] = op_6a;  /* ROR A */
    cpu->optable[0x6C] = op_6c;  /* JMP (indirect) */
    cpu->optable[0x6D] = op_6d;  /* ADC abs */
    cpu->optable[0x6E] = op_6e;  /* ROR abs */
    cpu->optable[0x70] = op_70;  /* BVS */
    cpu->optable[0x71] = op_71;  /* ADC (zp),Y */
    cpu->optable[0x75] = op_75;  /* ADC zp,X */
    cpu->optable[0x76] = op_76;  /* ROR zp,X */
    cpu->optable[0x78] = op_78;  /* SEI */
    cpu->optable[0x79] = op_79;  /* ADC abs,Y */
    cpu->optable[0x7D] = op_7d;  /* ADC abs,X */
    cpu->optable[0x7E] = op_7e;  /* ROR abs,X */
    cpu->optable[0x81] = op_81;  /* STA (zp,X) */
    cpu->optable[0x84] = op_84;  /* STY zp */
    cpu->optable[0x85] = op_85;  /* STA zp */
    cpu->optable[0x86] = op_86;  /* STX zp */
    cpu->optable[0x88] = op_88;  /* DEY */
    cpu->optable[0x8A] = op_8a;  /* TXA */
    cpu->optable[0x8C] = op_8c;  /* STY abs */
    cpu->optable[0x8D] = op_8d;  /* STA abs */
    cpu->optable[0x8E] = op_8e;  /* STX abs */
    cpu->optable[0x90] = op_90;  /* BCC */
    cpu->optable[0x91] = op_91;  /* STA (zp),Y */
    cpu->optable[0x94] = op_94;  /* STY zp,X */
    cpu->optable[0x95] = op_95;  /* STA zp,X */
    cpu->optable[0x96] = op_96;  /* STX zp,Y */
    cpu->optable[0x98] = op_98;  /* TYA */
    cpu->optable[0x99] = op_99;  /* STA abs,Y */
    cpu->optable[0x9A] = op_9a;  /* TXS */
    cpu->optable[0x9D] = op_9d;  /* STA abs,X */
    cpu->optable[0xA0] = op_a0;  /* LDY #imm */
    cpu->optable[0xA1] = op_a1;  /* LDA (zp,X) */
    cpu->optable[0xA2] = op_a2;  /* LDX #imm */
    cpu->optable[0xA4] = op_a4;  /* LDY zp */
    cpu->optable[0xA5] = op_a5;  /* LDA zp */
    cpu->optable[0xA6] = op_a6;  /* LDX zp */
    cpu->optable[0xA8] = op_a8;  /* TAY */
    cpu->optable[0xA9] = op_a9;  /* LDA #imm */
    cpu->optable[0xAA] = op_aa;  /* TAX */
    cpu->optable[0xAC] = op_ac;  /* LDY abs */
    cpu->optable[0xAD] = op_ad;  /* LDA abs */
    cpu->optable[0xAE] = op_ae;  /* LDX abs */
    cpu->optable[0xB0] = op_b0;  /* BCS */
    cpu->optable[0xB1] = op_b1;  /* LDA (zp),Y */
    cpu->optable[0xB4] = op_b4;  /* LDY zp,X */
    cpu->optable[0xB5] = op_b5;  /* LDA zp,X */
    cpu->optable[0xB6] = op_b6;  /* LDX zp,Y */
    cpu->optable[0xB8] = op_b8;  /* CLV */
    cpu->optable[0xB9] = op_b9;  /* LDA abs,Y */
    cpu->optable[0xBA] = op_ba;  /* TSX */
    cpu->optable[0xBC] = op_bc;  /* LDY abs,X */
    cpu->optable[0xBD] = op_bd;  /* LDA abs,X */
    cpu->optable[0xBE] = op_be;  /* LDX abs,Y */
    cpu->optable[0xC0] = op_c0;  /* CPY #imm */
    cpu->optable[0xC1] = op_c1;  /* CMP (zp,X) */
    cpu->optable[0xC4] = op_c4;  /* CPY zp */
    cpu->optable[0xC5] = op_c5;  /* CMP zp */
    cpu->optable[0xC6] = op_c6;  /* DEC zp */
    cpu->optable[0xC8] = op_c8;  /* INY */
    cpu->optable[0xC9] = op_c9;  /* CMP #imm */
    cpu->optable[0xCA] = op_ca;  /* DEX */
    cpu->optable[0xCC] = op_cc;  /* CPY abs */
    cpu->optable[0xCD] = op_cd;  /* CMP abs */
    cpu->optable[0xCE] = op_ce;  /* DEC abs */
    cpu->optable[0xD0] = op_d0;  /* BNE */
    cpu->optable[0xD1] = op_d1;  /* CMP (zp),Y */
    cpu->optable[0xD5] = op_d5;  /* CMP zp,X */
    cpu->optable[0xD6] = op_d6;  /* DEC zp,X */
    cpu->optable[0xD8] = op_d8;  /* CLD */
    cpu->optable[0xD9] = op_d9;  /* CMP abs,Y */
    cpu->optable[0xDD] = op_dd;  /* CMP abs,X */
    cpu->optable[0xDE] = op_de;  /* DEC abs,X */
    cpu->optable[0xE0] = op_e0;  /* CPX #imm */
    cpu->optable[0xE1] = op_e1;  /* SBC (zp,X) */
    cpu->optable[0xE4] = op_e4;  /* CPX zp */
    cpu->optable[0xE5] = op_e5;  /* SBC zp */
    cpu->optable[0xE6] = op_e6;  /* INC zp */
    cpu->optable[0xE8] = op_e8;  /* INX */
    cpu->optable[0xE9] = op_e9;  /* SBC #imm */
    cpu->optable[0xEA] = op_ea;  /* NOP */
    cpu->optable[0xEC] = op_ec;  /* CPX abs */
    cpu->optable[0xED] = op_ed;  /* SBC abs */
    cpu->optable[0xEE] = op_ee;  /* INC abs */
    cpu->optable[0xF0] = op_f0;  /* BEQ */
    cpu->optable[0xF1] = op_f1;  /* SBC (zp),Y */
    cpu->optable[0xF5] = op_f5;  /* SBC zp,X */
    cpu->optable[0xF6] = op_f6;  /* INC zp,X */
    cpu->optable[0xF8] = op_f8;  /* SED */
    cpu->optable[0xF9] = op_f9;  /* SBC abs,Y */
    cpu->optable[0xFD] = op_fd;  /* SBC abs,X */
    cpu->optable[0xFE] = op_fe;  /* INC abs,X */
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                  */
/* -------------------------------------------------------------------------- */

void cpu_init(cpu_state *cpu, cpu_bus_read_fn read_fn, cpu_bus_write_fn write_fn) {
    memset(cpu, 0, sizeof(cpu_state));
    cpu->bus_read = read_fn;
    cpu->bus_write = write_fn;
    cpu->status = CPU_FLAG_U | CPU_FLAG_I;
    cpu->sp = 0xFD;
    build_optable(cpu);
}

void cpu_reset(cpu_state *cpu) {
    cpu->a = 0;
    cpu->x = 0;
    cpu->y = 0;
    cpu->sp = 0xFD;
    cpu->status = CPU_FLAG_U | CPU_FLAG_I;
    cpu->pc = cpu_read16(cpu, CPU_VEC_RESET);
    cpu->nmi_pending = false;
    cpu->irq_pending = false;
    cpu->cycles += 7; /* Reset takes 7 cycles */
}

uint8_t cpu_step(cpu_state *cpu) {
    /* Handle pending NMI (higher priority than IRQ) */
    if (cpu->nmi_pending) {
        cpu->nmi_pending = false;
        cpu_push16(cpu, cpu->pc);
        cpu_push(cpu, (cpu->status | CPU_FLAG_U) & ~CPU_FLAG_B);
        cpu_set_flag(cpu, CPU_FLAG_I, true);
        cpu->pc = cpu_read16(cpu, CPU_VEC_NMI);
        cpu->cycles += 7;
        return 7;
    }

    /* Handle pending IRQ if interrupts are enabled */
    if (cpu->irq_pending && !cpu_get_flag(cpu, CPU_FLAG_I)) {
        cpu->irq_pending = false;
        cpu_push16(cpu, cpu->pc);
        cpu_push(cpu, (cpu->status | CPU_FLAG_U) & ~CPU_FLAG_B);
        cpu_set_flag(cpu, CPU_FLAG_I, true);
        cpu->pc = cpu_read16(cpu, CPU_VEC_IRQ);
        cpu->cycles += 7;
        return 7;
    }

    /* Fetch and execute one instruction */
    cpu->step_cycles = 0;
    uint8_t opcode = cpu_read(cpu, cpu->pc++);
    cpu->optable[opcode](cpu);

    cpu->cycles += cpu->step_cycles;
    return cpu->step_cycles;
}

void cpu_irq(cpu_state *cpu) {
    cpu->irq_pending = true;
}

void cpu_nmi(cpu_state *cpu) {
    cpu->nmi_pending = true;
}
