#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>

/**
 * memory.h - Apple 1 memory bus emulation
 *
 * Provides a 64KB flat address space with:
 *   - Configurable RAM (4KB, 8KB, 32KB, 48KB)
 *   - ROM regions that reject writes
 *   - Memory-mapped I/O dispatch to the PIA at $D010-$D013
 */

/* Initialize memory subsystem with the given RAM size in kilobytes.
 * Valid values: 4, 8, 32, 48. Other values default to 4. */
void mem_init(int ram_kb);

/* Read a byte from the given 16-bit address.
 * Dispatches to PIA for $D010-$D013. Returns $00 for reads above RAM limit
 * (unless the address is in a ROM or I/O region). */
uint8_t mem_read(uint16_t addr);

/* Write a byte to the given 16-bit address.
 * Dispatches to PIA for $D010-$D013. Writes to ROM regions are silently ignored. */
void mem_write(uint16_t addr, uint8_t val);

/* Load ROM data into memory at the specified base address. */
void mem_load_rom(uint16_t base_addr, const uint8_t *data, uint16_t size);

/* Load ROM data from a file into memory at the specified base address.
 * Returns 0 on success, -1 on failure. */
void mem_load_rom_file(uint16_t base_addr, const char *filepath);

/* Get a direct pointer into the backing memory array.
 * Useful for DMA-style access and snapshot/restore operations. */
uint8_t *mem_get_ptr(uint16_t addr);

#endif /* MEMORY_H */
