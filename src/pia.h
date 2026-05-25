#ifndef PIA_H
#define PIA_H

#include <stdbool.h>
#include <stdint.h>

/**
 * pia.h - Motorola 6820 PIA emulation for Apple 1
 *
 * Register map (accent addresses $D010-$D013):
 *   $D010 (KBD)   - Keyboard data register. Read returns last key with bit 7 set.
 *                   Reading this register clears the KBDCR flag.
 *   $D011 (KBDCR) - Keyboard control/status register. Bit 7 = key available.
 *   $D012 (DSP)   - Display data register. Write bits 0-6 to output a character.
 *                   Bit 7 read indicates display ready (1 = ready).
 *   $D013 (DSPCR) - Display control register.
 */

/* PIA register addresses */
#define PIA_KBD    0xD010
#define PIA_KBDCR  0xD011
#define PIA_DSP    0xD012
#define PIA_DSPCR  0xD013

/* Initialize PIA state (reset all registers and flags). */
void pia_init(void);

/* Read a PIA register. addr should be in the range $D010-$D013. */
uint8_t pia_read(uint16_t addr);

/* Write a PIA register. addr should be in the range $D010-$D013. */
void pia_write(uint16_t addr, uint8_t val);

/* Simulate a key press: sets the KBD register to ch|0x80 and sets KBDCR bit 7. */
void pia_key_press(uint8_t ch);

/* Returns true if the display is ready to accept a new character. */
bool pia_display_ready(void);

/* Get the pending display character. Returns -1 if no character is pending. */
int pia_get_display_char(void);

/* Set a callback function that is invoked whenever a character is written to DSP. */
void pia_set_display_callback(void (*cb)(uint8_t ch));

#endif /* PIA_H */
