#include "pia.h"

#include <stddef.h>

/**
 * pia.c - Motorola 6820 PIA emulation for Apple 1 I/O
 *
 * The Apple 1 uses a single PIA mapped at $D010-$D013 to interface the
 * keyboard and display. This module emulates the subset of 6820 behavior
 * that the Woz Monitor and Apple 1 BASIC rely upon.
 */

/* Internal PIA state */
static uint8_t kbd_register;      /* $D010: last key pressed (bit 7 set) */
static uint8_t kbdcr_register;    /* $D011: bit 7 = key available */
static uint8_t dsp_register;      /* $D012: display data register */
static uint8_t dspcr_register;    /* $D013: display control register */

static bool key_available;        /* true when a key is waiting */
static bool display_char_pending; /* true when a char has been written to DSP */

static void (*display_callback)(uint8_t ch);

void pia_init(void)
{
    kbd_register = 0x00;
    kbdcr_register = 0x00;
    dsp_register = 0x00;
    dspcr_register = 0x00;

    key_available = false;
    display_char_pending = false;
    display_callback = NULL;
}

uint8_t pia_read(uint16_t addr)
{
    switch (addr) {
    case PIA_KBD:
        /* Reading KBD returns the key data with bit 7 set and clears the
         * KBDCR flag so the CPU knows it has consumed the character. */
        kbdcr_register &= 0x7F;
        key_available = false;
        return kbd_register;

    case PIA_KBDCR:
        /* Bit 7 indicates whether a key is available. */
        return kbdcr_register;

    case PIA_DSP:
        /* Bit 7 read: display NOT ready (busy) flag.
         * On real hardware: bit 7 = 1 means busy, bit 7 = 0 means ready.
         * The Woz Monitor's ECHO routine loops while bit 7 is set (BMI).
         * Since our emulated display processes chars instantly, always
         * return 0 (ready). */
        return 0x00;

    case PIA_DSPCR:
        return dspcr_register;

    default:
        return 0x00;
    }
}

void pia_write(uint16_t addr, uint8_t val)
{
    switch (addr) {
    case PIA_KBD:
        /* KBD register is read-only from the CPU side on the Apple 1. */
        break;

    case PIA_KBDCR:
        /* The Apple 1 doesn't use KBDCR writes in a meaningful way,
         * but store the value for completeness. */
        kbdcr_register = (kbdcr_register & 0x80) | (val & 0x7F);
        break;

    case PIA_DSP:
        /* Write bits 0-6 as a character for display output.
         * The Woz Monitor masks with 0x7F before writing. */
        dsp_register = val & 0x7F;
        display_char_pending = true;

        if (display_callback) {
            display_callback(dsp_register);
        }
        break;

    case PIA_DSPCR:
        dspcr_register = val;
        break;

    default:
        break;
    }
}

void pia_key_press(uint8_t ch)
{
    /* Set the key data with bit 7 forced high, per Apple 1 convention. */
    kbd_register = ch | 0x80;
    kbdcr_register |= 0x80;
    key_available = true;
}

bool pia_display_ready(void)
{
    /* In emulation, display is always ready. An external module may
     * override this behavior by tracking timing externally. */
    return true;
}

int pia_get_display_char(void)
{
    if (!display_char_pending) {
        return -1;
    }
    display_char_pending = false;
    return dsp_register;
}

void pia_set_display_callback(void (*cb)(uint8_t ch))
{
    display_callback = cb;
}
