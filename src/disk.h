#ifndef APPLE1_DISK_H
#define APPLE1_DISK_H

#include <stdbool.h>
#include <stdint.h>

/**
 * disk.h - Virtual disk filesystem for the Apple 1 emulator.
 *
 * Provides a sandboxed virtual filesystem that Apple 1 BASIC programs
 * can use to save and load files. Files are stored on the host filesystem
 * at ~/.apple1emu/disks/ with strict path and name validation to prevent
 * escape from the sandbox.
 *
 * Constraints:
 *   - Filenames: max 15 characters, uppercase A-Z, 0-9, period only
 *   - Max 256 files per disk
 *   - Max file size: 32KB per file
 *   - Total disk capacity: configurable, default 140KB
 */

/* Maximum filename length (not including null terminator). */
#define DISK_MAX_NAME_LEN    15

/* Maximum files per disk. */
#define DISK_MAX_FILES       256

/* Maximum file size in bytes (32KB). */
#define DISK_MAX_FILE_SIZE   (32 * 1024)

/* Default total disk capacity in bytes (140KB, matching Apple II floppy). */
#define DISK_DEFAULT_CAPACITY (140 * 1024)

/* On-disk file header magic bytes. */
#define DISK_MAGIC           "A1DF"
#define DISK_MAGIC_LEN       4

/* File header size: 4 (magic) + 1 (type) + 2 (load_addr) + 2 (size) = 9 */
#define DISK_HEADER_SIZE     9

/* File type codes. */
#define DISK_TYPE_BINARY     'B'
#define DISK_TYPE_TEXT       'T'
#define DISK_TYPE_APPLESOFT  'A'
#define DISK_TYPE_INTEGER    'I'

/**
 * Catalog entry structure representing a file on the virtual disk.
 */
typedef struct {
    char     name[16];      /* Null-terminated filename */
    char     type;          /* 'B', 'T', 'A', 'I' */
    uint16_t size;          /* Size in bytes */
    uint16_t load_addr;     /* Load address (for binary files) */
} disk_entry_t;

/**
 * Initialize the virtual disk subsystem.
 *
 * Determines the disk directory (~/.apple1emu/disks/), creates it if
 * it does not exist, and verifies permissions.
 */
void disk_init(void);

/**
 * Shut down the virtual disk subsystem.
 */
void disk_shutdown(void);

/**
 * Read the catalog of files on the virtual disk.
 *
 * entries     - array to fill with catalog entries.
 * max_entries - maximum number of entries to return.
 *
 * Returns the number of entries read, or -1 on error.
 */
int disk_catalog(disk_entry_t *entries, int max_entries);

/**
 * Save BASIC program data to the virtual disk.
 *
 * name - filename (validated against naming rules).
 * data - pointer to tokenized BASIC data.
 * size - size of data in bytes.
 *
 * Returns true on success, false on failure.
 */
bool disk_save_basic(const char *name, const uint8_t *data, uint16_t size);

/**
 * Load BASIC program data from the virtual disk.
 *
 * name - filename to load.
 * data - buffer to receive tokenized BASIC data.
 * size - on success, set to the size of loaded data.
 *
 * Returns true on success, false on failure.
 */
bool disk_load_basic(const char *name, uint8_t *data, uint16_t *size);

/**
 * Save a binary blob to the virtual disk with a load address.
 *
 * name - filename (validated against naming rules).
 * addr - load address for the binary data.
 * size - size of data in bytes (read from emulator memory at addr).
 *
 * Returns true on success, false on failure.
 */
bool disk_save_binary(const char *name, uint16_t addr, uint16_t size);

/**
 * Load a binary blob from the virtual disk into emulator memory.
 *
 * name - filename to load.
 * addr - on success, set to the load address from the file.
 * size - on success, set to the size of loaded data.
 *
 * Returns true on success, false on failure.
 */
bool disk_load_binary(const char *name, uint16_t *addr, uint16_t *size);

/**
 * Save text data to the virtual disk.
 *
 * name - filename (validated against naming rules).
 * text - pointer to text data.
 * size - size of text in bytes.
 *
 * Returns true on success, false on failure.
 */
bool disk_save_text(const char *name, const char *text, uint16_t size);

/**
 * Load text data from the virtual disk.
 *
 * name        - filename to load.
 * text        - buffer to receive text data.
 * max_size    - maximum bytes to read into text buffer.
 * actual_size - on success, set to the actual size of loaded data.
 *
 * Returns true on success, false on failure.
 */
bool disk_load_text(const char *name, char *text, uint16_t max_size,
                    uint16_t *actual_size);

/**
 * Delete a file from the virtual disk.
 *
 * name - filename to delete.
 *
 * Returns true on success, false on failure.
 */
bool disk_delete(const char *name);

/**
 * Rename a file on the virtual disk.
 *
 * old_name - current filename.
 * new_name - new filename (validated against naming rules).
 *
 * Returns true on success, false on failure.
 */
bool disk_rename(const char *old_name, const char *new_name);

/**
 * Check if a file exists on the virtual disk.
 *
 * name - filename to check.
 *
 * Returns true if the file exists, false otherwise.
 */
bool disk_exists(const char *name);

/**
 * Get the path to the virtual disk directory.
 *
 * Returns a pointer to the internal path string (do not free).
 */
const char *disk_get_dir(void);

/**
 * Validate a filename against the disk naming rules.
 *
 * Rules:
 *   - Maximum 15 characters
 *   - Only uppercase A-Z, digits 0-9, and period allowed
 *   - Must not contain '/', '\', or ".."
 *   - Must not be empty
 *
 * name - filename to validate.
 *
 * Returns true if the name is valid, false otherwise.
 */
bool disk_validate_name(const char *name);

#endif /* APPLE1_DISK_H */
