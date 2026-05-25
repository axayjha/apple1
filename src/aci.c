#include "aci.h"
#include "memory.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/**
 * aci.c - Apple Cassette Interface (virtual) implementation
 *
 * Provides save/load of memory regions to virtual "tape" files stored on
 * the host filesystem.  A small trap ROM is installed at $C100 consisting
 * of NOP; NOP; BRK sequences that the main emulation loop can intercept
 * before the CPU steps into them.
 *
 * SAVE entry ($C100): saves memory[$00/$01 .. $02/$03] to a tape file.
 * LOAD entry ($C108): loads a tape file into memory at its stored address.
 *
 * The filename is read from the Apple 1 input buffer at $0200 (null or CR
 * terminated, up to 32 characters).
 */

/* Static buffer for the default tapes directory path. */
static char tapes_dir[512];
static bool dir_initialized = false;

/* Apple 1 input buffer location (Woz Monitor uses $0200-$027F). */
#define INPUT_BUFFER_ADDR  0x0200
#define INPUT_BUFFER_MAX   32

/* ACI ROM contents: NOP NOP BRK (x2 blocks, padded with BRK). */
#define ACI_ROM_SIZE  16

static const uint8_t aci_rom[ACI_ROM_SIZE] = {
    /* $C100: SAVE entry */
    0xEA, 0xEA, 0x00,  /* NOP NOP BRK */
    0xEA, 0xEA, 0xEA,  /* NOP NOP NOP (padding) */
    0xEA, 0xEA,         /* NOP NOP (padding) */
    /* $C108: LOAD entry */
    0xEA, 0xEA, 0x00,  /* NOP NOP BRK */
    0xEA, 0xEA, 0xEA,  /* NOP NOP NOP (padding) */
    0xEA, 0xEA          /* NOP NOP (padding) */
};

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                   */
/* ------------------------------------------------------------------ */

/**
 * Create a directory (and parents) if it does not exist.
 */
static void ensure_directory(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) {
        return;
    }

    char parent[512];
    snprintf(parent, sizeof(parent), "%s", path);
    char *slash = strrchr(parent, '/');
    if (slash && slash != parent) {
        *slash = '\0';
        ensure_directory(parent);
    }

    mkdir(path, 0755);
}

/**
 * Read a null/CR-terminated filename from the Apple 1 input buffer ($0200).
 * Copies up to INPUT_BUFFER_MAX characters into dest.
 */
static void read_filename_from_buffer(char *dest, size_t dest_size)
{
    size_t max = dest_size - 1;
    if (max > INPUT_BUFFER_MAX) {
        max = INPUT_BUFFER_MAX;
    }

    size_t i;
    for (i = 0; i < max; i++) {
        uint8_t ch = mem_read(INPUT_BUFFER_ADDR + (uint16_t)i);
        /* Apple 1 uses uppercase ASCII with bit 7 clear.
         * CR ($0D) or null ($00) terminates the string. */
        if (ch == 0x00 || ch == 0x0D) {
            break;
        }
        /* Strip bit 7 if set, convert to printable filename char */
        ch &= 0x7F;
        dest[i] = (char)ch;
    }
    dest[i] = '\0';
}

/**
 * Build the full path to a tape file.
 */
static void build_tape_path(char *dest, size_t dest_size, const char *filename)
{
    snprintf(dest, dest_size, "%s/%s", tapes_dir, filename);
}

/**
 * Write a 16-bit value in little-endian to a buffer.
 */
static void write_le16(uint8_t *buf, uint16_t val)
{
    buf[0] = (uint8_t)(val & 0xFF);
    buf[1] = (uint8_t)((val >> 8) & 0xFF);
}

/**
 * Read a 16-bit little-endian value from a buffer.
 */
static uint16_t read_le16(const uint8_t *buf)
{
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

void aci_init(void)
{
    if (!dir_initialized) {
        const char *home = getenv("HOME");
        if (home) {
            snprintf(tapes_dir, sizeof(tapes_dir),
                     "%s/.apple1emu/tapes", home);
        } else {
            snprintf(tapes_dir, sizeof(tapes_dir),
                     "./.apple1emu/tapes");
        }

        ensure_directory(tapes_dir);
        dir_initialized = true;
    }

    /* Install the ACI trap ROM at $C100. */
    mem_load_rom(ACI_ROM_BASE, aci_rom, ACI_ROM_SIZE);
}

const char *aci_get_dir(void)
{
    if (!dir_initialized) {
        aci_init();
    }
    return tapes_dir;
}

bool aci_save_tape(const char *filename, uint16_t start, uint16_t end)
{
    if (!filename || filename[0] == '\0') {
        fprintf(stderr, "aci: no filename specified for tape save\n");
        return false;
    }

    if (end < start) {
        fprintf(stderr, "aci: invalid address range $%04X-$%04X\n",
                start, end);
        return false;
    }

    uint16_t length = end - start + 1;

    char path[600];
    build_tape_path(path, sizeof(path), filename);

    FILE *fp = fopen(path, "wb");
    if (!fp) {
        fprintf(stderr, "aci: failed to create tape file: %s\n", path);
        return false;
    }

    /* Write tape header */
    uint8_t header[ACI_TAPE_HEADER_SIZE];
    header[0] = 'A';
    header[1] = '1';
    header[2] = 'T';
    header[3] = 'P';
    write_le16(&header[4], start);
    write_le16(&header[6], length);

    if (fwrite(header, 1, ACI_TAPE_HEADER_SIZE, fp) != ACI_TAPE_HEADER_SIZE) {
        fprintf(stderr, "aci: failed to write tape header\n");
        fclose(fp);
        return false;
    }

    /* Write the memory data */
    uint8_t *data = mem_get_ptr(start);
    if (fwrite(data, 1, length, fp) != length) {
        fprintf(stderr, "aci: failed to write tape data\n");
        fclose(fp);
        return false;
    }

    fclose(fp);
    return true;
}

bool aci_load_tape(const char *filename, uint16_t *load_addr)
{
    if (!filename || filename[0] == '\0') {
        fprintf(stderr, "aci: no filename specified for tape load\n");
        return false;
    }

    char path[600];
    build_tape_path(path, sizeof(path), filename);

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "aci: tape file not found: %s\n", path);
        return false;
    }

    /* Read and validate header */
    uint8_t header[ACI_TAPE_HEADER_SIZE];
    if (fread(header, 1, ACI_TAPE_HEADER_SIZE, fp) != ACI_TAPE_HEADER_SIZE) {
        fprintf(stderr, "aci: tape file too short (header)\n");
        fclose(fp);
        return false;
    }

    if (header[0] != 'A' || header[1] != '1' ||
        header[2] != 'T' || header[3] != 'P') {
        fprintf(stderr, "aci: invalid tape file magic\n");
        fclose(fp);
        return false;
    }

    uint16_t addr   = read_le16(&header[4]);
    uint16_t length = read_le16(&header[6]);

    if (length == 0) {
        fprintf(stderr, "aci: tape file has zero length\n");
        fclose(fp);
        return false;
    }

    /* Check for address space overflow */
    if ((uint32_t)addr + length > 0x10000) {
        fprintf(stderr, "aci: tape data would overflow address space\n");
        fclose(fp);
        return false;
    }

    /* Read tape data directly into memory backing store */
    uint8_t *dest = mem_get_ptr(addr);
    if (fread(dest, 1, length, fp) != length) {
        fprintf(stderr, "aci: failed to read tape data (file truncated?)\n");
        fclose(fp);
        return false;
    }

    if (load_addr) {
        *load_addr = addr;
    }

    fclose(fp);
    return true;
}

bool aci_is_trapped(uint16_t pc)
{
    return (pc == ACI_TRAP_SAVE || pc == ACI_TRAP_LOAD);
}

void aci_handle_trap(cpu_state *cpu)
{
    if (!cpu) {
        return;
    }

    /* Read the filename from the input buffer at $0200. */
    char filename[INPUT_BUFFER_MAX + 1];
    read_filename_from_buffer(filename, sizeof(filename));

    if (cpu->pc == ACI_TRAP_SAVE) {
        /* SAVE: read start address from $00-$01, end address from $02-$03
         * (little-endian, as the 6502 stores them). */
        uint16_t start = (uint16_t)mem_read(0x0000) |
                         ((uint16_t)mem_read(0x0001) << 8);
        uint16_t end   = (uint16_t)mem_read(0x0002) |
                         ((uint16_t)mem_read(0x0003) << 8);

        if (aci_save_tape(filename, start, end)) {
            /* Signal success: set A=0 (no error) */
            cpu->a = 0x00;
        } else {
            /* Signal failure: set A=$FF */
            cpu->a = 0xFF;
        }

        /* Advance PC past the trap (NOP NOP BRK = 3 bytes) */
        cpu->pc = ACI_TRAP_SAVE + 3;

    } else if (cpu->pc == ACI_TRAP_LOAD) {
        uint16_t load_addr = 0;

        if (aci_load_tape(filename, &load_addr)) {
            /* Signal success: A=0, store load address in $00-$01 */
            cpu->a = 0x00;
            mem_write(0x0000, (uint8_t)(load_addr & 0xFF));
            mem_write(0x0001, (uint8_t)((load_addr >> 8) & 0xFF));
        } else {
            cpu->a = 0xFF;
        }

        /* Advance PC past the trap */
        cpu->pc = ACI_TRAP_LOAD + 3;
    }
}
