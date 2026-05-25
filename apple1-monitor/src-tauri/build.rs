fn main() {
    // Compile the Apple 1 emulator C sources into a static library
    cc::Build::new()
        .files(&[
            "../../src/cpu.c",
            "../../src/memory.c",
            "../../src/pia.c",
            "../../src/basic.c",
            "../../src/roms_builtin.c",
            "../../src/log.c",
            "../../src/aci.c",
            "../../src/disk.c",
            "../../src/config.c",
            "../../src/snapshot.c",
            "../../src/assembler.c",
            "../../src/cpu_bridge.c",
        ])
        .include("../../src")
        .flag("-std=c17")
        .flag("-DAPPLE1_LOG_DISABLE")
        .compile("apple1core");

    tauri_build::build();
}
