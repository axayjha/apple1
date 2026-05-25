#ifndef APPLE1_BASIC_H
#define APPLE1_BASIC_H

#include <stdbool.h>
#include <stdint.h>

/**
 * basic.h - Built-in Integer BASIC interpreter for Apple 1 emulator
 *
 * Implements a subset of Wozniak Integer BASIC as a native C interpreter
 * that hooks into the emulator at ROM trap addresses. When the CPU's PC
 * reaches $E000 (cold start) or $E2B3 (warm start), the main loop calls
 * into this interpreter which uses the PIA for I/O.
 *
 * Memory layout within emulator RAM:
 *   $0800-$xxxx  BASIC program lines (grows up)
 *   $xxxx-$7FFF  Variables and arrays (grows down from top)
 *
 * Supported features:
 *   - 26 integer variables (A-Z), 16-bit signed
 *   - 26 string variables (A$-Z$), up to 255 chars each
 *   - Single-dimension arrays via DIM
 *   - Line editor with tokenized storage
 *   - Control flow: GOTO, GOSUB/RETURN, FOR/NEXT, IF/THEN
 *   - Arithmetic, comparison, logical operators
 *   - Built-in functions: ABS, SGN, RND, PEEK, LEN, etc.
 *   - String functions: LEFT$, RIGHT$, MID$, CHR$, ASC, STR$, VAL
 */

/* Trap addresses for BASIC entry */
#define BASIC_COLD_START_ADDR  0xE000
#define BASIC_WARM_START_ADDR  0xE2B3
#define BASIC_MONITOR_RETURN   0xFF1F

/* Initialize the BASIC interpreter (call once at startup). */
void basic_init(void);

/* Called when PC hits $E000 - prints banner, clears program, shows prompt. */
void basic_cold_start(void);

/* Called when PC hits $E2B3 - shows prompt, preserves existing program. */
void basic_warm_start(void);

/* Returns true if the BASIC interpreter is currently active (not in monitor). */
bool basic_is_running(void);

/* Process one BASIC step. Called from the main emulation loop.
 * In RUNNING state, executes one statement.
 * In IDLE state, processes input characters.
 * Returns quickly to keep the main loop responsive. */
void basic_step(void);

/* Handle Ctrl+C break - stops program execution, returns to prompt. */
void basic_break(void);

/* Deactivate BASIC and return to monitor (called on system reset). */
void basic_deactivate(void);

/* Feed a character to the BASIC interpreter's input buffer. */
void basic_input_char(uint8_t ch);

#endif /* APPLE1_BASIC_H */
