#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Global singleton instance */
static config_t g_config;
static bool     g_config_initialized = false;

/* Helper: trim leading and trailing whitespace in place */
static char *trim(char *str)
{
    while (isspace((unsigned char)*str)) {
        str++;
    }
    if (*str == '\0') {
        return str;
    }
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }
    *(end + 1) = '\0';
    return str;
}

/* Helper: expand leading ~ to HOME */
static void expand_tilde(char *dest, size_t dest_size, const char *src)
{
    if (src[0] == '~' && (src[1] == '/' || src[1] == '\0')) {
        const char *home = getenv("HOME");
        if (home) {
            snprintf(dest, dest_size, "%s%s", home, src + 1);
            return;
        }
    }
    snprintf(dest, dest_size, "%s", src);
}

/* Helper: parse a boolean value from string */
static bool parse_bool(const char *val)
{
    return (strcmp(val, "true") == 0 ||
            strcmp(val, "yes") == 0 ||
            strcmp(val, "1") == 0);
}

void config_init(config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));

    cfg->ram_kb            = 32;
    cfg->lowercase_enabled = false;
    cfg->col80_enabled     = false;
    cfg->fast_mode         = false;
    cfg->scanlines         = false;
    cfg->phosphor_glow     = true;
    cfg->auto_repeat       = false;
    cfg->enhanced_basic    = false;
    cfg->virtual_disk      = true;
    cfg->debug_overlay     = false;
    cfg->log_level         = 2;  /* INFO */

    expand_tilde(cfg->rom_dir, sizeof(cfg->rom_dir), "~/.apple1emu/roms");
    expand_tilde(cfg->data_dir, sizeof(cfg->data_dir), "~/.apple1emu");
}

bool config_load(config_t *cfg, const char *filepath)
{
    char resolved[512];
    expand_tilde(resolved, sizeof(resolved), filepath);

    FILE *fp = fopen(resolved, "r");
    if (!fp) {
        return false;
    }

    char line[512];
    /* section is tracked but not strictly required for flat key matching */
    char section[64] = "";

    while (fgets(line, sizeof(line), fp)) {
        char *p = trim(line);

        /* Skip empty lines and comments */
        if (*p == '\0' || *p == '#' || *p == ';') {
            continue;
        }

        /* Section header */
        if (*p == '[') {
            char *end = strchr(p, ']');
            if (end) {
                *end = '\0';
                snprintf(section, sizeof(section), "%s", p + 1);
            }
            continue;
        }

        /* Key=value pair */
        char *eq = strchr(p, '=');
        if (!eq) {
            continue;
        }

        *eq = '\0';
        char *key = trim(p);
        char *val = trim(eq + 1);

        /* Match keys */
        if (strcmp(key, "ram_kb") == 0) {
            int v = atoi(val);
            if (v == 4 || v == 8 || v == 32 || v == 48) {
                cfg->ram_kb = v;
            }
        } else if (strcmp(key, "lowercase") == 0) {
            cfg->lowercase_enabled = parse_bool(val);
        } else if (strcmp(key, "col80") == 0) {
            cfg->col80_enabled = parse_bool(val);
        } else if (strcmp(key, "fast_mode") == 0) {
            cfg->fast_mode = parse_bool(val);
        } else if (strcmp(key, "scanlines") == 0) {
            cfg->scanlines = parse_bool(val);
        } else if (strcmp(key, "phosphor_glow") == 0) {
            cfg->phosphor_glow = parse_bool(val);
        } else if (strcmp(key, "auto_repeat") == 0) {
            cfg->auto_repeat = parse_bool(val);
        } else if (strcmp(key, "enhanced_basic") == 0) {
            cfg->enhanced_basic = parse_bool(val);
        } else if (strcmp(key, "virtual_disk") == 0) {
            cfg->virtual_disk = parse_bool(val);
        } else if (strcmp(key, "debug_overlay") == 0) {
            cfg->debug_overlay = parse_bool(val);
        } else if (strcmp(key, "log_level") == 0) {
            int v = atoi(val);
            if (v >= 0 && v <= 4) {
                cfg->log_level = v;
            }
        } else if (strcmp(key, "rom_dir") == 0) {
            expand_tilde(cfg->rom_dir, sizeof(cfg->rom_dir), val);
        } else if (strcmp(key, "data_dir") == 0) {
            expand_tilde(cfg->data_dir, sizeof(cfg->data_dir), val);
        }
    }

    fclose(fp);
    return true;
}

bool config_save(const config_t *cfg, const char *filepath)
{
    char resolved[512];
    expand_tilde(resolved, sizeof(resolved), filepath);

    FILE *fp = fopen(resolved, "w");
    if (!fp) {
        return false;
    }

    fprintf(fp, "[emulator]\n");
    fprintf(fp, "ram_kb=%d\n", cfg->ram_kb);
    fprintf(fp, "lowercase=%s\n", cfg->lowercase_enabled ? "true" : "false");
    fprintf(fp, "col80=%s\n", cfg->col80_enabled ? "true" : "false");
    fprintf(fp, "fast_mode=%s\n", cfg->fast_mode ? "true" : "false");
    fprintf(fp, "scanlines=%s\n", cfg->scanlines ? "true" : "false");
    fprintf(fp, "phosphor_glow=%s\n", cfg->phosphor_glow ? "true" : "false");
    fprintf(fp, "auto_repeat=%s\n", cfg->auto_repeat ? "true" : "false");
    fprintf(fp, "enhanced_basic=%s\n", cfg->enhanced_basic ? "true" : "false");
    fprintf(fp, "virtual_disk=%s\n", cfg->virtual_disk ? "true" : "false");
    fprintf(fp, "debug_overlay=%s\n", cfg->debug_overlay ? "true" : "false");
    fprintf(fp, "log_level=%d\n", cfg->log_level);
    fprintf(fp, "\n");
    fprintf(fp, "[paths]\n");
    fprintf(fp, "rom_dir=%s\n", cfg->rom_dir);
    fprintf(fp, "data_dir=%s\n", cfg->data_dir);

    fclose(fp);
    return true;
}

config_t *config_get(void)
{
    if (!g_config_initialized) {
        config_init(&g_config);
        g_config_initialized = true;
    }
    return &g_config;
}
