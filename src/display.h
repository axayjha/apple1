#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

/**
 * display.h - Apple 1 terminal display emulation via ncurses
 *
 * Renders a 40x24 (or 80x24) character display with Apple Monitor II
 * green phosphor aesthetics: bright green text on black, with optional
 * scanline and phosphor glow effects.
 *
 * The display accepts characters in the Apple 1 printable range ($20-$5F,
 * uppercase-only in stock 40-column mode).  CR ($0D) advances to the next
 * line and scrolls when at the bottom.  A blinking '@' cursor indicates
 * the current write position.
 *
 * Typical usage:
 *   display_init();
 *   pia_set_display_callback(display_putchar);
 *   ...
 *   while (running) { cpu_step(&cpu); display_update(); }
 *   display_shutdown();
 */

/* Default geometry */
#define DISPLAY_COLS_40  40
#define DISPLAY_COLS_80  80
#define DISPLAY_ROWS     24

/* Initialize ncurses, set up color pairs, and clear the display buffer.
 * Must be called before any other display_*() function. */
void display_init(void);

/* Shut down ncurses and restore terminal state. */
void display_shutdown(void);

/* Output a character to the display.
 * Handles CR ($0D), printable range ($20-$5F), auto-wrap, and scrolling.
 * This function is suitable as a PIA display callback. */
void display_putchar(uint8_t ch);

/* Refresh the ncurses screen.  Handles cursor blink timing (~2Hz).
 * Should be called periodically (e.g., every frame or after a batch of
 * CPU cycles). */
void display_update(void);

/* Clear the display buffer and home the cursor (col=0, row=0). */
void display_clear(void);

/* Enable or disable the scanline effect (every other row dimmer). */
void display_set_scanlines(bool enabled);

/* Enable or disable the phosphor glow effect (recently written characters
 * appear brighter, older characters fade to dim green). */
void display_set_glow(bool enabled);

/* Switch between 40-column (stock Apple 1) and 80-column mode.
 * Clears the screen when the mode changes. */
void display_set_80col(bool enabled);

/* Retrieve the current cursor position. */
void display_get_cursor(int *col, int *row);

/* Returns true if the display is currently in 80-column mode. */
bool display_get_80col(void);

#endif /* DISPLAY_H */
