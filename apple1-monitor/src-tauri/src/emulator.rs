use std::sync::Mutex;

// C FFI declarations matching our header files
extern "C" {
    // cpu.h
    fn cpu_init(cpu: *mut CpuState, read: unsafe extern "C" fn(u16) -> u8, write: unsafe extern "C" fn(u16, u8));
    fn cpu_reset(cpu: *mut CpuState);
    fn cpu_step(cpu: *mut CpuState) -> u8;
    fn cpu_irq(cpu: *mut CpuState);

    // memory.h
    fn mem_init(ram_kb: i32);
    fn mem_read(addr: u16) -> u8;
    fn mem_write(addr: u16, val: u8);

    // pia.h
    fn pia_init();
    fn pia_key_press(ch: u8);
    fn pia_set_display_callback(cb: Option<extern "C" fn(u8)>);

    // roms_builtin.h
    fn roms_load_builtin();

    // basic.h
    fn basic_init();
    fn basic_cold_start();
    fn basic_warm_start();
    fn basic_is_running() -> bool;
    fn basic_step();
    fn basic_input_char(ch: u8);
    fn basic_break();
    fn basic_deactivate();

    // aci.h
    fn aci_init();
    fn aci_is_trapped(pc: u16) -> bool;
    fn aci_handle_trap(cpu: *mut CpuState);

    // disk.h
    fn disk_init();
}

// The CPU state struct — we don't need to know the exact layout,
// just need enough size. The actual struct is ~300 bytes.
#[repr(C)]
pub struct CpuState {
    _data: [u8; 4096], // oversized to be safe
}

impl CpuState {
    fn new() -> Self {
        Self { _data: [0u8; 4096] }
    }
}

// Access the PC field (offset depends on struct layout — we use a C bridge)
#[allow(dead_code)]
extern "C" {
    fn cpu_get_pc(cpu: *const CpuState) -> u16;
    fn cpu_get_a(cpu: *const CpuState) -> u8;
    fn cpu_get_x(cpu: *const CpuState) -> u8;
    fn cpu_get_y(cpu: *const CpuState) -> u8;
    fn cpu_get_sp(cpu: *const CpuState) -> u8;
    fn cpu_get_status(cpu: *const CpuState) -> u8;
    fn cpu_set_pc(cpu: *mut CpuState, pc: u16);
}

// Display buffer — captures output from PIA
static SCREEN: Mutex<[[u8; 40]; 24]> = Mutex::new([[b' '; 40]; 24]);
static CURSOR: Mutex<(usize, usize)> = Mutex::new((0, 0));

extern "C" fn display_callback(ch: u8) {
    let ch = ch & 0x7F;
    let mut screen = SCREEN.lock().unwrap();
    let mut cursor = CURSOR.lock().unwrap();

    if ch == 0x0D {
        // Carriage return
        cursor.0 = 0;
        cursor.1 += 1;
        if cursor.1 >= 24 {
            // Scroll up
            for row in 0..23 {
                screen[row] = screen[row + 1];
            }
            screen[23] = [b' '; 40];
            cursor.1 = 23;
        }
    } else if ch >= 0x20 && ch <= 0x5F {
        let (col, row) = *cursor;
        if row < 24 && col < 40 {
            screen[row][col] = ch;
            cursor.0 += 1;
            if cursor.0 >= 40 {
                cursor.0 = 0;
                cursor.1 += 1;
                if cursor.1 >= 24 {
                    for row in 0..23 {
                        screen[row] = screen[row + 1];
                    }
                    screen[23] = [b' '; 40];
                    cursor.1 = 23;
                }
            }
        }
    }
}

pub struct Emulator {
    cpu: CpuState,
    initialized: bool,
}

impl Emulator {
    pub fn new() -> Self {
        Self {
            cpu: CpuState::new(),
            initialized: false,
        }
    }

    pub fn init(&mut self) {
        unsafe {
            mem_init(32);
            pia_init();
            roms_load_builtin();
            basic_init();
            aci_init();
            disk_init();
            pia_set_display_callback(Some(display_callback));
            cpu_init(&mut self.cpu, mem_read, mem_write);
            cpu_reset(&mut self.cpu);
        }
        self.initialized = true;
    }

    pub fn step_frame(&mut self) {
        if !self.initialized { return; }
        let cycles_per_frame: i32 = 17045;
        let mut cycles_done: i32 = 0;

        unsafe {
            while cycles_done < cycles_per_frame {
                let pc = cpu_get_pc(&self.cpu);

                // BASIC traps
                if pc == 0xE000 && !basic_is_running() {
                    basic_cold_start();
                    cpu_set_pc(&mut self.cpu, 0xE003);
                    break;
                }
                if pc == 0xE2B3 && !basic_is_running() {
                    basic_warm_start();
                    cpu_set_pc(&mut self.cpu, 0xE2B6);
                    break;
                }

                // ACI trap
                if aci_is_trapped(pc) {
                    aci_handle_trap(&mut self.cpu);
                    continue;
                }

                // BASIC step
                if basic_is_running() {
                    basic_step();
                    cycles_done += 50;
                    continue;
                }

                cycles_done += cpu_step(&mut self.cpu) as i32;
            }
        }
    }

    pub fn key_press(&mut self, ch: u8) {
        unsafe {
            if basic_is_running() {
                basic_input_char(ch);
            } else {
                pia_key_press(ch);
            }
        }
    }

    pub fn break_signal(&mut self) {
        unsafe {
            if basic_is_running() {
                basic_break();
            } else {
                cpu_irq(&mut self.cpu);
            }
        }
    }

    pub fn reset(&mut self) {
        unsafe {
            basic_deactivate();
            cpu_reset(&mut self.cpu);
        }
    }

    pub fn get_screen(&self) -> [[u8; 40]; 24] {
        *SCREEN.lock().unwrap()
    }

    pub fn get_cursor(&self) -> (usize, usize) {
        *CURSOR.lock().unwrap()
    }
}
