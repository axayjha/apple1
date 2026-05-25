#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod emulator;

use std::sync::Mutex;
use std::thread;
use std::time::Duration;
use tauri::State;
use emulator::Emulator;

struct EmulatorState(Mutex<Emulator>);

#[tauri::command]
fn step_frame(state: State<EmulatorState>) {
    let mut emu = state.0.lock().unwrap();
    emu.step_frame();
}

#[tauri::command]
fn send_key(state: State<EmulatorState>, ch: u8) {
    let mut emu = state.0.lock().unwrap();
    emu.key_press(ch);
}

#[tauri::command]
fn send_break(state: State<EmulatorState>) {
    let mut emu = state.0.lock().unwrap();
    emu.break_signal();
}

#[tauri::command]
fn reset(state: State<EmulatorState>) {
    let mut emu = state.0.lock().unwrap();
    emu.reset();
}

#[tauri::command]
fn get_screen(state: State<EmulatorState>) -> Vec<String> {
    let emu = state.0.lock().unwrap();
    let screen = emu.get_screen();
    screen.iter().map(|row| {
        row.iter().map(|&ch| ch as char).collect::<String>()
    }).collect()
}

#[tauri::command]
fn get_cursor(state: State<EmulatorState>) -> (usize, usize) {
    let emu = state.0.lock().unwrap();
    emu.get_cursor()
}

#[tauri::command]
fn load_demo(state: State<EmulatorState>, program: String) {
    let mut emu = state.0.lock().unwrap();
    emu.reset();

    // First enter BASIC by sending E000R + CR
    let enter_basic = "E000R\r";
    for ch in enter_basic.bytes() {
        let mapped = if ch == b'\r' { 0x0D } else { ch };
        emu.key_press(mapped);
        // Run some frames to let the monitor process
        for _ in 0..10 {
            emu.step_frame();
        }
    }

    // Wait for BASIC to activate
    for _ in 0..30 {
        emu.step_frame();
    }

    // Now feed the BASIC program line by line
    for ch in program.bytes() {
        let mapped = if ch == b'\n' { 0x0D } else { ch.to_ascii_uppercase() };
        if mapped < 0x0D || (mapped > 0x0D && mapped < 0x20) { continue; }
        emu.key_press(mapped);
        // Small step between chars
        for _ in 0..3 {
            emu.step_frame();
        }
    }

    // Send RUN command
    thread::sleep(Duration::from_millis(10));
    for ch in b"RUN\r" {
        let mapped = if *ch == b'\r' { 0x0D } else { *ch };
        emu.key_press(mapped);
        for _ in 0..3 {
            emu.step_frame();
        }
    }
}

#[tauri::command]
fn get_demos() -> Vec<(String, String)> {
    vec![
        ("hello.bas".to_string(), "Hello World".to_string()),
        ("fibonacci.bas".to_string(), "Fibonacci Numbers".to_string()),
        ("guess.bas".to_string(), "Number Guessing Game".to_string()),
        ("sieve.bas".to_string(), "Sieve of Eratosthenes".to_string()),
        ("adventure.bas".to_string(), "Text Adventure".to_string()),
        ("lunarlander.bas".to_string(), "Lunar Lander".to_string()),
        ("wumpus.bas".to_string(), "Hunt the Wumpus".to_string()),
        ("hamurabi.bas".to_string(), "Hamurabi (Kingdom)".to_string()),
        ("life.bas".to_string(), "Conway's Game of Life".to_string()),
        ("startrek.bas".to_string(), "Star Trek".to_string()),
        ("nim.bas".to_string(), "Nim".to_string()),
        ("mastermind.bas".to_string(), "Mastermind".to_string()),
        ("star.bas".to_string(), "Star Pattern".to_string()),
        ("sort.bas".to_string(), "Bubble Sort Demo".to_string()),
        ("calendar.bas".to_string(), "Calendar".to_string()),
    ]
}

fn main() {
    let mut emu = Emulator::new();
    emu.init();

    tauri::Builder::default()
        .plugin(tauri_plugin_shell::init())
        .manage(EmulatorState(Mutex::new(emu)))
        .invoke_handler(tauri::generate_handler![
            step_frame,
            send_key,
            send_break,
            reset,
            get_screen,
            get_cursor,
            load_demo,
            get_demos,
        ])
        .run(tauri::generate_context!())
        .expect("error running apple1 monitor");
}
