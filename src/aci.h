#ifndef ACI_H
#define ACI_H

#include <stdbool.h>
#include <stdint.h>

#include "cpu.h"

/**
 * aci.h - Apple Cassette Interface (virtual)
 *
 * Emulates the Apple 1 Cassette Interface by providing save/load of memory
 * regions to virtual "tape" files.  The real ACI occupied ROM at $C100-$C1FF;
 * our implementation places a trap sequence (NOP NOP BRK) at $C100 and
 * intercepts execution there to perform file-based cassette I/O.
 *
 * Tape file format:
 *   Offset  Size    Content
 *   0x00    4       Magic: "A1TP"
 *   0x04    2       Load address (little-endian)
 *   0x06    2       Data length in bytes (little-endian)
 *   0x08    N       Data bytes
 *
 * Tape files are stored in ~/.apple1emu/tapes/ by default.
 */

/* ACI ROM base address. */
#define ACI_ROM_BASE   0xC100

/* Entry point for SAVE operation. */
#define ACI_TRAP_SAVE  0xC100

/* Entry point for LOAD operation. */
#define ACI_TRAP_LOAD  0xC108

/* Magic bytes identifying a valid tape file. */
#define ACI_TAPE_MAGIC "A1TP"

/* Tape file header size. */
#define ACI_TAPE_HEADER_SIZE 8

/**
 * Initialize the ACI module.
 * - Ensures the tapes directory (~/.apple1emu/tapes/) exists.
 * - Installs the ACI trap ROM at $C100.
 */
void aci_init(void);

/**
 * Save a region of memory to a virtual tape file.
 *
 * @param filename  Name of the tape file (stored in the tapes directory).
 * @param start     Start address of the memory region to save (inclusive).
 * @param end       End address of the memory region to save (inclusive).
 * @return true on success, false on error.
 */
bool aci_save_tape(const char *filename, uint16_t start, uint16_t end);

/**
 * Load a virtual tape file into memory at its stored load address.
 *
 * @param filename   Name of the tape file to load.
 * @param load_addr  Output: receives the address where data was loaded.
 * @return true on success, false on error.
 */
bool aci_load_tape(const char *filename, uint16_t *load_addr);

/**
 * Check whether the current PC is at an ACI trap address.
 *
 * @param pc  Current program counter value.
 * @return true if the PC is at an ACI trap ($C100 or $C108).
 */
bool aci_is_trapped(uint16_t pc);

/**
 * Handle an ACI trap.  Called from the main emulation loop when
 * aci_is_trapped() returns true.
 *
 * For SAVE ($C100): reads start address from $00-$01, end address from
 * $02-$03 (little-endian), and filename from the input buffer at $0200.
 *
 * For LOAD ($C108): reads filename from the input buffer at $0200 and loads
 * the tape into memory at the address stored in its header.
 *
 * After handling, the CPU PC is advanced past the trap to $C103 or $C10B.
 *
 * @param cpu  Pointer to the CPU state.
 */
void aci_handle_trap(cpu_state *cpu);

/**
 * Return the path to the default tapes directory.
 * The returned pointer is to a static internal buffer.
 */
const char *aci_get_dir(void);

#endif /* ACI_H */
