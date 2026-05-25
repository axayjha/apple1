/*
 * assembler.c - Built-in 6502 mini-assembler for Apple 1 emulator
 *
 * A simple two-pass assembler that can assemble 6502 programs directly
 * within the emulator, outputting machine code into the emulator's memory.
 *
 * I/O is performed through the PIA: output via pia_write to $D012,
 * input via the character buffer (same model as BASIC).
 */

#include "assembler.h"
#include "memory.h"
#include "pia.h"

#include <string.h>
#include <ctype.h>

/* ======================================================================
 * Constants and limits
 * ====================================================================== */

#define MAX_LINE_LEN     80
#define MAX_LABELS       64
#define MAX_LABEL_LEN    8
#define MAX_SOURCE_LINES 256
#define INPUT_BUF_SIZE   128
#define DEFAULT_ORG      0x0300

/* ======================================================================
 * Addressing modes
 * ====================================================================== */

typedef enum {
    AM_IMPLIED = 0,
    AM_ACCUMULATOR,
    AM_IMMEDIATE,
    AM_ZERO_PAGE,
    AM_ZERO_PAGE_X,
    AM_ZERO_PAGE_Y,
    AM_ABSOLUTE,
    AM_ABSOLUTE_X,
    AM_ABSOLUTE_Y,
    AM_INDIRECT,
    AM_INDIRECT_X,
    AM_INDIRECT_Y,
    AM_RELATIVE,
    AM_COUNT
} addr_mode_t;

/* ======================================================================
 * Opcode table
 * ====================================================================== */

typedef struct {
    const char *mnemonic;
    uint8_t opcodes[13]; /* one per addressing mode, 0xFF if unsupported */
} opcode_entry_t;

/*
 * Opcode table for all 56 official 6502 mnemonics.
 * Index into opcodes[] corresponds to addr_mode_t enum.
 * 0xFF means the addressing mode is not valid for that instruction.
 */
static const opcode_entry_t opcode_table[] = {
    /* Mnemonic   IMP   ACC   IMM   ZP    ZPX   ZPY   ABS   ABX   ABY   IND   IXI   IYI   REL */
    {"ADC",     {0xFF, 0xFF, 0x69, 0x65, 0x75, 0xFF, 0x6D, 0x7D, 0x79, 0xFF, 0x61, 0x71, 0xFF}},
    {"AND",     {0xFF, 0xFF, 0x29, 0x25, 0x35, 0xFF, 0x2D, 0x3D, 0x39, 0xFF, 0x21, 0x31, 0xFF}},
    {"ASL",     {0xFF, 0x0A, 0xFF, 0x06, 0x16, 0xFF, 0x0E, 0x1E, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"BCC",     {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x90}},
    {"BCS",     {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xB0}},
    {"BEQ",     {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0}},
    {"BIT",     {0xFF, 0xFF, 0xFF, 0x24, 0xFF, 0xFF, 0x2C, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"BMI",     {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x30}},
    {"BNE",     {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xD0}},
    {"BPL",     {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x10}},
    {"BRK",     {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"BVC",     {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x50}},
    {"BVS",     {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x70}},
    {"CLC",     {0x18, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"CLD",     {0xD8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"CLI",     {0x58, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"CLV",     {0xB8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"CMP",     {0xFF, 0xFF, 0xC9, 0xC5, 0xD5, 0xFF, 0xCD, 0xDD, 0xD9, 0xFF, 0xC1, 0xD1, 0xFF}},
    {"CPX",     {0xFF, 0xFF, 0xE0, 0xE4, 0xFF, 0xFF, 0xEC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"CPY",     {0xFF, 0xFF, 0xC0, 0xC4, 0xFF, 0xFF, 0xCC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"DEC",     {0xFF, 0xFF, 0xFF, 0xC6, 0xD6, 0xFF, 0xCE, 0xDE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"DEX",     {0xCA, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"DEY",     {0x88, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"EOR",     {0xFF, 0xFF, 0x49, 0x45, 0x55, 0xFF, 0x4D, 0x5D, 0x59, 0xFF, 0x41, 0x51, 0xFF}},
    {"INC",     {0xFF, 0xFF, 0xFF, 0xE6, 0xF6, 0xFF, 0xEE, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"INX",     {0xE8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"INY",     {0xC8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"JMP",     {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x4C, 0xFF, 0xFF, 0x6C, 0xFF, 0xFF, 0xFF}},
    {"JSR",     {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x20, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"LDA",     {0xFF, 0xFF, 0xA9, 0xA5, 0xB5, 0xFF, 0xAD, 0xBD, 0xB9, 0xFF, 0xA1, 0xB1, 0xFF}},
    {"LDX",     {0xFF, 0xFF, 0xA2, 0xA6, 0xFF, 0xB6, 0xAE, 0xFF, 0xBE, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"LDY",     {0xFF, 0xFF, 0xA0, 0xA4, 0xB4, 0xFF, 0xAC, 0xBC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"LSR",     {0xFF, 0x4A, 0xFF, 0x46, 0x56, 0xFF, 0x4E, 0x5E, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"NOP",     {0xEA, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"ORA",     {0xFF, 0xFF, 0x09, 0x05, 0x15, 0xFF, 0x0D, 0x1D, 0x19, 0xFF, 0x01, 0x11, 0xFF}},
    {"PHA",     {0x48, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"PHP",     {0x08, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"PLA",     {0x68, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"PLP",     {0x28, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"ROL",     {0xFF, 0x2A, 0xFF, 0x26, 0x36, 0xFF, 0x2E, 0x3E, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"ROR",     {0xFF, 0x6A, 0xFF, 0x66, 0x76, 0xFF, 0x6E, 0x7E, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"RTI",     {0x40, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"RTS",     {0x60, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"SBC",     {0xFF, 0xFF, 0xE9, 0xE5, 0xF5, 0xFF, 0xED, 0xFD, 0xF9, 0xFF, 0xE1, 0xF1, 0xFF}},
    {"SEC",     {0x38, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"SED",     {0xF8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"SEI",     {0x78, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"STA",     {0xFF, 0xFF, 0xFF, 0x85, 0x95, 0xFF, 0x8D, 0x9D, 0x99, 0xFF, 0x81, 0x91, 0xFF}},
    {"STX",     {0xFF, 0xFF, 0xFF, 0x86, 0xFF, 0x96, 0x8E, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"STY",     {0xFF, 0xFF, 0xFF, 0x84, 0x94, 0xFF, 0x8C, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"TAX",     {0xAA, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"TAY",     {0xA8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"TSX",     {0xBA, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"TXA",     {0x8A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"TXS",     {0x9A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    {"TYA",     {0x98, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
};

#define NUM_OPCODES (sizeof(opcode_table) / sizeof(opcode_table[0]))

/* ======================================================================
 * Label table
 * ====================================================================== */

typedef struct {
    char name[MAX_LABEL_LEN + 1];
    uint16_t addr;
    bool defined;
} label_entry_t;

/* ======================================================================
 * Source line storage (for multi-line mode)
 * ====================================================================== */

typedef struct {
    char text[MAX_LINE_LEN + 1];
} source_line_t;

/* ======================================================================
 * Assembler state
 * ====================================================================== */

typedef enum {
    ASM_STATE_IDLE,
    ASM_STATE_PROMPT,
    ASM_STATE_INPUT,
    ASM_STATE_ASSEMBLE,
    ASM_STATE_OUTPUT,
} asm_state_t;

typedef enum {
    ASM_MODE_SINGLE,
    ASM_MODE_MULTI,
} asm_mode_t;

static struct {
    bool active;
    asm_state_t state;
    asm_mode_t mode;

    /* Current assembly address */
    uint16_t origin;
    uint16_t pc;

    /* Input buffer */
    uint8_t input_buf[INPUT_BUF_SIZE];
    int input_head;
    int input_tail;

    /* Current line being edited */
    char line_buf[MAX_LINE_LEN + 1];
    int line_pos;

    /* Label table */
    label_entry_t labels[MAX_LABELS];
    int num_labels;

    /* Source buffer for multi-line mode */
    source_line_t source[MAX_SOURCE_LINES];
    int num_source_lines;

    /* Output message buffer */
    const char *output_msg;
    int output_pos;

    /* Pass tracking */
    int current_pass; /* 1 or 2 */

    /* Error state */
    bool had_error;
    int error_line;
} asm_ctx;

/* ======================================================================
 * I/O helpers
 * ====================================================================== */

static void asm_putchar(uint8_t ch)
{
    pia_write(PIA_DSP, ch | 0x80);
}

static void asm_print(const char *s)
{
    while (*s) {
        if (*s == '\n') {
            asm_putchar('\r');
        } else {
            asm_putchar((uint8_t)*s);
        }
        s++;
    }
}

static void asm_newline(void)
{
    asm_putchar('\r');
}

static void asm_print_hex4(uint8_t nib)
{
    nib &= 0x0F;
    asm_putchar(nib < 10 ? '0' + nib : 'A' + nib - 10);
}

static void asm_print_hex8(uint8_t val)
{
    asm_print_hex4(val >> 4);
    asm_print_hex4(val);
}

static void asm_print_hex16(uint16_t val)
{
    asm_print_hex8((uint8_t)(val >> 8));
    asm_print_hex8((uint8_t)(val & 0xFF));
}

/* ======================================================================
 * Input buffer management
 * ====================================================================== */

static bool input_available(void)
{
    return asm_ctx.input_head != asm_ctx.input_tail;
}

static uint8_t input_get(void)
{
    uint8_t ch = asm_ctx.input_buf[asm_ctx.input_tail];
    asm_ctx.input_tail = (asm_ctx.input_tail + 1) % INPUT_BUF_SIZE;
    return ch;
}

/* ======================================================================
 * String/parsing utilities
 * ====================================================================== */

static void str_toupper(char *s)
{
    while (*s) {
        *s = (char)toupper((unsigned char)*s);
        s++;
    }
}

static void skip_spaces(const char **p)
{
    while (**p == ' ' || **p == '\t') {
        (*p)++;
    }
}

/* Parse a numeric value from the string.
 * Supports: $xx/$xxxx (hex), %nnnnnnnn (binary), 'c' (char), decimal.
 * Returns true on success, stores value in *out. */
static bool parse_number(const char **p, uint16_t *out)
{
    const char *s = *p;

    if (*s == '$') {
        /* Hexadecimal */
        s++;
        if (!isxdigit((unsigned char)*s)) return false;
        uint16_t val = 0;
        while (isxdigit((unsigned char)*s)) {
            val <<= 4;
            if (*s >= '0' && *s <= '9') val |= (uint16_t)(*s - '0');
            else if (*s >= 'A' && *s <= 'F') val |= (uint16_t)(*s - 'A' + 10);
            else if (*s >= 'a' && *s <= 'f') val |= (uint16_t)(*s - 'a' + 10);
            s++;
        }
        *out = val;
        *p = s;
        return true;
    }

    if (*s == '%') {
        /* Binary */
        s++;
        if (*s != '0' && *s != '1') return false;
        uint16_t val = 0;
        while (*s == '0' || *s == '1') {
            val = (uint16_t)((val << 1) | (*s - '0'));
            s++;
        }
        *out = val;
        *p = s;
        return true;
    }

    if (*s == '\'') {
        /* Character literal */
        s++;
        if (*s == '\0') return false;
        uint8_t ch = (uint8_t)*s;
        s++;
        if (*s == '\'') s++; /* skip closing quote if present */
        *out = ch;
        *p = s;
        return true;
    }

    if (isdigit((unsigned char)*s)) {
        /* Decimal */
        uint16_t val = 0;
        while (isdigit((unsigned char)*s)) {
            val = (uint16_t)(val * 10 + (*s - '0'));
            s++;
        }
        *out = val;
        *p = s;
        return true;
    }

    return false;
}

/* Check if a string is a branch mnemonic */
static bool is_branch(const char *mnemonic)
{
    return (strcmp(mnemonic, "BCC") == 0 ||
            strcmp(mnemonic, "BCS") == 0 ||
            strcmp(mnemonic, "BEQ") == 0 ||
            strcmp(mnemonic, "BMI") == 0 ||
            strcmp(mnemonic, "BNE") == 0 ||
            strcmp(mnemonic, "BPL") == 0 ||
            strcmp(mnemonic, "BVC") == 0 ||
            strcmp(mnemonic, "BVS") == 0);
}

/* Check if a mnemonic supports accumulator mode */
static bool is_shift(const char *mnemonic)
{
    return (strcmp(mnemonic, "ASL") == 0 ||
            strcmp(mnemonic, "LSR") == 0 ||
            strcmp(mnemonic, "ROL") == 0 ||
            strcmp(mnemonic, "ROR") == 0);
}

/* ======================================================================
 * Label management
 * ====================================================================== */

static int find_label(const char *name)
{
    for (int i = 0; i < asm_ctx.num_labels; i++) {
        if (strcmp(asm_ctx.labels[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int add_label(const char *name, uint16_t addr)
{
    int idx = find_label(name);
    if (idx >= 0) {
        asm_ctx.labels[idx].addr = addr;
        asm_ctx.labels[idx].defined = true;
        return idx;
    }
    if (asm_ctx.num_labels >= MAX_LABELS) {
        return -1; /* table full */
    }
    idx = asm_ctx.num_labels++;
    strncpy(asm_ctx.labels[idx].name, name, MAX_LABEL_LEN);
    asm_ctx.labels[idx].name[MAX_LABEL_LEN] = '\0';
    asm_ctx.labels[idx].addr = addr;
    asm_ctx.labels[idx].defined = true;
    return idx;
}

/* Try to resolve a label to its address. Returns true on success. */
static bool resolve_label(const char *name, uint16_t *addr)
{
    int idx = find_label(name);
    if (idx < 0 || !asm_ctx.labels[idx].defined) {
        return false;
    }
    *addr = asm_ctx.labels[idx].addr;
    return true;
}

/* Parse a label-like identifier from the string */
static bool parse_identifier(const char **p, char *buf, int max_len)
{
    const char *s = *p;
    if (!isalpha((unsigned char)*s) && *s != '_') return false;
    int i = 0;
    while ((isalnum((unsigned char)*s) || *s == '_') && i < max_len) {
        buf[i++] = (char)toupper((unsigned char)*s);
        s++;
    }
    buf[i] = '\0';
    *p = s;
    return i > 0;
}

/* ======================================================================
 * Opcode lookup
 * ====================================================================== */

static const opcode_entry_t *find_opcode(const char *mnemonic)
{
    for (int i = 0; i < (int)NUM_OPCODES; i++) {
        if (strcmp(opcode_table[i].mnemonic, mnemonic) == 0) {
            return &opcode_table[i];
        }
    }
    return NULL;
}

/* ======================================================================
 * Operand parsing and addressing mode detection
 * ====================================================================== */

typedef struct {
    addr_mode_t mode;
    uint16_t value;
    bool has_value;
} operand_t;

/* Parse operand and determine addressing mode.
 * Returns true on success. On failure, sets error. */
static bool parse_operand(const char *operand_str, const char *mnemonic,
                          operand_t *result, uint16_t current_pc)
{
    const char *p = operand_str;
    skip_spaces(&p);

    result->has_value = false;
    result->value = 0;

    /* No operand -> implied or accumulator */
    if (*p == '\0') {
        if (is_shift(mnemonic)) {
            result->mode = AM_ACCUMULATOR;
        } else {
            result->mode = AM_IMPLIED;
        }
        return true;
    }

    /* Accumulator: "A" */
    if ((*p == 'A' || *p == 'a') && (p[1] == '\0' || p[1] == ' ')) {
        if (is_shift(mnemonic)) {
            result->mode = AM_ACCUMULATOR;
            return true;
        }
    }

    /* Immediate: #$nn or #nn or #%nn or #'c' */
    if (*p == '#') {
        p++;
        uint16_t val;
        if (!parse_number(&p, &val)) {
            /* Try label for immediate */
            char lbl[MAX_LABEL_LEN + 1];
            if (parse_identifier(&p, lbl, MAX_LABEL_LEN)) {
                if (!resolve_label(lbl, &val)) {
                    if (asm_ctx.current_pass == 2) return false;
                    val = 0; /* placeholder for pass 1 */
                }
            } else {
                return false;
            }
        }
        result->mode = AM_IMMEDIATE;
        result->value = val & 0xFF;
        result->has_value = true;
        return true;
    }

    /* Indirect modes: start with ( */
    if (*p == '(') {
        p++;
        uint16_t val;
        bool got_val = false;

        /* Try number */
        if (*p == '$' || isdigit((unsigned char)*p) || *p == '%') {
            if (!parse_number(&p, &val)) return false;
            got_val = true;
        } else {
            /* Try label */
            char lbl[MAX_LABEL_LEN + 1];
            if (parse_identifier(&p, lbl, MAX_LABEL_LEN)) {
                if (!resolve_label(lbl, &val)) {
                    if (asm_ctx.current_pass == 2) return false;
                    val = 0;
                }
                got_val = true;
            }
        }
        if (!got_val) return false;

        skip_spaces(&p);

        /* ($nn,X) - Indexed Indirect */
        if (*p == ',') {
            p++;
            skip_spaces(&p);
            if (*p == 'X' || *p == 'x') {
                p++;
                skip_spaces(&p);
                if (*p != ')') return false;
                result->mode = AM_INDIRECT_X;
                result->value = val & 0xFF;
                result->has_value = true;
                return true;
            }
            return false;
        }

        /* ($nn) or ($nnnn) followed by ),Y */
        if (*p == ')') {
            p++;
            skip_spaces(&p);
            if (*p == ',') {
                p++;
                skip_spaces(&p);
                if (*p == 'Y' || *p == 'y') {
                    /* ($nn),Y - Indirect Indexed */
                    result->mode = AM_INDIRECT_Y;
                    result->value = val & 0xFF;
                    result->has_value = true;
                    return true;
                }
                return false;
            }
            /* ($nnnn) - Indirect (JMP only) */
            result->mode = AM_INDIRECT;
            result->value = val;
            result->has_value = true;
            return true;
        }
        return false;
    }

    /* Direct value or label: could be ZP, ABS, ZPX, ZPY, ABX, ABY, or relative */
    uint16_t val;
    bool got_val = false;
    bool is_label_ref = false;

    if (*p == '$' || isdigit((unsigned char)*p) || *p == '%' || *p == '\'') {
        if (!parse_number(&p, &val)) return false;
        got_val = true;
    } else if (isalpha((unsigned char)*p) || *p == '_') {
        char lbl[MAX_LABEL_LEN + 1];
        const char *saved = p;
        if (parse_identifier(&p, lbl, MAX_LABEL_LEN)) {
            /* Make sure it's not a mnemonic being misread */
            if (!resolve_label(lbl, &val)) {
                if (asm_ctx.current_pass == 2) return false;
                val = 0;
            }
            got_val = true;
            is_label_ref = true;
        } else {
            p = saved;
            return false;
        }
    }

    if (!got_val) return false;

    skip_spaces(&p);

    /* Check for ,X or ,Y */
    if (*p == ',') {
        p++;
        skip_spaces(&p);
        if (*p == 'X' || *p == 'x') {
            if (val <= 0xFF) {
                result->mode = AM_ZERO_PAGE_X;
                result->value = val & 0xFF;
            } else {
                result->mode = AM_ABSOLUTE_X;
                result->value = val;
            }
            result->has_value = true;
            return true;
        }
        if (*p == 'Y' || *p == 'y') {
            if (val <= 0xFF) {
                result->mode = AM_ZERO_PAGE_Y;
                result->value = val & 0xFF;
            } else {
                result->mode = AM_ABSOLUTE_Y;
                result->value = val;
            }
            result->has_value = true;
            return true;
        }
        return false;
    }

    /* Branch instructions use relative addressing */
    if (is_branch(mnemonic)) {
        result->mode = AM_RELATIVE;
        result->value = val;
        result->has_value = true;
        return true;
    }

    /* Zero page vs absolute - determine by value size */
    if (val <= 0xFF && !is_label_ref) {
        /* Could be zero page - check if instruction supports it */
        const opcode_entry_t *op = find_opcode(mnemonic);
        if (op && op->opcodes[AM_ZERO_PAGE] != 0xFF) {
            result->mode = AM_ZERO_PAGE;
            result->value = val & 0xFF;
        } else {
            result->mode = AM_ABSOLUTE;
            result->value = val;
        }
    } else {
        result->mode = AM_ABSOLUTE;
        result->value = val;
    }

    result->has_value = true;
    return true;

    (void)current_pc;
}

/* ======================================================================
 * Instruction size calculation
 * ====================================================================== */

static int instruction_size(addr_mode_t mode)
{
    switch (mode) {
        case AM_IMPLIED:
        case AM_ACCUMULATOR:
            return 1;
        case AM_IMMEDIATE:
        case AM_ZERO_PAGE:
        case AM_ZERO_PAGE_X:
        case AM_ZERO_PAGE_Y:
        case AM_INDIRECT_X:
        case AM_INDIRECT_Y:
        case AM_RELATIVE:
            return 2;
        case AM_ABSOLUTE:
        case AM_ABSOLUTE_X:
        case AM_ABSOLUTE_Y:
        case AM_INDIRECT:
            return 3;
        default:
            return 1;
    }
}

/* ======================================================================
 * Assemble a single line
 * Returns number of bytes emitted (0 on error, or for directives that
 * only set state).
 * If emit is true, writes bytes to memory. Otherwise just calculates size.
 * ====================================================================== */

typedef enum {
    ASM_OK = 0,
    ASM_ERR_SYNTAX,
    ASM_ERR_ILLEGAL_OPCODE,
    ASM_ERR_INVALID_MODE,
    ASM_ERR_BRANCH_TOO_FAR,
    ASM_ERR_LABEL_FULL,
    ASM_ERR_UNDEF_LABEL,
} asm_error_t;

static asm_error_t assemble_line(const char *line, uint16_t *pc, bool emit)
{
    const char *p = line;
    char token[MAX_LABEL_LEN + 1];

    skip_spaces(&p);

    /* Empty line */
    if (*p == '\0' || *p == ';') return ASM_OK;

    /* Check for label at start of line (no leading space) */
    if (line[0] != ' ' && line[0] != '\t' && isalpha((unsigned char)*p)) {
        const char *saved = p;
        if (parse_identifier(&p, token, MAX_LABEL_LEN)) {
            /* It's a label if followed by space, colon, or end */
            if (*p == ':') p++;
            skip_spaces(&p);

            /* Define the label at current PC */
            if (asm_ctx.current_pass == 1) {
                if (add_label(token, *pc) < 0) {
                    return ASM_ERR_LABEL_FULL;
                }
            }

            /* If nothing else on line, it's just a label definition */
            if (*p == '\0' || *p == ';') return ASM_OK;
        } else {
            p = saved;
        }
    }

    skip_spaces(&p);
    if (*p == '\0' || *p == ';') return ASM_OK;

    /* Parse mnemonic or directive */
    bool is_directive = (*p == '.');
    if (is_directive) p++;

    const char *mnem_start = p;
    if (!parse_identifier(&p, token, MAX_LABEL_LEN)) {
        return ASM_ERR_SYNTAX;
    }

    /* Handle directives */
    if (is_directive || strcmp(token, "ORG") == 0) {
        if (strcmp(token, "ORG") == 0) {
            skip_spaces(&p);
            uint16_t addr;
            if (!parse_number(&p, &addr)) return ASM_ERR_SYNTAX;
            *pc = addr;
            asm_ctx.origin = addr;
            return ASM_OK;
        }
        if (strcmp(token, "BYTE") == 0) {
            skip_spaces(&p);
            while (*p && *p != ';') {
                uint16_t val;
                if (!parse_number(&p, &val)) return ASM_ERR_SYNTAX;
                if (emit) mem_write(*pc, (uint8_t)(val & 0xFF));
                (*pc)++;
                skip_spaces(&p);
                if (*p == ',') { p++; skip_spaces(&p); }
            }
            return ASM_OK;
        }
        if (strcmp(token, "WORD") == 0) {
            skip_spaces(&p);
            while (*p && *p != ';') {
                uint16_t val;
                if (*p == '$' || isdigit((unsigned char)*p) || *p == '%') {
                    if (!parse_number(&p, &val)) return ASM_ERR_SYNTAX;
                } else {
                    char lbl[MAX_LABEL_LEN + 1];
                    if (!parse_identifier(&p, lbl, MAX_LABEL_LEN)) return ASM_ERR_SYNTAX;
                    if (!resolve_label(lbl, &val)) {
                        if (asm_ctx.current_pass == 2) return ASM_ERR_UNDEF_LABEL;
                        val = 0;
                    }
                }
                if (emit) {
                    mem_write(*pc, (uint8_t)(val & 0xFF));
                    mem_write((uint16_t)(*pc + 1), (uint8_t)(val >> 8));
                }
                *pc += 2;
                skip_spaces(&p);
                if (*p == ',') { p++; skip_spaces(&p); }
            }
            return ASM_OK;
        }
        if (strcmp(token, "TEXT") == 0) {
            skip_spaces(&p);
            if (*p != '"') return ASM_ERR_SYNTAX;
            p++;
            while (*p && *p != '"') {
                if (emit) mem_write(*pc, (uint8_t)*p);
                (*pc)++;
                p++;
            }
            return ASM_OK;
        }
        if (strcmp(token, "END") == 0) {
            return ASM_OK; /* handled at higher level */
        }
        return ASM_ERR_SYNTAX;
    }

    /* Look up mnemonic */
    const opcode_entry_t *op = find_opcode(token);
    if (!op) {
        /* Maybe it was a label-like token on an indented line that
         * we misidentified. Check if it looks like a directive without dot. */
        if (strcmp(token, "ASM") == 0 || strcmp(token, "END") == 0) {
            return ASM_OK;
        }
        return ASM_ERR_ILLEGAL_OPCODE;
    }

    /* Parse operand */
    skip_spaces(&p);
    char operand_buf[MAX_LINE_LEN];
    int oi = 0;
    while (*p && *p != ';' && oi < MAX_LINE_LEN - 1) {
        operand_buf[oi++] = *p++;
    }
    operand_buf[oi] = '\0';
    /* Trim trailing spaces */
    while (oi > 0 && (operand_buf[oi - 1] == ' ' || operand_buf[oi - 1] == '\t')) {
        operand_buf[--oi] = '\0';
    }
    str_toupper(operand_buf);

    operand_t operand;
    if (!parse_operand(operand_buf, token, &operand, *pc)) {
        if (asm_ctx.current_pass == 2) return ASM_ERR_UNDEF_LABEL;
        /* Pass 1: assume absolute for forward references */
        operand.mode = is_branch(token) ? AM_RELATIVE : AM_ABSOLUTE;
        operand.value = 0;
        operand.has_value = true;
    }

    /* Validate addressing mode for this instruction */
    uint8_t opcode = op->opcodes[operand.mode];
    if (opcode == 0xFF) {
        /* Try upgrading zero page to absolute if ZP mode not supported */
        if (operand.mode == AM_ZERO_PAGE && op->opcodes[AM_ABSOLUTE] != 0xFF) {
            operand.mode = AM_ABSOLUTE;
            opcode = op->opcodes[AM_ABSOLUTE];
        } else if (operand.mode == AM_ZERO_PAGE_X && op->opcodes[AM_ABSOLUTE_X] != 0xFF) {
            operand.mode = AM_ABSOLUTE_X;
            opcode = op->opcodes[AM_ABSOLUTE_X];
        } else if (operand.mode == AM_ZERO_PAGE_Y && op->opcodes[AM_ABSOLUTE_Y] != 0xFF) {
            operand.mode = AM_ABSOLUTE_Y;
            opcode = op->opcodes[AM_ABSOLUTE_Y];
        } else {
            return ASM_ERR_INVALID_MODE;
        }
    }

    /* Calculate instruction size and emit bytes */
    int size = instruction_size(operand.mode);

    if (emit) {
        mem_write(*pc, opcode);
        if (size >= 2) {
            if (operand.mode == AM_RELATIVE) {
                /* Calculate relative offset */
                int16_t offset = (int16_t)(operand.value - (*pc + 2));
                if (offset < -128 || offset > 127) {
                    return ASM_ERR_BRANCH_TOO_FAR;
                }
                mem_write((uint16_t)(*pc + 1), (uint8_t)(offset & 0xFF));
            } else {
                mem_write((uint16_t)(*pc + 1), (uint8_t)(operand.value & 0xFF));
            }
        }
        if (size == 3) {
            mem_write((uint16_t)(*pc + 2), (uint8_t)(operand.value >> 8));
        }
    }

    *pc += (uint16_t)size;
    return ASM_OK;

    (void)mnem_start;
}

/* ======================================================================
 * Multi-line assembly (two-pass)
 * ====================================================================== */

static void do_multiline_assemble(void)
{
    asm_ctx.had_error = false;
    asm_ctx.error_line = 0;

    /* Pass 1: collect labels and calculate sizes */
    asm_ctx.current_pass = 1;
    asm_ctx.num_labels = 0;
    uint16_t pc = asm_ctx.origin;

    for (int i = 0; i < asm_ctx.num_source_lines; i++) {
        asm_error_t err = assemble_line(asm_ctx.source[i].text, &pc, false);
        if (err == ASM_ERR_LABEL_FULL) {
            asm_ctx.had_error = true;
            asm_ctx.error_line = i + 1;
            asm_print("?LABEL TABLE FULL");
            asm_newline();
            return;
        }
        /* Other errors in pass 1 are deferred to pass 2 */
    }

    /* Pass 2: emit code */
    asm_ctx.current_pass = 2;
    pc = asm_ctx.origin;

    for (int i = 0; i < asm_ctx.num_source_lines; i++) {
        asm_error_t err = assemble_line(asm_ctx.source[i].text, &pc, true);
        if (err != ASM_OK) {
            asm_ctx.had_error = true;
            asm_ctx.error_line = i + 1;
            switch (err) {
                case ASM_ERR_SYNTAX:
                    asm_print("?SYNTAX ERROR");
                    break;
                case ASM_ERR_ILLEGAL_OPCODE:
                    asm_print("?ILLEGAL OPCODE");
                    break;
                case ASM_ERR_INVALID_MODE:
                    asm_print("?INVALID MODE");
                    break;
                case ASM_ERR_BRANCH_TOO_FAR:
                    asm_print("?BRANCH TOO FAR");
                    break;
                case ASM_ERR_LABEL_FULL:
                    asm_print("?LABEL TABLE FULL");
                    break;
                case ASM_ERR_UNDEF_LABEL:
                    asm_print("?UNDEFINED LABEL");
                    break;
                default:
                    asm_print("?ERROR");
                    break;
            }
            asm_print(" IN LINE ");
            /* Print line number as decimal */
            int ln = i + 1;
            char numbuf[8];
            int ni = 0;
            if (ln >= 100) numbuf[ni++] = (char)('0' + ln / 100);
            if (ln >= 10)  numbuf[ni++] = (char)('0' + (ln / 10) % 10);
            numbuf[ni++] = (char)('0' + ln % 10);
            numbuf[ni] = '\0';
            asm_print(numbuf);
            asm_newline();
            return;
        }
    }

    /* Success */
    asm_print("OK ");
    asm_print_hex16(asm_ctx.origin);
    asm_putchar('-');
    asm_print_hex16((uint16_t)(pc - 1));
    asm_print(" (");
    uint16_t bytes = (uint16_t)(pc - asm_ctx.origin);
    char numbuf[8];
    int ni = 0;
    if (bytes >= 100) numbuf[ni++] = (char)('0' + bytes / 100);
    if (bytes >= 10)  numbuf[ni++] = (char)('0' + (bytes / 10) % 10);
    numbuf[ni++] = (char)('0' + bytes % 10);
    numbuf[ni] = '\0';
    asm_print(numbuf);
    asm_print(" BYTES)");
    asm_newline();
}

/* ======================================================================
 * Single-line assembly
 * ====================================================================== */

static void do_single_line(const char *line)
{
    const char *p = line;
    skip_spaces(&p);

    /* Check if line starts with an address */
    if (*p == '$' || isxdigit((unsigned char)*p)) {
        uint16_t addr;
        if (*p == '$') {
            p++;
        }
        if (!isxdigit((unsigned char)*p)) {
            asm_print("?SYNTAX ERROR");
            asm_newline();
            return;
        }
        addr = 0;
        while (isxdigit((unsigned char)*p)) {
            addr <<= 4;
            if (*p >= '0' && *p <= '9') addr |= (uint16_t)(*p - '0');
            else if (*p >= 'A' && *p <= 'F') addr |= (uint16_t)(*p - 'A' + 10);
            else if (*p >= 'a' && *p <= 'f') addr |= (uint16_t)(*p - 'a' + 10);
            p++;
        }

        skip_spaces(&p);

        /* Just an address with no instruction -> set origin */
        if (*p == '\0') {
            asm_ctx.pc = addr;
            return;
        }

        asm_ctx.pc = addr;
    }

    /* Check for multi-line mode entry */
    const char *check = p;
    skip_spaces(&check);
    if (*check == '.') check++;
    char cmd[MAX_LABEL_LEN + 1];
    if (parse_identifier(&check, cmd, MAX_LABEL_LEN)) {
        if (strcmp(cmd, "ORG") == 0) {
            skip_spaces(&check);
            uint16_t addr;
            if (parse_number(&check, &addr)) {
                asm_ctx.origin = addr;
                asm_ctx.pc = addr;
                asm_ctx.mode = ASM_MODE_MULTI;
                asm_ctx.num_source_lines = 0;
                asm_ctx.num_labels = 0;
                asm_print("MULTI-LINE MODE (USE .END TO ASSEMBLE)");
                asm_newline();
                return;
            }
        }
        if (strcmp(cmd, "ASM") == 0) {
            asm_ctx.mode = ASM_MODE_MULTI;
            asm_ctx.num_source_lines = 0;
            asm_ctx.num_labels = 0;
            asm_print("MULTI-LINE MODE (USE .END TO ASSEMBLE)");
            asm_newline();
            return;
        }
    }

    /* Single-line assembly: assemble at current PC */
    asm_ctx.current_pass = 1;
    asm_ctx.num_labels = 0;
    uint16_t temp_pc = asm_ctx.pc;
    asm_error_t err = assemble_line(p, &temp_pc, false);

    if (err != ASM_OK && err != ASM_ERR_UNDEF_LABEL) {
        goto report_error;
    }

    /* Pass 2 with emit */
    asm_ctx.current_pass = 2;
    temp_pc = asm_ctx.pc;
    err = assemble_line(p, &temp_pc, true);

    if (err != ASM_OK) {
        goto report_error;
    }

    /* Show assembled bytes */
    asm_print_hex16(asm_ctx.pc);
    asm_print(": ");
    uint16_t bytes_emitted = (uint16_t)(temp_pc - asm_ctx.pc);
    for (uint16_t i = 0; i < bytes_emitted; i++) {
        asm_print_hex8(mem_read((uint16_t)(asm_ctx.pc + i)));
        asm_putchar(' ');
    }
    asm_newline();

    asm_ctx.pc = temp_pc;
    return;

report_error:
    switch (err) {
        case ASM_ERR_SYNTAX:
            asm_print("?SYNTAX ERROR");
            break;
        case ASM_ERR_ILLEGAL_OPCODE:
            asm_print("?ILLEGAL OPCODE");
            break;
        case ASM_ERR_INVALID_MODE:
            asm_print("?INVALID MODE");
            break;
        case ASM_ERR_BRANCH_TOO_FAR:
            asm_print("?BRANCH TOO FAR");
            break;
        case ASM_ERR_LABEL_FULL:
            asm_print("?LABEL TABLE FULL");
            break;
        case ASM_ERR_UNDEF_LABEL:
            asm_print("?UNDEFINED LABEL");
            break;
        default:
            asm_print("?ERROR");
            break;
    }
    asm_newline();
}

/* ======================================================================
 * Multi-line mode input handling
 * ====================================================================== */

static void handle_multiline_input(const char *line)
{
    char upper_line[MAX_LINE_LEN + 1];
    strncpy(upper_line, line, MAX_LINE_LEN);
    upper_line[MAX_LINE_LEN] = '\0';
    str_toupper(upper_line);

    const char *p = upper_line;
    skip_spaces(&p);

    /* Check for .END */
    if (*p == '.') p++;
    char cmd[MAX_LABEL_LEN + 1];
    if (parse_identifier(&p, cmd, MAX_LABEL_LEN)) {
        if (strcmp(cmd, "END") == 0) {
            /* Trigger assembly */
            do_multiline_assemble();
            asm_ctx.mode = ASM_MODE_SINGLE;
            asm_ctx.num_source_lines = 0;
            return;
        }
    }

    /* Store line in source buffer */
    if (asm_ctx.num_source_lines >= MAX_SOURCE_LINES) {
        asm_print("?SOURCE BUFFER FULL");
        asm_newline();
        return;
    }

    strncpy(asm_ctx.source[asm_ctx.num_source_lines].text, upper_line, MAX_LINE_LEN);
    asm_ctx.source[asm_ctx.num_source_lines].text[MAX_LINE_LEN] = '\0';
    asm_ctx.num_source_lines++;
}

/* ======================================================================
 * Process a completed input line
 * ====================================================================== */

static void process_line(void)
{
    asm_ctx.line_buf[asm_ctx.line_pos] = '\0';

    /* Empty line or ESC exits the assembler */
    if (asm_ctx.line_pos == 0) {
        if (asm_ctx.mode == ASM_MODE_MULTI) {
            /* In multi-line mode, empty line is just a blank line */
            return;
        }
        asm_deactivate();
        return;
    }

    /* Convert to uppercase for processing */
    char upper[MAX_LINE_LEN + 1];
    strncpy(upper, asm_ctx.line_buf, MAX_LINE_LEN);
    upper[MAX_LINE_LEN] = '\0';
    str_toupper(upper);

    if (asm_ctx.mode == ASM_MODE_MULTI) {
        handle_multiline_input(upper);
    } else {
        do_single_line(upper);
    }

    asm_ctx.line_pos = 0;
}

/* ======================================================================
 * Show the prompt
 * ====================================================================== */

static void show_prompt(void)
{
    if (asm_ctx.mode == ASM_MODE_MULTI) {
        char numbuf[8];
        int ln = asm_ctx.num_source_lines + 1;
        int ni = 0;
        if (ln >= 100) numbuf[ni++] = (char)('0' + ln / 100);
        if (ln >= 10)  numbuf[ni++] = (char)('0' + (ln / 10) % 10);
        numbuf[ni++] = (char)('0' + ln % 10);
        numbuf[ni] = '\0';
        asm_print(numbuf);
        asm_putchar(':');
    } else {
        asm_putchar('*');
        asm_print_hex16(asm_ctx.pc);
        asm_putchar(' ');
    }
}

/* ======================================================================
 * Public interface
 * ====================================================================== */

void asm_init(void)
{
    memset(&asm_ctx, 0, sizeof(asm_ctx));
    asm_ctx.active = false;
    asm_ctx.state = ASM_STATE_IDLE;
    asm_ctx.mode = ASM_MODE_SINGLE;
    asm_ctx.origin = DEFAULT_ORG;
    asm_ctx.pc = DEFAULT_ORG;
    asm_ctx.current_pass = 1;
}

bool asm_is_active(void)
{
    return asm_ctx.active;
}

void asm_activate(void)
{
    asm_ctx.active = true;
    asm_ctx.state = ASM_STATE_PROMPT;
    asm_ctx.mode = ASM_MODE_SINGLE;
    asm_ctx.line_pos = 0;
    asm_ctx.num_source_lines = 0;
    asm_ctx.num_labels = 0;
    asm_ctx.input_head = 0;
    asm_ctx.input_tail = 0;

    asm_newline();
    asm_print("6502 MINI-ASSEMBLER");
    asm_newline();
}

void asm_deactivate(void)
{
    asm_ctx.active = false;
    asm_ctx.state = ASM_STATE_IDLE;
    asm_newline();
}

void asm_input_char(uint8_t ch)
{
    if (!asm_ctx.active) return;
    int next = (asm_ctx.input_head + 1) % INPUT_BUF_SIZE;
    if (next != asm_ctx.input_tail) {
        asm_ctx.input_buf[asm_ctx.input_head] = ch;
        asm_ctx.input_head = next;
    }
}

void asm_step(void)
{
    if (!asm_ctx.active) return;

    switch (asm_ctx.state) {
        case ASM_STATE_IDLE:
            break;

        case ASM_STATE_PROMPT:
            show_prompt();
            asm_ctx.state = ASM_STATE_INPUT;
            asm_ctx.line_pos = 0;
            break;

        case ASM_STATE_INPUT:
            if (!input_available()) return;
            {
                uint8_t ch = input_get();

                /* ESC exits assembler */
                if (ch == 0x1B) {
                    asm_deactivate();
                    return;
                }

                /* Carriage return / newline - process line */
                if (ch == '\r' || ch == '\n') {
                    asm_newline();
                    process_line();
                    if (asm_ctx.active) {
                        asm_ctx.state = ASM_STATE_PROMPT;
                    }
                    return;
                }

                /* Backspace/delete */
                if (ch == 0x08 || ch == 0x7F) {
                    if (asm_ctx.line_pos > 0) {
                        asm_ctx.line_pos--;
                        asm_putchar(0x08); /* backspace */
                        asm_putchar(' ');
                        asm_putchar(0x08);
                    }
                    return;
                }

                /* Printable character */
                if (ch >= 0x20 && ch < 0x7F) {
                    if (asm_ctx.line_pos < MAX_LINE_LEN - 1) {
                        asm_ctx.line_buf[asm_ctx.line_pos++] = (char)ch;
                        asm_putchar(ch); /* echo */
                    }
                }
            }
            break;

        case ASM_STATE_ASSEMBLE:
            /* Reserved for future async assembly */
            asm_ctx.state = ASM_STATE_PROMPT;
            break;

        case ASM_STATE_OUTPUT:
            /* Reserved for future paged output */
            asm_ctx.state = ASM_STATE_PROMPT;
            break;
    }
}
