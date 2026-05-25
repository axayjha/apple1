# APPLE 1 EMULATOR — USER MANUAL

```
                    ╔══════════════════════════════════════╗
                    ║       APPLE 1 EMULATOR v0.1.0       ║
                    ║   MOS 6502 @ 1.022727 MHz | macOS   ║
                    ╚══════════════════════════════════════╝
```

---

## Table of Contents

- [1. Introduction](#1-introduction)
  - [1.1 What This Emulator Does](#11-what-this-emulator-does)
  - [1.2 System Requirements](#12-system-requirements)
  - [1.3 Quick Start Guide](#13-quick-start-guide)
- [2. Installation](#2-installation)
  - [2.1 Building from Source](#21-building-from-source)
  - [2.2 First Run Setup](#22-first-run-setup)
  - [2.3 Command-Line Options](#23-command-line-options)
- [3. The Apple 1 Computer](#3-the-apple-1-computer)
  - [3.1 Brief History](#31-brief-history)
  - [3.2 Hardware Overview](#32-hardware-overview)
  - [3.3 Why It Matters](#33-why-it-matters)
- [4. Woz Monitor](#4-woz-monitor)
  - [4.1 Overview](#41-overview)
  - [4.2 Command Reference](#42-command-reference)
  - [4.3 Examining Memory](#43-examining-memory)
  - [4.4 Depositing Bytes](#44-depositing-bytes)
  - [4.5 Running Programs](#45-running-programs)
  - [4.6 Monitor Zero-Page Variables](#46-monitor-zero-page-variables)
  - [4.7 Tips and Tricks](#47-tips-and-tricks)
- [5. BASIC Programming](#5-basic-programming)
  - [5.1 Entering BASIC Mode](#51-entering-basic-mode)
  - [5.2 Immediate Mode vs Stored Programs](#52-immediate-mode-vs-stored-programs)
  - [5.3 Complete Keyword Reference](#53-complete-keyword-reference)
  - [5.4 Data Types](#54-data-types)
  - [5.5 Operators and Precedence](#55-operators-and-precedence)
  - [5.6 Control Flow](#56-control-flow)
  - [5.7 String Handling](#57-string-handling)
  - [5.8 Array Usage](#58-array-usage)
  - [5.9 Built-in Functions Reference](#59-built-in-functions-reference)
  - [5.10 Error Messages](#510-error-messages)
  - [5.11 Returning to Monitor](#511-returning-to-monitor)
  - [5.12 Example Programs](#512-example-programs)
- [6. Mini-Assembler](#6-mini-assembler)
  - [6.1 Entering the Assembler](#61-entering-the-assembler)
  - [6.2 Single-Line Mode](#62-single-line-mode)
  - [6.3 Multi-Line Mode with Labels](#63-multi-line-mode-with-labels)
  - [6.4 Supported Directives](#64-supported-directives)
  - [6.5 Number Formats](#65-number-formats)
  - [6.6 6502 Instruction Set Quick Reference](#66-6502-instruction-set-quick-reference)
  - [6.7 Example: A Simple Assembly Program](#67-example-a-simple-assembly-program)
- [7. Virtual Disk System](#7-virtual-disk-system)
  - [7.1 Overview](#71-overview)
  - [7.2 File Types](#72-file-types)
  - [7.3 Disk Commands](#73-disk-commands)
  - [7.4 File Location on Host](#74-file-location-on-host)
- [8. Virtual Cassette (ACI)](#8-virtual-cassette-aci)
  - [8.1 How the Cassette Interface Works](#81-how-the-cassette-interface-works)
  - [8.2 Saving Programs to Tape](#82-saving-programs-to-tape)
  - [8.3 Loading Programs from Tape](#83-loading-programs-from-tape)
  - [8.4 Tape File Location](#84-tape-file-location)
- [9. Snapshots (Save State)](#9-snapshots-save-state)
  - [9.1 Saving a Complete Machine State](#91-saving-a-complete-machine-state)
  - [9.2 Loading a Saved State](#92-loading-a-saved-state)
  - [9.3 Snapshot Files](#93-snapshot-files)
- [10. Configuration](#10-configuration)
  - [10.1 Configuration Menu](#101-configuration-menu)
  - [10.2 Settings Explained](#102-settings-explained)
  - [10.3 Configuration File Format](#103-configuration-file-format)
  - [10.4 Display Effects](#104-display-effects)
- [11. Keyboard Reference](#11-keyboard-reference)
  - [11.1 Full Keyboard Mapping](#111-full-keyboard-mapping)
  - [11.2 Control Key Combinations](#112-control-key-combinations)
  - [11.3 Apple 1 vs Modern Keyboard Differences](#113-apple-1-vs-modern-keyboard-differences)
  - [11.4 Special Emulator Controls](#114-special-emulator-controls)
- [12. Display](#12-display)
  - [12.1 Apple Monitor II Emulation](#121-apple-monitor-ii-emulation)
  - [12.2 Color Scheme and Phosphor Effects](#122-color-scheme-and-phosphor-effects)
  - [12.3 40-Column vs 80-Column Modes](#123-40-column-vs-80-column-modes)
  - [12.4 Character Set](#124-character-set)
- [13. Advanced Topics](#13-advanced-topics)
  - [13.1 Memory Map Diagram](#131-memory-map-diagram)
  - [13.2 Using PEEK and POKE](#132-using-peek-and-poke)
  - [13.3 Writing Machine Language Programs](#133-writing-machine-language-programs)
  - [13.4 Calling Machine Language from BASIC](#134-calling-machine-language-from-basic)
  - [13.5 Cassette Loading for Pre-Written Programs](#135-cassette-loading-for-pre-written-programs)
- [14. Demo Programs](#14-demo-programs)
  - [14.1 How to Enter Programs](#141-how-to-enter-programs)
  - [14.2 Demo Program Summary](#142-demo-program-summary)
  - [14.3 HELLO.BAS](#143-hellobas)
  - [14.4 FIBONACCI.BAS](#144-fibonaccibas)
  - [14.5 GUESS.BAS](#145-guessbas)
  - [14.6 SIEVE.BAS](#146-sievebas)
  - [14.7 ADVENTURE.BAS](#147-adventurebas)
  - [14.8 STAR.BAS](#148-starbas)
  - [14.9 SORT.BAS](#149-sortbas)
  - [14.10 CALENDAR.BAS](#1410-calendarbas)
  - [14.11 LUNARLANDER.BAS](#1411-lunarlanderbas)
  - [14.12 WUMPUS.BAS](#1412-wumpusbas)
  - [14.13 HAMURABI.BAS](#1413-hamurabibas)
  - [14.14 LIFE.BAS](#1414-lifebas)
  - [14.15 STARTREK.BAS](#1415-startrekbas)
  - [14.16 NIM.BAS](#1416-nimbas)
  - [14.17 MASTERMIND.BAS](#1417-mastermindbas)
  - [14.18 Loading Demos from Disk or Tape](#1418-loading-demos-from-disk-or-tape)
- [15. Troubleshooting](#15-troubleshooting)
  - [15.1 Common Issues and Solutions](#151-common-issues-and-solutions)
  - [15.2 Terminal Compatibility](#152-terminal-compatibility)
  - [15.3 Resetting if Things Go Wrong](#153-resetting-if-things-go-wrong)
- [Appendix A: 6502 Instruction Set](#appendix-a-6502-instruction-set)
- [Appendix B: Apple 1 Memory Map](#appendix-b-apple-1-memory-map)
- [Appendix C: BASIC Quick Reference Card](#appendix-c-basic-quick-reference-card)

---

## 1. Introduction

### 1.1 What This Emulator Does

The Apple 1 Emulator (`apple1emu`) is a faithful recreation of the original 1976 Apple 1 computer, running as a native terminal application on macOS. It emulates the complete hardware stack:

- **MOS 6502 CPU** at 1.022727 MHz with cycle-accurate instruction execution
- **Woz Monitor** — the original 256-byte system monitor
- **Integer BASIC** — Wozniak's hand-written BASIC interpreter
- **Apple Cassette Interface** — for saving and loading programs
- **PIA 6820** — the peripheral interface adapter handling keyboard and display

The emulator recreates the authentic experience of the green phosphor Apple Monitor II display, including character-by-character output timing, a blinking `@` cursor, and optional scanline effects. It also provides modern conveniences: a virtual disk system, machine state snapshots, an 80-column mode, and a built-in mini-assembler.

### 1.2 System Requirements

| Requirement | Minimum |
|-------------|---------|
| Operating System | macOS (ARM64 or x86_64) |
| Terminal | Any terminal supporting ANSI/256-color (Terminal.app, iTerm2, Alacritty, etc.) |
| Terminal Size | 80x24 characters minimum (40x24 for stock mode) |
| Compiler | Apple Clang (Xcode Command Line Tools) or GCC |
| Library | ncurses (system-provided on macOS) |
| Disk Space | ~2 MB for the binary and data |

### 1.3 Quick Start Guide

Get running in four steps:

```bash
# 1. Build the emulator
make

# 2. Run it
./apple1emu

# 3. You are now in Woz Monitor. Type this to enter BASIC:
E000R

# 4. Write your first program:
10 PRINT "HELLO WORLD"
20 GOTO 10
RUN
```

Press **Ctrl+C** to stop a running program. Press **Ctrl+Q** to quit the emulator.

---

## 2. Installation

### 2.1 Building from Source

Ensure you have the Xcode Command Line Tools installed:

```bash
xcode-select --install
```

Then build the emulator:

```bash
# Standard release build
make

# Build with debug symbols and address sanitizer
make debug

# Build and run all test suites
make test

# Remove build artifacts
make clean
```

The build produces a single binary called `apple1emu` in the project root.

To install system-wide:

```bash
sudo make install
```

This copies the binary to `/usr/local/bin/` and creates the data directory at `~/.apple1emu/`.

### 2.2 First Run Setup

On first launch, the emulator automatically creates its data directory structure:

```
~/.apple1emu/
├── config.ini          Configuration file
├── roms/               ROM images (built-in by default)
├── disks/              Virtual disk storage
├── tapes/              Virtual cassette files
├── snapshots/          Machine state saves
└── logs/               Debug and diagnostic logs
```

No manual setup is required. ROMs are compiled into the binary — external ROM files are optional and used only if you wish to substitute custom images.

### 2.3 Command-Line Options

| Option | Argument | Description |
|--------|----------|-------------|
| `--ram` | `4k`, `8k`, `32k`, `48k` | Set RAM size (default: 32k) |
| `--rom` | `DIR` | Custom ROM directory path |
| `--data` | `DIR` | Custom data directory (default: `~/.apple1emu`) |
| `--enhanced` | — | Start in enhanced mode (lowercase, 80-col, Applesoft) |
| `--fast` | — | Disable timing delays (maximum speed) |
| `--no-glow` | — | Disable phosphor glow effect |
| `--no-scanlines` | — | Disable scanline effect |
| `--load` | `FILE` | Load virtual tape on startup |
| `--snapshot` | `FILE` | Restore from saved snapshot |
| `--log` | `LEVEL` | Set log level: `error`, `warn`, `info`, `debug`, `trace` |
| `--test` | `SUITE` | Run test suite and exit (`all`, `cpu`, `basic`, `disk`, `sandbox`) |
| `--headless` | — | No display output (for testing/scripting) |
| `--version` | — | Print version string and exit |
| `--help` | — | Print usage information and exit |

**Examples:**

```bash
# Start with 48KB RAM and fast mode
./apple1emu --ram 48k --fast

# Start in full enhanced mode
./apple1emu --enhanced

# Load a tape file on startup
./apple1emu --load mytape.a1t

# Restore a previous session
./apple1emu --snapshot ~/.apple1emu/snapshots/session1.a1s

# Run CPU validation tests
./apple1emu --test cpu
```

---

## 3. The Apple 1 Computer

### 3.1 Brief History

The Apple 1 was designed and hand-built by Steve Wozniak in 1976. It was the first product sold by Apple Computer Company (founded by Wozniak, Steve Jobs, and Ronald Wayne on April 1, 1976).

Unlike other personal computers of the era, the Apple 1 was sold as a fully assembled circuit board — though buyers still needed to provide their own case, power supply, keyboard, and display. It retailed for $666.66 at the Byte Shop in Mountain View, California.

Only about 200 units were produced. Approximately 80 are known to survive today, with working units selling at auction for hundreds of thousands of dollars.

### 3.2 Hardware Overview

The original Apple 1 hardware consisted of:

| Component | Specification |
|-----------|---------------|
| CPU | MOS Technology 6502 @ 1.022727 MHz |
| RAM | 4 KB (expandable to 8 KB on-board) |
| ROM | 256 bytes (Woz Monitor at $FF00-$FFFF) |
| I/O | Motorola 6820 PIA (keyboard + display) |
| Display | 40 columns x 24 rows, uppercase only |
| Keyboard | ASCII keyboard (no built-in — user-supplied) |
| Storage | Optional Apple Cassette Interface ($75 add-on) |
| Bus | Edge connector for expansion |

The system had no built-in BASIC — it was loaded from cassette tape. The only software in ROM was the 256-byte Woz Monitor, which allowed users to examine and modify memory, and execute programs.

### 3.3 Why It Matters

The Apple 1 holds a unique place in computing history:

- **First personal computer sold as an assembled board** — previous kits (like the Altair 8800) required extensive assembly
- **First Apple product** — the seed that grew into the most valuable company in the world
- **Wozniak's masterpiece of minimalism** — the entire monitor ROM is only 256 bytes, yet provides a complete machine-language programming environment
- **Predecessor to the Apple II** — many Apple 1 design decisions carried forward into the far more successful Apple II (1977)
- **Cultural artifact** — surviving units are among the most valuable vintage computers in existence

---

## 4. Woz Monitor

### 4.1 Overview

The Woz Monitor is the system firmware residing in the top 256 bytes of memory ($FF00-$FFFF). It is the first thing you see when the Apple 1 powers on — a simple backslash (`\`) prompt awaiting your commands.

Written by Steve Wozniak, the monitor allows you to:
- Examine (read) any memory location
- Deposit (write) bytes to any memory location
- Run (execute) code at any address
- Load programs byte-by-byte

The monitor operates entirely in hexadecimal. All addresses are 4 hex digits; all data values are 2 hex digits.

### 4.2 Command Reference

| Command | Syntax | Description | Example |
|---------|--------|-------------|---------|
| Examine | `ADDR` | Display byte at address | `FF00` |
| Range | `ADDR1.ADDR2` | Display bytes from ADDR1 to ADDR2 | `FF00.FF0F` |
| Deposit | `ADDR:VAL` | Store a byte at address | `0300:A9` |
| Sequential Deposit | `:VAL VAL ...` | Continue storing at next addresses | `:00 8D 12 D0` |
| Run | `ADDRR` | Jump to address and execute | `0300R` |
| Multiple Examine | `ADDR1 ADDR2` | Examine several addresses | `FF00 FFFC FFFE` |

**Important notes:**
- All input is in hexadecimal (no `$` prefix needed)
- The monitor prompt is a backslash character: `\`
- Press **Return** to execute a command
- Press **Escape** to cancel the current input line
- Backspace removes from the input buffer but does not erase from the display (authentic behavior)

### 4.3 Examining Memory

To look at what is stored in memory, simply type the address:

```
\FF00
FF00: D8
```

To examine a range of memory, use the dot notation:

```
\FF00.FF0F
FF00: D8 58 A0 7F 8C 12 D0 A9
FF08: A7 8D 11 D0 8D 13 D0 C9
```

The display shows 8 bytes per line, with the starting address on the left.

To examine multiple separate locations:

```
\FFFC FFFE
FFFC: 00 FF
FFFE: 00 FF
```

### 4.4 Depositing Bytes

To write a single byte:

```
\0300:A9
```

This stores the value $A9 (the LDA immediate opcode) at address $0300.

To continue depositing at sequential addresses, use the colon-less form:

```
\0300:A9 41 8D 12 D0 4C 00 03
```

This writes 8 bytes starting at $0300. You can also split across lines:

```
\0300:A9 41
\:8D 12 D0
\:4C 00 03
```

The colon on a line by itself continues depositing from where the last deposit ended.

### 4.5 Running Programs

To execute code at an address, append `R` to the address:

```
\0300R
```

This sets the program counter to $0300 and begins execution. The CPU will run until it encounters a BRK instruction ($00) or you press **Ctrl+R** to reset.

**Common entry points:**

| Address | What It Does |
|---------|--------------|
| `E000R` | Enter Integer BASIC (cold start) |
| `E2B3R` | Re-enter BASIC (warm start, preserves program) |
| `FF00R` | Return to Woz Monitor |
| `C100R` | Save to cassette |
| `C108R` | Load from cassette |

### 4.6 Monitor Zero-Page Variables

The Woz Monitor uses a few zero-page locations for its operation:

| Address | Name | Purpose |
|---------|------|---------|
| $24 | XAML | Examine address (low byte) |
| $25 | XAMH | Examine address (high byte) |
| $26 | STL | Store address (low byte) |
| $27 | STH | Store address (high byte) |
| $28 | L | Hex parsing low byte |
| $29 | H | Hex parsing high byte |
| $2A | YSAV | Saved Y register |
| $2B | MODE | Current mode ($00=XAM, $74=STORE, $B8=BLOCK XAM) |

### 4.7 Tips and Tricks

- **Enter BASIC quickly:** Type `E000R` at the monitor prompt.
- **Return to monitor from anywhere:** Press **Ctrl+R** for a warm reset.
- **Verify a deposit:** After depositing bytes, examine the same range to confirm.
- **Type a short program manually:**
  ```
  \0300:A9 48 20 EF FF A9 49 20 EF FF 60
  \0300R
  ```
  (This prints "HI" using the monitor ECHO subroutine at $FFEF.)
- **Abort a long memory dump:** Press **Escape** or **Ctrl+R**.
- **Check the reset vector:** `\FFFC` shows where the CPU starts on reset.

---

## 5. BASIC Programming

### 5.1 Entering BASIC Mode

From the Woz Monitor prompt, type:

```
\E000R
```

BASIC will initialize and display its prompt character (`>`). You are now in Integer BASIC.

To re-enter BASIC without losing your current program (warm start):

```
\E2B3R
```

### 5.2 Immediate Mode vs Stored Programs

**Immediate mode:** Type a statement without a line number. It executes immediately:

```
>PRINT 2+2
4
>PRINT "HELLO"
HELLO
```

**Stored programs:** Type a statement with a line number. It is stored for later execution:

```
>10 PRINT "HELLO WORLD"
>20 GOTO 10
>RUN
HELLO WORLD
HELLO WORLD
HELLO WORLD
...
```

Press **Ctrl+C** to break out of a running program.

### 5.3 Complete Keyword Reference

Listed alphabetically:

| Keyword | Description |
|---------|-------------|
| `ABS` | Absolute value function |
| `AND` | Logical/bitwise AND operator |
| `ASC` | ASCII value of first character in string |
| `AUTO` | Auto-number lines (AUTO start, increment) |
| `CALL` | Call machine language subroutine |
| `CHR$` | Character from ASCII value |
| `CLR` | Clear all variables (keep program) |
| `COLOR=` | Set lo-res graphics color (0-15) |
| `CON` | Continue execution after STOP |
| `DEL` | Delete program lines (DEL start,end) |
| `DIM` | Dimension an array |
| `DSP` | Display variable on each assignment |
| `END` | End program execution |
| `FOR` | Begin FOR/NEXT loop |
| `GOSUB` | Call subroutine at line number |
| `GOTO` | Unconditional branch to line number |
| `GR` | Switch to lo-res graphics mode |
| `HLIN` | Draw horizontal line (lo-res) |
| `IF` | Conditional execution |
| `INPUT` | Read user input into variable |
| `LEFT$` | Left substring |
| `LEN` | Length of a string |
| `LET` | Variable assignment (optional keyword) |
| `LIST` | Display program listing |
| `LOAD` | Load program from disk |
| `MID$` | Middle substring |
| `MOD` | Modulo operator |
| `NEW` | Erase current program and variables |
| `NEXT` | End of FOR/NEXT loop |
| `NOT` | Logical NOT operator |
| `NOTRACE` | Disable line tracing |
| `OR` | Logical/bitwise OR operator |
| `PEEK` | Read byte from memory address |
| `PLOT` | Plot a point (lo-res) |
| `POKE` | Write byte to memory address |
| `PRINT` | Output to display |
| `REM` | Remark (comment) |
| `RETURN` | Return from GOSUB subroutine |
| `RIGHT$` | Right substring |
| `RND` | Random number |
| `RUN` | Execute stored program |
| `SAVE` | Save program to disk |
| `SCRN(` | Read lo-res screen color at position |
| `SGN` | Sign function (-1, 0, 1) |
| `STEP` | Increment for FOR/NEXT |
| `STOP` | Stop execution (resumable with CON) |
| `STR$` | Convert number to string |
| `TAB` | Advance cursor to column |
| `TEXT` | Switch to text mode from graphics |
| `THEN` | Part of IF/THEN construct |
| `TO` | Part of FOR/TO construct |
| `TRACE` | Enable line tracing (show line numbers as they execute) |
| `VAL` | Convert string to number |
| `VLIN` | Draw vertical line (lo-res) |

### 5.4 Data Types

**Integers:**
- Range: -32767 to 32767 (16-bit signed)
- All arithmetic is integer-only; division truncates

**Strings:**
- Maximum 255 characters per string
- String variables are named A$ through Z$
- Strings must be dimensioned before use: `DIM A$(50)`
- Substring access uses the form: `A$(start, end)`

**Variables:**
- Single-letter names: A through Z for integers
- A$ through Z$ for strings
- All 26 variables are pre-allocated (no declaration needed for integers)

### 5.5 Operators and Precedence

From highest to lowest precedence:

| Precedence | Operators | Description |
|------------|-----------|-------------|
| 1 | `( )` | Parentheses (grouping) |
| 2 | `-` (unary) | Negation |
| 3 | `^` | Exponentiation |
| 4 | `*  /  MOD` | Multiplication, division, modulo |
| 5 | `+  -` | Addition, subtraction |
| 6 | `=  <>  <  >  <=  >=` | Comparison (returns 0 or 1) |
| 7 | `NOT` | Logical NOT |
| 8 | `AND` | Logical AND |
| 9 | `OR` | Logical OR |

**Examples:**

```basic
>PRINT 3 + 4 * 2
11
>PRINT (3 + 4) * 2
14
>PRINT 17 MOD 5
2
>PRINT 2 ^ 10
1024
```

### 5.6 Control Flow

**IF/THEN:**

```basic
10 INPUT "ENTER A NUMBER: ";N
20 IF N > 100 THEN PRINT "BIG"
30 IF N <= 100 THEN PRINT "SMALL"
```

THEN can be followed by a statement or a line number:

```basic
20 IF N > 100 THEN 50
```

**FOR/NEXT:**

```basic
10 FOR I = 1 TO 10
20 PRINT I
30 NEXT I
```

With STEP:

```basic
10 FOR I = 10 TO 0 STEP -1
20 PRINT I
30 NEXT I
40 PRINT "LIFTOFF!"
```

**GOTO:**

```basic
10 PRINT "FOREVER"
20 GOTO 10
```

**GOSUB/RETURN:**

```basic
10 GOSUB 100
20 GOSUB 100
30 END
100 PRINT "IN SUBROUTINE"
110 RETURN
```

### 5.7 String Handling

Strings in Integer BASIC use an array-based model:

```basic
10 DIM A$(20)
20 A$ = "HELLO WORLD"
30 PRINT A$
40 PRINT LEN(A$)
50 PRINT A$(1,5)
```

Output:
```
HELLO WORLD
11
HELLO
```

**String operations:**

| Function | Description | Example |
|----------|-------------|---------|
| `LEN(A$)` | Length of string | `LEN("HI")` returns 2 |
| `A$(S,E)` | Substring from S to E | `A$(1,3)` = first 3 chars |
| `LEFT$(A$,N)` | First N characters | `LEFT$("HELLO",3)` = "HEL" |
| `RIGHT$(A$,N)` | Last N characters | `RIGHT$("HELLO",3)` = "LLO" |
| `MID$(A$,S,N)` | N chars starting at S | `MID$("HELLO",2,3)` = "ELL" |
| `ASC(A$)` | ASCII code of first char | `ASC("A")` returns 65 |
| `CHR$(N)` | Character from ASCII code | `CHR$(65)` returns "A" |
| `STR$(N)` | Number to string | `STR$(42)` returns "42" |
| `VAL(A$)` | String to number | `VAL("42")` returns 42 |

**String concatenation** is not directly supported. To build strings, use character-by-character assignment or PRINT multiple values:

```basic
10 PRINT "HELLO" " " "WORLD"
```

### 5.8 Array Usage

Arrays must be dimensioned before use:

```basic
10 DIM A(100)
20 FOR I = 1 TO 100
30 A(I) = I * I
40 NEXT I
50 PRINT A(10)
```

Output: `100`

Arrays are one-dimensional only. Index starts at 0 (or 1, depending on usage). Maximum dimension is limited by available RAM.

### 5.9 Built-in Functions Reference

| Function | Arguments | Returns | Description |
|----------|-----------|---------|-------------|
| `ABS(N)` | Integer | Integer | Absolute value |
| `SGN(N)` | Integer | -1, 0, 1 | Sign of number |
| `RND(N)` | Integer | Integer | Random number from 0 to N-1 |
| `PEEK(A)` | Address | 0-255 | Read byte from memory address A |
| `LEN(S$)` | String | Integer | Length of string |
| `ASC(S$)` | String | 0-127 | ASCII value of first character |
| `SCRN(X,Y)` | Coords | 0-15 | Lo-res screen color at X,Y |

**PEEK and POKE:**

```basic
>PRINT PEEK(65280)
```

This reads the byte at address $FF00 (65280 decimal = $FF00 hex).

```basic
>POKE 768, 169
```

This writes the value 169 ($A9) to address 768 ($0300).

### 5.10 Error Messages

| Error | Meaning |
|-------|---------|
| `*** SYNTAX ERR` | Unrecognized keyword or malformed statement |
| `*** OVERFLOW ERR` | Number exceeds -32767 to 32767 range |
| `*** OUT OF MEM ERR` | Program or variables exceed available RAM |
| `*** BAD SUBSCRIPT ERR` | Array index out of DIM range |
| `*** REDIMD ARRAY ERR` | Array already dimensioned |
| `*** UNDEF STATEMENT ERR` | GOTO/GOSUB to non-existent line |
| `*** BAD RETURN ERR` | RETURN without matching GOSUB |
| `*** BAD NEXT ERR` | NEXT without matching FOR |
| `*** STOPPED AT LINE nnn` | Program halted by STOP statement |
| `*** TYPE MISMATCH ERR` | String used where number expected, or vice versa |
| `*** STRING TOO LONG ERR` | String exceeds DIM allocation |
| `*** DIVISION BY ZERO ERR` | Division or MOD by zero |
| `*** ILLEGAL QUANTITY ERR` | Argument out of valid range |

When an error occurs in a running program, the line number is displayed:

```
*** SYNTAX ERR AT LINE 50
```

### 5.11 Returning to Monitor

To exit BASIC and return to the Woz Monitor:

```basic
>CALL -151
```

The value -151 is a two's-complement shortcut for $FF69 (a location that jumps to the monitor). Alternatively, press **Ctrl+R** for a warm reset, which returns directly to the monitor prompt.

**Note:** Returning to the monitor does not erase your BASIC program from memory. You can re-enter BASIC with `E2B3R` (warm start) to resume where you left off.

### 5.12 Example Programs

**Hello World:**

```basic
10 PRINT "HELLO, WORLD!"
```

**Fibonacci Sequence:**

```basic
10 A = 0
20 B = 1
30 FOR I = 1 TO 20
40 PRINT A
50 C = A + B
60 A = B
70 B = C
80 NEXT I
```

**Number Guessing Game:**

```basic
10 N = RND(100) + 1
20 PRINT "I'M THINKING OF A NUMBER 1-100"
30 INPUT "YOUR GUESS? ";G
40 IF G = N THEN PRINT "CORRECT!": END
50 IF G < N THEN PRINT "TOO LOW"
60 IF G > N THEN PRINT "TOO HIGH"
70 GOTO 30
```

**Multiplication Table:**

```basic
10 FOR I = 1 TO 12
20 FOR J = 1 TO 12
30 PRINT TAB(J*4); I*J;
40 NEXT J
50 PRINT
60 NEXT I
```

**Sieve of Eratosthenes:**

```basic
10 DIM A(200)
20 FOR I = 2 TO 200
30 A(I) = 1
40 NEXT I
50 FOR I = 2 TO 14
60 IF A(I) = 0 THEN 100
70 FOR J = I*I TO 200 STEP I
80 A(J) = 0
90 NEXT J
100 NEXT I
110 FOR I = 2 TO 200
120 IF A(I) = 1 THEN PRINT I;" ";
130 NEXT I
```

---

## 6. Mini-Assembler

### 6.1 Entering the Assembler

The built-in mini-assembler allows you to write 6502 assembly language programs directly within the emulator. To enter the assembler from the Woz Monitor:

```
\F666R
```

Or from BASIC:

```basic
>CALL -2458
```

The assembler prompt is displayed as `!` to distinguish it from the monitor's `\` prompt.

To exit the assembler and return to the monitor, type an empty line or press **Escape**.

### 6.2 Single-Line Mode

In single-line mode, you type one instruction at a time with its target address:

```
!0300: LDA #$41
!0302: STA $D012
!0305: JMP $0300
```

Each line specifies the address followed by a colon, then the instruction. The assembler shows the assembled bytes and advances to the next address automatically:

```
!0300: LDA #$41        A9 41
!0302: STA $D012       8D 12 D0
!0305: JMP $0300       4C 00 03
```

### 6.3 Multi-Line Mode with Labels

For longer programs, use the multi-line mode with labels and directives:

```
!.ORG $0300
!
!LOOP   LDA #$48
!       JSR $FFEF
!       LDA #$49
!       JSR $FFEF
!       JMP LOOP
!.END
```

Labels are defined by placing a name at the beginning of a line (no whitespace before it). They can be referenced in operands:

```
!.ORG $0300
!
!START  LDX #$00
!LOOP   LDA MSG,X
!       BEQ DONE
!       STA $D012
!WAIT   LDA $D012
!       BMI WAIT
!       INX
!       BNE LOOP
!DONE   RTS
!
!MSG    .TEXT "HELLO"
!       .BYTE $00
!.END
```

### 6.4 Supported Directives

| Directive | Description | Example |
|-----------|-------------|---------|
| `.ORG addr` | Set assembly origin address | `.ORG $0300` |
| `.BYTE val [,val...]` | Emit literal byte(s) | `.BYTE $00, $FF, 42` |
| `.WORD val [,val...]` | Emit 16-bit word(s), little-endian | `.WORD $FF00, LOOP` |
| `.TEXT "string"` | Emit ASCII string bytes | `.TEXT "HELLO"` |
| `.END` | End of source, assemble and deposit | `.END` |

### 6.5 Number Formats

The assembler accepts numbers in several formats:

| Format | Prefix | Example | Value |
|--------|--------|---------|-------|
| Hexadecimal | `$` | `$FF` | 255 |
| Binary | `%` | `%10101010` | 170 |
| Decimal | (none) | `42` | 42 |
| Character | `'` | `'A` | 65 |

**Addressing mode syntax:**

| Mode | Syntax | Example |
|------|--------|---------|
| Immediate | `#value` | `LDA #$41` |
| Zero Page | `$ZZ` | `LDA $00` |
| Zero Page,X | `$ZZ,X` | `LDA $10,X` |
| Zero Page,Y | `$ZZ,Y` | `LDX $10,Y` |
| Absolute | `$HHHH` | `LDA $FF00` |
| Absolute,X | `$HHHH,X` | `LDA $1000,X` |
| Absolute,Y | `$HHHH,Y` | `LDA $1000,Y` |
| Indirect | `($HHHH)` | `JMP ($FFFC)` |
| (Indirect,X) | `($ZZ,X)` | `LDA ($10,X)` |
| (Indirect),Y | `($ZZ),Y` | `LDA ($10),Y` |
| Accumulator | `A` | `ROL A` |
| Implied | (none) | `RTS` |
| Relative | label or `$HHHH` | `BNE LOOP` |

### 6.6 6502 Instruction Set Quick Reference

**Load/Store:**
`LDA`, `LDX`, `LDY`, `STA`, `STX`, `STY`

**Arithmetic:**
`ADC`, `SBC`, `INC`, `DEC`, `INX`, `INY`, `DEX`, `DEY`

**Logic:**
`AND`, `ORA`, `EOR`

**Shift/Rotate:**
`ASL`, `LSR`, `ROL`, `ROR`

**Compare:**
`CMP`, `CPX`, `CPY`

**Branch:**
`BCC`, `BCS`, `BEQ`, `BNE`, `BMI`, `BPL`, `BVC`, `BVS`

**Jump/Call:**
`JMP`, `JSR`, `RTS`, `RTI`, `BRK`

**Stack:**
`PHA`, `PLA`, `PHP`, `PLP`

**Status Flags:**
`CLC`, `SEC`, `CLD`, `SED`, `CLI`, `SEI`, `CLV`

**Transfer:**
`TAX`, `TAY`, `TXA`, `TYA`, `TSX`, `TXS`

**Other:**
`NOP`, `BIT`

### 6.7 Example: A Simple Assembly Program

This program prints "APPLE 1" to the display:

```
!.ORG $0300
!
!       LDX #$00       ; STRING INDEX
!LOOP   LDA MSG,X      ; GET NEXT CHARACTER
!       BEQ DONE       ; ZERO = END OF STRING
!       STA $D012      ; WRITE TO DISPLAY
!WAIT   LDA $D012      ; CHECK DISPLAY READY
!       BMI WAIT       ; WAIT IF BUSY (BIT 7 SET)
!       INX            ; NEXT CHARACTER
!       BNE LOOP       ; CONTINUE (MAX 256 CHARS)
!DONE   RTS            ; RETURN TO CALLER
!
!MSG    .TEXT "APPLE 1"
!       .BYTE $0D      ; CARRIAGE RETURN
!       .BYTE $00      ; NULL TERMINATOR
!.END
```

After `.END` assembles the code, run it from the monitor:

```
\0300R
APPLE 1
\
```

---

## 7. Virtual Disk System

### 7.1 Overview

The virtual disk system provides persistent file storage accessible from within the emulator. It emulates a simplified flat filesystem inspired by Apple DOS 3.3/ProDOS. All files are stored safely within the emulator's sandboxed data directory — there is no access to the host macOS filesystem.

**Constraints:**
- Filenames: maximum 15 characters
- Allowed characters: uppercase A-Z, digits 0-9, and period (.)
- Maximum 256 files per disk
- Maximum file size: 32 KB per file
- Total disk capacity: 140 KB (matching the original Apple II floppy disk)

### 7.2 File Types

| Code | Type | Description |
|------|------|-------------|
| `I` | Integer BASIC | Tokenized Integer BASIC program |
| `A` | Applesoft BASIC | Tokenized Applesoft BASIC program |
| `B` | Binary | Raw binary data with load address |
| `T` | Text | Plain text file |

### 7.3 Disk Commands

All disk commands are available from the BASIC prompt:

**CATALOG — List files:**

```basic
>CATALOG
 DISK: USER
 I 003 HELLO
 I 005 FIBONACCI
 B 012 SPRITE.DAT
 T 002 NOTES

 FILES: 4  SECTORS FREE: 112
```

**SAVE — Save BASIC program:**

```basic
>SAVE MYPROG
```

Saves the current BASIC program in memory to the virtual disk with the name `MYPROG`.

**LOAD — Load BASIC program:**

```basic
>LOAD MYPROG
```

Loads a BASIC program from disk into memory. The previous program is replaced.

**DELETE — Remove a file:**

```basic
>DELETE OLDFILE
```

**RENAME — Rename a file:**

```basic
>RENAME OLDNAME,NEWNAME
```

**BSAVE — Save binary data:**

```basic
>BSAVE SPRITE.DAT,A$2000,L$0100
```

Saves 256 bytes ($0100) starting at address $2000 to a binary file called `SPRITE.DAT`.

**BLOAD — Load binary data:**

```basic
>BLOAD SPRITE.DAT
```

Loads a binary file into memory at the address stored in the file header. To override the load address:

```basic
>BLOAD SPRITE.DAT,A$4000
```

### 7.4 File Location on Host

Virtual disk files are stored on the host filesystem at:

```
~/.apple1emu/disks/
```

Each file is stored as an individual file with an `A1DF` ("Apple 1 Disk File") header containing the file type, load address, and size. These files are not human-editable — use the emulator's commands to manage them.

---

## 8. Virtual Cassette (ACI)

### 8.1 How the Cassette Interface Works

The Apple Cassette Interface (ACI) was an optional add-on for the original Apple 1 that allowed saving and loading programs using a standard audio cassette recorder. The emulator provides a virtual cassette implementation that saves and loads from files instead of audio tapes.

The ACI ROM occupies addresses $C100-$C1FF. Two entry points are provided:

| Address | Function |
|---------|----------|
| `$C100` | Write (save) memory to tape |
| `$C108` | Read (load) memory from tape |

### 8.2 Saving Programs to Tape

**From the Woz Monitor:**

First, set the start and end addresses in zero-page:

```
\0:00 03
\2:FF 03
```

This sets the start address to $0300 and end address to $03FF. Then provide a filename in the input buffer and execute the save:

```
\C100R
```

**From BASIC (simplified):**

The virtual cassette system intercepts the ACI trap and prompts for a filename:

```
TAPE FILENAME? MYPROG
SAVING $0300-$03FF... DONE
```

### 8.3 Loading Programs from Tape

**From the Woz Monitor:**

```
\C108R
```

The emulator prompts for the tape filename:

```
TAPE FILENAME? MYPROG
LOADING AT $0300... DONE
```

The data is loaded into memory at the address stored in the tape file header.

**Available tape files** can be found by examining the tapes directory on the host system (see section 8.4).

### 8.4 Tape File Location

Virtual tape files are stored at:

```
~/.apple1emu/tapes/
```

Tape files use the extension `.a1t` and contain an `A1TP` header with the load address, data length, and the raw program bytes.

Pre-loaded demo tapes (if provided) are in:

```
~/.apple1emu/tapes/demos/
```

---

## 9. Snapshots (Save State)

### 9.1 Saving a Complete Machine State

Press **Ctrl+S** at any time to save the complete machine state. This captures:

- All CPU registers (A, X, Y, SP, PC, Status)
- The CPU cycle counter
- The complete 64 KB memory image
- PIA register state (keyboard and display)
- Current configuration flags

The emulator briefly displays:

```
SNAPSHOT SAVED: session_20260525_103045.a1s
```

### 9.2 Loading a Saved State

Press **Ctrl+L** to load the most recent snapshot. The emulator displays a list of available snapshots if multiple exist:

```
AVAILABLE SNAPSHOTS:
 [1] session_20260525_103045.a1s
 [2] session_20260524_192211.a1s
 [3] session_20260523_140800.a1s
SELECT (1-3):
```

After loading, execution resumes exactly where it was saved — as if no time has passed.

You can also load a specific snapshot from the command line:

```bash
./apple1emu --snapshot ~/.apple1emu/snapshots/session_20260525_103045.a1s
```

### 9.3 Snapshot Files

Snapshots are stored at:

```
~/.apple1emu/snapshots/
```

Each snapshot file is exactly 65,568 bytes:

| Offset | Size | Content |
|--------|------|---------|
| $00 | 4 | Magic: `A1SS` |
| $04 | 2 | Format version ($0001) |
| $06 | 2 | Reserved |
| $08 | 2 | CPU PC (little-endian) |
| $0A | 1 | CPU A register |
| $0B | 1 | CPU X register |
| $0C | 1 | CPU Y register |
| $0D | 1 | CPU SP register |
| $0E | 1 | CPU Status register |
| $0F | 1 | Reserved |
| $10 | 8 | Cycle counter (uint64, little-endian) |
| $18 | 4 | PIA state |
| $1C | 4 | Config flags |
| $20 | 65536 | Full 64 KB memory image |

---

## 10. Configuration

### 10.1 Configuration Menu

Press **Ctrl+E** to open the configuration menu:

```
╔══════════════════════════════════════╗
║       APPLE 1 CONFIGURATION         ║
╠══════════════════════════════════════╣
║ [1] RAM Size:        32KB           ║
║ [2] Lowercase:       OFF            ║
║ [3] 80-Column:       OFF            ║
║ [4] BASIC Mode:      Integer        ║
║ [5] Character Delay: ON (16.7ms)    ║
║ [6] Auto-repeat:     OFF            ║
║ [7] Scanlines:       ON             ║
║ [8] Phosphor Glow:   ON             ║
║ [9] Sound:           OFF            ║
║ [0] Virtual Disk:    ON             ║
║                                     ║
║ Press number to toggle, ESC to exit ║
╚══════════════════════════════════════╝
```

Press a number key to toggle the corresponding setting. Press **Escape** to close the menu and return to the emulator. Changes take effect immediately.

### 10.2 Settings Explained

| Setting | Default | Description |
|---------|---------|-------------|
| **RAM Size** | 32KB | Cycles through 4K/8K/32K/48K. Larger RAM allows bigger programs. A cold reset is required after changing. |
| **Lowercase** | OFF | When ON, allows lowercase input and display. The original Apple 1 was uppercase-only. |
| **80-Column** | OFF | When ON, doubles the display width from 40 to 80 columns. Inspired by the Apple //e 80-column card. |
| **BASIC Mode** | Integer | Toggles between Integer BASIC (original) and Applesoft BASIC (enhanced, with floating-point). |
| **Character Delay** | ON | When ON, characters appear one at a time at ~16.7ms intervals, matching the original hardware. When OFF (fast mode), output is instant. |
| **Auto-repeat** | OFF | When ON, held keys repeat. The original Apple 1 keyboard had no auto-repeat. |
| **Scanlines** | ON | Simulates CRT scanline gaps by dimming alternate rows. |
| **Phosphor Glow** | ON | Recently printed characters appear brighter, fading over time like real phosphor. |
| **Sound** | OFF | Reserved for future audio emulation. |
| **Virtual Disk** | ON | Enables the virtual disk filesystem commands (CATALOG, SAVE, LOAD, etc.) |

### 10.3 Configuration File Format

Configuration is stored in INI format at `~/.apple1emu/config.ini`:

```ini
[general]
ram_kb = 32
log_level = 2

[display]
lowercase_enabled = false
col80_enabled = false
scanlines = true
phosphor_glow = true

[emulation]
fast_mode = false
auto_repeat = false
enhanced_basic = false
virtual_disk = true

[paths]
rom_dir = ~/.apple1emu/roms
data_dir = ~/.apple1emu
```

You can edit this file manually while the emulator is not running. Changes are loaded on next startup. Settings changed via the Ctrl+E menu are saved automatically.

### 10.4 Display Effects

**Scanlines:**
Every other row of the display is rendered at reduced brightness, simulating the visible gap between scan lines on a CRT monitor. This is purely cosmetic and can be disabled for readability.

**Phosphor Glow:**
When enabled, characters that were recently written to the display appear at full brightness. Over the next few frames, they fade to a dimmer "afterglow" green, simulating the phosphor decay of a real CRT. The effect gives the display a natural, warm look.

**Both effects can be disabled** via the configuration menu or command-line flags:

```bash
./apple1emu --no-glow --no-scanlines
```

---

## 11. Keyboard Reference

### 11.1 Full Keyboard Mapping

| Host Key | Apple 1 Input | Notes |
|----------|---------------|-------|
| A-Z | A-Z ($41-$5A) | Forced uppercase in stock mode |
| a-z | A-Z (or a-z) | Uppercase-only unless lowercase enabled |
| 0-9 | 0-9 ($30-$39) | Direct mapping |
| Space | Space ($20) | |
| ! | ! ($21) | |
| " | " ($22) | |
| # | # ($23) | |
| $ | $ ($24) | |
| % | % ($25) | |
| & | & ($26) | |
| ' | ' ($27) | |
| ( ) | ( ) ($28-$29) | |
| * + | * + ($2A-$2B) | |
| , - . / | , - . / ($2C-$2F) | |
| : ; | : ; ($3A-$3B) | |
| < = > | < = > ($3C-$3E) | |
| ? | ? ($3F) | |
| @ | @ ($40) | Also used as cursor character |
| [ \ ] ^ _ | [$5B-$5F] | |
| Return | CR ($0D) | Submits input line |
| Backspace | Rubout | Removes from buffer; no screen erase in stock mode |
| Escape | Cancel | Cancels current input line |

### 11.2 Control Key Combinations

| Key Combo | Emulator Action |
|-----------|-----------------|
| Ctrl+C | Break — stop running BASIC program |
| Ctrl+D | Toggle debug overlay (CPU registers, cycles) |
| Ctrl+E | Open/close configuration menu |
| Ctrl+F | Toggle fast mode (disable/enable timing delays) |
| Ctrl+H | Display help overlay |
| Ctrl+L | Load snapshot |
| Ctrl+P | Paste text from clipboard (simulates typing) |
| Ctrl+Q | Quit emulator |
| Ctrl+R | Warm reset (jump to Woz Monitor at $FF00) |
| Ctrl+Shift+R | Cold reset (full power cycle — clears all RAM) |
| Ctrl+S | Save snapshot |

### 11.3 Apple 1 vs Modern Keyboard Differences

The original Apple 1 keyboard had significant differences from modern keyboards:

| Feature | Original Apple 1 | Emulator Default | Emulator Enhanced |
|---------|-------------------|------------------|-------------------|
| Case | Uppercase only | Uppercase only | Both (toggle) |
| Backspace | Buffer only (no erase) | Buffer only | Visual erase |
| Auto-repeat | None | None | Available (toggle) |
| Arrow keys | Not available | Not available | Not available |
| Delete key | Not available | Mapped to backspace | Mapped to backspace |
| Function keys | Not available | Used for emulator controls | Same |

### 11.4 Special Emulator Controls

These key combinations are intercepted by the emulator and never reach the Apple 1:

```
Ctrl+Q .......... Quit emulator (with confirmation)
Ctrl+R .......... Warm reset to Woz Monitor
Ctrl+Shift+R .... Cold reset (full reboot)
Ctrl+E .......... Configuration menu
Ctrl+H .......... Help overlay
Ctrl+S .......... Save snapshot
Ctrl+L .......... Load snapshot
Ctrl+F .......... Toggle fast mode
Ctrl+D .......... Toggle debug overlay
Ctrl+P .......... Paste mode (type clipboard contents)
Ctrl+C .......... Break (BASIC interrupt)
```

---

## 12. Display

### 12.1 Apple Monitor II Emulation

The emulator recreates the visual appearance of the Apple Monitor II — the green-phosphor CRT display commonly paired with the Apple 1 and Apple II. The display renders using ncurses with 256-color terminal support.

**Startup sequence:**

```
[Screen clears to black]
[Brief phosphor warmup effect]

                APPLE 1 EMULATOR v0.1.0
          MOS 6502 @ 1.022727 MHz | 32KB RAM
     Type Ctrl+H for help | Ctrl+E for settings

\
@  (blinking cursor)
```

### 12.2 Color Scheme and Phosphor Effects

The display uses a carefully chosen palette:

| Element | Color | Hex Code |
|---------|-------|----------|
| Background | Pure black | `#000000` |
| Primary text | Bright green | `#33FF33` |
| Dim/faded text | Dark green | `#0B6623` |
| Cursor | Bright green (blinking) | `#33FF33` |
| Border/chrome | Very dark green | `#1A331A` |

The **cursor** is a blinking `@` symbol, toggling at approximately 2 Hz (matching the original NE555-based blink circuit).

**Phosphor glow** operates in three brightness levels:
1. **Bright** — character just written (current frame)
2. **Normal** — character written 2-3 frames ago
3. **Dim** — character written longer ago (afterglow)

### 12.3 40-Column vs 80-Column Modes

| Mode | Columns | Rows | Authentic? |
|------|---------|------|------------|
| Stock (40-col) | 40 | 24 | Yes — matches original hardware |
| Enhanced (80-col) | 80 | 24 | No — inspired by Apple //e 80-column card |

In 40-column mode, the terminal window can be as small as 40 characters wide. The display wraps text at column 40 and scrolls when line 24 is reached.

In 80-column mode, double the text fits on each line. This mode is useful for programming but breaks the authentic Apple 1 experience.

Toggle with **Ctrl+E** > option 3, or start with `--enhanced`.

### 12.4 Character Set

**Stock mode (uppercase only):**

The Apple 1 displays ASCII characters in the range $20 (space) through $5F (underscore). This includes:
- Digits: 0-9
- Uppercase letters: A-Z
- Symbols: ! " # $ % & ' ( ) * + , - . / : ; < = > ? @
- Brackets: [ \ ] ^ _

Lowercase letters ($60-$7A) are **not displayed** in stock mode — they are converted to uppercase.

**Enhanced mode (lowercase enabled):**

With lowercase enabled, the full printable ASCII range ($20-$7E) is available, including:
- Lowercase letters: a-z
- Additional symbols: ` { | } ~

---

## 13. Advanced Topics

### 13.1 Memory Map Diagram

```
$FFFF ┌────────────────────────────────┐
      │ Vectors: NMI, RESET, IRQ       │ $FFFA-$FFFF
$FF00 ├────────────────────────────────┤
      │ Woz Monitor ROM (256 bytes)    │ $FF00-$FFFF
$F000 ├────────────────────────────────┤
      │ Extended Utilities / Reserved   │ $F000-$FEFF
$E000 ├────────────────────────────────┤
      │ Integer BASIC ROM (4 KB)       │ $E000-$EFFF
$D014 ├────────────────────────────────┤
      │ PIA: DSPCR (Display control)   │ $D013
      │ PIA: DSP (Display data)        │ $D012
      │ PIA: KBDCR (Keyboard control)  │ $D011
      │ PIA: KBD (Keyboard data)       │ $D010
$D010 ├────────────────────────────────┤
      │ I/O Space (unmapped)           │ $C200-$D00F
$C200 ├────────────────────────────────┤
      │ ACI ROM (256 bytes)            │ $C100-$C1FF
$C100 ├────────────────────────────────┤
      │ ACI I/O Range                  │ $C000-$C0FF
$C000 ├────────────────────────────────┤
      │                                │
      │ Extended RAM                   │ $1000-$BFFF
      │ (when 32K or 48K mode)         │ (available in expanded configs)
      │                                │
$1000 ├────────────────────────────────┤
      │ User RAM (base)               │ $0280-$0FFF
$0280 ├────────────────────────────────┤
      │ Woz Monitor Input Buffer       │ $0200-$027F
$0200 ├────────────────────────────────┤
      │ Stack Page                     │ $0100-$01FF
$0100 ├────────────────────────────────┤
      │ Zero Page                      │ $0000-$00FF
$0000 └────────────────────────────────┘
```

### 13.2 Using PEEK and POKE

PEEK and POKE give BASIC programs direct access to the hardware:

**Reading the keyboard directly:**

```basic
10 IF PEEK(53265) < 128 THEN 10
20 K = PEEK(53264) - 128
30 PRINT CHR$(K)
40 GOTO 10
```

Address 53265 decimal = $D011 (KBDCR). Bit 7 is set when a key is available.
Address 53264 decimal = $D010 (KBD). Reading clears the key-available flag.

**Writing to the display directly:**

```basic
10 POKE 53266, 72
```

Address 53266 decimal = $D012 (DSP). Writing puts a character on screen.
72 decimal = $48 = ASCII 'H'.

**Useful PEEK/POKE addresses:**

| Decimal | Hex | Purpose |
|---------|-----|---------|
| 53264 | $D010 | Keyboard data (read: get key + clear flag) |
| 53265 | $D011 | Keyboard status (bit 7: key available) |
| 53266 | $D012 | Display data (write: output character) |
| 53267 | $D013 | Display control register |

### 13.3 Writing Machine Language Programs

To write machine language (assembly) programs without the mini-assembler, deposit bytes directly via the Woz Monitor:

**Example: A delay loop**

```
\0300:A2 FF A0 FF 88 D0 FD CA D0 F8 60
```

This assembles to:

```
0300: LDX #$FF      ; outer loop counter
0302: LDY #$FF      ; inner loop counter
0304: DEY           ; decrement inner
0305: BNE $0304     ; loop until Y=0
0307: DEX           ; decrement outer
0308: BNE $0302     ; loop until X=0
030A: RTS           ; return
```

Run with `0300R` from the monitor, or `CALL 768` from BASIC.

### 13.4 Calling Machine Language from BASIC

Use `CALL` to execute machine language routines from BASIC:

```basic
10 REM DEPOSIT A SHORT ML ROUTINE
20 POKE 768, 169   : REM LDA #65 (A9 41)
30 POKE 769, 65
40 POKE 770, 141   : REM STA $D012 (8D 12 D0)
50 POKE 771, 18
60 POKE 772, 208
70 POKE 773, 96    : REM RTS (60)
80 REM NOW CALL IT
90 CALL 768
```

This deposits a routine at $0300 that prints the letter 'A', then calls it.

**Passing values between BASIC and machine language:**

Use agreed-upon zero-page locations. For example, store a value in location $06 from BASIC, then read it in your ML routine:

```basic
10 POKE 6, 42
20 CALL 768
30 PRINT PEEK(7)
```

Your machine language at $0300 reads location $06, processes it, and stores the result in $07.

### 13.5 Cassette Loading for Pre-Written Programs

If you have pre-written programs distributed as tape files (`.a1t`), copy them to:

```
~/.apple1emu/tapes/
```

Then load from within the emulator:

```
\C108R
TAPE FILENAME? MYPROG
LOADING AT $0300... DONE
\0300R
```

Or to load a BASIC program from tape:

```
\C108R
TAPE FILENAME? GAME
LOADING AT $0800... DONE
\E2B3R
>RUN
```

---

## 14. Demo Programs

### 14.1 How to Enter Programs

Before diving into the demo programs, here is how to work with BASIC programs on the Apple 1.

#### Typing In a BASIC Program

Every line of a BASIC program begins with a line number. At the `>` prompt, type each line including its number and press **Return**:

```
>10 PRINT "HELLO"
>20 GOTO 10
```

Lines are stored in numeric order regardless of the order you type them. You can add lines between existing ones (e.g., line 15 goes between 10 and 20).

#### Fixing Typos

To correct a line, simply retype it with the same line number. The new version replaces the old:

```
>10 PRINT "HELLO WORLD"
```

This replaces whatever was previously on line 10.

#### Deleting a Line

Type just the line number with nothing after it and press **Return**:

```
>15
```

This removes line 15 from the program. To delete a range of lines, use the DEL command:

```
>DEL 10,50
```

#### Verifying Your Program

Use `LIST` to display the program stored in memory:

```
>LIST
10 PRINT "HELLO WORLD"
20 GOTO 10
```

To list a specific range:

```
>LIST 10,20
```

#### Running the Program

Type `RUN` and press **Return**:

```
>RUN
HELLO WORLD
HELLO WORLD
HELLO WORLD
...
```

#### Stopping a Running Program

Press **Ctrl+C** to interrupt execution. The emulator displays:

```
BREAK AT LINE 20
>
```

You are returned to the BASIC prompt and can edit or re-run the program.

#### Returning to the Woz Monitor

From BASIC, type:

```
>CALL -151
```

Or press **Ctrl+R** for a warm reset. Your program remains in memory -- re-enter BASIC with `E2B3R` (warm start) to resume.

#### Using Paste Mode (Fastest Method)

For long programs, use **Ctrl+P** (paste mode):

1. Copy the program text to your macOS clipboard
2. At the BASIC `>` prompt, press **Ctrl+P**
3. The emulator types the clipboard contents at keyboard speed
4. When done, type `LIST` to verify, then `RUN`

---

### 14.2 Demo Program Summary

The emulator includes 15 demo programs in the `demos/` directory:

| # | Filename | Category | Description |
|---|----------|----------|-------------|
| 1 | `hello.bas` | Utility | Classic "Hello, World!" program |
| 2 | `fibonacci.bas` | Math | Computes and displays the Fibonacci sequence |
| 3 | `guess.bas` | Game | Number guessing game (1-100) |
| 4 | `sieve.bas` | Math | Sieve of Eratosthenes prime finder |
| 5 | `adventure.bas` | Game | Text adventure -- explore a dungeon |
| 6 | `star.bas` | Graphics | Draws star/diamond patterns with asterisks |
| 7 | `sort.bas` | Utility | Bubble sort demonstration with random numbers |
| 8 | `calendar.bas` | Utility | Prints a monthly calendar for any year |
| 9 | `lunarlander.bas` | Game | Land a spacecraft on the moon |
| 10 | `wumpus.bas` | Game | Hunt the Wumpus cave exploration game |
| 11 | `hamurabi.bas` | Game | Rule ancient Sumeria for 10 years |
| 12 | `life.bas` | Simulation | Conway's Game of Life cellular automaton |
| 13 | `startrek.bas` | Game | Destroy Klingons in a 4x4 galaxy |
| 14 | `nim.bas` | Game | Matchstick strategy game vs computer |
| 15 | `mastermind.bas` | Game | Break a secret code (Bulls and Cows) |

---

### 14.3 HELLO.BAS

**Description:** The simplest possible program -- prints a greeting and exits.

**How to load:** Type the program at the BASIC prompt:

```
>10 PRINT "HELLO, APPLE 1 WORLD!"
>20 END
```

**How to run:** Type `RUN`

**Example session:**

```
>RUN
HELLO, APPLE 1 WORLD!

>
```

**Commands/controls:** None -- the program runs and exits immediately.

---

### 14.4 FIBONACCI.BAS

**Description:** Computes and displays the first 20 numbers in the Fibonacci sequence using integer arithmetic.

**How to load:** Type the program (16 lines) or use Ctrl+P to paste.

**How to run:** Type `RUN`

**Example session:**

```
>RUN
** FIBONACCI NUMBERS **

1: 0
2: 1
3: 1
4: 2
5: 3
6: 5
7: 8
8: 13
9: 21
10: 34
11: 55
12: 89
13: 144
14: 233
15: 377
16: 610
17: 987
18: 1597
19: 2584
20: 4181

DONE.
>
```

**Commands/controls:** None -- the program runs to completion automatically.

**Note:** Because Integer BASIC uses 16-bit signed integers (max 32767), the sequence stops at 20 terms to avoid overflow.

---

### 14.5 GUESS.BAS

**Description:** A number guessing game where the computer picks a random number between 1 and 100, and you try to guess it with hints.

**How to load:** Type the program (26 lines) or use Ctrl+P to paste.

**How to run:** Type `RUN`

**How to play:**

1. The computer picks a secret number from 1 to 100
2. You type a guess when prompted
3. The computer tells you "TOO LOW!" or "TOO HIGH!"
4. Keep guessing until you find the number
5. Try to minimize your number of guesses (7 or fewer is optimal)

**Example session:**

```
>RUN
** NUMBER GUESSING GAME **

I'M THINKING OF A NUMBER
BETWEEN 1 AND 100.

YOUR GUESS? 50
TOO HIGH!
YOUR GUESS? 25
TOO LOW!
YOUR GUESS? 37
TOO LOW!
YOUR GUESS? 43
TOO HIGH!
YOUR GUESS? 40
CORRECT!
YOU GOT IT IN 5 TRIES.

PLAY AGAIN? (1=YES, 0=NO)? 0
THANKS FOR PLAYING!
>
```

**Commands/controls:**

| Input | Action |
|-------|--------|
| Number 1-100 | Your guess |
| 1 at "PLAY AGAIN?" | Start a new game |
| 0 at "PLAY AGAIN?" | Quit |

---

### 14.6 SIEVE.BAS

**Description:** Implements the Sieve of Eratosthenes algorithm to find all prime numbers up to 100.

**How to load:** Type the program (31 lines) or use Ctrl+P to paste.

**How to run:** Type `RUN`

**Example session:**

```
>RUN
** SIEVE OF ERATOSTHENES **

PRIMES UP TO 100:

2 3 5 7 11 13 17 19 23 29 31 37 41
43 47 53 59 61 67 71 73 79 83 89 97

25 PRIMES FOUND.
>
```

**Commands/controls:** None -- the program runs to completion automatically.

**How it works:** The algorithm starts by assuming all numbers 2-100 are prime, then systematically marks multiples of each prime as composite. The numbers that remain unmarked are the primes.

---

### 14.7 ADVENTURE.BAS

**Description:** A mini text adventure game where you explore a five-room dungeon, find a key, unlock a vault, collect gold, and escape.

**How to load:** Type the program (52 lines) or use Ctrl+P to paste.

**How to run:** Type `RUN`

**How to play:**

1. You start in the Entrance Hall
2. Move between rooms using compass directions
3. Find the Key in the Armory (east of the Entrance Hall)
4. Use the Key to unlock the Vault (east of the Dark Corridor)
5. Collect the Gold in the Treasure Room (north of the Vault)
6. Return to the Entrance Hall with the Gold to win

**Map layout:**

```
                   [TREASURE ROOM]
                         |
                         N
                         |
[ENTRANCE HALL] --E-- [DARK CORRIDOR] --E-- [VAULT]
       |
       E
       |
   [ARMORY]
```

**Example session:**

```
>RUN
** DUNGEON ADVENTURE **

FIND THE GOLD AND ESCAPE!
COMMANDS: N S E W

ENTRANCE HALL. TORCHES FLICKER.
WHAT DO YOU DO? E

ARMORY. A KEY GLINTS!
WHAT DO YOU DO? W

ENTRANCE HALL. TORCHES FLICKER.
WHAT DO YOU DO? N

DARK CORRIDOR. COLD AIR.
WHAT DO YOU DO? E

LOCKED VAULT.
WHAT DO YOU DO? E

TREASURE ROOM! GOLD HERE!
WHAT DO YOU DO? S

LOCKED VAULT.
WHAT DO YOU DO? W

DARK CORRIDOR. COLD AIR.
WHAT DO YOU DO? S

ENTRANCE HALL. TORCHES FLICKER.

YOU ESCAPED WITH THE GOLD!
*** YOU WIN! ***
>
```

**Commands/controls:**

| Input | Action |
|-------|--------|
| N | Move north |
| S | Move south |
| E | Move east |
| W | Move west |

---

### 14.8 STAR.BAS

**Description:** Draws a diamond pattern made of asterisks, with user-selectable size.

**How to load:** Type the program (31 lines) or use Ctrl+P to paste.

**How to run:** Type `RUN`

**How to use:**

1. The program draws a diamond of default size 10
2. After drawing, enter a new size (1-12) to draw another
3. Enter 0 to quit

**Example session (size 5):**

```
>RUN
** STAR DIAMOND PATTERN **

    *
   ***
  *****
 *******
*********
 *******
  *****
   ***
    *

SIZE (1-12, 0=QUIT)? 3
  *
 ***
*****
 ***
  *

SIZE (1-12, 0=QUIT)? 0
>
```

**Commands/controls:**

| Input | Action |
|-------|--------|
| 1-12 | Draw a diamond of that size |
| 0 | Quit the program |

---

### 14.9 SORT.BAS

**Description:** Demonstrates the bubble sort algorithm by generating 10 random numbers, sorting them, and displaying before/after results.

**How to load:** Type the program (35 lines) or use Ctrl+P to paste.

**How to run:** Type `RUN`

**Example session:**

```
>RUN
** BUBBLE SORT DEMO **

UNSORTED: 47 82 13 91 5 68 34 76 22 59

SORTING...

SORTED:   5 13 22 34 47 59 68 76 82 91

AGAIN? (1=YES, 0=NO)? 1

UNSORTED: 33 71 8 55 94 17 62 40 86 29

SORTING...

SORTED:   8 17 29 33 40 55 62 71 86 94

AGAIN? (1=YES, 0=NO)? 0
>
```

**Commands/controls:**

| Input | Action |
|-------|--------|
| 1 at "AGAIN?" | Generate and sort new random numbers |
| 0 at "AGAIN?" | Quit |

---

### 14.10 CALENDAR.BAS

**Description:** Prints a formatted monthly calendar for any month and year, correctly computing the day of the week.

**How to load:** Type the program (51 lines) or use Ctrl+P to paste.

**How to run:** Type `RUN`

**How to use:**

1. Enter the month number (1-12)
2. Enter the year (e.g., 1976 or 2026)
3. The program prints a formatted calendar grid

**Example session:**

```
>RUN
** CALENDAR **

MONTH (1-12)? 7
YEAR? 1976

 SU MO TU WE TH FR SA
 ---------------------
              1  2  3
  4  5  6  7  8  9 10
 11 12 13 14 15 16 17
 18 19 20 21 22 23 24
 25 26 27 28 29 30 31
>
```

**Commands/controls:**

| Input | Action |
|-------|--------|
| Month (1-12) | Which month to display |
| Year | Which year (handles leap years) |

**Note:** The program uses Zeller's formula to compute the day of the week for the first of any month. It correctly handles leap years for February.

---

### 14.11 LUNARLANDER.BAS

**Description:** A physics simulation game where you pilot a spacecraft for a soft landing on the moon by controlling thrust each second.

**How to load:** Type the program (54 lines) or use Ctrl+P to paste.

**How to run:** Type `RUN`

**How to play:**

1. Choose whether to read instructions (1=yes, 0=no)
2. Each turn shows your altitude, velocity, and fuel
3. Enter a thrust value (0-9) -- higher thrust slows your descent
4. Land with velocity under 5 to survive
5. You start with 200 units of fuel -- use it wisely

**Strategy tips:**

- Gravity adds 5 to your velocity each second
- A thrust of 5 exactly cancels gravity (but wastes fuel)
- Let yourself fall fast at high altitude, then brake hard near the surface
- Keep at least 50 fuel for the final braking

**Example session:**

```
>RUN
** LUNAR LANDER **

WANT INSTRUCTIONS? (1=YES, 0=NO)? 0
-----------------------------
TIME: 0  ALT: 500  VEL: 50
FUEL: 200
THRUST (0-9)? 0
-----------------------------
TIME: 1  ALT: 445  VEL: 55
FUEL: 200
THRUST (0-9)? 0
-----------------------------
TIME: 2  ALT: 385  VEL: 60
FUEL: 200
THRUST (0-9)? 5
-----------------------------
TIME: 3  ALT: 325  VEL: 60
FUEL: 195
THRUST (0-9)? 9
-----------------------------
TIME: 4  ALT: 269  VEL: 56
FUEL: 186
THRUST (0-9)? 9
...
-----------------------------
*** CONTACT! ***
VELOCITY AT IMPACT: 3
PERFECT LANDING!
CONGRATULATIONS!
TIME ELAPSED: 12 SECONDS

PLAY AGAIN? (1=YES, 0=NO)? 0
GOODBYE, ASTRONAUT!
>
```

**Commands/controls:**

| Input | Action |
|-------|--------|
| 0-9 | Thrust level (fuel burned per second) |
| 1 at "PLAY AGAIN?" | Restart the game |
| 0 at "PLAY AGAIN?" | Quit |

---

### 14.12 WUMPUS.BAS

**Description:** The classic "Hunt the Wumpus" cave exploration game by Gregory Yob (1972). Navigate a dodecahedral cave system, avoid hazards, and shoot arrows to kill the Wumpus.

**How to load:** Type the program (101 lines) or use Ctrl+P to paste.

**How to run:** Type `RUN`

**How to play:**

1. You are in a cave of 20 rooms, each connected to 3 others
2. Hazards lurk in the cave: the Wumpus, 2 bottomless pits, 2 bat rooms
3. Your senses warn you of nearby hazards:
   - "I SMELL A WUMPUS!" -- Wumpus is in an adjacent room
   - "I FEEL A DRAFT!" -- a pit is in an adjacent room
   - "BATS NEARBY!" -- bats are in an adjacent room
4. Move to explore, or shoot an arrow into an adjacent room
5. You have 5 arrows total
6. If you enter the Wumpus's room, it eats you
7. If you enter a pit, you fall to your death
8. Bats carry you to a random room

**Example session:**

```
>RUN
** HUNT THE WUMPUS **

WANT INSTRUCTIONS? (1=YES, 0=NO)? 0

YOU ARE IN ROOM 7
TUNNELS LEAD TO 6 8 17
I SMELL A WUMPUS!
SHOOT OR MOVE (S/M)? M
WHERE TO? 6

YOU ARE IN ROOM 6
TUNNELS LEAD TO 5 7 15
I SMELL A WUMPUS!
BATS NEARBY!
SHOOT OR MOVE (S/M)? S
SHOOT INTO WHICH ROOM? 7
MISSED!

YOU ARE IN ROOM 6
TUNNELS LEAD TO 5 7 15
SHOOT OR MOVE (S/M)? S
SHOOT INTO WHICH ROOM? 17
*** YOU GOT THE WUMPUS! ***

PLAY AGAIN? (1=YES, 0=NO)? 0
>
```

**Commands/controls:**

| Input | Action |
|-------|--------|
| M | Move to an adjacent room |
| S | Shoot an arrow into an adjacent room |
| Room number | Which room to move to or shoot into |
| 1 at "PLAY AGAIN?" | Restart |
| 0 at "PLAY AGAIN?" | Quit |

---

### 14.13 HAMURABI.BAS

**Description:** A resource management game where you rule ancient Sumeria for 10 years, making decisions about land, food, and crops each turn.

**How to load:** Type the program (104 lines) or use Ctrl+P to paste.

**How to run:** Type `RUN`

**How to play:**

1. Each year you decide:
   - How many acres of land to buy or sell
   - How many bushels of grain to feed your people
   - How many acres to plant with crops
2. Random events affect your kingdom: harvests vary, rats eat grain, plagues strike
3. If you starve too many people, they revolt
4. Survive 10 years with a healthy population to win

**Decision guide:**

| Decision | Rule of Thumb |
|----------|---------------|
| Feeding | Each person needs 20 bushels per year |
| Planting | Each acre needs 0.5 bushels of seed |
| Planting | Each person can tend 10 acres |
| Land cost | Varies 17-26 bushels/acre each year |

**Example session:**

```
>RUN
** HAMURABI **

WANT INSTRUCTIONS? (1=YES, 0=NO)? 0

==== YEAR 1 ====
POPULATION: 100
ACRES: 1000
GRAIN IN STORE: 2800
LAND COSTS 22 BUSHELS/ACRE
ACRES TO BUY? 0
ACRES TO SELL? 0
BUSHELS TO FEED PEOPLE? 2000
ACRES TO PLANT? 800

==== YEAR 2 ====
POPULATION: 105
ACRES: 1000
GRAIN IN STORE: 4200
LAND COSTS 19 BUSHELS/ACRE
...

==== FINAL REPORT ====
YOU RULED 10 YEARS.
POPULATION: 132
ACRES: 1000  GRAIN: 5600
TOTAL STARVED: 12
SUPERB LEADERSHIP!

PLAY AGAIN? (1=YES, 0=NO)? 0
>
```

**Commands/controls:**

| Input | Action |
|-------|--------|
| Number at "ACRES TO BUY" | Acres to purchase (0 to skip) |
| Number at "ACRES TO SELL" | Acres to sell (only if not buying) |
| Number at "BUSHELS TO FEED" | Grain allocated for food |
| Number at "ACRES TO PLANT" | Acres to plant with crops |
| 1 at "PLAY AGAIN?" | Restart |
| 0 at "PLAY AGAIN?" | Quit |

**Rating at end of game:**

| Total Starved | Rating |
|---------------|--------|
| Under 30 | "SUPERB LEADERSHIP!" |
| 30-80 | "FAIR PERFORMANCE." |
| Over 80 | "THE PEOPLE REVOLTED!" |

---

### 14.14 LIFE.BAS

**Description:** Conway's Game of Life -- a cellular automaton simulation on a 20x20 grid where cells live or die based on their neighbors.

**How to load:** Type the program (73 lines) or use Ctrl+P to paste.

**How to run:** Type `RUN`

**How to play:**

1. Choose whether to read instructions (1=yes, 0=no)
2. Enter live cells one at a time as row,col pairs (1-20 each)
3. Enter `0,0` when done placing cells
4. Enter how many generations to simulate
5. Watch the pattern evolve

**Rules of Life:**

| Condition | Result |
|-----------|--------|
| Live cell with 2 or 3 neighbors | Survives |
| Dead cell with exactly 3 neighbors | Becomes alive |
| All other cells | Die or stay dead |

**Classic starting patterns to try:**

| Pattern | Cells to Enter | Behavior |
|---------|----------------|----------|
| Blinker | (10,10) (10,11) (10,12) | Oscillates period 2 |
| Glider | (2,3) (3,4) (4,2) (4,3) (4,4) | Moves diagonally |
| Block | (5,5) (5,6) (6,5) (6,6) | Static (stable) |

**Example session:**

```
>RUN
** GAME OF LIFE **

WANT INSTRUCTIONS? (1=YES, 0=NO)? 0

ENTER LIVE CELLS (ROW,COL)
ROW 1-20, COL 1-20
ENTER 0,0 WHEN DONE
ROW,COL? 10,10
ROW,COL? 10,11
ROW,COL? 10,12
ROW,COL? 0,0
HOW MANY GENERATIONS? 3

....................
....................
....................
....................
....................
....................
....................
....................
....................
.........***.......
....................
....................
....................
....................
....................
....................
....................
....................
....................
....................

....................
....................
....................
....................
....................
....................
....................
....................
.........*.........
.........*.........
.........*.........
....................
....................
....................
....................
....................
....................
....................
....................
....................
...

DONE. PLAY AGAIN? (1=YES, 0=NO)? 0
>
```

**Commands/controls:**

| Input | Action |
|-------|--------|
| Row,Col (e.g., 10,10) | Place a live cell |
| 0,0 | Finish placing cells |
| Number of generations | How many steps to simulate |
| 1 at "PLAY AGAIN?" | Restart with new pattern |
| 0 at "PLAY AGAIN?" | Quit |

---

### 14.15 STARTREK.BAS

**Description:** A simplified Star Trek strategy game where you command the USS Enterprise, navigating a 4x4 galaxy to destroy all Klingons within 30 stardates.

**How to load:** Type the program (122 lines) or use Ctrl+P to paste.

**How to run:** Type `RUN`

**How to play:**

1. You command a starship with 3000 energy units
2. The galaxy is a 4x4 grid of quadrants, each containing Klingons, stars, and bases
3. Navigate between quadrants (costs 100 energy, 1 stardate)
4. Fire phasers or torpedoes at Klingons in your quadrant
5. Destroy all Klingons before running out of time or energy

**Commands:**

| Command | Action | Cost |
|---------|--------|------|
| SRS | Short Range Scan -- show current quadrant status | Free |
| LRS | Long Range Scan -- show entire galaxy map | Free |
| NAV | Navigate to another quadrant | 100 energy, 1 stardate |
| PHA | Fire phasers (choose energy amount) | Variable energy |
| TOR | Fire photon torpedo (60% hit chance) | 50 energy |

**Long Range Scan format:** Each number is KSB (Klingons, Stars, Bases). E.g., "210" means 2 Klingons, 1 star, 0 bases.

**Example session:**

```
>RUN
** STAR TREK **

WANT INSTRUCTIONS? (1=YES, 0=NO)? 0
MISSION: DESTROY 7 KLINGONS
ENERGY: 3000  STARDATES LEFT: 30

COMMAND (SRS/LRS/NAV/PHA/TOR)? SRS
SECTOR [ 3 , 2 ]
POSITION: 5 , 7
ENERGY: 3000
KLINGONS HERE: 2
STARDATES LEFT: 30

COMMAND (SRS/LRS/NAV/PHA/TOR)? PHA
ENERGY TO FIRE? 500
KLINGON DESTROYED!

COMMAND (SRS/LRS/NAV/PHA/TOR)? PHA
ENERGY TO FIRE? 500
KLINGON DESTROYED!

COMMAND (SRS/LRS/NAV/PHA/TOR)? NAV
QUADRANT ROW (1-4)? 1
QUADRANT COL (1-4)? 1
ARRIVED AT QUADRANT 1 , 1

COMMAND (SRS/LRS/NAV/PHA/TOR)? LRS
LONG RANGE SCAN:
 100  210  30  0
 110  0  320  10
 0  200  0  120
 10  0  110  0

...
*** CONGRATULATIONS! ***
ALL KLINGONS DESTROYED!
PLAY AGAIN? (1=YES, 0=NO)? 0
>
```

**Commands/controls:**

| Input | Action |
|-------|--------|
| SRS | Short range scan |
| LRS | Long range scan |
| NAV | Navigate (then enter row, col) |
| PHA | Fire phasers (then enter energy) |
| TOR | Fire torpedo |
| 1 at "PLAY AGAIN?" | Restart |
| 0 at "PLAY AGAIN?" | Quit |

**Tips:**

- Use LRS first to locate Klingons (hundreds digit in each cell)
- Phasers require energy > 200 per Klingon to destroy
- Torpedoes are cheaper but have only 60% hit rate
- Conserve energy -- running out means game over

---

### 14.16 NIM.BAS

**Description:** The classic matchstick game of Nim. You and the computer take turns removing 1-3 sticks from a pile of 21. Whoever takes the last stick loses.

**How to load:** Type the program (58 lines) or use Ctrl+P to paste.

**How to run:** Type `RUN`

**How to play:**

1. The game starts with 21 sticks
2. You go first -- take 1, 2, or 3 sticks
3. The computer takes its turn
4. The player who takes the last stick loses

**Strategy hint:** The computer plays optimally -- it tries to leave a number of sticks that is 1 more than a multiple of 4 (5, 9, 13, 17, 21). To beat it, you must leave it in the same position. Since 21 is already in that pattern and you go first, the computer has the advantage! Try to make it stumble on its random fallback moves.

**Example session:**

```
>RUN
** NIM **

WANT INSTRUCTIONS? (1=YES, 0=NO)? 0

STICKS REMAINING: 21
IIIIIIIIIIIIIIIIIIIII

TAKE HOW MANY (1-3)? 2
YOU TOOK 2 . LEFT: 19
I TAKE 2 . LEFT: 17

STICKS REMAINING: 17
IIIIIIIIIIIIIIIII

TAKE HOW MANY (1-3)? 3
YOU TOOK 3 . LEFT: 14
I TAKE 1 . LEFT: 13

STICKS REMAINING: 13
IIIIIIIIIIIII

TAKE HOW MANY (1-3)? 3
YOU TOOK 3 . LEFT: 10
I TAKE 1 . LEFT: 9
...
I TAKE 3 . LEFT: 1

STICKS REMAINING: 1
I

TAKE HOW MANY (1-3)? 1
YOU TOOK THE LAST STICK!
*** I WIN! ***

PLAY AGAIN? (1=YES, 0=NO)? 0
THANKS FOR PLAYING!
>
```

**Commands/controls:**

| Input | Action |
|-------|--------|
| 1, 2, or 3 | Number of sticks to take |
| 1 at "PLAY AGAIN?" | Restart |
| 0 at "PLAY AGAIN?" | Quit |

---

### 14.17 MASTERMIND.BAS

**Description:** A code-breaking game (also known as "Bulls and Cows"). The computer picks 4 unique digits from 1-6 and you have 10 guesses to crack the code.

**How to load:** Type the program (70 lines) or use Ctrl+P to paste.

**How to run:** Type `RUN`

**How to play:**

1. The computer picks a secret code of 4 unique digits (each 1-6)
2. You guess by entering 4 digits one at a time
3. After each guess you get feedback:
   - **Bulls** = correct digit in the correct position
   - **Cows** = correct digit in the wrong position
4. Use logic to narrow down the code
5. You win if you get 4 bulls (all correct) within 10 guesses

**Example session:**

```
>RUN
** MASTERMIND **

WANT INSTRUCTIONS? (1=YES, 0=NO)? 0

GUESS # 1
ENTER 4 DIGITS (1-6)
DIGIT 1? 1
DIGIT 2? 2
DIGIT 3? 3
DIGIT 4? 4
BULLS: 1  COWS: 1

GUESS # 2
ENTER 4 DIGITS (1-6)
DIGIT 1? 1
DIGIT 2? 3
DIGIT 3? 5
DIGIT 4? 6
BULLS: 1  COWS: 2

GUESS # 3
ENTER 4 DIGITS (1-6)
DIGIT 1? 5
DIGIT 2? 3
DIGIT 3? 1
DIGIT 4? 6
BULLS: 2  COWS: 2

GUESS # 4
ENTER 4 DIGITS (1-6)
DIGIT 1? 5
DIGIT 2? 6
DIGIT 3? 1
DIGIT 4? 3
BULLS: 4  COWS: 0

*** YOU CRACKED THE CODE! ***
IN 4 GUESSES!

PLAY AGAIN? (1=YES, 0=NO)? 0
>
```

**Deduction strategy:**

| Clue | Meaning |
|------|---------|
| 0 Bulls, 0 Cows | None of those digits are in the code |
| 0 Bulls, N Cows | N digits are correct but all misplaced |
| N Bulls, 0 Cows | N digits are in the right spot; no others present |
| 4 Bulls | You win! |

**Commands/controls:**

| Input | Action |
|-------|--------|
| Number 1-6 (for each digit) | Your guess |
| 1 at "PLAY AGAIN?" | Restart with new code |
| 0 at "PLAY AGAIN?" | Quit |

---

### 14.18 Loading Demos from Disk or Tape

If demos have been pre-loaded to the virtual disk, you can load them directly:

```basic
>LOAD GUESS
>RUN
```

If demos are available as tape files, use the cassette interface:

```
\C108R
TAPE FILENAME? WUMPUS
LOADING... DONE
\E2B3R
>RUN
```

To check what is available on disk:

```basic
>CATALOG
```

---

## 15. Troubleshooting

### 15.1 Common Issues and Solutions

**Problem: Display looks garbled or colors are wrong**

Solution: Ensure your terminal supports 256 colors. Check with:
```bash
echo $TERM
```
Should report `xterm-256color` or similar. Add to your shell profile if needed:
```bash
export TERM=xterm-256color
```

**Problem: Terminal is too small**

Solution: The emulator requires at least 40 columns and 24 rows. Resize your terminal window, or switch to 40-column mode via Ctrl+E if you are in 80-column mode.

**Problem: No response to keypresses**

Solution: The emulator may be in a tight loop. Try:
- **Ctrl+C** to break a running BASIC program
- **Ctrl+R** to warm reset to the monitor
- **Ctrl+Shift+R** for a cold reset (clears everything)

**Problem: Characters appear too slowly**

Solution: The authentic 16.7ms character delay is enabled by default. Press **Ctrl+F** to toggle fast mode, or start with `--fast`.

**Problem: Cannot save files**

Solution: Ensure the data directory exists and is writable:
```bash
ls -la ~/.apple1emu/
```
If missing, run `make install` or create it manually:
```bash
mkdir -p ~/.apple1emu/{roms,tapes,disks,snapshots,logs}
```

**Problem: BASIC program disappeared after reset**

Solution: A cold reset (Ctrl+Shift+R) clears all RAM. Use warm reset (Ctrl+R) to preserve memory. Better yet, save your work frequently:
```basic
>SAVE MYPROG
```

**Problem: "OUT OF MEM" error with a small program**

Solution: You may be in 4K RAM mode. Press Ctrl+E and increase RAM to 32K or 48K. A cold reset is required after changing RAM size.

### 15.2 Terminal Compatibility

| Terminal | Status | Notes |
|----------|--------|-------|
| Terminal.app (macOS) | Fully supported | Default macOS terminal |
| iTerm2 | Fully supported | Best color reproduction |
| Alacritty | Fully supported | GPU-accelerated |
| kitty | Fully supported | |
| VS Code Terminal | Supported | May need color settings |
| tmux | Supported | Ensure 256-color: `set -g default-terminal "xterm-256color"` |
| screen | Partial | May have color issues |
| SSH (remote) | Supported | Ensure TERM is set correctly |

**Minimum terminal capabilities required:**
- 256-color support (ANSI escape sequences)
- ncurses-compatible input handling
- At least 40x24 character size (80x24 for enhanced mode)
- UTF-8 encoding support (for box-drawing characters in menus)

### 15.3 Resetting if Things Go Wrong

If the emulator becomes unresponsive or behaves unexpectedly, use this escalation sequence:

| Level | Action | Effect |
|-------|--------|--------|
| 1 | **Ctrl+C** | Break running BASIC program |
| 2 | **Ctrl+R** | Warm reset — return to Woz Monitor (preserves RAM) |
| 3 | **Ctrl+Shift+R** | Cold reset — full power cycle (clears RAM) |
| 4 | **Ctrl+Q** | Quit emulator entirely |
| 5 | Kill process | `kill $(pgrep apple1emu)` from another terminal |

After a cold reset, the emulator returns to the Woz Monitor prompt as if freshly powered on. Any unsaved programs will be lost.

If the configuration file becomes corrupted, delete it and restart:

```bash
rm ~/.apple1emu/config.ini
./apple1emu
```

A fresh default configuration will be generated.

---

## Appendix A: 6502 Instruction Set

### Complete Instruction Reference

#### Load and Store Operations

| Mnemonic | Description | Modes | Flags Affected |
|----------|-------------|-------|----------------|
| LDA | Load Accumulator | imm, zp, zp,X, abs, abs,X, abs,Y, (ind,X), (ind),Y | N, Z |
| LDX | Load X Register | imm, zp, zp,Y, abs, abs,Y | N, Z |
| LDY | Load Y Register | imm, zp, zp,X, abs, abs,X | N, Z |
| STA | Store Accumulator | zp, zp,X, abs, abs,X, abs,Y, (ind,X), (ind),Y | — |
| STX | Store X Register | zp, zp,Y, abs | — |
| STY | Store Y Register | zp, zp,X, abs | — |

#### Arithmetic Operations

| Mnemonic | Description | Modes | Flags Affected |
|----------|-------------|-------|----------------|
| ADC | Add with Carry | imm, zp, zp,X, abs, abs,X, abs,Y, (ind,X), (ind),Y | N, V, Z, C |
| SBC | Subtract with Carry | imm, zp, zp,X, abs, abs,X, abs,Y, (ind,X), (ind),Y | N, V, Z, C |
| INC | Increment Memory | zp, zp,X, abs, abs,X | N, Z |
| DEC | Decrement Memory | zp, zp,X, abs, abs,X | N, Z |
| INX | Increment X | implied | N, Z |
| INY | Increment Y | implied | N, Z |
| DEX | Decrement X | implied | N, Z |
| DEY | Decrement Y | implied | N, Z |

#### Logical Operations

| Mnemonic | Description | Modes | Flags Affected |
|----------|-------------|-------|----------------|
| AND | Logical AND | imm, zp, zp,X, abs, abs,X, abs,Y, (ind,X), (ind),Y | N, Z |
| ORA | Logical OR | imm, zp, zp,X, abs, abs,X, abs,Y, (ind,X), (ind),Y | N, Z |
| EOR | Exclusive OR | imm, zp, zp,X, abs, abs,X, abs,Y, (ind,X), (ind),Y | N, Z |
| BIT | Bit Test | zp, abs | N, V, Z |

#### Shift and Rotate

| Mnemonic | Description | Modes | Flags Affected |
|----------|-------------|-------|----------------|
| ASL | Arithmetic Shift Left | A, zp, zp,X, abs, abs,X | N, Z, C |
| LSR | Logical Shift Right | A, zp, zp,X, abs, abs,X | N, Z, C |
| ROL | Rotate Left | A, zp, zp,X, abs, abs,X | N, Z, C |
| ROR | Rotate Right | A, zp, zp,X, abs, abs,X | N, Z, C |

#### Compare Operations

| Mnemonic | Description | Modes | Flags Affected |
|----------|-------------|-------|----------------|
| CMP | Compare Accumulator | imm, zp, zp,X, abs, abs,X, abs,Y, (ind,X), (ind),Y | N, Z, C |
| CPX | Compare X Register | imm, zp, abs | N, Z, C |
| CPY | Compare Y Register | imm, zp, abs | N, Z, C |

#### Branch Operations

| Mnemonic | Description | Condition | Cycles |
|----------|-------------|-----------|--------|
| BCC | Branch if Carry Clear | C = 0 | 2/3/4 |
| BCS | Branch if Carry Set | C = 1 | 2/3/4 |
| BEQ | Branch if Equal (Zero) | Z = 1 | 2/3/4 |
| BNE | Branch if Not Equal | Z = 0 | 2/3/4 |
| BMI | Branch if Minus | N = 1 | 2/3/4 |
| BPL | Branch if Plus | N = 0 | 2/3/4 |
| BVC | Branch if Overflow Clear | V = 0 | 2/3/4 |
| BVS | Branch if Overflow Set | V = 1 | 2/3/4 |

Branch instructions take 2 cycles if not taken, 3 if taken (same page), 4 if taken (page crossing).

#### Jump and Call Operations

| Mnemonic | Description | Modes | Cycles |
|----------|-------------|-------|--------|
| JMP | Jump | abs, (ind) | 3/5 |
| JSR | Jump to Subroutine | abs | 6 |
| RTS | Return from Subroutine | implied | 6 |
| RTI | Return from Interrupt | implied | 6 |
| BRK | Software Interrupt | implied | 7 |

#### Stack Operations

| Mnemonic | Description | Cycles |
|----------|-------------|--------|
| PHA | Push Accumulator | 3 |
| PLA | Pull Accumulator | 4 |
| PHP | Push Processor Status | 3 |
| PLP | Pull Processor Status | 4 |

#### Flag Operations

| Mnemonic | Description | Cycles |
|----------|-------------|--------|
| CLC | Clear Carry Flag | 2 |
| SEC | Set Carry Flag | 2 |
| CLD | Clear Decimal Mode | 2 |
| SED | Set Decimal Mode | 2 |
| CLI | Clear Interrupt Disable | 2 |
| SEI | Set Interrupt Disable | 2 |
| CLV | Clear Overflow Flag | 2 |

#### Register Transfer Operations

| Mnemonic | Description | Cycles | Flags |
|----------|-------------|--------|-------|
| TAX | Transfer A to X | 2 | N, Z |
| TAY | Transfer A to Y | 2 | N, Z |
| TXA | Transfer X to A | 2 | N, Z |
| TYA | Transfer Y to A | 2 | N, Z |
| TSX | Transfer SP to X | 2 | N, Z |
| TXS | Transfer X to SP | 2 | — |

#### Miscellaneous

| Mnemonic | Description | Cycles | Flags |
|----------|-------------|--------|-------|
| NOP | No Operation | 2 | — |

### Addressing Mode Reference

| Mode | Syntax | Bytes | Description |
|------|--------|-------|-------------|
| Immediate | `#$nn` | 2 | Operand is the byte value itself |
| Zero Page | `$nn` | 2 | Address in zero page ($00-$FF) |
| Zero Page,X | `$nn,X` | 2 | Zero page address + X register |
| Zero Page,Y | `$nn,Y` | 2 | Zero page address + Y register |
| Absolute | `$nnnn` | 3 | Full 16-bit address |
| Absolute,X | `$nnnn,X` | 3 | Absolute address + X register |
| Absolute,Y | `$nnnn,Y` | 3 | Absolute address + Y register |
| Indirect | `($nnnn)` | 3 | Address points to address (JMP only) |
| (Indirect,X) | `($nn,X)` | 2 | Zero page indexed indirect |
| (Indirect),Y | `($nn),Y` | 2 | Zero page indirect indexed |
| Accumulator | `A` | 1 | Operates on accumulator |
| Implied | — | 1 | No operand needed |
| Relative | `$nnnn` | 2 | Signed offset for branches (-128 to +127) |

---

## Appendix B: Apple 1 Memory Map

### Full Address Space (64 KB)

```
Address Range   Size    Contents                    Access
─────────────────────────────────────────────────────────────────
$0000-$00FF     256B    Zero Page                   RAM (R/W)
  $00-$01         2B      ACI start address ptr
  $02-$03         2B      ACI end address ptr
  $04-$23        32B      (Available for user)
  $24-$2B         8B      Woz Monitor variables
  $2C-$FF       212B      (Available for user/BASIC)

$0100-$01FF     256B    Hardware Stack              RAM (R/W)

$0200-$027F     128B    Woz Monitor Input Buffer    RAM (R/W)
$0280-$02FF     128B    (Available for user)        RAM (R/W)

$0300-$03FF     256B    Common ML Program Area      RAM (R/W)

$0400-$07FF      1KB    (Available for user)        RAM (R/W)

$0800-$0FFF      2KB    BASIC Program Storage       RAM (R/W)
                         (grows upward)

$1000-$7FFF     28KB    Extended RAM                RAM (R/W)
                         (32K/48K modes only)        [if configured]

$8000-$BFFF     16KB    Additional Extended RAM     RAM (R/W)
                         (48K mode only)             [if configured]

$C000-$C0FF     256B    ACI I/O Range               I/O
$C100-$C1FF     256B    ACI ROM                     ROM (R)

$C200-$CFFF    3.5KB    Unmapped (open bus)         Returns $00
$D000-$D00F      16B    Unmapped (open bus)         Returns $00

$D010           1B      KBD - Keyboard Data         I/O (R)
                         Bit 7: strobe
                         Bits 0-6: ASCII character
$D011           1B      KBDCR - Keyboard Status     I/O (R)
                         Bit 7: key available
$D012           1B      DSP - Display Data          I/O (W)
                         Bits 0-6: character to display
                         Bit 7 (R): display busy
$D013           1B      DSPCR - Display Control     I/O (R/W)

$D014-$DFFF    3.5KB    Unmapped (open bus)         Returns $00

$E000-$EFFF      4KB    Integer BASIC ROM           ROM (R)
  $E000                   Cold start entry point
  $E2B3                   Warm start entry point

$F000-$FEFF    3.75KB   Extended ROM / Utilities    ROM (R)
                         [Reserved for future use]

$FF00-$FFF9     250B    Woz Monitor ROM             ROM (R)
  $FF00                   Monitor entry point
  $FFEF                   ECHO subroutine
  $FFE5                   PRBYTE subroutine

$FFFA-$FFFB      2B    NMI Vector                  ROM (R)
                         Default: $0F00
$FFFC-$FFFD      2B    RESET Vector                ROM (R)
                         Points to: $FF00
$FFFE-$FFFF      2B    IRQ/BRK Vector              ROM (R)
                         Default: $0000
```

### RAM Configuration Comparison

```
Mode      Total    Range           Typical Use
───────────────────────────────────────────────────────────
4 KB       4 KB    $0000-$0FFF    Original Apple 1 (1976)
8 KB       8 KB    $0000-$1FFF    Apple 1 with expansion card
32 KB     32 KB    $0000-$7FFF    Typical hobbyist expansion
48 KB     48 KB    $0000-$BFFF    Maximum usable configuration
```

---

## Appendix C: BASIC Quick Reference Card

### Program Management

```
NEW .............. Erase program and variables
LIST ............. List entire program
LIST 50 .......... List from line 50
LIST 10,50 ....... List lines 10 through 50
RUN .............. Execute program from first line
RUN 100 .......... Execute from line 100
CON .............. Continue after STOP
DEL 10,50 ........ Delete lines 10 through 50
AUTO 10,10 ....... Auto-number starting at 10, step 10
```

### Variables and Assignments

```
LET A = 42 ....... Assign integer (LET is optional)
A = 42 ........... Same as above
DIM A$(50) ....... Dimension string (50 chars max)
DIM B(100) ....... Dimension integer array (101 elements)
A$ = "HELLO" ..... Assign string
CLR .............. Clear all variables
```

### Input and Output

```
PRINT A .......... Print value of A
PRINT "TEXT" ..... Print string literal
PRINT A;B ........ Print A and B with no space
PRINT A,B ........ Print A and B in columns
PRINT ............ Print blank line
INPUT A .......... Read integer from keyboard
INPUT "? ";A ..... Print prompt, read integer
INPUT A$ ......... Read string from keyboard
TAB(N) ........... Move cursor to column N
```

### Control Flow

```
GOTO 100 ......... Jump to line 100
IF A>B THEN 200 .. Conditional branch
IF A=1 THEN PRINT "YES"  Conditional statement
FOR I=1 TO 10 .... Begin counted loop
FOR I=10 TO 0 STEP -1 .. Loop with step
NEXT I ........... End of loop
GOSUB 500 ........ Call subroutine
RETURN ........... Return from subroutine
END .............. End program
STOP ............. Stop (resumable with CON)
```

### Arithmetic Operators

```
+   Addition          -   Subtraction
*   Multiplication    /   Integer division
MOD Modulo            ^   Exponentiation
```

### Comparison Operators

```
=   Equal             <>  Not equal
<   Less than         >   Greater than
<=  Less or equal     >=  Greater or equal
```

### Logical Operators

```
AND   Logical AND     OR   Logical OR
NOT   Logical NOT
```

### Built-in Functions

```
ABS(N) ........... Absolute value
SGN(N) ........... Sign (-1, 0, 1)
RND(N) ........... Random number 0 to N-1
PEEK(A) .......... Read memory at address A
LEN(A$) .......... String length
ASC(A$) .......... ASCII code of first char
CHR$(N) .......... Character from code
LEFT$(A$,N) ...... First N characters
RIGHT$(A$,N) ..... Last N characters
MID$(A$,S,N) ..... Substring
STR$(N) .......... Number to string
VAL(A$) .......... String to number
```

### Machine Interface

```
POKE A,V ......... Write byte V at address A
CALL A ........... Execute ML at address A
CALL -151 ........ Return to Woz Monitor
```

### Disk Operations

```
CATALOG .......... List files on disk
SAVE name ........ Save program to disk
LOAD name ........ Load program from disk
DELETE name ....... Delete file from disk
RENAME old,new ... Rename a file
BSAVE name,A$addr,L$len  Save binary
BLOAD name ....... Load binary at stored address
```

### Debugging

```
TRACE ............ Show line numbers during execution
NOTRACE .......... Disable trace
DSP A ............ Display var A on each change
```

### Lo-Res Graphics (if available)

```
GR ............... Switch to graphics mode
TEXT ............. Switch to text mode
COLOR=N .......... Set color (0-15)
PLOT X,Y ......... Plot point at X,Y
HLIN X1,X2 AT Y .. Horizontal line
VLIN Y1,Y2 AT X .. Vertical line
SCRN(X,Y) ........ Read color at X,Y
```

### Shorthand and Tips

```
? ................ Shorthand for PRINT
E000R ............ Enter BASIC (from monitor)
E2B3R ............ Re-enter BASIC (warm, from monitor)
Ctrl+C ........... Break running program
Ctrl+R ........... Reset to monitor
```

---

*Apple 1 Emulator v0.1.0 — User Manual*
*Based on the original Apple 1 hardware designed by Steve Wozniak, 1976.*
*This emulator is an educational project and is not affiliated with Apple Inc.*
