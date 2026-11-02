#!/usr/bin/env python3
"""
benchmark_flash.py - Automated Flash Footprint Benchmark for WickedPostfix

Measures library-only and transitive flash memory footprints across target embedded
architectures (STM32G4 / Cortex-M4F and ESP32-S3 / Xtensa LX7) for Full, Slim,
CMSIS-DSP, and CORDIC configurations.

All compiler paths and platform SDKs are discovered dynamically or configurable via CLI flags:
  --arm-cxx, --arm-size
  --xtensa-cxx, --xtensa-size
  --stm32cube-fw
  --protocol-root
"""

import argparse
import glob
import os
import shutil
import subprocess
import sys
import tempfile

BASELINE_SRC = """\
volatile float g_out = 0.0f;
int main(void) {
    g_out = 1.0f;
    return 0;
}
"""

HARNESS_SRC = """\
#include "WickedPostfix.h"

volatile float g_out = 0.0f;
volatile WickedEvalStatus_t g_status;

int main(void) {
    uint8_t buf[64] = {0};
    const uint8_t* prog = nullptr;
    uint16_t size = 0;
    WickedPostfixRead(buf, sizeof(buf), &prog, &size);
    uint16_t min_cap = 0, final_sz = 0;
    g_status = WickedPostfixValidate(prog, size, 4, &min_cap, &final_sz);
    float stack[16] = {1.0f, 2.0f, 3.0f};
    WickedPostfixEvaluate(prog, size, 3, stack);
    g_out = stack[0];
    const char* name = WickedPostfixOpToString(WickedPostfixOp_Add);
    if (name[0] == 'A') g_out += 1.0f;
    return 0;
}
"""

BIN_EXT = ".exe" if os.name == "nt" else ""

def find_file_recursive(patterns):
    for pat in patterns:
        matches = glob.glob(pat, recursive=True)
        if matches:
            matches.sort(reverse=True)
            return matches[0]
    return None

def find_directory_recursive(patterns):
    for pat in patterns:
        matches = [d for d in glob.glob(pat, recursive=True) if os.path.isdir(d)]
        if matches:
            matches.sort(reverse=True)
            return matches[0]
    return None

def discover_arm_toolchain(cli_cxx, cli_size):
    cxx = cli_cxx or os.environ.get("ARM_NONE_EABI_GXX") or shutil.which("arm-none-eabi-g++")
    if not cxx:
        home = os.path.expanduser("~")
        bin_name = f"arm-none-eabi-g++{BIN_EXT}"
        patterns = [
            # Windows standard installation locations
            f"C:/ST/STM32CubeIDE*/**/tools/bin/{bin_name}",
            f"C:/Program Files*/**/{bin_name}",
            # Linux / macOS standard installation locations
            f"/opt/st/stm32cubeide*/**/tools/bin/{bin_name}",
            f"/opt/gcc-arm-none-eabi*/**/bin/{bin_name}",
            f"/opt/arm*/**/bin/{bin_name}",
            f"/usr/local/**/bin/{bin_name}",
            os.path.join(home, f".local/**/bin/{bin_name}"),
            os.path.join(home, f"bin/{bin_name}"),
        ]
        cxx = find_file_recursive(patterns)

    size = cli_size or os.environ.get("ARM_NONE_EABI_SIZE") or shutil.which("arm-none-eabi-size")
    if not size and cxx:
        candidate = os.path.join(os.path.dirname(cxx), f"arm-none-eabi-size{BIN_EXT}")
        if os.path.exists(candidate):
            size = candidate
    return cxx, size

def discover_xtensa_toolchain(cli_cxx, cli_size):
    cxx = cli_cxx or os.environ.get("XTENSA_ESP32S3_ELF_GXX") or shutil.which("xtensa-esp32s3-elf-g++")
    if not cxx:
        home = os.path.expanduser("~")
        bin_name = f"xtensa-esp32s3-elf-g++{BIN_EXT}"
        patterns = [
            # Windows standard installation locations
            f"C:/Espressif/tools/xtensa-esp-elf/**/bin/{bin_name}",
            # Default ESP-IDF install location on both Linux & Windows (~/.espressif)
            os.path.join(home, f".espressif/tools/xtensa-esp-elf/**/bin/{bin_name}"),
            # Linux common installation locations
            f"/opt/espressif/tools/xtensa-esp-elf/**/bin/{bin_name}",
            f"/opt/xtensa-esp-elf/**/bin/{bin_name}",
        ]
        cxx = find_file_recursive(patterns)

    size = cli_size or os.environ.get("XTENSA_ESP32S3_ELF_SIZE") or shutil.which("xtensa-esp32s3-elf-size")
    if not size and cxx:
        candidate = os.path.join(os.path.dirname(cxx), f"xtensa-esp32s3-elf-size{BIN_EXT}")
        if os.path.exists(candidate):
            size = candidate
    return cxx, size

def discover_stm32cube_fw(cli_path):
    path = cli_path or os.environ.get("STM32CUBE_FW_PATH") or os.environ.get("STM32CUBE_PATH")
    if not path or not os.path.isdir(path):
        home = os.path.expanduser("~")
        patterns = [
            # Default STM32CubeMX / CubeIDE firmware repository on both Windows & Linux
            os.path.join(home, "STM32Cube/Repository/STM32Cube_FW_G4_*"),
            # Windows ST repository path
            "C:/ST/STM32Cube/Repository/STM32Cube_FW_G4_*",
            # Linux ST repository paths
            "/opt/st/STM32Cube/Repository/STM32Cube_FW_G4_*",
            "/opt/STM32Cube/Repository/STM32Cube_FW_G4_*",
            "/usr/local/ST/STM32Cube/Repository/STM32Cube_FW_G4_*",
        ]
        path = find_directory_recursive(patterns)
    return path

def parse_size_berkeley(output):
    """Parses standard `size <elf>` Berkeley format: text, data, bss"""
    lines = [l.strip() for l in output.strip().splitlines() if l.strip()]
    parts = lines[-1].split()
    return int(parts[0]), int(parts[1]), int(parts[2])

def parse_size_sysv(output):
    """Parses `size -A <obj>` System V format: section -> size"""
    sections = {}
    for line in output.strip().splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[1].isdigit():
            sections[parts[0]] = int(parts[1])
    return sections

def calc_library_flash(sections):
    """
    Computes total flash used by reader and evaluator, excluding writer functions.
    Flash = .text.* + .rodata.* + .literal.* (for Xtensa) minus WickedPostfixWriter::Write.
    """
    total = 0
    for name, size in sections.items():
        if "WickedPostfixWriter5Write" in name:
            continue
        if name.startswith(".text") or name.startswith(".rodata") or name.startswith(".literal"):
            total += size
    return total

def run_cmd(cmd, verbose=False):
    """Runs command suppressing stderr linker warnings unless verbose is enabled."""
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if res.returncode != 0:
        print(f"Command failed with exit code {res.returncode}:\n{' '.join(cmd)}\n{res.stderr}", file=sys.stderr)
        res.check_returncode()
    if verbose and res.stderr:
        print(res.stderr, file=sys.stderr)
    return res.stdout

def run_benchmark_for_config(cfg, temp_dir, src_postfix, base_src, harn_src, verbose=False):
    cxx = cfg["cxx"]
    size_bin = cfg["size"]
    cflags = cfg["cflags"]
    ldflags = cfg["ldflags"]
    base_flash = cfg["base_flash"]

    obj = os.path.join(temp_dir, f"{cfg['key']}.o")
    elf = os.path.join(temp_dir, f"{cfg['key']}.elf")

    # 1. Compile WickedPostfix object
    compile_cmd = [cxx] + cflags + ["-c", src_postfix, "-o", obj]
    run_cmd(compile_cmd, verbose=verbose)

    # 2. Extract sections from object file
    sysv_out = run_cmd([size_bin, "-A", obj], verbose=verbose)
    sections = parse_size_sysv(sysv_out)
    lib_flash = calc_library_flash(sections)

    # 3. Link test harness
    link_cmd = [cxx] + cflags + [harn_src, obj, "-o", elf] + ldflags
    run_cmd(link_cmd, verbose=verbose)

    # 4. Measure linked ELF size
    berk_out = run_cmd([size_bin, elf], verbose=verbose)
    text_sz, data_sz, _ = parse_size_berkeley(berk_out)
    harn_flash = text_sz + data_sz
    transitive_flash = harn_flash - base_flash

    return {
        "name": cfg["name"],
        "key": cfg["key"],
        "lib_flash": lib_flash,
        "transitive_flash": transitive_flash,
        "harness_flash": harn_flash,
        "base_flash": base_flash,
        "sections": sections,
    }

def print_markdown_tables(results):
    print("\n### Formatted Markdown Tables for Documentation:\n")

    # Full Profile Table
    print("#### Full Profile (Default: All Opcodes)\n")
    print("| Target Architecture | Library-Only Flash (`.text` + `.rodata`) | Transitive Flash (Library + `libm` Delta) |")
    print("| :--- | :---: | :---: |")
    if "stm32_full" in results:
        r = results["stm32_full"]
        print(f"| **STM32G4** (Cortex-M4F) | **{r['lib_flash']:,} B** (~{r['lib_flash']/1024:.1f} KB) | **{r['transitive_flash']:,} B** (~{r['transitive_flash']/1024:.1f} KB) |")
    if "esp32s3_full" in results:
        r = results["esp32s3_full"]
        print(f"| **ESP32-S3** (Xtensa LX7) | **{r['lib_flash']:,} B** (~{r['lib_flash']/1024:.1f} KB) | **{r['transitive_flash']:,} B** (~{r['transitive_flash']/1024:.1f} KB) |")

    # Slim Profile Table
    print("\n#### Slim Profile (`-DWICKED_POSTFIX_SLIM`)\n")
    print("| Target Architecture | Library-Only Flash (`.text` + `.rodata`) | Transitive Flash (Library + `libm` Delta) | Transitive Savings vs. Full |")
    print("| :--- | :---: | :---: | :---: |")
    if "stm32_slim" in results:
        r = results["stm32_slim"]
        full_r = results.get("stm32_full")
        savings = f"{full_r['transitive_flash'] - r['transitive_flash']:,} B (~{(full_r['transitive_flash'] - r['transitive_flash'])/1024:.1f} KB)" if full_r else "—"
        print(f"| **STM32G4** (Cortex-M4F) | **{r['lib_flash']:,} B** (~{r['lib_flash']/1024:.1f} KB) | **{r['transitive_flash']:,} B** (~{r['transitive_flash']/1024:.1f} KB) | **{savings}** |")
    if "esp32s3_slim" in results:
        r = results["esp32s3_slim"]
        full_r = results.get("esp32s3_full")
        savings = f"{full_r['transitive_flash'] - r['transitive_flash']:,} B (~{(full_r['transitive_flash'] - r['transitive_flash'])/1024:.1f} KB)" if full_r else "—"
        print(f"| **ESP32-S3** (Xtensa LX7) | **{r['lib_flash']:,} B** (~{r['lib_flash']/1024:.1f} KB) | **{r['transitive_flash']:,} B** (~{r['transitive_flash']/1024:.1f} KB) | **{savings}** |")

    # CMSIS-DSP Table
    if "stm32_slim_cmsis_dsp" in results:
        print("\n#### Slim Profile with CMSIS-DSP (`-DWICKED_POSTFIX_SLIM -DWICKED_USE_CMSIS_DSP`)\n")
        print("| Target Architecture | Library-Only Flash (`.text` + `.rodata`) | Transitive Flash (Library + CMSIS-DSP + `libm` Delta) | Transitive Savings vs. Slim |")
        print("| :--- | :---: | :---: | :---: |")
        r = results["stm32_slim_cmsis_dsp"]
        slim_r = results.get("stm32_slim")
        savings = f"{slim_r['transitive_flash'] - r['transitive_flash']:,} B (~{(slim_r['transitive_flash'] - r['transitive_flash'])/1024:.1f} KB)" if slim_r else "—"
        print(f"| **STM32G4** (Cortex-M4F) | **{r['lib_flash']:,} B** (~{r['lib_flash']/1024:.1f} KB) | **{r['transitive_flash']:,} B** (~{r['transitive_flash']/1024:.1f} KB) | **{savings}** |")

    # CORDIC Table
    if "stm32_slim_cordic" in results:
        print("\n#### Slim Profile with Hardware CORDIC (`-DWICKED_POSTFIX_SLIM -DWICKED_USE_STM32_CORDIC`)\n")
        print("| Target Architecture | Library-Only Flash (`.text` + `.rodata`) | Transitive Flash (Library + `libm` Delta) | Transitive Savings vs. Slim |")
        print("| :--- | :---: | :---: | :---: |")
        r = results["stm32_slim_cordic"]
        slim_r = results.get("stm32_slim")
        savings = f"{slim_r['transitive_flash'] - r['transitive_flash']:,} B (~{(slim_r['transitive_flash'] - r['transitive_flash'])/1024:.1f} KB)" if slim_r else "—"
        print(f"| **STM32G4** (Cortex-M4F) | **{r['lib_flash']:,} B** (~{r['lib_flash']/1024:.1f} KB) | **{r['transitive_flash']:,} B** (~{r['transitive_flash']/1024:.1f} KB) | **{savings}** |")

def main():
    parser = argparse.ArgumentParser(description="Automated Flash Footprint Benchmark for WickedPostfix")
    parser.add_argument("--arm-cxx", help="Path to arm-none-eabi-g++")
    parser.add_argument("--arm-size", help="Path to arm-none-eabi-size")
    parser.add_argument("--xtensa-cxx", help="Path to xtensa-esp32s3-elf-g++")
    parser.add_argument("--xtensa-size", help="Path to xtensa-esp32s3-elf-size")
    parser.add_argument("--stm32cube-fw", help="Path to STM32Cube G4 firmware root")
    parser.add_argument("--protocol-root", help="Path to WickedWinchProtocol repository root")
    parser.add_argument("--verbose", action="store_true", help="Print verbose linker outputs")
    args = parser.parse_args()

    # Determine protocol root
    if args.protocol_root:
        protocol_root = os.path.abspath(args.protocol_root)
    else:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        protocol_root = os.path.abspath(os.path.join(script_dir, ".."))

    src_postfix = os.path.join(protocol_root, "cpp", "src", "WickedPostfix.cc")
    include_dir = os.path.join(protocol_root, "cpp", "include")

    if not os.path.isfile(src_postfix):
        print(f"Error: Could not find WickedPostfix.cc at {src_postfix}", file=sys.stderr)
        sys.exit(1)

    arm_cxx, arm_size = discover_arm_toolchain(args.arm_cxx, args.arm_size)
    xtensa_cxx, xtensa_size = discover_xtensa_toolchain(args.xtensa_cxx, args.xtensa_size)
    cube_fw = discover_stm32cube_fw(args.stm32cube_fw)

    print("=" * 70)
    print("WickedPostfix Flash Memory Footprint Benchmark")
    print("=" * 70)
    print(f"Protocol Root:    {protocol_root}")
    print(f"ARM Toolchain:    {arm_cxx or 'NOT FOUND'} (size: {arm_size or 'NOT FOUND'})")
    print(f"Xtensa Toolchain: {xtensa_cxx or 'NOT FOUND'} (size: {xtensa_size or 'NOT FOUND'})")
    print(f"STM32Cube FW:     {cube_fw or 'NOT FOUND'}")
    print("=" * 70)

    if not arm_cxx and not xtensa_cxx:
        print("Error: No cross-compilers found. Provide paths via --arm-cxx / --xtensa-cxx.", file=sys.stderr)
        sys.exit(1)

    results = {}

    with tempfile.TemporaryDirectory() as td:
        base_src = os.path.join(td, "baseline.cc")
        harn_src = os.path.join(td, "harness.cc")
        with open(base_src, "w") as f:
            f.write(BASELINE_SRC)
        with open(harn_src, "w") as f:
            f.write(HARNESS_SRC)

        # Baseline for ARM
        arm_base_flash = 0
        if arm_cxx and arm_size:
            base_elf_arm = os.path.join(td, "base_arm.elf")
            run_cmd([
                arm_cxx, "-mcpu=cortex-m4", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard", "-mthumb",
                "-std=c++20", "-Os", "-fno-exceptions", "-fno-rtti", "-ffunction-sections", "-fdata-sections",
                "--specs=nano.specs", "--specs=nosys.specs", "-Wl,--gc-sections",
                base_src, "-o", base_elf_arm, "-lm"
            ], verbose=args.verbose)
            t, d, _ = parse_size_berkeley(run_cmd([arm_size, base_elf_arm]))
            arm_base_flash = t + d

        # Baseline for Xtensa
        xt_base_flash = 0
        if xtensa_cxx and xtensa_size:
            base_elf_xt = os.path.join(td, "base_xt.elf")
            run_cmd([
                xtensa_cxx, "-mlongcalls",
                "-std=c++20", "-Os", "-fno-exceptions", "-fno-rtti", "-ffunction-sections", "-fdata-sections",
                "--specs=nosys.specs", "-Wl,--gc-sections",
                base_src, "-o", base_elf_xt, "-lm"
            ], verbose=args.verbose)
            t, d, _ = parse_size_berkeley(run_cmd([xtensa_size, base_elf_xt]))
            xt_base_flash = t + d

        # Configure targets
        configs = []
        if arm_cxx and arm_size:
            configs.append({
                "key": "stm32_full",
                "name": "STM32G4 Full",
                "cxx": arm_cxx, "size": arm_size, "base_flash": arm_base_flash,
                "cflags": ["-mcpu=cortex-m4", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard", "-mthumb",
                           "-std=c++20", "-Os", "-fno-exceptions", "-fno-rtti", "-ffunction-sections", "-fdata-sections",
                           f"-I{include_dir}"],
                "ldflags": ["--specs=nano.specs", "--specs=nosys.specs", "-Wl,--gc-sections", "-lm"]
            })
            configs.append({
                "key": "stm32_full_fast_exp",
                "name": "STM32G4 Full + Fast Exp",
                "cxx": arm_cxx, "size": arm_size, "base_flash": arm_base_flash,
                "cflags": ["-mcpu=cortex-m4", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard", "-mthumb",
                           "-std=c++20", "-Os", "-fno-exceptions", "-fno-rtti", "-ffunction-sections", "-fdata-sections",
                           "-DWICKED_USE_FAST_EXP_MATH", f"-I{include_dir}"],
                "ldflags": ["--specs=nano.specs", "--specs=nosys.specs", "-Wl,--gc-sections", "-lm"]
            })
            configs.append({
                "key": "stm32_slim",
                "name": "STM32G4 Slim",
                "cxx": arm_cxx, "size": arm_size, "base_flash": arm_base_flash,
                "cflags": ["-mcpu=cortex-m4", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard", "-mthumb",
                           "-std=c++20", "-Os", "-fno-exceptions", "-fno-rtti", "-ffunction-sections", "-fdata-sections",
                           "-DWICKED_POSTFIX_SLIM", f"-I{include_dir}"],
                "ldflags": ["--specs=nano.specs", "--specs=nosys.specs", "-Wl,--gc-sections", "-lm"]
            })

            if cube_fw:
                cmsis_dsp_inc = os.path.join(cube_fw, "Drivers", "CMSIS", "DSP", "Include")
                cmsis_core_inc = os.path.join(cube_fw, "Drivers", "CMSIS", "Include")
                cmsis_lib_dir = os.path.join(cube_fw, "Drivers", "CMSIS", "DSP", "Lib", "GCC")
                stm32g4_dev_inc = os.path.join(cube_fw, "Drivers", "CMSIS", "Device", "ST", "STM32G4xx", "Include")

                if os.path.isdir(cmsis_dsp_inc) and os.path.isdir(cmsis_lib_dir):
                    configs.append({
                        "key": "stm32_slim_cmsis_dsp",
                        "name": "STM32G4 Slim + CMSIS-DSP",
                        "cxx": arm_cxx, "size": arm_size, "base_flash": arm_base_flash,
                        "cflags": ["-mcpu=cortex-m4", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard", "-mthumb",
                                   "-std=c++20", "-Os", "-fno-exceptions", "-fno-rtti", "-ffunction-sections", "-fdata-sections",
                                   "-fpermissive", "-DARM_MATH_CM4", "-DWICKED_POSTFIX_SLIM", "-DWICKED_USE_CMSIS_DSP",
                                   f"-I{include_dir}", f"-I{cmsis_dsp_inc}", f"-I{cmsis_core_inc}"],
                        "ldflags": ["--specs=nano.specs", "--specs=nosys.specs", "-Wl,--gc-sections",
                                    f"-L{cmsis_lib_dir}", "-larm_cortexM4lf_math", "-lm"]
                    })
                else:
                    print(f"Note: CMSIS-DSP include/lib not found in {cube_fw}; skipping CMSIS-DSP benchmark.")

                if os.path.isdir(stm32g4_dev_inc):
                    configs.append({
                        "key": "stm32_slim_cordic",
                        "name": "STM32G4 Slim + CORDIC",
                        "cxx": arm_cxx, "size": arm_size, "base_flash": arm_base_flash,
                        "cflags": ["-mcpu=cortex-m4", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard", "-mthumb",
                                   "-std=c++20", "-Os", "-fno-exceptions", "-fno-rtti", "-ffunction-sections", "-fdata-sections",
                                   "-DWICKED_POSTFIX_SLIM", "-DWICKED_USE_STM32_CORDIC", "-DSTM32G474xx",
                                   f"-I{include_dir}", f"-I{stm32g4_dev_inc}", f"-I{cmsis_core_inc}"],
                        "ldflags": ["--specs=nano.specs", "--specs=nosys.specs", "-Wl,--gc-sections", "-lm"]
                    })
                    configs.append({
                        "key": "stm32_full_cordic_fast_exp",
                        "name": "STM32G4 Full + Fast Exp + CORDIC",
                        "cxx": arm_cxx, "size": arm_size, "base_flash": arm_base_flash,
                        "cflags": ["-mcpu=cortex-m4", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard", "-mthumb",
                                   "-std=c++20", "-Os", "-fno-exceptions", "-fno-rtti", "-ffunction-sections", "-fdata-sections",
                                   "-DWICKED_USE_FAST_EXP_MATH", "-DWICKED_USE_STM32_CORDIC", "-DSTM32G474xx",
                                   f"-I{include_dir}", f"-I{stm32g4_dev_inc}", f"-I{cmsis_core_inc}"],
                        "ldflags": ["--specs=nano.specs", "--specs=nosys.specs", "-Wl,--gc-sections", "-lm"]
                    })
                else:
                    print(f"Note: STM32G4 device CMSIS headers not found in {cube_fw}; skipping CORDIC benchmark.")
            else:
                print("Note: STM32Cube FW path not found. CMSIS-DSP and CORDIC configurations skipped.")

        if xtensa_cxx and xtensa_size:
            configs.append({
                "key": "esp32s3_full",
                "name": "ESP32-S3 Full",
                "cxx": xtensa_cxx, "size": xtensa_size, "base_flash": xt_base_flash,
                "cflags": ["-mlongcalls", "-std=c++20", "-Os", "-fno-exceptions", "-fno-rtti",
                           "-ffunction-sections", "-fdata-sections", f"-I{include_dir}"],
                "ldflags": ["--specs=nosys.specs", "-Wl,--gc-sections", "-lm"]
            })
            configs.append({
                "key": "esp32s3_full_fast_exp",
                "name": "ESP32-S3 Full + Fast Exp",
                "cxx": xtensa_cxx, "size": xtensa_size, "base_flash": xt_base_flash,
                "cflags": ["-mlongcalls", "-std=c++20", "-Os", "-fno-exceptions", "-fno-rtti",
                           "-ffunction-sections", "-fdata-sections", "-DWICKED_USE_FAST_EXP_MATH", f"-I{include_dir}"],
                "ldflags": ["--specs=nosys.specs", "-Wl,--gc-sections", "-lm"]
            })
            configs.append({
                "key": "esp32s3_slim",
                "name": "ESP32-S3 Slim",
                "cxx": xtensa_cxx, "size": xtensa_size, "base_flash": xt_base_flash,
                "cflags": ["-mlongcalls", "-std=c++20", "-Os", "-fno-exceptions", "-fno-rtti",
                           "-ffunction-sections", "-fdata-sections", "-DWICKED_POSTFIX_SLIM", f"-I{include_dir}"],
                "ldflags": ["--specs=nosys.specs", "-Wl,--gc-sections", "-lm"]
            })

        for cfg in configs:
            print(f"Compiling & linking {cfg['name']}...")
            res = run_benchmark_for_config(cfg, td, src_postfix, base_src, harn_src, verbose=args.verbose)
            results[cfg["key"]] = res

    # Print summary table
    print("\n" + "=" * 80)
    print("BENCHMARK SUMMARY")
    print("=" * 80)
    print(f"{'Configuration':<28} | {'Library-Only Flash':<18} | {'Transitive Flash':<18} | {'Notes'}")
    print("-" * 80)
    for key, r in results.items():
        lib_str = f"{r['lib_flash']:,} B (~{r['lib_flash']/1024:.1f} KB)"
        trans_str = f"{r['transitive_flash']:,} B (~{r['transitive_flash']/1024:.1f} KB)"
        notes = f"Harness: {r['harness_flash']}, Baseline: {r['base_flash']}"
        print(f"{r['name']:<28} | {lib_str:<18} | {trans_str:<18} | {notes}")
    print("=" * 80)

    # Print section breakdown for STM32 Full
    if "stm32_full" in results:
        secs = results["stm32_full"]["sections"]
        val_sz = sum(v for k, v in secs.items() if "WickedPostfixValidate" in k)
        eval_sz = sum(v for k, v in secs.items() if "WickedPostfixEvaluate" in k)
        read_sz = sum(v for k, v in secs.items() if "WickedPostfixRead" in k or "copy_n" in k)
        str_sz = sum(v for k, v in secs.items() if "WickedPostfixOpToString" in k or "CSWTCH" in k or "str1.1" in k)
        print("\nSection Breakdown for STM32G4 Full (Thumb-2):")
        print(f"  * WickedPostfixValidate:    ~{val_sz} B")
        print(f"  * WickedPostfixEvaluate:    ~{eval_sz} B")
        print(f"  * WickedPostfixRead/copy_n: ~{read_sz} B")
        print(f"  * OpToString / Switch:      ~{str_sz} B")

    if "stm32_slim" in results:
        eval_sz_slim = sum(v for k, v in results["stm32_slim"]["sections"].items() if "WickedPostfixEvaluate" in k)
        print(f"  * WickedPostfixEvaluate (Slim): ~{eval_sz_slim} B")

    print_markdown_tables(results)
    return results

if __name__ == "__main__":
    main()
