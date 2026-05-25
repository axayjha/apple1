#include "roms_builtin.h"
#include "memory.h"

/**
 * roms_builtin.c - Load built-in ROM images into emulator memory
 *
 * Loads the Woz Monitor at $FF00 and the BASIC stub at $E000.
 */

void roms_load_builtin(void)
{
    /* Load Woz Monitor ROM at $FF00 (256 bytes) */
    mem_load_rom(0xFF00, wozmon_rom, sizeof(wozmon_rom));

    /* Load BASIC stub at $E000 (provides cold/warm start entry points) */
    mem_load_rom(0xE000, basic_stub, sizeof(basic_stub));
}
