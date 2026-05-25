/*
 * main.c - Apple 1 Emulator entry point
 *
 * Ties together the CPU, memory, PIA, display, keyboard, and configuration
 * modules into a working emulation loop with real-time timing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <sys/stat.h>

#include "cpu.h"
#include "memory.h"
#include "pia.h"
#include "display.h"
#include "keyboard.h"
#include "config.h"
#include "log.h"
#include "roms_builtin.h"
#include "basic.h"
#include "snapshot.h"
#include "aci.h"
#include "disk.h"
#include "assembler.h"

#define VERSION_STRING "apple1emu 0.1.0"

/* Apple 1 clock frequency: 1.022727 MHz */
#define CPU_FREQ_HZ      1022727
#define TARGET_FPS        60
#define CYCLES_PER_FRAME  (CPU_FREQ_HZ / TARGET_FPS)  /* ~17045 */
#define FRAME_NS          (1000000000L / TARGET_FPS)   /* ~16.67ms */

/* Global emulator state */
static cpu_state   cpu;
static bool        quit          = false;
static bool        fast_mode     = false;
static bool        debug_overlay = false;
static bool        headless      = false;

/* Forward declarations */
static void parse_args(int argc, char **argv);
static void show_startup_screen(void);
static void handle_event(key_event_t ev);
static void render_debug_overlay(void);
static void print_usage(const char *progname);
static void print_version(void);
static int  parse_ram_size(const char *str);
static log_level_t parse_log_level(const char *str);

/* Frame timing helpers */
static void frame_sleep(struct timespec *frame_start);

int main(int argc, char **argv)
{
    config_t *cfg = config_get();

    /* Parse command-line arguments (may exit on --help/--version/--test) */
    parse_args(argc, argv);

    /* Initialize logging — when running with a display, log to file to avoid
     * corrupting the ncurses screen. In headless mode, log to stderr. */
    if (!headless) {
        char logpath[512];
        const char *home = getenv("HOME");
        if (home) {
            snprintf(logpath, sizeof(logpath), "%s/.apple1emu/logs", home);
            mkdir(logpath, 0755);
            snprintf(logpath, sizeof(logpath), "%s/.apple1emu/logs/emulator.log", home);
        } else {
            snprintf(logpath, sizeof(logpath), "/tmp/apple1emu.log");
        }
        log_init((log_level_t)cfg->log_level, logpath);
    } else {
        log_init((log_level_t)cfg->log_level, NULL);
    }
    LOG_INFO(LOG_COMP_EMU, "Starting %s", VERSION_STRING);
    LOG_INFO(LOG_COMP_EMU, "RAM: %d KB, Fast mode: %s",
             cfg->ram_kb, cfg->fast_mode ? "on" : "off");

    /* Initialize memory subsystem */
    mem_init(cfg->ram_kb);

    /* Initialize PIA */
    pia_init();

    /* Load built-in ROMs (Woz Monitor + BASIC stub) */
    roms_load_builtin();

    /* Initialize BASIC interpreter */
    basic_init();

    /* Initialize mini-assembler */
    asm_init();

    /* Initialize ACI (virtual cassette) */
    aci_init();

    /* Initialize virtual disk */
    disk_init();

    /* Initialize snapshot system */
    snapshot_init();

    /* Initialize display (unless headless) */
    if (!headless) {
        display_init();
        display_set_scanlines(cfg->scanlines);
        display_set_glow(cfg->phosphor_glow);
    }

    /* Initialize keyboard */
    keyboard_init();

    /* Register display callback with PIA */
    if (!headless) {
        pia_set_display_callback(display_putchar);
    }

    /* Show startup branding screen (unless headless) */
    if (!headless) {
        show_startup_screen();
    }

    /* Initialize and reset CPU */
    cpu_init(&cpu, mem_read, mem_write);
    cpu_reset(&cpu);

    LOG_INFO(LOG_COMP_EMU, "CPU reset complete, PC=$%04X", cpu.pc);

    /* Apply runtime config to local state */
    fast_mode     = cfg->fast_mode;
    debug_overlay = cfg->debug_overlay;

    /* Main emulation loop */
    struct timespec frame_start;

    while (!quit) {
        /* Record frame start time */
        if (!fast_mode) {
            clock_gettime(CLOCK_MONOTONIC, &frame_start);
        }

        /* Poll keyboard and handle all pending events */
        for (int ki = 0; ki < 16; ki++) {
            key_event_t ev = keyboard_read();
            if (ev.type == KEY_NONE) break;
            handle_event(ev);
        }

        /* Execute CPU cycles for one frame */
        int cycles_done = 0;
        while (cycles_done < CYCLES_PER_FRAME) {
            /* Check BASIC trap (PC at $E000 or $E2B3) */
            if (cpu.pc == BASIC_COLD_START_ADDR && !basic_is_running()) {
                basic_cold_start();
                cpu.pc = BASIC_COLD_START_ADDR + 3;
                break;
            }
            if (cpu.pc == BASIC_WARM_START_ADDR && !basic_is_running()) {
                basic_warm_start();
                cpu.pc = BASIC_WARM_START_ADDR + 3;
                break;
            }

            /* Check ACI trap */
            if (aci_is_trapped(cpu.pc)) {
                aci_handle_trap(&cpu);
                continue;
            }

            /* If BASIC is running, step it instead of the CPU */
            if (basic_is_running()) {
                basic_step();
                cycles_done += 50; /* Approximate cycles per BASIC step */
                continue;
            }

            cycles_done += cpu_step(&cpu);
        }

        /* Update display */
        if (!headless) {
            if (debug_overlay) {
                render_debug_overlay();
            }
            display_update();
        }

        /* Frame timing (unless fast mode) */
        if (!fast_mode) {
            frame_sleep(&frame_start);
        }
    }

    /* Cleanup */
    LOG_INFO(LOG_COMP_EMU, "Shutting down");

    if (!headless) {
        display_shutdown();
    }

    log_shutdown();

    return 0;
}

/*
 * Parse command-line arguments and apply to global config.
 */
static void parse_args(int argc, char **argv)
{
    config_t *cfg = config_get();

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-V") == 0) {
            print_version();
            exit(0);
        } else if (strcmp(argv[i], "--ram") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --ram requires a size argument (4k, 8k, 32k, 48k)\n");
                exit(1);
            }
            i++;
            int kb = parse_ram_size(argv[i]);
            if (kb < 0) {
                fprintf(stderr, "Error: invalid RAM size '%s' (use 4k, 8k, 32k, or 48k)\n", argv[i]);
                exit(1);
            }
            cfg->ram_kb = kb;
        } else if (strcmp(argv[i], "--fast") == 0) {
            cfg->fast_mode = true;
        } else if (strcmp(argv[i], "--enhanced") == 0) {
            cfg->enhanced_basic = true;
        } else if (strcmp(argv[i], "--no-glow") == 0) {
            cfg->phosphor_glow = false;
        } else if (strcmp(argv[i], "--no-scanlines") == 0) {
            cfg->scanlines = false;
        } else if (strcmp(argv[i], "--log") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --log requires a level (error, warn, info, debug, trace)\n");
                exit(1);
            }
            i++;
            log_level_t level = parse_log_level(argv[i]);
            if ((int)level < 0) {
                fprintf(stderr, "Error: invalid log level '%s'\n", argv[i]);
                exit(1);
            }
            cfg->log_level = (int)level;
        } else if (strcmp(argv[i], "--test") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --test requires a suite name\n");
                exit(1);
            }
            i++;
            printf("Tests not implemented: %s\n", argv[i]);
            exit(0);
        } else if (strcmp(argv[i], "--headless") == 0) {
            headless = true;
        } else {
            fprintf(stderr, "Error: unknown option '%s'\n", argv[i]);
            fprintf(stderr, "Try '%s --help' for usage information.\n", argv[0]);
            exit(1);
        }
    }
}

/*
 * Display a brief startup/branding screen before the Woz Monitor begins.
 * Renders an ASCII apple logo and system info, then waits up to 2 seconds
 * or until any key is pressed.
 */
static void show_startup_screen(void)
{
    config_t *cfg = config_get();

    static const char *lines[] = {
        "                                        ",
        "                                        ",
        "       A P P L E  1                     ",
        "       ============                     ",
        "                                        ",
        "       PERSONAL COMPUTER                ",
        "                                        ",
        "                                        ",
        NULL,  /* placeholder for RAM/SPEED line */
        "       MOS 6502 CPU @ 1.022 MHZ         ",
        "                                        ",
        "                                        ",
        "       APPLE 1 EMULATOR V1.0            ",
        "       (C) 2026                         ",
        "                                        ",
        "                                        ",
        "                                        ",
        "       PRESS ANY KEY                    ",
        NULL   /* sentinel */
    };

    /* Build the dynamic RAM info line */
    char ram_line[41];
    snprintf(ram_line, sizeof(ram_line),
             "       %dK RAM SYSTEM                    ", cfg->ram_kb);

    /* Write each line to the display */
    for (int i = 0; i < 18; i++) {
        const char *line = lines[i];
        if (i == 8) line = ram_line;
        if (!line) break;

        for (const char *p = line; *p != '\0'; p++) {
            display_putchar((uint8_t)*p);
        }
        display_putchar(0x0D);
    }

    /* Render the branding to screen */
    display_update();

    /* Wait up to 2 seconds or until a key is pressed */
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (;;) {
        /* Check for keypress */
        key_event_t ev = keyboard_read();
        if (ev.type != KEY_NONE) {
            if (ev.type == KEY_QUIT) {
                /* Honor quit even during splash */
                quit = true;
            }
            break;
        }

        /* Check elapsed time (2 seconds) */
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed_ms = (now.tv_sec - start.tv_sec) * 1000L
                        + (now.tv_nsec - start.tv_nsec) / 1000000L;
        if (elapsed_ms >= 2000) {
            break;
        }

        /* Brief sleep to avoid busy-waiting (~16ms) */
        struct timespec nap = { .tv_sec = 0, .tv_nsec = 16000000L };
        nanosleep(&nap, NULL);

        /* Keep display refreshed (cursor blink) */
        display_update();
    }

    /* Clear screen and prepare for Woz Monitor */
    display_clear();
    display_update();
}

/*
 * Handle a keyboard event from the host.
 */
static void handle_event(key_event_t ev)
{
    switch (ev.type) {
    case KEY_CHAR:
        if (basic_is_running()) {
            basic_input_char(ev.ch);
        } else {
            pia_key_press(ev.ch);
        }
        LOG_TRACE(LOG_COMP_KBD, "Key press: $%02X '%c'",
                  ev.ch, (ev.ch >= 0x20 && ev.ch < 0x7F) ? ev.ch : '.');
        break;

    case KEY_QUIT:
        LOG_INFO(LOG_COMP_EMU, "Quit requested");
        quit = true;
        break;

    case KEY_RESET:
        LOG_INFO(LOG_COMP_EMU, "Warm reset");
        cpu_reset(&cpu);
        break;

    case KEY_FAST_TOGGLE:
        fast_mode = !fast_mode;
        LOG_INFO(LOG_COMP_EMU, "Fast mode: %s", fast_mode ? "on" : "off");
        break;

    case KEY_DEBUG_TOGGLE:
        debug_overlay = !debug_overlay;
        LOG_INFO(LOG_COMP_EMU, "Debug overlay: %s", debug_overlay ? "on" : "off");
        break;

    case KEY_CONFIG:
        /* Placeholder: beep */
        putchar('\a');
        fflush(stdout);
        break;

    case KEY_BREAK:
        if (basic_is_running()) {
            basic_break();
        } else {
            LOG_DEBUG(LOG_COMP_EMU, "Break (IRQ)");
            cpu_irq(&cpu);
        }
        break;

    case KEY_COLD_RESET:
        LOG_INFO(LOG_COMP_EMU, "Cold reset");
        mem_init(config_get()->ram_kb);
        roms_load_builtin();
        cpu_reset(&cpu);
        break;

    case KEY_SNAPSHOT_SAVE:
        {
            char path[512];
            snprintf(path, sizeof(path), "%s/quick.a1ss", snapshot_get_dir());
            if (snapshot_save(path, &cpu)) {
                LOG_INFO(LOG_COMP_EMU, "Snapshot saved: %s", path);
            }
        }
        break;

    case KEY_SNAPSHOT_LOAD:
        {
            char path[512];
            snprintf(path, sizeof(path), "%s/quick.a1ss", snapshot_get_dir());
            if (snapshot_load(path, &cpu)) {
                LOG_INFO(LOG_COMP_EMU, "Snapshot loaded: %s", path);
            }
        }
        break;

    case KEY_NONE:
    case KEY_HELP:
    case KEY_PASTE:
        break;
    }
}

/*
 * Render a debug overlay showing CPU registers at the bottom of the screen.
 * Uses ncurses mvprintw directly (display module owns the ncurses window,
 * but we poke at it here for the overlay).
 */
static void render_debug_overlay(void)
{
    /*
     * For now, write debug info via the logging system.
     * A full ncurses overlay would require coordination with the display module.
     * This is a placeholder that logs register state each frame in trace mode.
     */
    LOG_TRACE(LOG_COMP_CPU,
              "A=%02X X=%02X Y=%02X SP=%02X PC=%04X P=%02X [%c%c%c%c%c%c%c%c] cyc=%llu",
              cpu.a, cpu.x, cpu.y, cpu.sp, cpu.pc, cpu.status,
              (cpu.status & CPU_FLAG_N) ? 'N' : '.',
              (cpu.status & CPU_FLAG_V) ? 'V' : '.',
              (cpu.status & CPU_FLAG_U) ? 'U' : '.',
              (cpu.status & CPU_FLAG_B) ? 'B' : '.',
              (cpu.status & CPU_FLAG_D) ? 'D' : '.',
              (cpu.status & CPU_FLAG_I) ? 'I' : '.',
              (cpu.status & CPU_FLAG_Z) ? 'Z' : '.',
              (cpu.status & CPU_FLAG_C) ? 'C' : '.',
              (unsigned long long)cpu.cycles);
}

/*
 * Sleep for the remainder of the frame to maintain ~60fps.
 */
static void frame_sleep(struct timespec *frame_start)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    long elapsed_ns = (now.tv_sec - frame_start->tv_sec) * 1000000000L
                    + (now.tv_nsec - frame_start->tv_nsec);

    long sleep_ns = FRAME_NS - elapsed_ns;

    if (sleep_ns > 0) {
        struct timespec ts;
        ts.tv_sec  = sleep_ns / 1000000000L;
        ts.tv_nsec = sleep_ns % 1000000000L;
        nanosleep(&ts, NULL);
    }
}

/*
 * Parse a RAM size string like "4k", "8k", "32k", "48k".
 * Returns the size in kilobytes, or -1 on error.
 */
static int parse_ram_size(const char *str)
{
    if (strcasecmp(str, "4k") == 0)  return 4;
    if (strcasecmp(str, "8k") == 0)  return 8;
    if (strcasecmp(str, "32k") == 0) return 32;
    if (strcasecmp(str, "48k") == 0) return 48;
    return -1;
}

/*
 * Parse a log level string.
 * Returns the level, or -1 cast to log_level_t on error.
 */
static log_level_t parse_log_level(const char *str)
{
    if (strcasecmp(str, "error") == 0) return LOG_LEVEL_ERROR;
    if (strcasecmp(str, "warn") == 0)  return LOG_LEVEL_WARN;
    if (strcasecmp(str, "info") == 0)  return LOG_LEVEL_INFO;
    if (strcasecmp(str, "debug") == 0) return LOG_LEVEL_DEBUG;
    if (strcasecmp(str, "trace") == 0) return LOG_LEVEL_TRACE;
    return (log_level_t)-1;
}

/*
 * Print usage information.
 */
static void print_usage(const char *progname)
{
    printf("Usage: %s [OPTIONS]\n", progname);
    printf("\n");
    printf("Apple 1 Emulator - MOS 6502 with Woz Monitor\n");
    printf("\n");
    printf("Options:\n");
    printf("  --ram SIZE        Set RAM size (4k, 8k, 32k, 48k) [default: 32k]\n");
    printf("  --fast            Disable timing delays (run at max speed)\n");
    printf("  --enhanced        Start in enhanced mode (Applesoft BASIC)\n");
    printf("  --no-glow         Disable phosphor glow effect\n");
    printf("  --no-scanlines    Disable scanline effect\n");
    printf("  --log LEVEL       Set log level (error, warn, info, debug, trace)\n");
    printf("  --test SUITE      Run test suite and exit\n");
    printf("  --headless        No display output (for testing)\n");
    printf("  --version, -V     Print version and exit\n");
    printf("  --help, -h        Print this help and exit\n");
    printf("\n");
    printf("Runtime controls:\n");
    printf("  Ctrl+Q            Quit emulator\n");
    printf("  Ctrl+R            Warm reset (re-enter Woz Monitor)\n");
    printf("  Ctrl+F            Toggle fast mode\n");
    printf("  Ctrl+D            Toggle debug overlay\n");
    printf("  Ctrl+E            Configuration menu\n");
    printf("  Ctrl+C            Break (generate IRQ)\n");
    printf("\n");
}

/*
 * Print version information.
 */
static void print_version(void)
{
    printf("%s\n", VERSION_STRING);
    printf("MOS 6502 emulator with Woz Monitor ROM\n");
    printf("Display: ncurses terminal (40/80 column)\n");
}
