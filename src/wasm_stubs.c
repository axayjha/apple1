#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cpu.h"

// Stubs for modules that use filesystem/ncurses/terminal

// config.c stubs
void config_init(void) {}
int config_get_ram_kb(void) { return 32; }

// disk.c stubs
void disk_init(void) {}

// snapshot.c stubs
void snapshot_save(const char *filename, void *cpu) { (void)filename; (void)cpu; }
void snapshot_load(const char *filename, void *cpu) { (void)filename; (void)cpu; }

// aci.c stubs (uses filesystem for tapes)
void aci_init(void) {}
bool aci_is_trapped(uint16_t pc) { (void)pc; return false; }
void aci_handle_trap(cpu_state *cpu) { (void)cpu; }

// log.c - provide no-op stubs
void log_init(const char *path, int level) { (void)path; (void)level; }
void log_close(void) {}
void log_msg(int level, int component, const char *fmt, ...) { (void)level; (void)component; (void)fmt; }
int log_get_level(void) { return 0; }
void log_emit(int level, int component, const char *fmt, ...) { (void)level; (void)component; (void)fmt; }

