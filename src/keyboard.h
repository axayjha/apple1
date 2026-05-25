#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdbool.h>
#include <stdint.h>

/**
 * keyboard.h - Keyboard input handling for Apple 1 emulator
 *
 * Reads from the terminal via ncurses and converts host key events into
 * Apple 1 compatible 7-bit ASCII codes or host-side control events.
 *
 * The Apple 1 PIA expects 7-bit ASCII in bits 0-6; the PIA itself sets
 * bit 7 as the "key available" strobe.  This module provides only the
 * 7-bit value.
 *
 * Special host-side control combinations (Ctrl+Q, Ctrl+R, etc.) are
 * returned as distinct event types so the main loop can handle them
 * without forwarding to the emulated machine.
 */

/* Keyboard event types */
typedef enum {
    KEY_NONE = 0,        /* No key available (non-blocking poll) */
    KEY_CHAR,            /* Normal character for the emulator (see .ch) */
    KEY_QUIT,            /* Ctrl+Q: quit emulator */
    KEY_RESET,           /* Ctrl+R: warm reset */
    KEY_COLD_RESET,      /* (reserved) cold reset */
    KEY_CONFIG,          /* Ctrl+E: config menu toggle */
    KEY_HELP,            /* Ctrl+H: help overlay */
    KEY_SNAPSHOT_SAVE,   /* Ctrl+S: save snapshot */
    KEY_SNAPSHOT_LOAD,   /* Ctrl+L: load snapshot */
    KEY_FAST_TOGGLE,     /* Ctrl+F: toggle fast mode */
    KEY_DEBUG_TOGGLE,    /* Ctrl+D: toggle debug overlay */
    KEY_PASTE,           /* Ctrl+P: paste mode */
    KEY_BREAK            /* Ctrl+C: break */
} keyboard_event_t;

/* Keyboard event structure */
typedef struct {
    keyboard_event_t type;  /* Event type */
    uint8_t          ch;    /* 7-bit ASCII character (valid when type == KEY_CHAR) */
} key_event_t;

/* Initialize keyboard state.  Call once at startup after ncurses init. */
void keyboard_init(void);

/* Non-blocking check for a keypress.
 * Returns -1 if no key is available, otherwise the raw ncurses key code. */
int keyboard_poll(void);

/* Convert a raw terminal key code to Apple 1 7-bit ASCII.
 * Forces uppercase when uppercase-only mode is active.
 * Returns the mapped 7-bit value. */
uint8_t keyboard_map(int raw_key);

/* Enable or disable uppercase-only mode (default: enabled).
 * When enabled, lowercase a-z are converted to A-Z. */
void keyboard_set_uppercase_only(bool enabled);

/* Query whether uppercase-only mode is active. */
bool keyboard_is_uppercase_only(void);

/* Main polling function.  Reads from ncurses, identifies host control
 * sequences, and maps everything else to Apple 1 key codes.
 * Non-blocking: returns KEY_NONE immediately if no input is pending. */
key_event_t keyboard_read(void);

#endif /* KEYBOARD_H */
