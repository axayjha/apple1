# Apple 1 Emulator

A faithful terminal-based emulator of the 1976 Apple 1 computer, featuring the MOS 6502 CPU, Woz Monitor, Integer BASIC, and the green phosphor glow of an Apple Monitor II.

## Features

- **Authentic 6502 CPU** — All 151 official opcodes, BCD mode, correct cycle timing
- **Woz Monitor** — The original 256-byte ROM that started it all
- **Integer BASIC** — Compatible interpreter with full arithmetic, strings, arrays, control flow
- **6502 Mini-Assembler** — Write and assemble machine code interactively
- **Apple Monitor II Display** — Green phosphor with scanlines and glow effects
- **Virtual Cassette** — Save/load programs to virtual tape files
- **Virtual Disk** — Sandboxed filesystem for persistent storage
- **Machine Snapshots** — Save and restore complete emulator state
- **40/80 Column Modes** — Toggle between authentic and enhanced display

## Quick Start

```bash
make
./apple1emu
```

You'll see the Woz Monitor prompt (`\`). Type `E000R` to enter BASIC.

## Requirements

- macOS (ARM64 or x86_64)
- Xcode Command Line Tools (`xcode-select --install`)
- Terminal with color support

## Controls

| Key | Action |
|-----|--------|
| Ctrl+Q | Quit |
| Ctrl+R | Reset |
| Ctrl+F | Toggle fast mode |
| Ctrl+D | Debug overlay |
| Ctrl+E | Configuration |
| Ctrl+S | Save snapshot |
| Ctrl+L | Load snapshot |
| Ctrl+C | Break |

## Documentation

- [MANUAL.md](MANUAL.md) — Complete user manual
- [SPEC.md](SPEC.md) — Technical specification
- [demos/](demos/) — Example BASIC programs

## Testing

```bash
make test    # 180 automated tests (CPU, integration, BASIC)
```

## License

Educational/hobby project. Woz Monitor ROM is historically published in the Apple 1 Operation Manual.
