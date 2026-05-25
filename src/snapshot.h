#ifndef SNAPSHOT_H
#define SNAPSHOT_H

#include <stdbool.h>
#include <stdint.h>

#include "cpu.h"

/**
 * snapshot.h - Machine state save/restore for the Apple 1 emulator
 *
 * Captures the complete emulator state (CPU registers, cycle count, and full
 * 64KB memory image) to a binary file so that execution can be perfectly
 * resumed later.
 *
 * File format (65568 bytes total):
 *   Offset  Size    Content
 *   0x00    4       Magic: "A1SS" (Apple 1 Snapshot State)
 *   0x04    2       Version: 0x0001
 *   0x06    2       Reserved (zero)
 *   0x08    2       CPU PC (little-endian)
 *   0x0A    1       CPU A
 *   0x0B    1       CPU X
 *   0x0C    1       CPU Y
 *   0x0D    1       CPU SP
 *   0x0E    1       CPU Status
 *   0x0F    1       Reserved (zero)
 *   0x10    8       CPU cycle counter (uint64_t, little-endian)
 *   0x18    4       PIA state (kbd_reg, kbd_flag, dsp_reg, dsp_flag)
 *   0x1C    4       Config flags (ram_size_kb LE16, mode flags LE16)
 *   0x20    65536   Full 64KB memory contents
 *
 * Snapshot files are stored in ~/.apple1emu/snapshots/ by default.
 */

/* Size of the snapshot file header (before the memory image). */
#define SNAPSHOT_HEADER_SIZE  0x20

/* Total snapshot file size: header + 64KB memory. */
#define SNAPSHOT_FILE_SIZE    (SNAPSHOT_HEADER_SIZE + 65536)

/* Magic bytes identifying a valid snapshot file. */
#define SNAPSHOT_MAGIC        "A1SS"

/* Current snapshot format version. */
#define SNAPSHOT_VERSION      0x0001

/**
 * Ensure the snapshot directory (~/.apple1emu/snapshots/) exists.
 * Creates it (and parent directories) if necessary.
 */
void snapshot_init(void);

/**
 * Save the complete emulator state to a binary file.
 *
 * @param filepath  Path to the output file (NULL uses auto-generated name in
 *                  the default snapshots directory).
 * @param cpu       Pointer to the CPU state to save.
 * @return true on success, false on I/O or other error.
 */
bool snapshot_save(const char *filepath, const cpu_state *cpu);

/**
 * Load a previously saved snapshot and restore the emulator state.
 *
 * @param filepath  Path to the snapshot file to load.
 * @param cpu       Pointer to the CPU state to restore into.
 * @return true on success, false on error (file not found, corrupt, etc.).
 */
bool snapshot_load(const char *filepath, cpu_state *cpu);

/**
 * Return the path to the default snapshots directory.
 * The returned pointer is to a static internal buffer.
 */
const char *snapshot_get_dir(void);

#endif /* SNAPSHOT_H */
