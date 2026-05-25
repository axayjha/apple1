/*
 * basic.c - Built-in Integer BASIC interpreter for Apple 1 emulator
 *
 * Implements a compatible subset of Wozniak Integer BASIC as a native C
 * interpreter. This runs as a "trap" -- when the 6502 PC reaches $E000 or
 * $E2B3, the emulation loop calls into this module instead of executing
 * ROM code.
 *
 * I/O is performed through the PIA: output via pia_write to $D012,
 * input via pia_key_press / polling KBDCR.
 */

#include "basic.h"
#include "memory.h"
#include "pia.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* ======================================================================
 * Constants and limits
 * ====================================================================== */

#define PROG_BASE       0x0800  /* Program storage starts here (grows up) */
#define PROG_TOP        0x6000  /* Maximum program extent */
#define VAR_TOP         0x7FFF  /* Variables/arrays grow down from here */

#define MAX_LINE_LEN    256     /* Maximum input line length */
#define MAX_LINE_NUM    32767   /* Maximum line number */
#define MAX_GOSUB_DEPTH 32      /* GOSUB stack depth */
#define MAX_FOR_DEPTH   16      /* FOR/NEXT nesting depth */
#define MAX_STRING_LEN  255     /* Maximum string variable length */
#define INPUT_BUF_SIZE  256     /* Input ring buffer size */

#define NUM_VARS        26      /* A-Z */
#define NUM_STR_VARS    26      /* A$-Z$ */

/* ======================================================================
 * Token definitions
 * ====================================================================== */

typedef enum {
    TOK_END = 0,        /* End of tokenized line */
    TOK_NUM,            /* Followed by 2-byte integer */
    TOK_STRING,         /* Followed by length byte + chars */
    TOK_VAR,            /* Followed by variable index (0-25) */
    TOK_STRVAR,         /* Followed by variable index (0-25) for A$-Z$ */
    TOK_ARRAYVAR,       /* Followed by variable index (0-25) for array */

    /* Keywords - statements */
    TOK_PRINT = 20,
    TOK_INPUT,
    TOK_LET,
    TOK_IF,
    TOK_THEN,
    TOK_GOTO,
    TOK_GOSUB,
    TOK_RETURN,
    TOK_FOR,
    TOK_TO,
    TOK_STEP,
    TOK_NEXT,
    TOK_DIM,
    TOK_REM,
    TOK_STOP,
    TOK_END_STMT,
    TOK_POKE,
    TOK_CALL,
    TOK_DEL,

    /* Keywords - commands (immediate mode) */
    TOK_RUN = 50,
    TOK_LIST,
    TOK_NEW,
    TOK_CLR,

    /* Operators */
    TOK_PLUS = 70,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_MOD,
    TOK_EQ,
    TOK_NE,
    TOK_LT,
    TOK_GT,
    TOK_LE,
    TOK_GE,
    TOK_AND,
    TOK_OR,
    TOK_NOT,

    /* Punctuation */
    TOK_LPAREN = 100,
    TOK_RPAREN,
    TOK_COMMA,
    TOK_SEMICOLON,
    TOK_COLON,

    /* Built-in functions */
    TOK_ABS = 120,
    TOK_SGN,
    TOK_RND,
    TOK_PEEK,
    TOK_LEN,
    TOK_ASC,
    TOK_VAL,
    TOK_CHR,
    TOK_STR,
    TOK_LEFT,
    TOK_RIGHT,
    TOK_MID,

    TOK_INVALID = 255
} token_t;

/* Keyword table for tokenizer */
typedef struct {
    const char *word;
    token_t     token;
} keyword_entry_t;

static const keyword_entry_t keywords[] = {
    {"PRINT",   TOK_PRINT},
    {"INPUT",   TOK_INPUT},
    {"LET",     TOK_LET},
    {"IF",      TOK_IF},
    {"THEN",    TOK_THEN},
    {"GOTO",    TOK_GOTO},
    {"GOSUB",   TOK_GOSUB},
    {"RETURN",  TOK_RETURN},
    {"FOR",     TOK_FOR},
    {"TO",      TOK_TO},
    {"STEP",    TOK_STEP},
    {"NEXT",    TOK_NEXT},
    {"DIM",     TOK_DIM},
    {"REM",     TOK_REM},
    {"STOP",    TOK_STOP},
    {"END",     TOK_END_STMT},
    {"POKE",    TOK_POKE},
    {"CALL",    TOK_CALL},
    {"DEL",     TOK_DEL},
    {"RUN",     TOK_RUN},
    {"LIST",    TOK_LIST},
    {"NEW",     TOK_NEW},
    {"CLR",     TOK_CLR},
    {"MOD",     TOK_MOD},
    {"AND",     TOK_AND},
    {"OR",      TOK_OR},
    {"NOT",     TOK_NOT},
    {"ABS",     TOK_ABS},
    {"SGN",     TOK_SGN},
    {"RND",     TOK_RND},
    {"PEEK",    TOK_PEEK},
    {"LEN",     TOK_LEN},
    {"ASC",     TOK_ASC},
    {"VAL",     TOK_VAL},
    {"CHR$",    TOK_CHR},
    {"STR$",    TOK_STR},
    {"LEFT$",   TOK_LEFT},
    {"RIGHT$",  TOK_RIGHT},
    {"MID$",    TOK_MID},
    {NULL, TOK_INVALID}
};

/* ======================================================================
 * Interpreter state
 * ====================================================================== */

typedef enum {
    STATE_INACTIVE,     /* BASIC not active (in monitor) */
    STATE_IDLE,         /* Waiting for input line at prompt */
    STATE_RUNNING,      /* Executing program */
    STATE_INPUT_WAIT,   /* Waiting for INPUT statement data */
    STATE_STOPPED       /* After END/STOP/error */
} basic_state_t;

/* FOR loop stack entry */
typedef struct {
    uint8_t  var_idx;       /* Loop variable index (0-25) */
    int16_t  limit;         /* TO value */
    int16_t  step;          /* STEP value */
    uint16_t line_addr;     /* Address of FOR line (for NEXT return) */
    uint16_t tok_offset;    /* Offset into tokenized line after FOR stmt */
} for_entry_t;

/* GOSUB stack entry */
typedef struct {
    uint16_t line_addr;     /* Return address: line */
    uint16_t tok_offset;    /* Return address: offset in tokenized line */
} gosub_entry_t;

/* Main interpreter state */
static struct {
    basic_state_t state;

    /* Input buffer (ring buffer fed by basic_input_char) */
    uint8_t  input_buf[INPUT_BUF_SIZE];
    int      input_head;
    int      input_tail;
    int      input_count;

    /* Line editing buffer */
    char     line_buf[MAX_LINE_LEN];
    int      line_pos;

    /* Tokenization buffer */
    uint8_t  tok_buf[MAX_LINE_LEN * 2];
    int      tok_len;

    /* Program storage */
    uint16_t prog_base;     /* Start of program in emulator RAM */
    uint16_t prog_end;      /* One past last byte of program */

    /* Execution state */
    uint16_t exec_line_addr;    /* Address of current line being executed */
    int      exec_tok_pos;      /* Position within tokenized content */
    uint16_t exec_line_num;     /* Current line number (for errors) */
    bool     exec_advance;      /* Should we advance to next line? */

    /* Variables */
    int16_t  vars[NUM_VARS];                        /* A-Z */
    char     str_vars[NUM_STR_VARS][MAX_STRING_LEN + 1]; /* A$-Z$ */

    /* Arrays: stored as base address + size in emulator memory */
    uint16_t array_base[NUM_VARS];  /* Base address for each array */
    uint16_t array_size[NUM_VARS];  /* Number of elements */
    uint16_t array_top;             /* Current top of array storage (grows down) */

    /* Control flow stacks */
    gosub_entry_t gosub_stack[MAX_GOSUB_DEPTH];
    int           gosub_sp;

    for_entry_t   for_stack[MAX_FOR_DEPTH];
    int           for_sp;

    /* INPUT statement state */
    uint8_t  input_var_idx;         /* Variable being input */
    bool     input_is_string;       /* Is this a string INPUT? */
    char     input_line[MAX_LINE_LEN];
    int      input_line_pos;

    /* Break flag */
    bool     break_flag;

    /* Temporary string result buffer for string functions */
    char     tmp_str[MAX_STRING_LEN + 1];

} basic;

/* ======================================================================
 * Forward declarations
 * ====================================================================== */

/* I/O */
static void basic_putchar(uint8_t ch);
static void basic_puts(const char *s);
static void basic_print_int(int16_t val);
static void basic_newline(void);
static void basic_prompt(void);
static int  basic_poll_char(void);

/* Tokenizer */
static int  tokenize_line(const char *src, uint8_t *dst, int dst_size);

/* Program storage */
static void program_clear(void);
static void program_insert_line(uint16_t linenum, const uint8_t *tokens, int tok_len);
static void program_delete_line(uint16_t linenum);
static uint16_t program_find_line(uint16_t linenum);
static uint16_t program_first_line(void);
static uint16_t program_next_line(uint16_t addr);
static uint16_t program_get_linenum(uint16_t addr);

/* Execution */
static void exec_start(uint16_t start_line);
static void exec_statement(void);
static void exec_error(const char *msg);
static void exec_error_line(const char *msg, uint16_t linenum);

/* Statement handlers */
static void stmt_print(void);
static void stmt_input(void);
static void stmt_let(void);
static void stmt_if(void);
static void stmt_goto(void);
static void stmt_gosub(void);
static void stmt_return(void);
static void stmt_for(void);
static void stmt_next(void);
static void stmt_dim(void);
static void stmt_poke(void);
static void stmt_call(void);
static void stmt_end(void);
static void stmt_stop(void);
static void stmt_del(void);
static void stmt_rem(void);

/* Commands (immediate mode) */
static void cmd_run(void);
static void cmd_list(void);
static void cmd_new(void);
static void cmd_clr(void);

/* Expression evaluator */
static int16_t eval_expression(void);
static int16_t eval_or(void);
static int16_t eval_and(void);
static int16_t eval_not(void);
static int16_t eval_comparison(void);
static int16_t eval_add_sub(void);
static int16_t eval_mul_div(void);
static int16_t eval_unary(void);
static int16_t eval_primary(void);
static int16_t eval_function(token_t func);

/* String expression evaluator */
static void eval_string_expr(char *result, int max_len);
static void eval_string_function(token_t func, char *result, int max_len);

/* Token stream helpers */
static uint8_t tok_peek(void);
static uint8_t tok_read(void);
static int16_t tok_read_int(void);
static void    tok_read_string(char *buf, int max_len);
static bool    tok_at_end(void);
static void    tok_skip_spaces(void);

/* Utility */
static void clear_variables(void);
static bool is_line_number(const char *s, uint16_t *linenum);
static void upper_string(char *s);

/* ======================================================================
 * Public API implementation
 * ====================================================================== */

void basic_init(void)
{
    memset(&basic, 0, sizeof(basic));
    basic.state = STATE_INACTIVE;
    basic.prog_base = PROG_BASE;
    basic.prog_end = PROG_BASE;
    basic.array_top = VAR_TOP;

    /* Seed RNG */
    srand((unsigned int)time(NULL));

    LOG_INFO(LOG_COMP_EMU, "BASIC interpreter initialized");
}

void basic_cold_start(void)
{
    basic.state = STATE_IDLE;
    basic.line_pos = 0;
    basic.input_head = 0;
    basic.input_tail = 0;
    basic.input_count = 0;
    basic.break_flag = false;

    /* Clear program and variables */
    program_clear();
    clear_variables();

    /* Print banner */
    basic_newline();
    basic_puts("APPLE 1 BASIC");
    basic_newline();
    basic_prompt();

    LOG_INFO(LOG_COMP_EMU, "BASIC cold start");
}

void basic_warm_start(void)
{
    basic.state = STATE_IDLE;
    basic.line_pos = 0;
    basic.input_head = 0;
    basic.input_tail = 0;
    basic.input_count = 0;
    basic.break_flag = false;
    basic.gosub_sp = 0;
    basic.for_sp = 0;

    /* Keep program intact, just reset execution state */
    basic_newline();
    basic_prompt();

    LOG_INFO(LOG_COMP_EMU, "BASIC warm start");
}

bool basic_is_running(void)
{
    return basic.state != STATE_INACTIVE;
}

void basic_break(void)
{
    if (basic.state == STATE_RUNNING) {
        basic.break_flag = true;
    } else if (basic.state == STATE_INPUT_WAIT) {
        basic.state = STATE_IDLE;
        basic_newline();
        basic_puts("BREAK");
        basic_newline();
        basic_prompt();
    }
}

void basic_input_char(uint8_t ch)
{
    if (basic.input_count < INPUT_BUF_SIZE) {
        basic.input_buf[basic.input_head] = ch;
        basic.input_head = (basic.input_head + 1) % INPUT_BUF_SIZE;
        basic.input_count++;
    }
}

void basic_step(void)
{
    if (basic.state == STATE_INACTIVE) return;

    /* Check for break */
    if (basic.break_flag && basic.state == STATE_RUNNING) {
        basic.break_flag = false;
        basic.state = STATE_IDLE;
        basic_newline();
        basic_puts("BREAK IN ");
        basic_print_int((int16_t)basic.exec_line_num);
        basic_newline();
        basic_prompt();
        return;
    }

    switch (basic.state) {
    case STATE_IDLE:
    case STATE_STOPPED:
        /* Process input characters */
        {
            int ch = basic_poll_char();
            if (ch < 0) return;

            if (ch == 0x0D || ch == '\n') {
                /* CR - process the line */
                basic_newline();
                basic.line_buf[basic.line_pos] = '\0';

                if (basic.line_pos > 0) {
                    /* Convert to uppercase */
                    upper_string(basic.line_buf);

                    /* Check if it starts with a line number */
                    uint16_t linenum;
                    if (is_line_number(basic.line_buf, &linenum)) {
                        /* Find start of statement after line number */
                        char *p = basic.line_buf;
                        while (*p >= '0' && *p <= '9') p++;
                        while (*p == ' ') p++;

                        if (*p == '\0') {
                            /* Bare line number = delete line */
                            program_delete_line(linenum);
                        } else {
                            /* Tokenize and store */
                            int tlen = tokenize_line(p, basic.tok_buf,
                                                     sizeof(basic.tok_buf));
                            if (tlen > 0) {
                                program_insert_line(linenum, basic.tok_buf, tlen);
                            }
                            /* Error already reported by tokenizer if tlen <= 0 */
                        }
                    } else {
                        /* Immediate mode - tokenize and execute */
                        int tlen = tokenize_line(basic.line_buf, basic.tok_buf,
                                                 sizeof(basic.tok_buf));
                        if (tlen > 0) {
                            basic.exec_line_addr = 0; /* No stored line */
                            basic.exec_tok_pos = 0;
                            basic.exec_line_num = 0;

                            /* Copy tokens to a safe location for execution */
                            /* We execute from tok_buf directly for immediate mode */
                            exec_statement();
                        }
                    }
                }

                basic.line_pos = 0;
                if (basic.state == STATE_IDLE || basic.state == STATE_STOPPED) {
                    basic_prompt();
                }
            } else if (ch == 0x08 || ch == 0x7F || ch == '_') {
                /* Backspace / DEL / underscore (Apple 1 rubout) */
                if (basic.line_pos > 0) {
                    basic.line_pos--;
                    basic_putchar('\\');  /* Apple 1 style backspace echo */
                }
            } else if (ch == 0x18) {
                /* Ctrl+X - cancel line */
                basic.line_pos = 0;
                basic_newline();
                basic_prompt();
            } else if (ch >= 0x20 && ch < 0x7F) {
                /* Printable character */
                if (basic.line_pos < MAX_LINE_LEN - 1) {
                    basic.line_buf[basic.line_pos++] = (char)ch;
                    basic_putchar((uint8_t)ch);
                }
            }
        }
        break;

    case STATE_RUNNING:
        /* Execute one statement */
        if (basic.exec_line_addr == 0) {
            /* Should not happen */
            basic.state = STATE_IDLE;
            basic_prompt();
            return;
        }

        /* Read tokens from program memory */
        {
            /* Load the tokenized content of the current line into tok_buf */
            uint16_t addr = basic.exec_line_addr;
            /* Line format: [2-byte next][2-byte linenum][tokens...][0x00] */
            uint16_t next_ptr = mem_read(addr) | (mem_read(addr + 1) << 8);
            basic.exec_line_num = mem_read(addr + 2) | (mem_read(addr + 3) << 8);

            /* Calculate token length */
            int content_len = 0;
            if (next_ptr > addr + 4) {
                content_len = next_ptr - addr - 4 - 1; /* -1 for terminator */
            }
            if (content_len > 0 && content_len < (int)sizeof(basic.tok_buf)) {
                for (int i = 0; i < content_len; i++) {
                    basic.tok_buf[i] = mem_read(addr + 4 + i);
                }
                basic.tok_buf[content_len] = TOK_END;
                basic.tok_len = content_len;
            } else {
                basic.tok_buf[0] = TOK_END;
                basic.tok_len = 0;
            }

            /* Execute from the current position */
            basic.exec_advance = true;
            exec_statement();

            /* Advance to next line if needed */
            if (basic.state == STATE_RUNNING && basic.exec_advance) {
                if (next_ptr == 0 || next_ptr <= addr) {
                    /* End of program */
                    basic.state = STATE_IDLE;
                    basic_prompt();
                } else {
                    basic.exec_line_addr = next_ptr;
                    basic.exec_tok_pos = 0;
                }
            }
        }
        break;

    case STATE_INPUT_WAIT:
        /* Process input for INPUT statement */
        {
            int ch = basic_poll_char();
            if (ch < 0) return;

            if (ch == 0x0D || ch == '\n') {
                basic_newline();
                basic.input_line[basic.input_line_pos] = '\0';

                if (basic.input_is_string) {
                    /* Store string */
                    strncpy(basic.str_vars[basic.input_var_idx],
                            basic.input_line, MAX_STRING_LEN);
                    basic.str_vars[basic.input_var_idx][MAX_STRING_LEN] = '\0';
                } else {
                    /* Parse integer */
                    int val = atoi(basic.input_line);
                    if (val > 32767) val = 32767;
                    if (val < -32768) val = -32768;
                    basic.vars[basic.input_var_idx] = (int16_t)val;
                }

                basic.state = STATE_RUNNING;
                basic.exec_advance = true;
                /* Resume execution: advance to next line */
                {
                    uint16_t addr = basic.exec_line_addr;
                    uint16_t next_ptr = mem_read(addr) | (mem_read(addr + 1) << 8);
                    if (next_ptr == 0 || next_ptr <= addr) {
                        basic.state = STATE_IDLE;
                        basic_prompt();
                    } else {
                        basic.exec_line_addr = next_ptr;
                        basic.exec_tok_pos = 0;
                    }
                }
            } else if (ch == 0x08 || ch == 0x7F) {
                if (basic.input_line_pos > 0) {
                    basic.input_line_pos--;
                    basic_putchar('\\');
                }
            } else if (ch >= 0x20 && ch < 0x7F) {
                if (basic.input_line_pos < MAX_LINE_LEN - 1) {
                    basic.input_line[basic.input_line_pos++] = (char)ch;
                    basic_putchar((uint8_t)ch);
                }
            }
        }
        break;

    case STATE_INACTIVE:
        break;
    }
}

/* ======================================================================
 * I/O helpers
 * ====================================================================== */

static void basic_putchar(uint8_t ch)
{
    /* Output via PIA display register */
    pia_write(PIA_DSP, ch | 0x80);
}

static void basic_puts(const char *s)
{
    while (*s) {
        basic_putchar((uint8_t)*s);
        s++;
    }
}

static void basic_print_int(int16_t val)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", val);
    basic_puts(buf);
}

static void basic_newline(void)
{
    basic_putchar(0x0D);
}

static void basic_prompt(void)
{
    basic_putchar('>');
}

static int basic_poll_char(void)
{
    if (basic.input_count > 0) {
        uint8_t ch = basic.input_buf[basic.input_tail];
        basic.input_tail = (basic.input_tail + 1) % INPUT_BUF_SIZE;
        basic.input_count--;
        return ch;
    }
    return -1;
}

/* ======================================================================
 * Tokenizer
 *
 * Converts a BASIC source line (without line number) into tokenized form.
 * Returns the token length, or -1 on error.
 * ====================================================================== */

static int tokenize_line(const char *src, uint8_t *dst, int dst_size)
{
    int dpos = 0;
    int spos = 0;
    int slen = (int)strlen(src);

    while (spos < slen && dpos < dst_size - 2) {
        /* Skip spaces */
        while (spos < slen && src[spos] == ' ') spos++;
        if (spos >= slen) break;

        /* Check for REM - rest of line is comment */
        if (spos < slen - 2 &&
            src[spos] == 'R' && src[spos+1] == 'E' && src[spos+2] == 'M') {
            dst[dpos++] = TOK_REM;
            spos += 3;
            /* Store rest of line as a string */
            int rem_start = spos;
            int rem_len = slen - rem_start;
            if (rem_len > 127) rem_len = 127;
            dst[dpos++] = (uint8_t)rem_len;
            for (int i = 0; i < rem_len && dpos < dst_size - 1; i++) {
                dst[dpos++] = (uint8_t)src[rem_start + i];
            }
            break;
        }

        /* Check for string literal */
        if (src[spos] == '"') {
            spos++;  /* skip opening quote */
            dst[dpos++] = TOK_STRING;
            int len_pos = dpos++;  /* placeholder for length */
            int str_len = 0;
            while (spos < slen && src[spos] != '"' && dpos < dst_size - 1) {
                dst[dpos++] = (uint8_t)src[spos++];
                str_len++;
            }
            if (spos < slen && src[spos] == '"') spos++;  /* skip closing */
            dst[len_pos] = (uint8_t)str_len;
            continue;
        }

        /* Check for number */
        if (src[spos] >= '0' && src[spos] <= '9') {
            int val = 0;
            while (spos < slen && src[spos] >= '0' && src[spos] <= '9') {
                val = val * 10 + (src[spos] - '0');
                spos++;
            }
            if (val > 32767) val = 32767;
            dst[dpos++] = TOK_NUM;
            dst[dpos++] = (uint8_t)(val & 0xFF);
            dst[dpos++] = (uint8_t)((val >> 8) & 0xFF);
            continue;
        }

        /* Check for keywords (longest match first) */
        bool found_keyword = false;
        for (int k = 0; keywords[k].word != NULL; k++) {
            int kwlen = (int)strlen(keywords[k].word);
            if (spos + kwlen <= slen &&
                strncmp(&src[spos], keywords[k].word, kwlen) == 0) {
                /* Make sure it's not part of a longer identifier */
                if (spos + kwlen < slen && isalpha((unsigned char)src[spos + kwlen]) &&
                    src[spos + kwlen] != '(' && src[spos + kwlen] != '$') {
                    /* Could be a variable name starting with keyword letters */
                    /* Only match if keyword is followed by non-alpha or $ */
                    /* Exception: function names with $ like CHR$, STR$, etc are handled */
                    if (keywords[k].token < TOK_ABS) {
                        continue;  /* skip this keyword, try next */
                    }
                }
                dst[dpos++] = keywords[k].token;
                spos += kwlen;
                found_keyword = true;
                break;
            }
        }
        if (found_keyword) continue;

        /* Check for variable (A-Z, optionally followed by $) */
        if (src[spos] >= 'A' && src[spos] <= 'Z') {
            uint8_t var_idx = (uint8_t)(src[spos] - 'A');
            spos++;
            if (spos < slen && src[spos] == '$') {
                dst[dpos++] = TOK_STRVAR;
                dst[dpos++] = var_idx;
                spos++;
            } else if (spos < slen && src[spos] == '(') {
                /* Could be array reference */
                dst[dpos++] = TOK_ARRAYVAR;
                dst[dpos++] = var_idx;
                /* Don't consume the '(' - it will be tokenized as LPAREN */
            } else {
                dst[dpos++] = TOK_VAR;
                dst[dpos++] = var_idx;
            }
            continue;
        }

        /* Operators and punctuation */
        switch (src[spos]) {
        case '+': dst[dpos++] = TOK_PLUS;  spos++; continue;
        case '-': dst[dpos++] = TOK_MINUS; spos++; continue;
        case '*': dst[dpos++] = TOK_STAR;  spos++; continue;
        case '/': dst[dpos++] = TOK_SLASH; spos++; continue;
        case '(': dst[dpos++] = TOK_LPAREN; spos++; continue;
        case ')': dst[dpos++] = TOK_RPAREN; spos++; continue;
        case ',': dst[dpos++] = TOK_COMMA;  spos++; continue;
        case ';': dst[dpos++] = TOK_SEMICOLON; spos++; continue;
        case ':': dst[dpos++] = TOK_COLON;  spos++; continue;
        case '=': dst[dpos++] = TOK_EQ;    spos++; continue;
        case '<':
            spos++;
            if (spos < slen && src[spos] == '>') {
                dst[dpos++] = TOK_NE; spos++;
            } else if (spos < slen && src[spos] == '=') {
                dst[dpos++] = TOK_LE; spos++;
            } else {
                dst[dpos++] = TOK_LT;
            }
            continue;
        case '>':
            spos++;
            if (spos < slen && src[spos] == '=') {
                dst[dpos++] = TOK_GE; spos++;
            } else {
                dst[dpos++] = TOK_GT;
            }
            continue;
        default:
            /* Unknown character - skip it */
            spos++;
            continue;
        }
    }

    dst[dpos++] = TOK_END;
    return dpos;
}

/* ======================================================================
 * Program storage
 *
 * Lines stored in emulator RAM at PROG_BASE, format:
 *   [2-byte next-line pointer][2-byte line number][tokens...][0x00]
 * ====================================================================== */

static void program_clear(void)
{
    basic.prog_end = basic.prog_base;
    /* Write a zero next-pointer to terminate program list */
    mem_write(basic.prog_base, 0x00);
    mem_write(basic.prog_base + 1, 0x00);
}

static void program_insert_line(uint16_t linenum, const uint8_t *tokens, int tok_len)
{
    /* First, delete any existing line with this number */
    program_delete_line(linenum);

    /* Calculate new line size: 2(next) + 2(linenum) + tok_len + 1(terminator) */
    int line_size = 4 + tok_len + 1;

    /* Find insertion point (lines are stored in ascending order) */
    uint16_t insert_addr = basic.prog_base;
    uint16_t prev_addr = 0;

    while (insert_addr < basic.prog_end) {
        uint16_t next_ptr = mem_read(insert_addr) | (mem_read(insert_addr + 1) << 8);
        if (next_ptr == 0) break;
        uint16_t ln = mem_read(insert_addr + 2) | (mem_read(insert_addr + 3) << 8);
        if (ln > linenum) break;
        prev_addr = insert_addr;
        insert_addr = next_ptr;
    }

    /* Check memory limits */
    if (basic.prog_end + line_size >= PROG_TOP) {
        exec_error_line("OUT OF MEMORY", linenum);
        return;
    }

    /* Make room: shift everything from insert_addr to prog_end up by line_size */
    if (insert_addr < basic.prog_end) {
        int bytes_to_move = basic.prog_end - insert_addr;
        /* Move from end to avoid overlap issues */
        for (int i = bytes_to_move - 1; i >= 0; i--) {
            mem_write(insert_addr + line_size + i, mem_read(insert_addr + i));
        }

        /* Update next-pointers for all shifted lines */
        uint16_t scan = insert_addr + line_size;
        while (scan < basic.prog_end + line_size) {
            uint16_t np = mem_read(scan) | (mem_read(scan + 1) << 8);
            if (np == 0) break;
            np += line_size;
            mem_write(scan, np & 0xFF);
            mem_write(scan + 1, (np >> 8) & 0xFF);
            scan = np;
        }
    }

    /* Write the new line */
    uint16_t next_val = insert_addr + line_size;
    /* Check if this is the last line */
    if (insert_addr >= basic.prog_end) {
        /* We're appending at the end; next line has zero pointer */
        /* Write terminator after our line */
        mem_write(next_val, 0x00);
        mem_write(next_val + 1, 0x00);
    }

    mem_write(insert_addr, next_val & 0xFF);
    mem_write(insert_addr + 1, (next_val >> 8) & 0xFF);
    mem_write(insert_addr + 2, linenum & 0xFF);
    mem_write(insert_addr + 3, (linenum >> 8) & 0xFF);

    for (int i = 0; i < tok_len; i++) {
        mem_write(insert_addr + 4 + i, tokens[i]);
    }
    mem_write(insert_addr + 4 + tok_len, 0x00);  /* terminator */

    /* Update prev_addr's next pointer if we inserted in the middle */
    if (prev_addr != 0) {
        mem_write(prev_addr, insert_addr & 0xFF);
        mem_write(prev_addr + 1, (insert_addr >> 8) & 0xFF);
    }

    basic.prog_end += line_size;
}

static void program_delete_line(uint16_t linenum)
{
    uint16_t addr = basic.prog_base;
    uint16_t prev_addr = 0;

    while (addr < basic.prog_end) {
        uint16_t next_ptr = mem_read(addr) | (mem_read(addr + 1) << 8);
        if (next_ptr == 0) return;  /* End of program, line not found */

        uint16_t ln = mem_read(addr + 2) | (mem_read(addr + 3) << 8);
        if (ln == linenum) {
            /* Found it - remove by shifting subsequent lines down */
            int line_size = next_ptr - addr;
            int bytes_after = basic.prog_end - next_ptr;

            for (int i = 0; i < bytes_after; i++) {
                mem_write(addr + i, mem_read(next_ptr + i));
            }

            /* Update next-pointers for shifted lines */
            uint16_t scan = addr;
            while (scan < basic.prog_end - line_size) {
                uint16_t np = mem_read(scan) | (mem_read(scan + 1) << 8);
                if (np == 0) break;
                if (np > addr) {
                    np -= line_size;
                    mem_write(scan, np & 0xFF);
                    mem_write(scan + 1, (np >> 8) & 0xFF);
                }
                scan = np;
            }

            /* Update prev pointer */
            if (prev_addr != 0) {
                uint16_t new_next = (next_ptr > addr) ? addr : 0;
                /* Recalculate: prev should point to where 'next_ptr' content now lives */
                mem_write(prev_addr, addr & 0xFF);
                mem_write(prev_addr + 1, (addr >> 8) & 0xFF);
                (void)new_next;
            }

            basic.prog_end -= line_size;
            return;
        }

        if (ln > linenum) return;  /* Past where it would be */
        prev_addr = addr;
        addr = next_ptr;
    }
}

static uint16_t program_find_line(uint16_t linenum)
{
    uint16_t addr = basic.prog_base;

    while (addr < basic.prog_end) {
        uint16_t next_ptr = mem_read(addr) | (mem_read(addr + 1) << 8);
        if (next_ptr == 0) return 0;

        uint16_t ln = mem_read(addr + 2) | (mem_read(addr + 3) << 8);
        if (ln == linenum) return addr;
        if (ln > linenum) return 0;  /* Not found */

        addr = next_ptr;
    }
    return 0;
}

static uint16_t program_first_line(void)
{
    if (basic.prog_end <= basic.prog_base) return 0;
    uint16_t next_ptr = mem_read(basic.prog_base) | (mem_read(basic.prog_base + 1) << 8);
    if (next_ptr == 0) return 0;
    return basic.prog_base;
}

static uint16_t program_next_line(uint16_t addr)
{
    if (addr == 0) return 0;
    uint16_t next_ptr = mem_read(addr) | (mem_read(addr + 1) << 8);
    if (next_ptr == 0) return 0;
    /* Verify next line has a valid next pointer (not end) */
    uint16_t nn = mem_read(next_ptr) | (mem_read(next_ptr + 1) << 8);
    (void)nn;  /* We still return it even if it's the last */
    return next_ptr;
}

static uint16_t program_get_linenum(uint16_t addr)
{
    if (addr == 0) return 0;
    return mem_read(addr + 2) | (mem_read(addr + 3) << 8);
}

/* ======================================================================
 * Execution engine
 * ====================================================================== */

static void exec_start(uint16_t start_line)
{
    uint16_t addr;
    if (start_line == 0) {
        addr = program_first_line();
    } else {
        addr = program_find_line(start_line);
    }

    if (addr == 0) {
        exec_error("UNDEF'D STATEMENT");
        return;
    }

    basic.exec_line_addr = addr;
    basic.exec_tok_pos = 0;
    basic.state = STATE_RUNNING;
    basic.break_flag = false;
    basic.gosub_sp = 0;
    basic.for_sp = 0;
}

static void exec_statement(void)
{
    tok_skip_spaces();

    if (tok_at_end()) return;

    uint8_t tok = tok_peek();

    switch (tok) {
    case TOK_PRINT:     tok_read(); stmt_print(); break;
    case TOK_INPUT:     tok_read(); stmt_input(); break;
    case TOK_LET:       tok_read(); stmt_let();   break;
    case TOK_IF:        tok_read(); stmt_if();    break;
    case TOK_GOTO:      tok_read(); stmt_goto();  break;
    case TOK_GOSUB:     tok_read(); stmt_gosub(); break;
    case TOK_RETURN:    tok_read(); stmt_return(); break;
    case TOK_FOR:       tok_read(); stmt_for();   break;
    case TOK_NEXT:      tok_read(); stmt_next();  break;
    case TOK_DIM:       tok_read(); stmt_dim();   break;
    case TOK_REM:       tok_read(); stmt_rem();   break;
    case TOK_STOP:      tok_read(); stmt_stop();  break;
    case TOK_END_STMT:  tok_read(); stmt_end();   break;
    case TOK_POKE:      tok_read(); stmt_poke();  break;
    case TOK_CALL:      tok_read(); stmt_call();  break;
    case TOK_DEL:       tok_read(); stmt_del();   break;
    case TOK_RUN:       tok_read(); cmd_run();    break;
    case TOK_LIST:      tok_read(); cmd_list();   break;
    case TOK_NEW:       tok_read(); cmd_new();    break;
    case TOK_CLR:       tok_read(); cmd_clr();    break;

    case TOK_VAR:
        /* Implicit LET */
        stmt_let();
        break;

    case TOK_STRVAR:
        /* Implicit LET for string variable */
        stmt_let();
        break;

    case TOK_ARRAYVAR:
        /* Implicit LET for array element */
        stmt_let();
        break;

    default:
        exec_error("SYNTAX");
        break;
    }

    /* Handle colon (multi-statement line) */
    if (basic.state == STATE_RUNNING && !tok_at_end()) {
        tok_skip_spaces();
        if (tok_peek() == TOK_COLON) {
            tok_read();
            exec_statement();  /* Execute next statement on same line */
        }
    }
}

static void exec_error(const char *msg)
{
    basic_newline();
    basic_putchar('?');
    basic_puts(msg);
    basic_puts(" ERROR");
    if (basic.exec_line_num > 0) {
        basic_puts(" IN ");
        basic_print_int((int16_t)basic.exec_line_num);
    }
    basic_newline();
    basic.state = STATE_STOPPED;
}

/*
 * Report an error with an explicit line number (used when the current
 * exec_line_num may not reflect the error location, e.g. for GOTO targets).
 */
static void exec_error_line(const char *msg, uint16_t linenum)
{
    basic_newline();
    basic_putchar('?');
    basic_puts(msg);
    basic_puts(" ERROR IN ");
    basic_print_int((int16_t)linenum);
    basic_newline();
    basic.state = STATE_STOPPED;
}

/* ======================================================================
 * Statement handlers
 * ====================================================================== */

static void stmt_print(void)
{
    bool need_newline = true;

    while (!tok_at_end() && tok_peek() != TOK_COLON) {
        tok_skip_spaces();
        uint8_t tok = tok_peek();

        if (tok == TOK_END || tok == TOK_COLON) break;

        if (tok == TOK_SEMICOLON) {
            tok_read();
            need_newline = false;
            continue;
        }

        if (tok == TOK_COMMA) {
            tok_read();
            /* Tab to next column (every 8 chars) */
            basic_putchar(' ');
            basic_putchar(' ');
            basic_putchar(' ');
            basic_putchar(' ');
            need_newline = true;
            continue;
        }

        /* Check if this is a string expression */
        if (tok == TOK_STRING || tok == TOK_STRVAR ||
            tok == TOK_CHR || tok == TOK_LEFT ||
            tok == TOK_RIGHT || tok == TOK_MID || tok == TOK_STR) {
            char str_result[MAX_STRING_LEN + 1];
            eval_string_expr(str_result, MAX_STRING_LEN);
            basic_puts(str_result);
        } else {
            /* Numeric expression */
            int16_t val = eval_expression();
            if (basic.state == STATE_STOPPED) return;
            basic_print_int(val);
        }
        need_newline = true;

        /* Check for semicolon/comma after expression */
        tok_skip_spaces();
        tok = tok_peek();
        if (tok == TOK_SEMICOLON) {
            tok_read();
            need_newline = false;
        } else if (tok == TOK_COMMA) {
            /* Don't consume - will be handled at top of loop */
        }
    }

    if (need_newline) {
        basic_newline();
    }
}

static void stmt_input(void)
{
    tok_skip_spaces();

    /* Check for optional prompt string */
    if (tok_peek() == TOK_STRING) {
        char prompt[MAX_STRING_LEN + 1];
        tok_read();
        tok_read_string(prompt, MAX_STRING_LEN);
        basic_puts(prompt);

        /* Expect semicolon after prompt */
        tok_skip_spaces();
        if (tok_peek() == TOK_SEMICOLON) {
            tok_read();
        }
    } else {
        basic_putchar('?');
    }

    tok_skip_spaces();

    /* Get the variable */
    uint8_t tok = tok_peek();
    if (tok == TOK_VAR) {
        tok_read();
        basic.input_var_idx = tok_read();
        basic.input_is_string = false;
    } else if (tok == TOK_STRVAR) {
        tok_read();
        basic.input_var_idx = tok_read();
        basic.input_is_string = true;
    } else {
        exec_error("SYNTAX");
        return;
    }

    /* Switch to input wait state */
    basic.input_line_pos = 0;
    basic.state = STATE_INPUT_WAIT;
    basic.exec_advance = false;
}

static void stmt_let(void)
{
    tok_skip_spaces();
    uint8_t tok = tok_peek();

    if (tok == TOK_VAR) {
        /* Simple variable assignment: V = expr */
        tok_read();
        uint8_t var_idx = tok_read();

        tok_skip_spaces();
        if (tok_peek() != TOK_EQ) {
            exec_error("SYNTAX");
            return;
        }
        tok_read();  /* consume '=' */

        int16_t val = eval_expression();
        if (basic.state == STATE_STOPPED) return;
        basic.vars[var_idx] = val;

    } else if (tok == TOK_STRVAR) {
        /* String variable assignment: V$ = string_expr */
        tok_read();
        uint8_t var_idx = tok_read();

        tok_skip_spaces();
        if (tok_peek() != TOK_EQ) {
            exec_error("SYNTAX");
            return;
        }
        tok_read();  /* consume '=' */

        char str_result[MAX_STRING_LEN + 1];
        eval_string_expr(str_result, MAX_STRING_LEN);
        if (basic.state == STATE_STOPPED) return;
        strncpy(basic.str_vars[var_idx], str_result, MAX_STRING_LEN);
        basic.str_vars[var_idx][MAX_STRING_LEN] = '\0';

    } else if (tok == TOK_ARRAYVAR) {
        /* Array element assignment: V(idx) = expr */
        tok_read();
        uint8_t var_idx = tok_read();

        /* Expect '(' index ')' */
        tok_skip_spaces();
        if (tok_peek() != TOK_LPAREN) {
            exec_error("SYNTAX");
            return;
        }
        tok_read();

        int16_t index = eval_expression();
        if (basic.state == STATE_STOPPED) return;

        tok_skip_spaces();
        if (tok_peek() != TOK_RPAREN) {
            exec_error("SYNTAX");
            return;
        }
        tok_read();

        /* Check array bounds */
        if (basic.array_size[var_idx] == 0) {
            exec_error("DIM");
            return;
        }
        if (index < 0 || index >= (int16_t)basic.array_size[var_idx]) {
            exec_error("RANGE");
            return;
        }

        tok_skip_spaces();
        if (tok_peek() != TOK_EQ) {
            exec_error("SYNTAX");
            return;
        }
        tok_read();

        int16_t val = eval_expression();
        if (basic.state == STATE_STOPPED) return;

        /* Write to array in emulator memory */
        uint16_t elem_addr = basic.array_base[var_idx] + (uint16_t)(index * 2);
        mem_write(elem_addr, val & 0xFF);
        mem_write(elem_addr + 1, (val >> 8) & 0xFF);

    } else {
        exec_error("SYNTAX");
    }
}

static void stmt_if(void)
{
    /* Determine if this is a string comparison or numeric */
    tok_skip_spaces();

    /* Try to detect string comparison: A$ = "..." or A$ <> "..." */
    bool is_string_cmp = false;
    int saved_pos = basic.exec_tok_pos;

    if (tok_peek() == TOK_STRVAR) {
        is_string_cmp = true;
    }

    if (is_string_cmp) {
        /* String comparison */
        char left[MAX_STRING_LEN + 1];
        eval_string_expr(left, MAX_STRING_LEN);
        if (basic.state == STATE_STOPPED) return;

        tok_skip_spaces();
        uint8_t op = tok_read();
        if (op != TOK_EQ && op != TOK_NE && op != TOK_LT &&
            op != TOK_GT && op != TOK_LE && op != TOK_GE) {
            exec_error("SYNTAX");
            return;
        }

        char right[MAX_STRING_LEN + 1];
        eval_string_expr(right, MAX_STRING_LEN);
        if (basic.state == STATE_STOPPED) return;

        int cmp = strcmp(left, right);
        bool result = false;
        switch (op) {
        case TOK_EQ: result = (cmp == 0); break;
        case TOK_NE: result = (cmp != 0); break;
        case TOK_LT: result = (cmp < 0);  break;
        case TOK_GT: result = (cmp > 0);  break;
        case TOK_LE: result = (cmp <= 0); break;
        case TOK_GE: result = (cmp >= 0); break;
        }

        tok_skip_spaces();
        if (tok_peek() != TOK_THEN) {
            exec_error("SYNTAX");
            return;
        }
        tok_read();

        if (result) {
            exec_statement();
        }
        /* If false, skip rest of line (don't advance to else) */
        /* We signal "already handled" by consuming to end */
        if (!result) {
            while (!tok_at_end()) tok_read();
        }
    } else {
        /* Numeric IF */
        basic.exec_tok_pos = saved_pos;  /* restore in case we peeked */
        int16_t cond = eval_expression();
        if (basic.state == STATE_STOPPED) return;

        tok_skip_spaces();
        if (tok_peek() != TOK_THEN) {
            exec_error("SYNTAX");
            return;
        }
        tok_read();

        if (cond != 0) {
            /* Condition is true - check if THEN is followed by line number */
            tok_skip_spaces();
            if (tok_peek() == TOK_NUM) {
                /* GOTO the line number */
                int16_t target = tok_read_int();
                uint16_t addr = program_find_line((uint16_t)target);
                if (addr == 0) {
                    exec_error("UNDEF'D STATEMENT");
                    return;
                }
                basic.exec_line_addr = addr;
                basic.exec_tok_pos = 0;
                basic.exec_advance = false;
            } else {
                exec_statement();
            }
        } else {
            /* Skip rest of line */
            while (!tok_at_end()) tok_read();
        }
    }
}

static void stmt_goto(void)
{
    tok_skip_spaces();
    int16_t target = eval_expression();
    if (basic.state == STATE_STOPPED) return;

    uint16_t addr = program_find_line((uint16_t)target);
    if (addr == 0) {
        exec_error("UNDEF'D STATEMENT");
        return;
    }
    basic.exec_line_addr = addr;
    basic.exec_tok_pos = 0;
    basic.exec_advance = false;
}

static void stmt_gosub(void)
{
    if (basic.gosub_sp >= MAX_GOSUB_DEPTH) {
        exec_error("OUT OF MEMORY");
        return;
    }

    tok_skip_spaces();
    int16_t target = eval_expression();
    if (basic.state == STATE_STOPPED) return;

    uint16_t addr = program_find_line((uint16_t)target);
    if (addr == 0) {
        exec_error("UNDEF'D STATEMENT");
        return;
    }

    /* Push return address */
    basic.gosub_stack[basic.gosub_sp].line_addr = basic.exec_line_addr;
    basic.gosub_stack[basic.gosub_sp].tok_offset = (uint16_t)basic.exec_tok_pos;
    basic.gosub_sp++;

    basic.exec_line_addr = addr;
    basic.exec_tok_pos = 0;
    basic.exec_advance = false;
}

static void stmt_return(void)
{
    if (basic.gosub_sp <= 0) {
        exec_error("RETURN WITHOUT GOSUB");
        return;
    }

    basic.gosub_sp--;

    /* Return to the line AFTER the GOSUB */
    uint16_t ret_addr = basic.gosub_stack[basic.gosub_sp].line_addr;
    uint16_t next_ptr = mem_read(ret_addr) | (mem_read(ret_addr + 1) << 8);

    if (next_ptr == 0) {
        basic.state = STATE_IDLE;
        basic_prompt();
    } else {
        basic.exec_line_addr = next_ptr;
        basic.exec_tok_pos = 0;
        basic.exec_advance = false;
    }
}

static void stmt_for(void)
{
    if (basic.for_sp >= MAX_FOR_DEPTH) {
        exec_error("OUT OF MEMORY");
        return;
    }

    tok_skip_spaces();
    if (tok_peek() != TOK_VAR) {
        exec_error("SYNTAX");
        return;
    }
    tok_read();
    uint8_t var_idx = tok_read();

    tok_skip_spaces();
    if (tok_peek() != TOK_EQ) {
        exec_error("SYNTAX");
        return;
    }
    tok_read();

    int16_t start_val = eval_expression();
    if (basic.state == STATE_STOPPED) return;

    tok_skip_spaces();
    if (tok_peek() != TOK_TO) {
        exec_error("SYNTAX");
        return;
    }
    tok_read();

    int16_t limit = eval_expression();
    if (basic.state == STATE_STOPPED) return;

    int16_t step = 1;
    tok_skip_spaces();
    if (tok_peek() == TOK_STEP) {
        tok_read();
        step = eval_expression();
        if (basic.state == STATE_STOPPED) return;
        if (step == 0) {
            exec_error("SYNTAX");
            return;
        }
    }

    /* Set the loop variable */
    basic.vars[var_idx] = start_val;

    /* Check if we should even enter the loop */
    bool skip = false;
    if (step > 0 && start_val > limit) skip = true;
    if (step < 0 && start_val < limit) skip = true;

    if (skip) {
        /* Skip to matching NEXT */
        /* For simplicity, we scan forward through program lines looking for NEXT var */
        /* For now, just skip to end of line and hope NEXT is on a following line */
        while (!tok_at_end()) tok_read();
        /* We'd need to skip multiple lines - complex. For now, just proceed */
        /* and let the NEXT handle it */
    }

    /* Push FOR entry */
    /* First check if we already have a FOR for this variable - replace it */
    int existing = -1;
    for (int i = 0; i < basic.for_sp; i++) {
        if (basic.for_stack[i].var_idx == var_idx) {
            existing = i;
            break;
        }
    }

    if (existing >= 0) {
        basic.for_stack[existing].limit = limit;
        basic.for_stack[existing].step = step;
        basic.for_stack[existing].line_addr = basic.exec_line_addr;
        basic.for_stack[existing].tok_offset = (uint16_t)basic.exec_tok_pos;
    } else {
        basic.for_stack[basic.for_sp].var_idx = var_idx;
        basic.for_stack[basic.for_sp].limit = limit;
        basic.for_stack[basic.for_sp].step = step;
        basic.for_stack[basic.for_sp].line_addr = basic.exec_line_addr;
        basic.for_stack[basic.for_sp].tok_offset = (uint16_t)basic.exec_tok_pos;
        basic.for_sp++;
    }
}

static void stmt_next(void)
{
    tok_skip_spaces();

    /* Get optional variable name */
    uint8_t var_idx = 255;
    if (tok_peek() == TOK_VAR) {
        tok_read();
        var_idx = tok_read();
    }

    /* Find matching FOR on stack */
    int found = -1;
    if (var_idx == 255) {
        /* No variable specified - use top of stack */
        if (basic.for_sp > 0) {
            found = basic.for_sp - 1;
            var_idx = basic.for_stack[found].var_idx;
        }
    } else {
        /* Search for matching variable */
        for (int i = basic.for_sp - 1; i >= 0; i--) {
            if (basic.for_stack[i].var_idx == var_idx) {
                found = i;
                break;
            }
        }
    }

    if (found < 0) {
        exec_error("NEXT WITHOUT FOR");
        return;
    }

    /* Increment variable */
    basic.vars[var_idx] += basic.for_stack[found].step;

    /* Check if loop is done */
    bool done = false;
    if (basic.for_stack[found].step > 0) {
        done = (basic.vars[var_idx] > basic.for_stack[found].limit);
    } else {
        done = (basic.vars[var_idx] < basic.for_stack[found].limit);
    }

    if (done) {
        /* Pop FOR stack entries from 'found' upward */
        basic.for_sp = found;
        /* Continue to next statement */
    } else {
        /* Loop back: go to the line after the FOR statement */
        uint16_t for_line = basic.for_stack[found].line_addr;
        uint16_t next_ptr = mem_read(for_line) | (mem_read(for_line + 1) << 8);
        if (next_ptr != 0) {
            basic.exec_line_addr = next_ptr;
            basic.exec_tok_pos = 0;
            basic.exec_advance = false;
        }
    }
}

static void stmt_dim(void)
{
    tok_skip_spaces();

    /* Expect: VAR(size) or ARRAYVAR(size) */
    uint8_t tok = tok_peek();
    uint8_t var_idx;

    if (tok == TOK_VAR) {
        tok_read();
        var_idx = tok_read();
    } else if (tok == TOK_ARRAYVAR) {
        tok_read();
        var_idx = tok_read();
    } else {
        exec_error("SYNTAX");
        return;
    }

    tok_skip_spaces();
    if (tok_peek() != TOK_LPAREN) {
        exec_error("SYNTAX");
        return;
    }
    tok_read();

    int16_t size = eval_expression();
    if (basic.state == STATE_STOPPED) return;

    tok_skip_spaces();
    if (tok_peek() != TOK_RPAREN) {
        exec_error("SYNTAX");
        return;
    }
    tok_read();

    if (size <= 0 || size > 4096) {
        exec_error("RANGE");
        return;
    }

    /* Check if already dimensioned */
    if (basic.array_size[var_idx] != 0) {
        exec_error("REDIM");
        return;
    }

    /* Allocate from array_top growing downward */
    uint16_t bytes_needed = (uint16_t)(size * 2);  /* 2 bytes per integer */
    if (basic.array_top - bytes_needed <= basic.prog_end) {
        exec_error("OUT OF MEMORY");
        return;
    }

    basic.array_top -= bytes_needed;
    basic.array_base[var_idx] = basic.array_top;
    basic.array_size[var_idx] = (uint16_t)size;

    /* Zero-fill the array */
    for (uint16_t i = 0; i < bytes_needed; i++) {
        mem_write(basic.array_top + i, 0);
    }
}

static void stmt_poke(void)
{
    tok_skip_spaces();
    int16_t addr_val = eval_expression();
    if (basic.state == STATE_STOPPED) return;

    tok_skip_spaces();
    if (tok_peek() != TOK_COMMA) {
        exec_error("SYNTAX");
        return;
    }
    tok_read();

    int16_t val = eval_expression();
    if (basic.state == STATE_STOPPED) return;

    mem_write((uint16_t)addr_val, (uint8_t)(val & 0xFF));
}

static void stmt_call(void)
{
    tok_skip_spaces();
    int16_t addr_val = eval_expression();
    if (basic.state == STATE_STOPPED) return;

    /* CALL -151 = exit to monitor */
    if (addr_val == -151 || (uint16_t)addr_val == 0xFF69) {
        basic.state = STATE_INACTIVE;
        basic_newline();
        /* Return to monitor - the main loop will resume normal CPU execution */
        return;
    }

    /* For other addresses, we just ignore (can't execute arbitrary 6502 code) */
    /* Could potentially set the CPU PC and return to normal execution */
}

static void stmt_end(void)
{
    basic.state = STATE_IDLE;
    basic_prompt();
}

static void stmt_stop(void)
{
    basic_newline();
    basic_puts("BREAK IN ");
    basic_print_int((int16_t)basic.exec_line_num);
    basic_newline();
    basic.state = STATE_STOPPED;
    basic_prompt();
}

static void stmt_del(void)
{
    tok_skip_spaces();
    int16_t start = eval_expression();
    if (basic.state == STATE_STOPPED) return;

    int16_t end = start;
    tok_skip_spaces();
    if (tok_peek() == TOK_COMMA) {
        tok_read();
        end = eval_expression();
        if (basic.state == STATE_STOPPED) return;
    }

    /* Delete lines in range */
    for (int16_t ln = start; ln <= end; ln++) {
        program_delete_line((uint16_t)ln);
    }
}

static void stmt_rem(void)
{
    /* Skip the rest of the tokenized content (length byte + chars) */
    if (!tok_at_end()) {
        uint8_t len = tok_read();
        for (int i = 0; i < len && !tok_at_end(); i++) {
            tok_read();
        }
    }
}

/* ======================================================================
 * Immediate mode commands
 * ====================================================================== */

static void cmd_run(void)
{
    clear_variables();

    tok_skip_spaces();
    uint16_t start_line = 0;
    if (!tok_at_end() && tok_peek() == TOK_NUM) {
        start_line = (uint16_t)tok_read_int();
    }

    exec_start(start_line);
}

static void cmd_list(void)
{
    uint16_t start_line = 0;
    uint16_t end_line = MAX_LINE_NUM;

    tok_skip_spaces();
    if (!tok_at_end() && tok_peek() == TOK_NUM) {
        start_line = (uint16_t)tok_read_int();
        end_line = start_line;

        tok_skip_spaces();
        if (tok_peek() == TOK_MINUS || tok_peek() == TOK_COMMA) {
            tok_read();
            tok_skip_spaces();
            if (!tok_at_end() && tok_peek() == TOK_NUM) {
                end_line = (uint16_t)tok_read_int();
            } else {
                end_line = MAX_LINE_NUM;
            }
        }
    }

    /* Walk through program lines and detokenize */
    uint16_t addr = program_first_line();
    while (addr != 0) {
        uint16_t linenum = program_get_linenum(addr);
        if (linenum > end_line) break;

        if (linenum >= start_line) {
            /* Print line number */
            basic_print_int((int16_t)linenum);
            basic_putchar(' ');

            /* Detokenize the line content */
            uint16_t next_ptr = mem_read(addr) | (mem_read(addr + 1) << 8);
            int content_len = 0;
            if (next_ptr > addr + 4) {
                content_len = next_ptr - addr - 4 - 1;
            }

            /* Read tokens and print them */
            for (int i = 0; i < content_len; ) {
                uint8_t t = mem_read(addr + 4 + i);
                i++;

                if (t == TOK_END) break;

                switch (t) {
                case TOK_NUM: {
                    if (i + 1 < content_len) {
                        int16_t v = (int16_t)(mem_read(addr + 4 + i) |
                                    (mem_read(addr + 4 + i + 1) << 8));
                        i += 2;
                        basic_print_int(v);
                    }
                    break;
                }
                case TOK_STRING: {
                    uint8_t slen = mem_read(addr + 4 + i);
                    i++;
                    basic_putchar('"');
                    for (int j = 0; j < slen && i < content_len; j++, i++) {
                        basic_putchar(mem_read(addr + 4 + i));
                    }
                    basic_putchar('"');
                    break;
                }
                case TOK_VAR: {
                    uint8_t vi = mem_read(addr + 4 + i);
                    i++;
                    basic_putchar('A' + vi);
                    break;
                }
                case TOK_STRVAR: {
                    uint8_t vi = mem_read(addr + 4 + i);
                    i++;
                    basic_putchar('A' + vi);
                    basic_putchar('$');
                    break;
                }
                case TOK_ARRAYVAR: {
                    uint8_t vi = mem_read(addr + 4 + i);
                    i++;
                    basic_putchar('A' + vi);
                    break;
                }
                case TOK_REM: {
                    basic_puts("REM");
                    if (i < content_len) {
                        uint8_t rlen = mem_read(addr + 4 + i);
                        i++;
                        for (int j = 0; j < rlen && i < content_len; j++, i++) {
                            basic_putchar(mem_read(addr + 4 + i));
                        }
                    }
                    break;
                }

                /* Keywords */
                case TOK_PRINT:    basic_puts("PRINT "); break;
                case TOK_INPUT:    basic_puts("INPUT "); break;
                case TOK_LET:      basic_puts("LET "); break;
                case TOK_IF:       basic_puts("IF "); break;
                case TOK_THEN:     basic_puts("THEN "); break;
                case TOK_GOTO:     basic_puts("GOTO "); break;
                case TOK_GOSUB:    basic_puts("GOSUB "); break;
                case TOK_RETURN:   basic_puts("RETURN"); break;
                case TOK_FOR:      basic_puts("FOR "); break;
                case TOK_TO:       basic_puts(" TO "); break;
                case TOK_STEP:     basic_puts(" STEP "); break;
                case TOK_NEXT:     basic_puts("NEXT "); break;
                case TOK_DIM:      basic_puts("DIM "); break;
                case TOK_STOP:     basic_puts("STOP"); break;
                case TOK_END_STMT: basic_puts("END"); break;
                case TOK_POKE:     basic_puts("POKE "); break;
                case TOK_CALL:     basic_puts("CALL "); break;
                case TOK_DEL:      basic_puts("DEL "); break;
                case TOK_RUN:      basic_puts("RUN"); break;
                case TOK_LIST:     basic_puts("LIST"); break;
                case TOK_NEW:      basic_puts("NEW"); break;
                case TOK_CLR:      basic_puts("CLR"); break;

                /* Operators */
                case TOK_PLUS:      basic_putchar('+'); break;
                case TOK_MINUS:     basic_putchar('-'); break;
                case TOK_STAR:      basic_putchar('*'); break;
                case TOK_SLASH:     basic_putchar('/'); break;
                case TOK_MOD:       basic_puts(" MOD "); break;
                case TOK_EQ:        basic_putchar('='); break;
                case TOK_NE:        basic_puts("<>"); break;
                case TOK_LT:        basic_putchar('<'); break;
                case TOK_GT:        basic_putchar('>'); break;
                case TOK_LE:        basic_puts("<="); break;
                case TOK_GE:        basic_puts(">="); break;
                case TOK_AND:       basic_puts(" AND "); break;
                case TOK_OR:        basic_puts(" OR "); break;
                case TOK_NOT:       basic_puts("NOT "); break;

                /* Punctuation */
                case TOK_LPAREN:    basic_putchar('('); break;
                case TOK_RPAREN:    basic_putchar(')'); break;
                case TOK_COMMA:     basic_putchar(','); break;
                case TOK_SEMICOLON: basic_putchar(';'); break;
                case TOK_COLON:     basic_putchar(':'); break;

                /* Functions */
                case TOK_ABS:   basic_puts("ABS"); break;
                case TOK_SGN:   basic_puts("SGN"); break;
                case TOK_RND:   basic_puts("RND"); break;
                case TOK_PEEK:  basic_puts("PEEK"); break;
                case TOK_LEN:   basic_puts("LEN"); break;
                case TOK_ASC:   basic_puts("ASC"); break;
                case TOK_VAL:   basic_puts("VAL"); break;
                case TOK_CHR:   basic_puts("CHR$"); break;
                case TOK_STR:   basic_puts("STR$"); break;
                case TOK_LEFT:  basic_puts("LEFT$"); break;
                case TOK_RIGHT: basic_puts("RIGHT$"); break;
                case TOK_MID:   basic_puts("MID$"); break;

                default:
                    basic_putchar('?');
                    break;
                }
            }
            basic_newline();
        }

        addr = program_next_line(addr);
    }
}

static void cmd_new(void)
{
    program_clear();
    clear_variables();
}

static void cmd_clr(void)
{
    clear_variables();
}

/* ======================================================================
 * Expression evaluator (recursive descent)
 *
 * Precedence (lowest to highest):
 *   OR
 *   AND
 *   NOT
 *   = <> < > <= >=
 *   + -
 *   * / MOD
 *   unary - / NOT
 *   primary (number, variable, function, parenthesized)
 * ====================================================================== */

static int16_t eval_expression(void)
{
    return eval_or();
}

static int16_t eval_or(void)
{
    int16_t left = eval_and();
    if (basic.state == STATE_STOPPED) return 0;

    while (tok_peek() == TOK_OR) {
        tok_read();
        int16_t right = eval_and();
        if (basic.state == STATE_STOPPED) return 0;
        left = (left || right) ? 1 : 0;
    }
    return left;
}

static int16_t eval_and(void)
{
    int16_t left = eval_not();
    if (basic.state == STATE_STOPPED) return 0;

    while (tok_peek() == TOK_AND) {
        tok_read();
        int16_t right = eval_not();
        if (basic.state == STATE_STOPPED) return 0;
        left = (left && right) ? 1 : 0;
    }
    return left;
}

static int16_t eval_not(void)
{
    if (tok_peek() == TOK_NOT) {
        tok_read();
        int16_t val = eval_not();
        if (basic.state == STATE_STOPPED) return 0;
        return val ? 0 : 1;
    }
    return eval_comparison();
}

static int16_t eval_comparison(void)
{
    int16_t left = eval_add_sub();
    if (basic.state == STATE_STOPPED) return 0;

    tok_skip_spaces();
    uint8_t op = tok_peek();

    if (op == TOK_EQ || op == TOK_NE || op == TOK_LT ||
        op == TOK_GT || op == TOK_LE || op == TOK_GE) {
        tok_read();
        int16_t right = eval_add_sub();
        if (basic.state == STATE_STOPPED) return 0;

        switch (op) {
        case TOK_EQ: return (left == right) ? 1 : 0;
        case TOK_NE: return (left != right) ? 1 : 0;
        case TOK_LT: return (left < right)  ? 1 : 0;
        case TOK_GT: return (left > right)  ? 1 : 0;
        case TOK_LE: return (left <= right) ? 1 : 0;
        case TOK_GE: return (left >= right) ? 1 : 0;
        }
    }
    return left;
}

static int16_t eval_add_sub(void)
{
    int16_t left = eval_mul_div();
    if (basic.state == STATE_STOPPED) return 0;

    while (1) {
        tok_skip_spaces();
        uint8_t op = tok_peek();
        if (op == TOK_PLUS) {
            tok_read();
            int16_t right = eval_mul_div();
            if (basic.state == STATE_STOPPED) return 0;
            int32_t result = (int32_t)left + (int32_t)right;
            if (result > 32767 || result < -32768) {
                exec_error("OVERFLOW");
                return 0;
            }
            left = (int16_t)result;
        } else if (op == TOK_MINUS) {
            tok_read();
            int16_t right = eval_mul_div();
            if (basic.state == STATE_STOPPED) return 0;
            int32_t result = (int32_t)left - (int32_t)right;
            if (result > 32767 || result < -32768) {
                exec_error("OVERFLOW");
                return 0;
            }
            left = (int16_t)result;
        } else {
            break;
        }
    }
    return left;
}

static int16_t eval_mul_div(void)
{
    int16_t left = eval_unary();
    if (basic.state == STATE_STOPPED) return 0;

    while (1) {
        tok_skip_spaces();
        uint8_t op = tok_peek();
        if (op == TOK_STAR) {
            tok_read();
            int16_t right = eval_unary();
            if (basic.state == STATE_STOPPED) return 0;
            int32_t result = (int32_t)left * (int32_t)right;
            if (result > 32767 || result < -32768) {
                exec_error("OVERFLOW");
                return 0;
            }
            left = (int16_t)result;
        } else if (op == TOK_SLASH) {
            tok_read();
            int16_t right = eval_unary();
            if (basic.state == STATE_STOPPED) return 0;
            if (right == 0) {
                exec_error("DIVISION BY ZERO");
                return 0;
            }
            left = left / right;
        } else if (op == TOK_MOD) {
            tok_read();
            int16_t right = eval_unary();
            if (basic.state == STATE_STOPPED) return 0;
            if (right == 0) {
                exec_error("DIVISION BY ZERO");
                return 0;
            }
            left = left % right;
        } else {
            break;
        }
    }
    return left;
}

static int16_t eval_unary(void)
{
    tok_skip_spaces();
    if (tok_peek() == TOK_MINUS) {
        tok_read();
        int16_t val = eval_primary();
        if (basic.state == STATE_STOPPED) return 0;
        if (val == -32768) {
            exec_error("OVERFLOW");
            return 0;
        }
        return -val;
    }
    if (tok_peek() == TOK_PLUS) {
        tok_read();
    }
    return eval_primary();
}

static int16_t eval_primary(void)
{
    tok_skip_spaces();
    uint8_t tok = tok_peek();

    switch (tok) {
    case TOK_NUM:
        tok_read();
        return tok_read_int();

    case TOK_VAR: {
        tok_read();
        uint8_t idx = tok_read();
        return basic.vars[idx];
    }

    case TOK_ARRAYVAR: {
        tok_read();
        uint8_t idx = tok_read();

        tok_skip_spaces();
        if (tok_peek() != TOK_LPAREN) {
            exec_error("SYNTAX");
            return 0;
        }
        tok_read();

        int16_t index = eval_expression();
        if (basic.state == STATE_STOPPED) return 0;

        tok_skip_spaces();
        if (tok_peek() != TOK_RPAREN) {
            exec_error("SYNTAX");
            return 0;
        }
        tok_read();

        if (basic.array_size[idx] == 0) {
            exec_error("DIM");
            return 0;
        }
        if (index < 0 || index >= (int16_t)basic.array_size[idx]) {
            exec_error("RANGE");
            return 0;
        }

        uint16_t elem_addr = basic.array_base[idx] + (uint16_t)(index * 2);
        return (int16_t)(mem_read(elem_addr) | (mem_read(elem_addr + 1) << 8));
    }

    case TOK_LPAREN: {
        tok_read();
        int16_t val = eval_expression();
        if (basic.state == STATE_STOPPED) return 0;
        tok_skip_spaces();
        if (tok_peek() != TOK_RPAREN) {
            exec_error("SYNTAX");
            return 0;
        }
        tok_read();
        return val;
    }

    case TOK_ABS:
    case TOK_SGN:
    case TOK_RND:
    case TOK_PEEK:
    case TOK_LEN:
    case TOK_ASC:
    case TOK_VAL:
        tok_read();
        return eval_function(tok);

    default:
        exec_error("SYNTAX");
        return 0;
    }
}

static int16_t eval_function(token_t func)
{
    tok_skip_spaces();
    if (tok_peek() != TOK_LPAREN) {
        exec_error("SYNTAX");
        return 0;
    }
    tok_read();

    int16_t result = 0;

    switch (func) {
    case TOK_ABS: {
        int16_t val = eval_expression();
        if (basic.state == STATE_STOPPED) return 0;
        result = (val < 0) ? -val : val;
        break;
    }
    case TOK_SGN: {
        int16_t val = eval_expression();
        if (basic.state == STATE_STOPPED) return 0;
        result = (val > 0) ? 1 : (val < 0) ? -1 : 0;
        break;
    }
    case TOK_RND: {
        int16_t val = eval_expression();
        if (basic.state == STATE_STOPPED) return 0;
        if (val <= 0) {
            result = 0;
        } else {
            result = (int16_t)(rand() % val);
        }
        break;
    }
    case TOK_PEEK: {
        int16_t addr_val = eval_expression();
        if (basic.state == STATE_STOPPED) return 0;
        result = (int16_t)mem_read((uint16_t)addr_val);
        break;
    }
    case TOK_LEN: {
        /* LEN(A$) - need string expression */
        if (tok_peek() == TOK_STRVAR) {
            tok_read();
            uint8_t idx = tok_read();
            result = (int16_t)strlen(basic.str_vars[idx]);
        } else {
            /* Try evaluating as string expression */
            char str_buf[MAX_STRING_LEN + 1];
            eval_string_expr(str_buf, MAX_STRING_LEN);
            if (basic.state == STATE_STOPPED) return 0;
            result = (int16_t)strlen(str_buf);
        }
        break;
    }
    case TOK_ASC: {
        /* ASC(A$) - get ASCII value of first char */
        if (tok_peek() == TOK_STRVAR) {
            tok_read();
            uint8_t idx = tok_read();
            result = (basic.str_vars[idx][0] != '\0') ?
                     (int16_t)(unsigned char)basic.str_vars[idx][0] : 0;
        } else {
            char str_buf[MAX_STRING_LEN + 1];
            eval_string_expr(str_buf, MAX_STRING_LEN);
            if (basic.state == STATE_STOPPED) return 0;
            result = (str_buf[0] != '\0') ? (int16_t)(unsigned char)str_buf[0] : 0;
        }
        break;
    }
    case TOK_VAL: {
        /* VAL(A$) - convert string to number */
        char str_buf[MAX_STRING_LEN + 1];
        if (tok_peek() == TOK_STRVAR) {
            tok_read();
            uint8_t idx = tok_read();
            strncpy(str_buf, basic.str_vars[idx], MAX_STRING_LEN);
            str_buf[MAX_STRING_LEN] = '\0';
        } else {
            eval_string_expr(str_buf, MAX_STRING_LEN);
            if (basic.state == STATE_STOPPED) return 0;
        }
        result = (int16_t)atoi(str_buf);
        break;
    }
    default:
        exec_error("SYNTAX");
        return 0;
    }

    tok_skip_spaces();
    if (tok_peek() != TOK_RPAREN) {
        exec_error("SYNTAX");
        return 0;
    }
    tok_read();

    return result;
}

/* ======================================================================
 * String expression evaluator
 * ====================================================================== */

static void eval_string_expr(char *result, int max_len)
{
    result[0] = '\0';
    tok_skip_spaces();
    uint8_t tok = tok_peek();

    switch (tok) {
    case TOK_STRVAR: {
        tok_read();
        uint8_t idx = tok_read();
        strncpy(result, basic.str_vars[idx], max_len);
        result[max_len] = '\0';
        break;
    }
    case TOK_STRING: {
        tok_read();
        tok_read_string(result, max_len);
        break;
    }
    case TOK_CHR:
    case TOK_LEFT:
    case TOK_RIGHT:
    case TOK_MID:
    case TOK_STR:
        tok_read();
        eval_string_function(tok, result, max_len);
        break;
    default:
        exec_error("SYNTAX");
        break;
    }
}

static void eval_string_function(token_t func, char *result, int max_len)
{
    result[0] = '\0';

    tok_skip_spaces();
    if (tok_peek() != TOK_LPAREN) {
        exec_error("SYNTAX");
        return;
    }
    tok_read();

    switch (func) {
    case TOK_CHR: {
        /* CHR$(n) */
        int16_t val = eval_expression();
        if (basic.state == STATE_STOPPED) return;
        if (val >= 0 && val <= 127) {
            result[0] = (char)val;
            result[1] = '\0';
        }
        break;
    }
    case TOK_STR: {
        /* STR$(n) */
        int16_t val = eval_expression();
        if (basic.state == STATE_STOPPED) return;
        snprintf(result, max_len, "%d", val);
        break;
    }
    case TOK_LEFT: {
        /* LEFT$(A$, n) */
        char src[MAX_STRING_LEN + 1];
        eval_string_expr(src, MAX_STRING_LEN);
        if (basic.state == STATE_STOPPED) return;

        tok_skip_spaces();
        if (tok_peek() != TOK_COMMA) { exec_error("SYNTAX"); return; }
        tok_read();

        int16_t n = eval_expression();
        if (basic.state == STATE_STOPPED) return;

        if (n < 0) n = 0;
        int slen = (int)strlen(src);
        if (n > slen) n = (int16_t)slen;
        if (n > max_len) n = (int16_t)max_len;
        strncpy(result, src, (size_t)n);
        result[n] = '\0';
        break;
    }
    case TOK_RIGHT: {
        /* RIGHT$(A$, n) */
        char src[MAX_STRING_LEN + 1];
        eval_string_expr(src, MAX_STRING_LEN);
        if (basic.state == STATE_STOPPED) return;

        tok_skip_spaces();
        if (tok_peek() != TOK_COMMA) { exec_error("SYNTAX"); return; }
        tok_read();

        int16_t n = eval_expression();
        if (basic.state == STATE_STOPPED) return;

        int slen = (int)strlen(src);
        if (n < 0) n = 0;
        if (n > slen) n = (int16_t)slen;
        int start = slen - n;
        strncpy(result, &src[start], (size_t)n);
        result[n] = '\0';
        break;
    }
    case TOK_MID: {
        /* MID$(A$, start, len) */
        char src[MAX_STRING_LEN + 1];
        eval_string_expr(src, MAX_STRING_LEN);
        if (basic.state == STATE_STOPPED) return;

        tok_skip_spaces();
        if (tok_peek() != TOK_COMMA) { exec_error("SYNTAX"); return; }
        tok_read();

        int16_t start = eval_expression();
        if (basic.state == STATE_STOPPED) return;

        int16_t len = (int16_t)strlen(src);  /* default: rest of string */
        tok_skip_spaces();
        if (tok_peek() == TOK_COMMA) {
            tok_read();
            len = eval_expression();
            if (basic.state == STATE_STOPPED) return;
        }

        int slen = (int)strlen(src);
        /* MID$ uses 1-based indexing */
        start--;
        if (start < 0) start = 0;
        if (start >= slen) { result[0] = '\0'; break; }
        if (len < 0) len = 0;
        if (start + len > slen) len = (int16_t)(slen - start);
        if (len > max_len) len = (int16_t)max_len;
        strncpy(result, &src[start], (size_t)len);
        result[len] = '\0';
        break;
    }
    default:
        exec_error("SYNTAX");
        return;
    }

    tok_skip_spaces();
    if (tok_peek() != TOK_RPAREN) {
        exec_error("SYNTAX");
        return;
    }
    tok_read();
}

/* ======================================================================
 * Token stream helpers
 *
 * These work with basic.tok_buf[] and basic.exec_tok_pos for stored
 * program execution, or with the same buffer for immediate mode.
 * ====================================================================== */

static uint8_t tok_peek(void)
{
    if (basic.exec_tok_pos >= (int)sizeof(basic.tok_buf)) return TOK_END;
    return basic.tok_buf[basic.exec_tok_pos];
}

static uint8_t tok_read(void)
{
    if (basic.exec_tok_pos >= (int)sizeof(basic.tok_buf)) return TOK_END;
    return basic.tok_buf[basic.exec_tok_pos++];
}

static int16_t tok_read_int(void)
{
    uint8_t lo = tok_read();
    uint8_t hi = tok_read();
    return (int16_t)(lo | (hi << 8));
}

static void tok_read_string(char *buf, int max_len)
{
    uint8_t len = tok_read();
    int copy_len = (len < max_len) ? len : max_len;
    for (int i = 0; i < copy_len; i++) {
        buf[i] = (char)tok_read();
    }
    buf[copy_len] = '\0';
    /* Skip any remaining chars if string was truncated */
    for (int i = copy_len; i < len; i++) {
        tok_read();
    }
}

static bool tok_at_end(void)
{
    if (basic.exec_tok_pos >= (int)sizeof(basic.tok_buf)) return true;
    return basic.tok_buf[basic.exec_tok_pos] == TOK_END;
}

static void tok_skip_spaces(void)
{
    /* Tokens don't have spaces, but handle edge cases */
    /* (spaces are eaten during tokenization) */
}

/* ======================================================================
 * Utility functions
 * ====================================================================== */

static void clear_variables(void)
{
    memset(basic.vars, 0, sizeof(basic.vars));
    memset(basic.str_vars, 0, sizeof(basic.str_vars));
    memset(basic.array_base, 0, sizeof(basic.array_base));
    memset(basic.array_size, 0, sizeof(basic.array_size));
    basic.array_top = VAR_TOP;
    basic.gosub_sp = 0;
    basic.for_sp = 0;
}

static bool is_line_number(const char *s, uint16_t *linenum)
{
    if (!s || !(*s >= '0' && *s <= '9')) return false;

    int val = 0;
    const char *p = s;
    while (*p >= '0' && *p <= '9') {
        val = val * 10 + (*p - '0');
        if (val > MAX_LINE_NUM) return false;
        p++;
    }

    *linenum = (uint16_t)val;
    return true;
}

static void upper_string(char *s)
{
    /* Convert to uppercase, but preserve strings in quotes */
    bool in_quotes = false;
    while (*s) {
        if (*s == '"') {
            in_quotes = !in_quotes;
        } else if (!in_quotes && *s >= 'a' && *s <= 'z') {
            *s = *s - 'a' + 'A';
        }
        s++;
    }
}
