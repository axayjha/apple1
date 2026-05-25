#include "disk.h"
#include "log.h"
#include "memory.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* --------------------------------------------------------------------------
 * Internal state
 * -------------------------------------------------------------------------- */

/* Path to the virtual disk directory. */
static char disk_dir[PATH_MAX];

/* Whether the subsystem has been initialized. */
static bool disk_initialized = false;

/* File extension for virtual disk files. */
static const char *DISK_EXT = ".a1f";

/* --------------------------------------------------------------------------
 * Internal helpers
 * -------------------------------------------------------------------------- */

/**
 * Create directories recursively (equivalent to mkdir -p).
 * Returns true on success or if the directory already exists.
 */
static bool mkdir_p(const char *path)
{
    char tmp[PATH_MAX];
    size_t len;

    len = strnlen(path, sizeof(tmp) - 1);
    if (len == 0) {
        return false;
    }

    memcpy(tmp, path, len);
    tmp[len] = '\0';

    /* Remove trailing slash. */
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                return false;
            }
            *p = '/';
        }
    }

    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return false;
    }

    return true;
}

/**
 * Construct the full host path for a given validated filename.
 * Returns true on success, false if the path cannot be safely resolved.
 */
static bool construct_path(const char *name, char *out, size_t out_size)
{
    int n = snprintf(out, out_size, "%s/%s%s", disk_dir, name, DISK_EXT);
    if (n < 0 || (size_t)n >= out_size) {
        LOG_ERROR(LOG_COMP_DISK, "path too long for file '%s'", name);
        return false;
    }

    /* Resolve to an absolute path and verify it is within the disk directory. */
    char resolved[PATH_MAX];
    if (realpath(out, resolved) != NULL) {
        /* File exists: verify it resolves within the disk directory. */
        if (strncmp(resolved, disk_dir, strlen(disk_dir)) != 0) {
            LOG_ERROR(LOG_COMP_DISK, "path escape detected for '%s'", name);
            return false;
        }
    } else {
        /*
         * File doesn't exist yet (e.g., during save). Resolve the directory
         * portion and verify it is the disk directory.
         */
        char dir_resolved[PATH_MAX];
        if (realpath(disk_dir, dir_resolved) == NULL) {
            LOG_ERROR(LOG_COMP_DISK, "cannot resolve disk directory");
            return false;
        }
        /* Rebuild the path using the resolved directory. */
        n = snprintf(out, out_size, "%s/%s%s", dir_resolved, name, DISK_EXT);
        if (n < 0 || (size_t)n >= out_size) {
            LOG_ERROR(LOG_COMP_DISK, "resolved path too long for file '%s'", name);
            return false;
        }
    }

    return true;
}

/**
 * Check that a path does not refer to a symlink.
 * Returns true if the path is safe (not a symlink or doesn't exist).
 */
static bool check_not_symlink(const char *path)
{
    struct stat st;
    if (lstat(path, &st) == 0) {
        if (S_ISLNK(st.st_mode)) {
            LOG_ERROR(LOG_COMP_DISK, "rejecting symlink: %s", path);
            return false;
        }
    }
    /* If lstat fails (file doesn't exist), that's fine for save operations. */
    return true;
}

/**
 * Write the .a1f file header.
 */
static bool write_header(FILE *fp, char type, uint16_t load_addr, uint16_t size)
{
    uint8_t header[DISK_HEADER_SIZE];

    memcpy(header, DISK_MAGIC, DISK_MAGIC_LEN);
    header[4] = (uint8_t)type;
    header[5] = (uint8_t)(load_addr & 0xFF);
    header[6] = (uint8_t)((load_addr >> 8) & 0xFF);
    header[7] = (uint8_t)(size & 0xFF);
    header[8] = (uint8_t)((size >> 8) & 0xFF);

    if (fwrite(header, 1, DISK_HEADER_SIZE, fp) != DISK_HEADER_SIZE) {
        LOG_ERROR(LOG_COMP_DISK, "failed to write file header");
        return false;
    }

    return true;
}

/**
 * Read and validate the .a1f file header.
 */
static bool read_header(FILE *fp, char *type, uint16_t *load_addr, uint16_t *size)
{
    uint8_t header[DISK_HEADER_SIZE];

    if (fread(header, 1, DISK_HEADER_SIZE, fp) != DISK_HEADER_SIZE) {
        LOG_ERROR(LOG_COMP_DISK, "failed to read file header");
        return false;
    }

    if (memcmp(header, DISK_MAGIC, DISK_MAGIC_LEN) != 0) {
        LOG_ERROR(LOG_COMP_DISK, "invalid file magic");
        return false;
    }

    *type = (char)header[4];
    *load_addr = (uint16_t)(header[5] | (header[6] << 8));
    *size = (uint16_t)(header[7] | (header[8] << 8));

    return true;
}

/**
 * Count the current number of .a1f files in the disk directory.
 */
static int count_files(void)
{
    DIR *dir = opendir(disk_dir);
    if (!dir) {
        return -1;
    }

    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        size_t len = strlen(ent->d_name);
        if (len > 4 && strcmp(ent->d_name + len - 4, DISK_EXT) == 0) {
            count++;
        }
    }

    closedir(dir);
    return count;
}

/**
 * Compute the total size of all .a1f files in the disk directory.
 */
static long total_disk_usage(void)
{
    DIR *dir = opendir(disk_dir);
    if (!dir) {
        return -1;
    }

    long total = 0;
    struct dirent *ent;
    char path[PATH_MAX];

    while ((ent = readdir(dir)) != NULL) {
        size_t len = strlen(ent->d_name);
        if (len > 4 && strcmp(ent->d_name + len - 4, DISK_EXT) == 0) {
            snprintf(path, sizeof(path), "%s/%s", disk_dir, ent->d_name);
            struct stat st;
            if (stat(path, &st) == 0) {
                total += st.st_size;
            }
        }
    }

    closedir(dir);
    return total;
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void disk_init(void)
{
    if (disk_initialized) {
        return;
    }

    const char *home = getenv("HOME");
    if (!home || home[0] == '\0') {
        LOG_ERROR(LOG_COMP_DISK, "HOME environment variable not set");
        return;
    }

    int n = snprintf(disk_dir, sizeof(disk_dir), "%s/.apple1emu/disks", home);
    if (n < 0 || (size_t)n >= sizeof(disk_dir)) {
        LOG_ERROR(LOG_COMP_DISK, "disk directory path too long");
        return;
    }

    if (!mkdir_p(disk_dir)) {
        LOG_ERROR(LOG_COMP_DISK, "failed to create disk directory: %s (%s)",
                  disk_dir, strerror(errno));
        return;
    }

    /* Verify we can access the directory. */
    if (access(disk_dir, R_OK | W_OK | X_OK) != 0) {
        LOG_ERROR(LOG_COMP_DISK, "insufficient permissions on disk directory: %s",
                  disk_dir);
        return;
    }

    disk_initialized = true;
    LOG_INFO(LOG_COMP_DISK, "virtual disk initialized at %s", disk_dir);
}

void disk_shutdown(void)
{
    if (!disk_initialized) {
        return;
    }

    disk_initialized = false;
    LOG_INFO(LOG_COMP_DISK, "virtual disk shut down");
}

bool disk_validate_name(const char *name)
{
    if (!name || name[0] == '\0') {
        return false;
    }

    size_t len = strlen(name);
    if (len > DISK_MAX_NAME_LEN) {
        return false;
    }

    /* Reject path separators and parent directory references. */
    if (strchr(name, '/') != NULL || strchr(name, '\\') != NULL) {
        return false;
    }
    if (strstr(name, "..") != NULL) {
        return false;
    }

    /* Validate each character: uppercase A-Z, digits 0-9, period only. */
    for (size_t i = 0; i < len; i++) {
        char c = name[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.')) {
            return false;
        }
    }

    return true;
}

const char *disk_get_dir(void)
{
    return disk_dir;
}

int disk_catalog(disk_entry_t *entries, int max_entries)
{
    if (!disk_initialized) {
        LOG_ERROR(LOG_COMP_DISK, "disk not initialized");
        return -1;
    }

    if (!entries || max_entries <= 0) {
        return -1;
    }

    DIR *dir = opendir(disk_dir);
    if (!dir) {
        LOG_ERROR(LOG_COMP_DISK, "failed to open disk directory: %s", strerror(errno));
        return -1;
    }

    int count = 0;
    struct dirent *ent;
    char path[PATH_MAX];

    while ((ent = readdir(dir)) != NULL && count < max_entries) {
        size_t namelen = strlen(ent->d_name);
        size_t extlen = strlen(DISK_EXT);

        /* Skip entries that don't end in .a1f */
        if (namelen <= extlen) {
            continue;
        }
        if (strcmp(ent->d_name + namelen - extlen, DISK_EXT) != 0) {
            continue;
        }

        snprintf(path, sizeof(path), "%s/%s", disk_dir, ent->d_name);

        /* Reject symlinks. */
        if (!check_not_symlink(path)) {
            continue;
        }

        FILE *fp = fopen(path, "rb");
        if (!fp) {
            continue;
        }

        char type;
        uint16_t load_addr, size;
        if (!read_header(fp, &type, &load_addr, &size)) {
            fclose(fp);
            continue;
        }
        fclose(fp);

        /* Extract the filename (without extension). */
        size_t base_len = namelen - extlen;
        if (base_len > DISK_MAX_NAME_LEN) {
            base_len = DISK_MAX_NAME_LEN;
        }

        memset(entries[count].name, 0, sizeof(entries[count].name));
        memcpy(entries[count].name, ent->d_name, base_len);
        entries[count].type = type;
        entries[count].size = size;
        entries[count].load_addr = load_addr;
        count++;
    }

    closedir(dir);
    return count;
}

bool disk_save_basic(const char *name, const uint8_t *data, uint16_t size)
{
    if (!disk_initialized) {
        LOG_ERROR(LOG_COMP_DISK, "disk not initialized");
        return false;
    }

    if (!disk_validate_name(name)) {
        LOG_ERROR(LOG_COMP_DISK, "invalid filename: '%s'", name ? name : "(null)");
        return false;
    }

    if (!data || size == 0) {
        LOG_ERROR(LOG_COMP_DISK, "no data to save");
        return false;
    }

    if (size > DISK_MAX_FILE_SIZE) {
        LOG_ERROR(LOG_COMP_DISK, "file too large: %u bytes (max %u)",
                  size, DISK_MAX_FILE_SIZE);
        return false;
    }

    /* Check file count limit (only if file doesn't already exist). */
    char path[PATH_MAX];
    if (!construct_path(name, path, sizeof(path))) {
        return false;
    }

    if (!check_not_symlink(path)) {
        return false;
    }

    struct stat st;
    if (stat(path, &st) != 0) {
        /* New file: check limits. */
        if (count_files() >= DISK_MAX_FILES) {
            LOG_ERROR(LOG_COMP_DISK, "disk full: maximum %d files", DISK_MAX_FILES);
            return false;
        }
        long usage = total_disk_usage();
        if (usage >= 0 && (usage + size + DISK_HEADER_SIZE) > DISK_DEFAULT_CAPACITY) {
            LOG_ERROR(LOG_COMP_DISK, "disk capacity exceeded");
            return false;
        }
    }

    FILE *fp = fopen(path, "wb");
    if (!fp) {
        LOG_ERROR(LOG_COMP_DISK, "failed to create file '%s': %s",
                  name, strerror(errno));
        return false;
    }

    if (!write_header(fp, DISK_TYPE_APPLESOFT, 0x0801, size)) {
        fclose(fp);
        return false;
    }

    if (fwrite(data, 1, size, fp) != size) {
        LOG_ERROR(LOG_COMP_DISK, "failed to write data for '%s'", name);
        fclose(fp);
        return false;
    }

    fclose(fp);
    LOG_INFO(LOG_COMP_DISK, "saved BASIC program '%s' (%u bytes)", name, size);
    return true;
}

bool disk_load_basic(const char *name, uint8_t *data, uint16_t *size)
{
    if (!disk_initialized) {
        LOG_ERROR(LOG_COMP_DISK, "disk not initialized");
        return false;
    }

    if (!disk_validate_name(name)) {
        LOG_ERROR(LOG_COMP_DISK, "invalid filename: '%s'", name ? name : "(null)");
        return false;
    }

    if (!data || !size) {
        LOG_ERROR(LOG_COMP_DISK, "null output parameters");
        return false;
    }

    char path[PATH_MAX];
    if (!construct_path(name, path, sizeof(path))) {
        return false;
    }

    if (!check_not_symlink(path)) {
        return false;
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        LOG_ERROR(LOG_COMP_DISK, "file not found: '%s'", name);
        return false;
    }

    char type;
    uint16_t load_addr, file_size;
    if (!read_header(fp, &type, &load_addr, &file_size)) {
        fclose(fp);
        return false;
    }

    if (type != DISK_TYPE_APPLESOFT && type != DISK_TYPE_INTEGER) {
        LOG_ERROR(LOG_COMP_DISK, "'%s' is not a BASIC program (type '%c')",
                  name, type);
        fclose(fp);
        return false;
    }

    if (file_size > DISK_MAX_FILE_SIZE) {
        LOG_ERROR(LOG_COMP_DISK, "file '%s' has invalid size %u", name, file_size);
        fclose(fp);
        return false;
    }

    if (fread(data, 1, file_size, fp) != file_size) {
        LOG_ERROR(LOG_COMP_DISK, "failed to read data from '%s'", name);
        fclose(fp);
        return false;
    }

    fclose(fp);
    *size = file_size;
    LOG_INFO(LOG_COMP_DISK, "loaded BASIC program '%s' (%u bytes)", name, file_size);
    return true;
}

bool disk_save_binary(const char *name, uint16_t addr, uint16_t size)
{
    if (!disk_initialized) {
        LOG_ERROR(LOG_COMP_DISK, "disk not initialized");
        return false;
    }

    if (!disk_validate_name(name)) {
        LOG_ERROR(LOG_COMP_DISK, "invalid filename: '%s'", name ? name : "(null)");
        return false;
    }

    if (size == 0) {
        LOG_ERROR(LOG_COMP_DISK, "no data to save");
        return false;
    }

    if (size > DISK_MAX_FILE_SIZE) {
        LOG_ERROR(LOG_COMP_DISK, "file too large: %u bytes (max %u)",
                  size, DISK_MAX_FILE_SIZE);
        return false;
    }

    char path[PATH_MAX];
    if (!construct_path(name, path, sizeof(path))) {
        return false;
    }

    if (!check_not_symlink(path)) {
        return false;
    }

    /* Check limits for new files. */
    struct stat st;
    if (stat(path, &st) != 0) {
        if (count_files() >= DISK_MAX_FILES) {
            LOG_ERROR(LOG_COMP_DISK, "disk full: maximum %d files", DISK_MAX_FILES);
            return false;
        }
        long usage = total_disk_usage();
        if (usage >= 0 && (usage + size + DISK_HEADER_SIZE) > DISK_DEFAULT_CAPACITY) {
            LOG_ERROR(LOG_COMP_DISK, "disk capacity exceeded");
            return false;
        }
    }

    FILE *fp = fopen(path, "wb");
    if (!fp) {
        LOG_ERROR(LOG_COMP_DISK, "failed to create file '%s': %s",
                  name, strerror(errno));
        return false;
    }

    if (!write_header(fp, DISK_TYPE_BINARY, addr, size)) {
        fclose(fp);
        return false;
    }

    /* Read directly from emulator memory. */
    uint8_t *mem = mem_get_ptr(addr);
    if (!mem) {
        LOG_ERROR(LOG_COMP_DISK, "invalid memory address $%04X", addr);
        fclose(fp);
        return false;
    }

    if (fwrite(mem, 1, size, fp) != size) {
        LOG_ERROR(LOG_COMP_DISK, "failed to write binary data for '%s'", name);
        fclose(fp);
        return false;
    }

    fclose(fp);
    LOG_INFO(LOG_COMP_DISK, "saved binary '%s' ($%04X, %u bytes)", name, addr, size);
    return true;
}

bool disk_load_binary(const char *name, uint16_t *addr, uint16_t *size)
{
    if (!disk_initialized) {
        LOG_ERROR(LOG_COMP_DISK, "disk not initialized");
        return false;
    }

    if (!disk_validate_name(name)) {
        LOG_ERROR(LOG_COMP_DISK, "invalid filename: '%s'", name ? name : "(null)");
        return false;
    }

    if (!addr || !size) {
        LOG_ERROR(LOG_COMP_DISK, "null output parameters");
        return false;
    }

    char path[PATH_MAX];
    if (!construct_path(name, path, sizeof(path))) {
        return false;
    }

    if (!check_not_symlink(path)) {
        return false;
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        LOG_ERROR(LOG_COMP_DISK, "file not found: '%s'", name);
        return false;
    }

    char type;
    uint16_t load_addr, file_size;
    if (!read_header(fp, &type, &load_addr, &file_size)) {
        fclose(fp);
        return false;
    }

    if (type != DISK_TYPE_BINARY) {
        LOG_ERROR(LOG_COMP_DISK, "'%s' is not a binary file (type '%c')",
                  name, type);
        fclose(fp);
        return false;
    }

    if (file_size > DISK_MAX_FILE_SIZE) {
        LOG_ERROR(LOG_COMP_DISK, "file '%s' has invalid size %u", name, file_size);
        fclose(fp);
        return false;
    }

    /* Load directly into emulator memory. */
    uint8_t *mem = mem_get_ptr(load_addr);
    if (!mem) {
        LOG_ERROR(LOG_COMP_DISK, "invalid load address $%04X", load_addr);
        fclose(fp);
        return false;
    }

    if (fread(mem, 1, file_size, fp) != file_size) {
        LOG_ERROR(LOG_COMP_DISK, "failed to read binary data from '%s'", name);
        fclose(fp);
        return false;
    }

    fclose(fp);
    *addr = load_addr;
    *size = file_size;
    LOG_INFO(LOG_COMP_DISK, "loaded binary '%s' at $%04X (%u bytes)",
             name, load_addr, file_size);
    return true;
}

bool disk_save_text(const char *name, const char *text, uint16_t size)
{
    if (!disk_initialized) {
        LOG_ERROR(LOG_COMP_DISK, "disk not initialized");
        return false;
    }

    if (!disk_validate_name(name)) {
        LOG_ERROR(LOG_COMP_DISK, "invalid filename: '%s'", name ? name : "(null)");
        return false;
    }

    if (!text || size == 0) {
        LOG_ERROR(LOG_COMP_DISK, "no data to save");
        return false;
    }

    if (size > DISK_MAX_FILE_SIZE) {
        LOG_ERROR(LOG_COMP_DISK, "file too large: %u bytes (max %u)",
                  size, DISK_MAX_FILE_SIZE);
        return false;
    }

    char path[PATH_MAX];
    if (!construct_path(name, path, sizeof(path))) {
        return false;
    }

    if (!check_not_symlink(path)) {
        return false;
    }

    /* Check limits for new files. */
    struct stat st;
    if (stat(path, &st) != 0) {
        if (count_files() >= DISK_MAX_FILES) {
            LOG_ERROR(LOG_COMP_DISK, "disk full: maximum %d files", DISK_MAX_FILES);
            return false;
        }
        long usage = total_disk_usage();
        if (usage >= 0 && (usage + size + DISK_HEADER_SIZE) > DISK_DEFAULT_CAPACITY) {
            LOG_ERROR(LOG_COMP_DISK, "disk capacity exceeded");
            return false;
        }
    }

    FILE *fp = fopen(path, "wb");
    if (!fp) {
        LOG_ERROR(LOG_COMP_DISK, "failed to create file '%s': %s",
                  name, strerror(errno));
        return false;
    }

    if (!write_header(fp, DISK_TYPE_TEXT, 0x0000, size)) {
        fclose(fp);
        return false;
    }

    if (fwrite(text, 1, size, fp) != size) {
        LOG_ERROR(LOG_COMP_DISK, "failed to write text data for '%s'", name);
        fclose(fp);
        return false;
    }

    fclose(fp);
    LOG_INFO(LOG_COMP_DISK, "saved text file '%s' (%u bytes)", name, size);
    return true;
}

bool disk_load_text(const char *name, char *text, uint16_t max_size,
                    uint16_t *actual_size)
{
    if (!disk_initialized) {
        LOG_ERROR(LOG_COMP_DISK, "disk not initialized");
        return false;
    }

    if (!disk_validate_name(name)) {
        LOG_ERROR(LOG_COMP_DISK, "invalid filename: '%s'", name ? name : "(null)");
        return false;
    }

    if (!text || !actual_size || max_size == 0) {
        LOG_ERROR(LOG_COMP_DISK, "invalid output parameters");
        return false;
    }

    char path[PATH_MAX];
    if (!construct_path(name, path, sizeof(path))) {
        return false;
    }

    if (!check_not_symlink(path)) {
        return false;
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        LOG_ERROR(LOG_COMP_DISK, "file not found: '%s'", name);
        return false;
    }

    char type;
    uint16_t load_addr, file_size;
    if (!read_header(fp, &type, &load_addr, &file_size)) {
        fclose(fp);
        return false;
    }

    if (type != DISK_TYPE_TEXT) {
        LOG_ERROR(LOG_COMP_DISK, "'%s' is not a text file (type '%c')", name, type);
        fclose(fp);
        return false;
    }

    uint16_t to_read = (file_size < max_size) ? file_size : max_size;

    if (fread(text, 1, to_read, fp) != to_read) {
        LOG_ERROR(LOG_COMP_DISK, "failed to read text data from '%s'", name);
        fclose(fp);
        return false;
    }

    fclose(fp);
    *actual_size = to_read;
    LOG_INFO(LOG_COMP_DISK, "loaded text file '%s' (%u bytes)", name, to_read);
    return true;
}

bool disk_delete(const char *name)
{
    if (!disk_initialized) {
        LOG_ERROR(LOG_COMP_DISK, "disk not initialized");
        return false;
    }

    if (!disk_validate_name(name)) {
        LOG_ERROR(LOG_COMP_DISK, "invalid filename: '%s'", name ? name : "(null)");
        return false;
    }

    char path[PATH_MAX];
    if (!construct_path(name, path, sizeof(path))) {
        return false;
    }

    if (!check_not_symlink(path)) {
        return false;
    }

    if (unlink(path) != 0) {
        LOG_ERROR(LOG_COMP_DISK, "failed to delete '%s': %s", name, strerror(errno));
        return false;
    }

    LOG_INFO(LOG_COMP_DISK, "deleted file '%s'", name);
    return true;
}

bool disk_rename(const char *old_name, const char *new_name)
{
    if (!disk_initialized) {
        LOG_ERROR(LOG_COMP_DISK, "disk not initialized");
        return false;
    }

    if (!disk_validate_name(old_name)) {
        LOG_ERROR(LOG_COMP_DISK, "invalid source filename: '%s'",
                  old_name ? old_name : "(null)");
        return false;
    }

    if (!disk_validate_name(new_name)) {
        LOG_ERROR(LOG_COMP_DISK, "invalid destination filename: '%s'",
                  new_name ? new_name : "(null)");
        return false;
    }

    char old_path[PATH_MAX];
    if (!construct_path(old_name, old_path, sizeof(old_path))) {
        return false;
    }

    char new_path[PATH_MAX];
    if (!construct_path(new_name, new_path, sizeof(new_path))) {
        return false;
    }

    if (!check_not_symlink(old_path)) {
        return false;
    }

    /* Check that destination doesn't already exist. */
    struct stat st;
    if (stat(new_path, &st) == 0) {
        LOG_ERROR(LOG_COMP_DISK, "destination '%s' already exists", new_name);
        return false;
    }

    if (rename(old_path, new_path) != 0) {
        LOG_ERROR(LOG_COMP_DISK, "failed to rename '%s' to '%s': %s",
                  old_name, new_name, strerror(errno));
        return false;
    }

    LOG_INFO(LOG_COMP_DISK, "renamed '%s' to '%s'", old_name, new_name);
    return true;
}

bool disk_exists(const char *name)
{
    if (!disk_initialized) {
        return false;
    }

    if (!disk_validate_name(name)) {
        return false;
    }

    char path[PATH_MAX];
    if (!construct_path(name, path, sizeof(path))) {
        return false;
    }

    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
}
