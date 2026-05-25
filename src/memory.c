#include "memory.h"
#include "pia.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * memory.c - Apple 1 memory bus emulation
 *
 * Address map:
 *   $0000-$BFFF  RAM (up to 48KB, configurable)
 *   $C100-$C1FF  ACI ROM (256 bytes, optional)
 *   $D010-$D013  PIA I/O registers (memory-mapped)
 *   $E000-$EFFF  BASIC ROM (4KB, optional)
 *   $FF00-$FFFF  Woz Monitor ROM (256 bytes)
 */

/* 64KB flat backing store */
static uint8_t memory[65536];

/* RAM limit in bytes */
static uint16_t ram_limit;

/* ROM region definitions */
#define ROM_WOZ_START   0xFF00
#define ROM_WOZ_END     0xFFFF
#define ROM_BASIC_START 0xE000
#define ROM_BASIC_END   0xEFFF
#define ROM_ACI_START   0xC100
#define ROM_ACI_END     0xC1FF

/* PIA I/O region */
#define PIA_START       0xD010
#define PIA_END         0xD013

/**
 * Check if an address falls within the PIA I/O region.
 */
static inline bool is_pia_address(uint16_t addr)
{
    return (addr >= PIA_START && addr <= PIA_END);
}

/**
 * Check if an address falls within a ROM region.
 */
static inline bool is_rom_address(uint16_t addr)
{
    if (addr >= ROM_WOZ_START && addr <= ROM_WOZ_END) return true;
    if (addr >= ROM_BASIC_START && addr <= ROM_BASIC_END) return true;
    if (addr >= ROM_ACI_START && addr <= ROM_ACI_END) return true;
    return false;
}

void mem_init(int ram_kb)
{
    /* Clear entire address space */
    memset(memory, 0, sizeof(memory));

    /* Set RAM limit based on requested size */
    switch (ram_kb) {
    case 4:
        ram_limit = 4 * 1024;
        break;
    case 8:
        ram_limit = 8 * 1024;
        break;
    case 32:
        ram_limit = 32 * 1024;
        break;
    case 48:
        ram_limit = 48 * 1024;
        break;
    default:
        /* Default to 4KB if an invalid size is specified */
        ram_limit = 4 * 1024;
        break;
    }
}

uint8_t mem_read(uint16_t addr)
{
    /* PIA I/O dispatch takes priority */
    if (is_pia_address(addr)) {
        return pia_read(addr);
    }

    /* ROM regions are always readable */
    if (is_rom_address(addr)) {
        return memory[addr];
    }

    /* RAM region: reads above the configured limit return $00 */
    if (addr < ram_limit) {
        return memory[addr];
    }

    /* Addresses in non-RAM, non-ROM, non-I/O space (e.g., unmapped regions
     * between RAM and ROM) return the backing store value which is $00
     * after initialization, or whatever was loaded there. For addresses
     * above ram_limit that are not ROM/IO, we return $00. */
    /* However, some regions (like $C000-$C0FF, $C200-$DFFF minus PIA,
     * $F000-$FEFF) may have data loaded via mem_load_rom, so we allow
     * reads from the backing store for all addresses above RAM that are
     * not strictly in the "above RAM, below ROM" gap.
     * Simplification: return backing store for any address >= $C000 since
     * ROM/IO/expansion lives there. For addresses between ram_limit and
     * $C000, return $00. */
    if (addr >= 0xC000) {
        return memory[addr];
    }

    return 0x00;
}

void mem_write(uint16_t addr, uint8_t val)
{
    /* PIA I/O dispatch takes priority */
    if (is_pia_address(addr)) {
        pia_write(addr, val);
        return;
    }

    /* ROM regions reject writes silently */
    if (is_rom_address(addr)) {
        return;
    }

    /* Only write to RAM within the configured limit */
    if (addr < ram_limit) {
        memory[addr] = val;
    }

    /* Writes to unmapped regions above RAM are silently ignored */
}

void mem_load_rom(uint16_t base_addr, const uint8_t *data, uint16_t size)
{
    /* Copy ROM data directly into the backing store.
     * This bypasses write protection since it is a direct load. */
    for (uint16_t i = 0; i < size; i++) {
        uint32_t target = (uint32_t)base_addr + i;
        if (target > 0xFFFF) break;
        memory[target] = data[i];
    }
}

void mem_load_rom_file(uint16_t base_addr, const char *filepath)
{
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        fprintf(stderr, "memory: failed to open ROM file: %s\n", filepath);
        return;
    }

    /* Determine file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0) {
        fprintf(stderr, "memory: ROM file is empty: %s\n", filepath);
        fclose(f);
        return;
    }

    /* Clamp to available address space from base_addr */
    uint32_t max_size = 0x10000 - (uint32_t)base_addr;
    if ((uint32_t)file_size > max_size) {
        file_size = (long)max_size;
    }

    size_t bytes_read = fread(&memory[base_addr], 1, (size_t)file_size, f);
    if (bytes_read != (size_t)file_size) {
        fprintf(stderr, "memory: partial read of ROM file: %s (%zu of %ld bytes)\n",
                filepath, bytes_read, file_size);
    }

    fclose(f);
}

uint8_t *mem_get_ptr(uint16_t addr)
{
    return &memory[addr];
}
