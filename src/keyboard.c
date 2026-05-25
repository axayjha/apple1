#include "keyboard.h"

#include <ncurses.h>
#include <ctype.h>

/* Internal state */
static bool uppercase_only = true;

void keyboard_init(void)
{
    uppercase_only = true;
}

int keyboard_poll(void)
{
    int ch = getch();
    if (ch == ERR) {
        return -1;
    }
    return ch;
}

uint8_t keyboard_map(int raw_key)
{
    uint8_t mapped;

    switch (raw_key) {
    case '\n':
    case '\r':
    case KEY_ENTER:
        /* Apple 1 expects CR ($0D) */
        mapped = 0x0D;
        break;

    case KEY_BACKSPACE:
    case 127:
    case '\b':
        /* Apple 1 uses underscore ($5F) as rubout/delete */
        mapped = 0x5F;
        break;

    case 27:
        /* ESC -> $1B */
        mapped = 0x1B;
        break;

    default:
        /* Mask to 7 bits */
        mapped = (uint8_t)(raw_key & 0x7F);

        /* Force uppercase if enabled */
        if (uppercase_only && mapped >= 'a' && mapped <= 'z') {
            mapped = mapped - 'a' + 'A';
        }
        break;
    }

    /* Ensure 7-bit result */
    return mapped & 0x7F;
}

void keyboard_set_uppercase_only(bool enabled)
{
    uppercase_only = enabled;
}

bool keyboard_is_uppercase_only(void)
{
    return uppercase_only;
}

key_event_t keyboard_read(void)
{
    key_event_t event = { .type = KEY_NONE, .ch = 0 };

    int raw = keyboard_poll();
    if (raw == -1) {
        return event;
    }

    /* Check for host-side control combinations (Ctrl+key = key & 0x1F) */
    switch (raw) {
    case 0x11:  /* Ctrl+Q */
        event.type = KEY_QUIT;
        return event;

    case 0x12:  /* Ctrl+R */
        event.type = KEY_RESET;
        return event;

    case 0x05:  /* Ctrl+E */
        event.type = KEY_CONFIG;
        return event;

    case 0x08:  /* Ctrl+H */
        event.type = KEY_HELP;
        return event;

    case 0x13:  /* Ctrl+S */
        event.type = KEY_SNAPSHOT_SAVE;
        return event;

    case 0x0C:  /* Ctrl+L */
        event.type = KEY_SNAPSHOT_LOAD;
        return event;

    case 0x06:  /* Ctrl+F */
        event.type = KEY_FAST_TOGGLE;
        return event;

    case 0x04:  /* Ctrl+D */
        event.type = KEY_DEBUG_TOGGLE;
        return event;

    case 0x10:  /* Ctrl+P */
        event.type = KEY_PASTE;
        return event;

    case 0x03:  /* Ctrl+C */
        event.type = KEY_BREAK;
        return event;

    default:
        break;
    }

    /* Normal character: map to Apple 1 ASCII */
    event.type = KEY_CHAR;
    event.ch = keyboard_map(raw);
    return event;
}
