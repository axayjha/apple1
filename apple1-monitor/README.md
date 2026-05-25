# Apple 1 Monitor — Desktop App

A native macOS desktop app that wraps the Apple 1 emulator in a window styled like an Apple Monitor II — green phosphor CRT with bezel, scanlines, and glow effects.

Built with **Tauri 2** (Rust backend + web frontend).

## Architecture

```
src-tauri/
├── build.rs          Compiles the C emulator core into a static library
├── src/
│   ├── main.rs       Tauri app entry, IPC commands
│   └── emulator.rs   Rust FFI bridge to C emulator (cpu, pia, basic, etc.)
dist/
├── index.html        App shell
├── style.css         Monitor bezel styling (dark housing, rounded CRT)
└── main.js           Canvas renderer (phosphor glow, scanlines, vignette)
```

## Prerequisites

- Rust (install via `rustup`)
- Node.js (for Tauri CLI)
- Xcode Command Line Tools

## Build & Run

```bash
cd apple1-monitor
npm run tauri dev     # Development mode with hot reload
npm run tauri build   # Production .app bundle
```

Or directly via cargo:

```bash
cd apple1-monitor/src-tauri
cargo build --release
./target/release/apple1-monitor
```

## Features

- Full Apple 1 emulation (6502 CPU, Woz Monitor, Integer BASIC)
- CRT phosphor rendering (green glow, scanlines, vignette)
- Apple Monitor II bezel styling
- Keyboard input mapped to Apple 1 conventions
- 60fps render loop
- ~10MB app bundle (vs 100MB+ Electron)
