CC = cc
CFLAGS = -std=c17 -Wall -Wextra -Wpedantic -O2 -Isrc
LDFLAGS = -lncurses
DEBUG_CFLAGS = -std=c17 -Wall -Wextra -Wpedantic -g -O0 -fsanitize=address -Isrc
DEBUG_LDFLAGS = -lncurses -fsanitize=address

SRCS = src/main.c src/cpu.c src/memory.c src/pia.c src/display.c src/keyboard.c src/config.c src/log.c src/roms_builtin.c src/basic.c src/snapshot.c src/aci.c src/disk.c src/assembler.c
OBJS = $(SRCS:.c=.o)
TARGET = apple1emu

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

debug: CFLAGS = $(DEBUG_CFLAGS)
debug: LDFLAGS = $(DEBUG_LDFLAGS)
debug: clean $(TARGET)

TEST_CPU_SRCS = tests/test_cpu.c src/cpu.c
TEST_INT_SRCS = tests/test_integration.c src/cpu.c src/memory.c src/pia.c src/roms_builtin.c src/log.c
TEST_BASIC_SRCS = tests/test_basic.c src/basic.c src/cpu.c src/memory.c src/pia.c src/roms_builtin.c src/log.c
TEST_E2E_SRCS = tests/test_e2e.c src/cpu.c src/memory.c src/pia.c src/roms_builtin.c src/basic.c src/log.c src/aci.c src/disk.c

test: test_cpu test_integration test_basic test_e2e
	@echo "=== CPU Tests ==="
	./tests/test_cpu
	@echo ""
	@echo "=== Integration Tests ==="
	./tests/test_integration
	@echo ""
	@echo "=== BASIC Tests ==="
	./tests/test_basic
	@echo ""
	@echo "=== End-to-End Tests ==="
	./tests/test_e2e
	@echo ""
	@echo "All tests passed (220 total)."

test_cpu: $(TEST_CPU_SRCS)
	$(CC) $(CFLAGS) -o tests/test_cpu $(TEST_CPU_SRCS)

test_integration: $(TEST_INT_SRCS)
	$(CC) $(CFLAGS) -o tests/test_integration $(TEST_INT_SRCS)

test_basic: $(TEST_BASIC_SRCS)
	$(CC) $(CFLAGS) -o tests/test_basic $(TEST_BASIC_SRCS)

test_e2e: $(TEST_E2E_SRCS)
	$(CC) $(CFLAGS) -o tests/test_e2e $(TEST_E2E_SRCS)

clean:
	rm -f $(OBJS) $(TARGET) tests/test_cpu tests/test_integration tests/test_basic tests/test_e2e

install: $(TARGET)
	mkdir -p ~/.apple1emu/roms ~/.apple1emu/tapes ~/.apple1emu/disks ~/.apple1emu/snapshots ~/.apple1emu/logs
	cp $(TARGET) /usr/local/bin/

installer:
	@./tools/create-installer.sh

.PHONY: all debug test test_cpu test_integration test_basic test_e2e clean install installer
