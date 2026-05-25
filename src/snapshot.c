#include "snapshot.h"
#include "memory.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/**
 * snapshot.c - Machine state save/restore implementation
 *
 * Saves the complete emulator state (CPU + 64KB memory) to a binary file
 * and restores it on load.  PIA state is captured via the memory image at
 * $D010-$D013 in the backing store; on restore the PIA will reinitialize
 * from subsequent accesses (the Woz Monitor polls KBDCR before reading
 * any key, so no stale state issues arise).
 */

/* Static buffer for the default snapshots directory path. */
static char snapshots_dir[512];
static bool dir_initialized = false;

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                   */
/* ------------------------------------------------------------------ */

/**
 * Write a 16-bit value in little-endian to a buffer.
 */
static void write_le16(uint8_t *buf, uint16_t val)
{
    buf[0] = (uint8_t)(val & 0xFF);
    buf[1] = (uint8_t)((val >> 8) & 0xFF);
}

/**
 * Write a 64-bit value in little-endian to a buffer.
 */
static void write_le64(uint8_t *buf, uint64_t val)
{
    for (int i = 0; i < 8; i++) {
        buf[i] = (uint8_t)(val & 0xFF);
        val >>= 8;
    }
}

/**
 * Read a 16-bit little-endian value from a buffer.
 */
static uint16_t read_le16(const uint8_t *buf)
{
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

/**
 * Read a 64-bit little-endian value from a buffer.
 */
static uint64_t read_le64(const uint8_t *buf)
{
    uint64_t val = 0;
    for (int i = 7; i >= 0; i--) {
        val = (val << 8) | buf[i];
    }
    return val;
}

/**
 * Create a directory (and parents) if it does not exist.
 * Simplified implementation using mkdir with mode 0755.
 */
static void ensure_directory(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) {
        return;  /* Already exists */
    }

    /* Try to create parent first (one level up). */
    char parent[512];
    snprintf(parent, sizeof(parent), "%s", path);
    char *slash = strrchr(parent, '/');
    if (slash && slash != parent) {
        *slash = '\0';
        ensure_directory(parent);
    }

    mkdir(path, 0755);
}

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

void snapshot_init(void)
{
    if (dir_initialized) {
        return;
    }

    const char *home = getenv("HOME");
    if (home) {
        snprintf(snapshots_dir, sizeof(snapshots_dir),
                 "%s/.apple1emu/snapshots", home);
    } else {
        snprintf(snapshots_dir, sizeof(snapshots_dir),
                 "./.apple1emu/snapshots");
    }

    ensure_directory(snapshots_dir);
    dir_initialized = true;
}

const char *snapshot_get_dir(void)
{
    if (!dir_initialized) {
        snapshot_init();
    }
    return snapshots_dir;
}

bool snapshot_save(const char *filepath, const cpu_state *cpu)
{
    if (!filepath || !cpu) {
        return false;
    }

    if (!dir_initialized) {
        snapshot_init();
    }

    FILE *fp = fopen(filepath, "wb");
    if (!fp) {
        fprintf(stderr, "snapshot: failed to open file for writing: %s\n",
                filepath);
        return false;
    }

    /* Build the 32-byte header. */
    uint8_t header[SNAPSHOT_HEADER_SIZE];
    memset(header, 0, sizeof(header));

    /* Magic: "A1SS" */
    header[0] = 'A';
    header[1] = '1';
    header[2] = 'S';
    header[3] = 'S';

    /* Version */
    write_le16(&header[0x04], SNAPSHOT_VERSION);

    /* Reserved at 0x06-0x07 already zero */

    /* CPU registers */
    write_le16(&header[0x08], cpu->pc);
    header[0x0A] = cpu->a;
    header[0x0B] = cpu->x;
    header[0x0C] = cpu->y;
    header[0x0D] = cpu->sp;
    header[0x0E] = cpu->status;
    /* 0x0F reserved */

    /* Cycle counter */
    write_le64(&header[0x10], cpu->cycles);

    /* PIA state: read from the memory backing store at $D010-$D013.
     * This captures whatever values are in the memory array at those
     * addresses (which may not perfectly reflect internal PIA state,
     * but is sufficient for basic snapshot/restore). */
    uint8_t *pia_mem = mem_get_ptr(0xD010);
    header[0x18] = pia_mem[0];  /* kbd_reg */
    header[0x19] = pia_mem[1];  /* kbd_flag */
    header[0x1A] = pia_mem[2];  /* dsp_reg */
    header[0x1B] = pia_mem[3];  /* dsp_flag */

    /* Config flags: store RAM size and mode flags from current config. */
    const config_t *cfg = config_get();
    write_le16(&header[0x1C], (uint16_t)cfg->ram_kb);

    uint16_t mode_flags = 0;
    if (cfg->lowercase_enabled) mode_flags |= 0x0001;
    if (cfg->fast_mode)         mode_flags |= 0x0002;
    if (cfg->enhanced_basic)    mode_flags |= 0x0004;
    write_le16(&header[0x1E], mode_flags);

    /* Write header */
    if (fwrite(header, 1, SNAPSHOT_HEADER_SIZE, fp) != SNAPSHOT_HEADER_SIZE) {
        fprintf(stderr, "snapshot: failed to write header\n");
        fclose(fp);
        return false;
    }

    /* Write full 64KB memory image */
    uint8_t *mem_base = mem_get_ptr(0x0000);
    if (fwrite(mem_base, 1, 65536, fp) != 65536) {
        fprintf(stderr, "snapshot: failed to write memory image\n");
        fclose(fp);
        return false;
    }

    fclose(fp);
    return true;
}

bool snapshot_load(const char *filepath, cpu_state *cpu)
{
    if (!filepath || !cpu) {
        return false;
    }

    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        fprintf(stderr, "snapshot: failed to open file for reading: %s\n",
                filepath);
        return false;
    }

    /* Read the header */
    uint8_t header[SNAPSHOT_HEADER_SIZE];
    if (fread(header, 1, SNAPSHOT_HEADER_SIZE, fp) != SNAPSHOT_HEADER_SIZE) {
        fprintf(stderr, "snapshot: file too short (header)\n");
        fclose(fp);
        return false;
    }

    /* Validate magic */
    if (header[0] != 'A' || header[1] != '1' ||
        header[2] != 'S' || header[3] != 'S') {
        fprintf(stderr, "snapshot: invalid magic (not an Apple 1 snapshot)\n");
        fclose(fp);
        return false;
    }

    /* Check version */
    uint16_t version = read_le16(&header[0x04]);
    if (version > SNAPSHOT_VERSION) {
        fprintf(stderr, "snapshot: unsupported version %u (max %u)\n",
                version, SNAPSHOT_VERSION);
        fclose(fp);
        return false;
    }

    /* Restore CPU registers */
    cpu->pc     = read_le16(&header[0x08]);
    cpu->a      = header[0x0A];
    cpu->x      = header[0x0B];
    cpu->y      = header[0x0C];
    cpu->sp     = header[0x0D];
    cpu->status = header[0x0E];

    /* Restore cycle counter */
    cpu->cycles = read_le64(&header[0x10]);

    /* Clear interrupt state on restore */
    cpu->nmi_pending = false;
    cpu->irq_pending = false;

    /* Read the full 64KB memory image directly into the backing store */
    uint8_t *mem_base = mem_get_ptr(0x0000);
    if (fread(mem_base, 1, 65536, fp) != 65536) {
        fprintf(stderr, "snapshot: file too short (memory image)\n");
        fclose(fp);
        return false;
    }

    fclose(fp);
    return true;
}
