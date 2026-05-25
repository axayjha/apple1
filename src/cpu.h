#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stdbool.h>

/* MOS 6502 CPU Emulation Core */

/* Status register flag bit positions */
#define CPU_FLAG_C  0x01  /* Carry */
#define CPU_FLAG_Z  0x02  /* Zero */
#define CPU_FLAG_I  0x04  /* Interrupt Disable */
#define CPU_FLAG_D  0x08  /* Decimal Mode */
#define CPU_FLAG_B  0x10  /* Break (exists only on stack) */
#define CPU_FLAG_U  0x20  /* Unused (always 1 on stack) */
#define CPU_FLAG_V  0x40  /* Overflow */
#define CPU_FLAG_N  0x80  /* Negative */

/* Interrupt vectors */
#define CPU_VEC_NMI   0xFFFA
#define CPU_VEC_RESET 0xFFFC
#define CPU_VEC_IRQ   0xFFFE

/* Bus interface function pointer types */
typedef uint8_t (*cpu_bus_read_fn)(uint16_t addr);
typedef void    (*cpu_bus_write_fn)(uint16_t addr, uint8_t val);

/* CPU state structure */
typedef struct cpu_state {
    /* Registers */
    uint8_t  a;       /* Accumulator */
    uint8_t  x;       /* X index register */
    uint8_t  y;       /* Y index register */
    uint8_t  sp;      /* Stack pointer */
    uint16_t pc;      /* Program counter */
    uint8_t  status;  /* Processor status register (NV-BDIZC) */

    /* Interrupt state */
    bool     nmi_pending;
    bool     irq_pending;

    /* Cycle counter */
    uint64_t cycles;

    /* Bus interface */
    cpu_bus_read_fn  bus_read;
    cpu_bus_write_fn bus_write;

    /* Instruction dispatch table (256 entries) */
    void (*optable[256])(struct cpu_state *cpu);

    /* Per-instruction cycle accumulator (used internally by cpu_step) */
    uint8_t  step_cycles;
} cpu_state;

/* Public API */

/* Initialize the CPU state and build the dispatch table.
 * Must be called before cpu_reset(). */
void cpu_init(cpu_state *cpu, cpu_bus_read_fn read_fn, cpu_bus_write_fn write_fn);

/* Perform a hardware reset sequence (reads reset vector, sets initial state). */
void cpu_reset(cpu_state *cpu);

/* Execute one instruction. Returns the number of cycles consumed. */
uint8_t cpu_step(cpu_state *cpu);

/* Signal a maskable interrupt request. The IRQ will be serviced at the
 * start of the next cpu_step() call if the I flag is clear. */
void cpu_irq(cpu_state *cpu);

/* Signal a non-maskable interrupt. The NMI will be serviced at the
 * start of the next cpu_step() call. */
void cpu_nmi(cpu_state *cpu);

#endif /* CPU_H */
