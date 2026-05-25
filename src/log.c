#include "log.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

/* ---------- Internal state ---------- */

static atomic_int g_log_level = LOG_LEVEL_INFO;
static FILE      *g_log_file  = NULL;
static int        g_owns_file = 0;  /* non-zero if we opened it and must close */
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ---------- String tables ---------- */

static const char *level_strings[] = {
    [LOG_LEVEL_ERROR] = "ERROR",
    [LOG_LEVEL_WARN]  = "WARN",
    [LOG_LEVEL_INFO]  = "INFO",
    [LOG_LEVEL_DEBUG] = "DEBUG",
    [LOG_LEVEL_TRACE] = "TRACE"
};

static const char *component_strings[] = {
    [LOG_COMP_CPU]  = "CPU",
    [LOG_COMP_MEM]  = "MEM",
    [LOG_COMP_PIA]  = "PIA",
    [LOG_COMP_DSP]  = "DSP",
    [LOG_COMP_KBD]  = "KBD",
    [LOG_COMP_ACI]  = "ACI",
    [LOG_COMP_DISK] = "DISK",
    [LOG_COMP_FS]   = "FS",
    [LOG_COMP_EMU]  = "EMU"
};

/* ---------- Timestamp helper ---------- */

/*
 * Write an ISO 8601 timestamp with millisecond precision into buf.
 * buf must be at least 24 bytes (e.g., "2026-05-25T10:30:01.234\0").
 */
static void format_timestamp(char *buf, size_t bufsize)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    struct tm tm_info;
    localtime_r(&tv.tv_sec, &tm_info);

    /* Format: YYYY-MM-DDTHH:MM:SS */
    size_t n = strftime(buf, bufsize, "%Y-%m-%dT%H:%M:%S", &tm_info);

    /* Append milliseconds */
    int ms = (int)(tv.tv_usec / 1000);
    snprintf(buf + n, bufsize - n, ".%03d", ms);
}

/* ---------- Public API ---------- */

void log_init(log_level_t level, const char *filepath)
{
    atomic_store(&g_log_level, (int)level);

    pthread_mutex_lock(&g_log_mutex);

    /* Close any previously opened file. */
    if (g_owns_file && g_log_file != NULL) {
        fclose(g_log_file);
    }

    if (filepath != NULL) {
        g_log_file = fopen(filepath, "a");
        if (g_log_file == NULL) {
            /* Fall back to stderr on failure. */
            g_log_file = stderr;
            g_owns_file = 0;
            fprintf(stderr, "[LOG] Failed to open log file: %s\n", filepath);
        } else {
            g_owns_file = 1;
        }
    } else {
        g_log_file = stderr;
        g_owns_file = 0;
    }

    pthread_mutex_unlock(&g_log_mutex);
}

void log_shutdown(void)
{
    pthread_mutex_lock(&g_log_mutex);

    if (g_log_file != NULL) {
        fflush(g_log_file);
        if (g_owns_file) {
            fclose(g_log_file);
        }
    }
    g_log_file = NULL;
    g_owns_file = 0;

    pthread_mutex_unlock(&g_log_mutex);
}

void log_set_level(log_level_t level)
{
    atomic_store(&g_log_level, (int)level);
}

log_level_t log_get_level(void)
{
    return (log_level_t)atomic_load(&g_log_level);
}

void log_emit(log_level_t level, log_component_t comp,
              const char *fmt, ...)
{
    char timestamp[32];
    format_timestamp(timestamp, sizeof(timestamp));

    const char *level_str = (level <= LOG_LEVEL_TRACE)
                            ? level_strings[level] : "???";
    const char *comp_str  = (comp <= LOG_COMP_EMU)
                            ? component_strings[comp] : "???";

    pthread_mutex_lock(&g_log_mutex);

    FILE *out = (g_log_file != NULL) ? g_log_file : stderr;

    fprintf(out, "[%s] [%-5s] [%-4s] ", timestamp, level_str, comp_str);

    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);

    fputc('\n', out);
    fflush(out);

    pthread_mutex_unlock(&g_log_mutex);
}
