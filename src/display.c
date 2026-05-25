/**
 * display.c - Apple 1 terminal display emulation via ncurses
 *
 * Implements a 40x24 (or 80x24) character terminal with green phosphor
 * aesthetics inspired by the Apple Monitor II.
 */

#include "display.h"

#include <ncurses.h>
#include <string.h>
#include <time.h>

/* --------------------------------------------------------------------------
 * Constants
 * -------------------------------------------------------------------------- */

#define MAX_COLS        DISPLAY_COLS_80  /* maximum column width (80-col mode) */
#define BLINK_INTERVAL_MS  250          /* ~2Hz: 250ms on, 250ms off */
#define GLOW_FADE_MS       500          /* phosphor afterglow duration */

/* ncurses color pair identifiers */
enum {
    PAIR_BRIGHT = 1,   /* bright green on black (A_BOLD applied separately) */
    PAIR_DIM    = 2,   /* dim green on black */
};

/* --------------------------------------------------------------------------
 * Internal state
 * -------------------------------------------------------------------------- */

/* Per-cell metadata */
typedef struct {
    uint8_t  ch;               /* displayed character (ASCII) */
    struct timespec write_time; /* timestamp when character was written */
} display_cell;

static display_cell screen[DISPLAY_ROWS][MAX_COLS];

static int  cursor_col;
static int  cursor_row;
static int  num_cols;          /* active column count (40 or 80) */
static bool mode_80col;

static bool opt_scanlines;
static bool opt_glow;

static bool cursor_visible;    /* current blink state */
static struct timespec last_blink;


/* --------------------------------------------------------------------------
 * Helpers
 * -------------------------------------------------------------------------- */

/* Return the current monotonic time. */
static void get_monotonic(struct timespec *ts)
{
    clock_gettime(CLOCK_MONOTONIC, ts);
}

/* Milliseconds elapsed since a given timespec. */
static long ms_since(const struct timespec *from)
{
    struct timespec now;
    get_monotonic(&now);
    long sec_diff  = (long)(now.tv_sec - from->tv_sec);
    long nsec_diff = (long)(now.tv_nsec - from->tv_nsec);
    return sec_diff * 1000 + nsec_diff / 1000000;
}

/* Scroll the display buffer up by one line.  The top row is discarded and
 * the bottom row is cleared. */
static void scroll_up(void)
{
    memmove(&screen[0], &screen[1],
            sizeof(display_cell) * (DISPLAY_ROWS - 1) * MAX_COLS);

    /* Clear the new bottom row */
    for (int c = 0; c < num_cols; c++) {
        screen[DISPLAY_ROWS - 1][c].ch = ' ';
        get_monotonic(&screen[DISPLAY_ROWS - 1][c].write_time);
    }
}

/* Advance cursor to column 0 of the next line, scrolling if necessary. */
static void newline(void)
{
    cursor_col = 0;
    cursor_row++;
    if (cursor_row >= DISPLAY_ROWS) {
        cursor_row = DISPLAY_ROWS - 1;
        scroll_up();
    }
}

/* Offset for centering display content within stdscr */
static int offset_y;
static int offset_x;

/* Calculate offsets to center the display in the terminal. */
static void calc_offsets(void)
{
    int term_rows, term_cols;
    getmaxyx(stdscr, term_rows, term_cols);

    offset_y = (term_rows > DISPLAY_ROWS) ? (term_rows - DISPLAY_ROWS) / 2 : 0;
    offset_x = (term_cols > num_cols) ? (term_cols - num_cols) / 2 : 0;
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void display_init(void)
{
    /* ncurses initialization */
    initscr();
    cbreak();          /* character-at-a-time input, signals still work */
    noecho();
    nonl();            /* don't translate CR to NL on input */
    intrflush(stdscr, FALSE);
    keypad(stdscr, TRUE);
    set_escdelay(25);  /* reduce ESC key delay from 1000ms to 25ms */
    nodelay(stdscr, TRUE);  /* non-blocking getch */
    curs_set(0);       /* hide the hardware cursor */
    scrollok(stdscr, FALSE);

    /* Color setup */
    start_color();
    use_default_colors();
    init_pair(PAIR_BRIGHT, COLOR_GREEN, COLOR_BLACK);
    init_pair(PAIR_DIM,    COLOR_GREEN, COLOR_BLACK);

    /* Set background to black */
    bkgd(COLOR_PAIR(PAIR_DIM));
    clear();
    refresh();

    /* Default configuration */
    mode_80col    = false;
    num_cols      = DISPLAY_COLS_40;
    opt_scanlines = false;
    opt_glow      = false;

    /* Calculate centering offsets */
    calc_offsets();

    /* Clear internal state */
    display_clear();

    /* Initialize blink timer */
    cursor_visible = true;
    get_monotonic(&last_blink);
}

void display_shutdown(void)
{
    endwin();
}

void display_putchar(uint8_t ch)
{
    /* Mask to 7 bits (Apple 1 outputs with bit 7 set sometimes) */
    ch &= 0x7F;

    if (ch == 0x0D) {
        /* Carriage return: move to start of next line */
        newline();
        return;
    }

    /* Only printable characters in the Apple 1 range $20-$5F */
    if (ch < 0x20 || ch > 0x5F) {
        return;
    }

    /* Place character at cursor position */
    screen[cursor_row][cursor_col].ch = ch;
    get_monotonic(&screen[cursor_row][cursor_col].write_time);

    /* Advance cursor */
    cursor_col++;
    if (cursor_col >= num_cols) {
        newline();
    }
}

void display_update(void)
{
    /* --- Cursor blink logic --- */
    long elapsed = ms_since(&last_blink);
    if (elapsed >= BLINK_INTERVAL_MS) {
        cursor_visible = !cursor_visible;
        get_monotonic(&last_blink);
    }

    /* --- Render the display buffer directly to stdscr --- */
    for (int row = 0; row < DISPLAY_ROWS; row++) {
        for (int col = 0; col < num_cols; col++) {
            uint8_t ch = screen[row][col].ch;

            /* Determine character to draw at cursor position */
            if (row == cursor_row && col == cursor_col) {
                ch = cursor_visible ? '@' : ' ';
            }

            /* Determine attributes */
            attr_t attrs = COLOR_PAIR(PAIR_BRIGHT) | A_BOLD;

            /* Phosphor glow: recently written characters are bright,
             * older characters fade to dim. */
            if (opt_glow && !(row == cursor_row && col == cursor_col)) {
                long age = ms_since(&screen[row][col].write_time);
                if (age > GLOW_FADE_MS) {
                    attrs = COLOR_PAIR(PAIR_BRIGHT);
                }
            }

            /* Scanline effect: odd rows lose bold (slightly dimmer) */
            if (opt_scanlines && (row % 2 == 1)) {
                attrs = COLOR_PAIR(PAIR_BRIGHT);
            }

            attron(attrs);
            mvaddch(offset_y + row, offset_x + col, ch);
            attroff(attrs);
        }
    }

    refresh();
}

void display_clear(void)
{
    for (int row = 0; row < DISPLAY_ROWS; row++) {
        for (int col = 0; col < MAX_COLS; col++) {
            screen[row][col].ch = ' ';
            get_monotonic(&screen[row][col].write_time);
        }
    }
    cursor_col = 0;
    cursor_row = 0;
}

void display_set_scanlines(bool enabled)
{
    opt_scanlines = enabled;
}

void display_set_glow(bool enabled)
{
    opt_glow = enabled;
}

void display_set_80col(bool enabled)
{
    if (enabled == mode_80col) {
        return;
    }
    mode_80col = enabled;
    num_cols   = enabled ? DISPLAY_COLS_80 : DISPLAY_COLS_40;

    /* Recalculate offsets and clear */
    calc_offsets();
    clear();
    display_clear();
}

void display_get_cursor(int *col, int *row)
{
    if (col) *col = cursor_col;
    if (row) *row = cursor_row;
}

bool display_get_80col(void)
{
    return mode_80col;
}
