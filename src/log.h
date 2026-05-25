#ifndef APPLE1_LOG_H
#define APPLE1_LOG_H

#include <stdarg.h>

/*
 * Logging framework for Apple 1 emulator.
 *
 * Provides leveled, component-tagged logging with thread safety,
 * configurable output destination, and macro-based API that
 * short-circuits when the log level is below threshold.
 */

/* Log levels, ordered by severity (lowest value = most severe). */
typedef enum {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN  = 1,
    LOG_LEVEL_INFO  = 2,
    LOG_LEVEL_DEBUG = 3,
    LOG_LEVEL_TRACE = 4
} log_level_t;

/* Emulator components. */
typedef enum {
    LOG_COMP_CPU  = 0,
    LOG_COMP_MEM  = 1,
    LOG_COMP_PIA  = 2,
    LOG_COMP_DSP  = 3,
    LOG_COMP_KBD  = 4,
    LOG_COMP_ACI  = 5,
    LOG_COMP_DISK = 6,
    LOG_COMP_FS   = 7,
    LOG_COMP_EMU  = 8
} log_component_t;

/*
 * Initialize the logging subsystem.
 *
 * level    - minimum log level to emit.
 * filepath - path to log file, or NULL for stderr.
 */
void log_init(log_level_t level, const char *filepath);

/*
 * Shut down the logging subsystem: flush and close output.
 */
void log_shutdown(void);

/*
 * Change the minimum log level at runtime.
 */
void log_set_level(log_level_t level);

/*
 * Query the current minimum log level.
 */
log_level_t log_get_level(void);

/*
 * Internal emit function. Do not call directly; use the macros below.
 */
void log_emit(log_level_t level, log_component_t comp,
              const char *fmt, ...) __attribute__((format(printf, 3, 4)));

/*
 * Macro-based logging API.
 *
 * When APPLE1_LOG_DISABLE is defined at compile time, all logging
 * macros expand to nothing (zero cost).
 *
 * Otherwise, the macros perform a cheap level check before evaluating
 * any arguments, avoiding overhead when the message would be filtered.
 */
#ifdef APPLE1_LOG_DISABLE

#define LOG_ERROR(comp, ...) ((void)0)
#define LOG_WARN(comp, ...)  ((void)0)
#define LOG_INFO(comp, ...)  ((void)0)
#define LOG_DEBUG(comp, ...) ((void)0)
#define LOG_TRACE(comp, ...) ((void)0)

#else

#define LOG_ERROR(comp, ...) \
    do { \
        if (log_get_level() >= LOG_LEVEL_ERROR) \
            log_emit(LOG_LEVEL_ERROR, (comp), __VA_ARGS__); \
    } while (0)

#define LOG_WARN(comp, ...) \
    do { \
        if (log_get_level() >= LOG_LEVEL_WARN) \
            log_emit(LOG_LEVEL_WARN, (comp), __VA_ARGS__); \
    } while (0)

#define LOG_INFO(comp, ...) \
    do { \
        if (log_get_level() >= LOG_LEVEL_INFO) \
            log_emit(LOG_LEVEL_INFO, (comp), __VA_ARGS__); \
    } while (0)

#define LOG_DEBUG(comp, ...) \
    do { \
        if (log_get_level() >= LOG_LEVEL_DEBUG) \
            log_emit(LOG_LEVEL_DEBUG, (comp), __VA_ARGS__); \
    } while (0)

#define LOG_TRACE(comp, ...) \
    do { \
        if (log_get_level() >= LOG_LEVEL_TRACE) \
            log_emit(LOG_LEVEL_TRACE, (comp), __VA_ARGS__); \
    } while (0)

#endif /* APPLE1_LOG_DISABLE */

#endif /* APPLE1_LOG_H */
