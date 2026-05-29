#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <emscripten.h>
#include "cpu.h"
#include "memory.h"
#include "pia.h"
#include "basic.h"
#include "roms_builtin.h"
extern bool aci_is_trapped(uint16_t pc);
extern void aci_handle_trap(cpu_state *cpu);

static cpu_state cpu;
static uint8_t screen[24][40];
static int cursor_col = 0;
static int cursor_row = 0;

static void scroll_up(void) {
    for (int r = 0; r < 23; r++)
        memcpy(screen[r], screen[r + 1], 40);
    memset(screen[23], ' ', 40);
}

static void display_callback(uint8_t ch) {
    ch &= 0x7F;
    if (ch == 0x0D) {
        cursor_col = 0;
        cursor_row++;
        if (cursor_row >= 24) {
            scroll_up();
            cursor_row = 23;
        }
    } else if (ch >= 0x20 && ch <= 0x5F) {
        if (cursor_row < 24 && cursor_col < 40) {
            screen[cursor_row][cursor_col] = ch;
            cursor_col++;
            if (cursor_col >= 40) {
                cursor_col = 0;
                cursor_row++;
                if (cursor_row >= 24) {
                    scroll_up();
                    cursor_row = 23;
                }
            }
        }
    }
}

EMSCRIPTEN_KEEPALIVE
void wasm_init(void) {
    mem_init(32);
    roms_load_builtin();
    pia_init();
    pia_set_display_callback(display_callback);
    basic_init();
    cpu_init(&cpu, mem_read, mem_write);
    cpu_reset(&cpu);
    memset(screen, ' ', sizeof(screen));
    cursor_col = 0;
    cursor_row = 0;
}

EMSCRIPTEN_KEEPALIVE
void wasm_step_frame(void) {
    int cycles = 17030; // ~1.022 MHz / 60 fps
    while (cycles > 0) {
        uint16_t pc = cpu.pc;

        if (pc == 0xE000) {
            basic_cold_start();
            cpu.pc = 0xFF1F;
        } else if (pc == 0xE2B3) {
            basic_warm_start();
            cpu.pc = 0xFF1F;
        }

        if (basic_is_running()) {
            basic_step();
            cycles -= 50;
        } else {
            if (aci_is_trapped(cpu.pc)) {
                aci_handle_trap(&cpu);
                cycles -= 100;
            } else {
                uint8_t c = cpu_step(&cpu);
                cycles -= c;
            }
        }
    }
}

EMSCRIPTEN_KEEPALIVE
void wasm_send_key(uint8_t ch) {
    if (basic_is_running()) {
        basic_input_char(ch);
    }
    pia_key_press(ch);
}

EMSCRIPTEN_KEEPALIVE
void wasm_reset(void) {
    basic_deactivate();
    cpu_reset(&cpu);
    memset(screen, ' ', sizeof(screen));
    cursor_col = 0;
    cursor_row = 0;
}

EMSCRIPTEN_KEEPALIVE
uint8_t wasm_get_screen_char(int row, int col) {
    if (row >= 0 && row < 24 && col >= 0 && col < 40)
        return screen[row][col];
    return ' ';
}

EMSCRIPTEN_KEEPALIVE
int wasm_get_cursor_col(void) { return cursor_col; }

EMSCRIPTEN_KEEPALIVE
int wasm_get_cursor_row(void) { return cursor_row; }

EMSCRIPTEN_KEEPALIVE
void wasm_load_program(const char *program) {
    // Reset
    basic_deactivate();
    cpu_reset(&cpu);
    memset(screen, ' ', sizeof(screen));
    cursor_col = 0;
    cursor_row = 0;

    // Enter BASIC: send E000R\r
    const char *enter = "E000R\r";
    for (int i = 0; enter[i]; i++) {
        uint8_t ch = (enter[i] == '\r') ? 0x0D : (uint8_t)enter[i];
        pia_key_press(ch);
        for (int f = 0; f < 10; f++) wasm_step_frame();
    }

    // Wait for BASIC
    for (int f = 0; f < 30; f++) wasm_step_frame();

    // Feed program
    for (int i = 0; program[i]; i++) {
        uint8_t ch;
        if (program[i] == '\n') ch = 0x0D;
        else if (program[i] >= 'a' && program[i] <= 'z') ch = program[i] - 32;
        else ch = (uint8_t)program[i];
        if (ch < 0x0D || (ch > 0x0D && ch < 0x20)) continue;
        if (basic_is_running()) basic_input_char(ch);
        pia_key_press(ch);
        for (int f = 0; f < 3; f++) wasm_step_frame();
    }

    // Send RUN
    const char *run = "RUN\r";
    for (int i = 0; run[i]; i++) {
        uint8_t ch = (run[i] == '\r') ? 0x0D : (uint8_t)run[i];
        if (basic_is_running()) basic_input_char(ch);
        pia_key_press(ch);
        for (int f = 0; f < 3; f++) wasm_step_frame();
    }
}

EMSCRIPTEN_KEEPALIVE
void wasm_send_break(void) {
    basic_break();
}
