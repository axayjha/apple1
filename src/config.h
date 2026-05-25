#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stdint.h>

/**
 * config.h - Emulator configuration management
 *
 * Manages runtime configuration with INI-style file persistence.
 * Provides a global singleton accessible via config_get().
 *
 * Default config file location: ~/.apple1emu/config.ini
 */

/* Configuration structure */
typedef struct {
    int  ram_kb;              /* RAM size in KB: 4, 8, 32, or 48 */
    bool lowercase_enabled;  /* Allow lowercase characters */
    bool col80_enabled;      /* 80-column display mode */
    bool fast_mode;          /* Disable timing delays (max speed) */
    bool scanlines;          /* CRT scanline visual effect */
    bool phosphor_glow;      /* Phosphor afterglow visual effect */
    bool auto_repeat;        /* Keyboard auto-repeat */
    bool enhanced_basic;     /* Use Applesoft BASIC instead of Integer BASIC */
    bool virtual_disk;       /* Enable virtual disk system */
    bool debug_overlay;      /* Show CPU register display */
    char rom_dir[256];       /* Path to ROM files */
    char data_dir[256];      /* Path to data directory */
    int  log_level;          /* 0=ERROR, 1=WARN, 2=INFO, 3=DEBUG, 4=TRACE */
} config_t;

/* Initialize config with sensible defaults.
 * (32KB RAM, uppercase only, scanlines on, phosphor glow on, etc.) */
void config_init(config_t *cfg);

/* Load configuration from an INI file.
 * Returns true on success, false on failure (file not found, parse error). */
bool config_load(config_t *cfg, const char *filepath);

/* Save configuration to an INI file.
 * Returns true on success, false on write failure. */
bool config_save(const config_t *cfg, const char *filepath);

/* Get a pointer to the global configuration singleton.
 * The singleton is initialized with defaults on first call. */
config_t *config_get(void);

#endif /* CONFIG_H */
