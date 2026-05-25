#include "cpu.h"

uint16_t cpu_get_pc(const cpu_state *cpu) { return cpu->pc; }
uint8_t cpu_get_a(const cpu_state *cpu) { return cpu->a; }
uint8_t cpu_get_x(const cpu_state *cpu) { return cpu->x; }
uint8_t cpu_get_y(const cpu_state *cpu) { return cpu->y; }
uint8_t cpu_get_sp(const cpu_state *cpu) { return cpu->sp; }
uint8_t cpu_get_status(const cpu_state *cpu) { return cpu->status; }
void cpu_set_pc(cpu_state *cpu, uint16_t pc) { cpu->pc = pc; }
