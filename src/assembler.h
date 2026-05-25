#ifndef APPLE1_ASSEMBLER_H
#define APPLE1_ASSEMBLER_H

#include <stdbool.h>
#include <stdint.h>

void asm_init(void);
bool asm_is_active(void);
void asm_activate(void);
void asm_deactivate(void);
void asm_input_char(uint8_t ch);
void asm_step(void);

#endif
