#!/usr/bin/env python3
"""
benchmark_flash.py - Automated Flash Footprint Benchmark for WickedPostfix

Measures library-only and transitive flash memory footprints across target embedded
architectures (STM32G4 / Cortex-M4F and ESP32-S3 / Xtensa LX7) for Default,
Fast Exp Math, CMSIS-DSP, and CORDIC configurations.

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
            f"C:/ST/STM32CubeIDE*/**/tools/bin/{bin_name}",
            f"C:/Program Files*/**/{bin_name}",
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
            f"C:/Espressif/tools/xtensa-esp-elf*/**/bin/{bin_name}",
            f"C:/.espressif/tools/xtensa-esp-elf*/**/bin/{bin_name}",
            f"{home}/.espressif/tools/xtensa-esp-elf*/**/bin/{bin_name}",
            f"/opt/espressif/tools/xtensa-esp-elf*/**/bin/{bin_name}",
        ]
        cxx = find_file_recursive(patterns)

    size = cli_size or os.environ.get("XTENSA_ESP32S3_ELF_SIZE") or shutil.which("xtensa-esp32s3-elf-size")
    if not size and cxx:
        candidate = os.path.join(os.path.dirname(cxx), f"xtensa-esp32s3-elf-size{BIN_EXT}")
        if os.path.exists(candidate):
            size = candidate
    return cxx, size

def discover_stm32cube_firmware(cli_fw):
    if cli_fw and os.path.isdir(cli_fw):
        return cli_fw
    env_fw = os.environ.get("STM32CUBE_FW_G4")
    if env_fw and os.path.isdir(env_fw):
        return env_fw
    home = os.path.expanduser("~")
    patterns = [
        f"C:/Users/*/STM32Cube/Repository/STM32Cube_FW_G4_*",
        f"{home}/STM32Cube/Repository/STM32Cube_FW_G4_*",
        f"/opt/STM32Cube/Repository/STM32Cube_FW_G4_*",
    ]
    return find_directory_recursive(patterns)

def parse_size_output(output):
    lines = [line.strip() for line in output.strip().splitlines() if line.strip()]
    if len(lines) < 2:
        return 0, 0, 0
    parts = lines[1].split()
    try:
        text = int(parts[0])
        data = int(parts[1])
        bss = int(parts[2])
        return text, data, bss
    except (ValueError, IndexError):
        return 0, 0, 0

def parse_objdump_sections(output):
    sections = {}
    for line in output.splitlines():
        parts = line.split()
        if len(parts) >= 4 and parts[0].isdigit():
            name = parts[1]
            try:
                sz = int(parts[2], 16)
                sections[name] = sz
            except ValueError:
                pass
    return sections

def run_benchmark_for_config(cfg, workdir, src_postfix, src_math, base_src, harn_src, verbose=False):
    cxx = cfg["cxx"]
    size_bin = cfg["size"]
    cflags = cfg["cflags"]
    ldflags = cfg["ldflags"]

    base_src_path = os.path.join(workdir, f"base_{cfg['key']}.cc")
    harn_src_path = os.path.join(workdir, f"harn_{cfg['key']}.cc")
    with open(base_src_path, "w") as f:
        f.write(base_src)
    with open(harn_src_path, "w") as f:
        f.write(harn_src)

    obj_postfix = os.path.join(workdir, f"postfix_{cfg['key']}.o")
    cmd_obj = [cxx] + cflags + ["-c", src_postfix, "-o", obj_postfix]
    if verbose:
        print("  [CMD]", " ".join(cmd_obj))
    res = subprocess.run(cmd_obj, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"Error compiling object for {cfg['name']}:\n{res.stderr}", file=sys.stderr)
        return None

    obj_math = os.path.join(workdir, f"math_{cfg['key']}.o")
    cmd_math = [cxx] + cflags + ["-c", src_math, "-o", obj_math]
    if verbose:
        print("  [CMD]", " ".join(cmd_math))
    res = subprocess.run(cmd_math, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"Error compiling math object for {cfg['name']}:\n{res.stderr}", file=sys.stderr)
        return None

    res_sz = subprocess.run([size_bin, obj_postfix], capture_output=True, text=True)
    t, d, b = parse_size_output(res_sz.stdout)
    res_sz_m = subprocess.run([size_bin, obj_math], capture_output=True, text=True)
    tm, dm, bm = parse_size_output(res_sz_m.stdout)
    lib_flash = (t + d) + (tm + dm)

    objdump_bin = size_bin.replace("size" + BIN_EXT, "objdump" + BIN_EXT)
    sections = {}
    if os.path.exists(objdump_bin):
        res_od = subprocess.run([objdump_bin, "-h", obj_postfix], capture_output=True, text=True)
        sections = parse_objdump_sections(res_od.stdout)
        res_od_m = subprocess.run([objdump_bin, "-h", obj_math], capture_output=True, text=True)
        sections.update(parse_objdump_sections(res_od_m.stdout))

    base_elf = os.path.join(workdir, f"base_{cfg['key']}.elf")
    cmd_base = [cxx] + cflags + [base_src_path, "-o", base_elf] + ldflags
    if verbose:
        print("  [CMD]", " ".join(cmd_base))
    res = subprocess.run(cmd_base, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"Error linking baseline for {cfg['name']}:\n{res.stderr}", file=sys.stderr)
        return None
    res_sz = subprocess.run([size_bin, base_elf], capture_output=True, text=True)
    base_t, base_d, _ = parse_size_output(res_sz.stdout)
    base_flash = base_t + base_d

    harn_elf = os.path.join(workdir, f"harn_{cfg['key']}.elf")
    cmd_harn = [cxx] + cflags + [harn_src_path, obj_postfix, obj_math, "-o", harn_elf] + ldflags
    if verbose:
        print("  [CMD]", " ".join(cmd_harn))
    res = subprocess.run(cmd_harn, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"Error linking harness for {cfg['name']}:\n{res.stderr}", file=sys.stderr)
        return None
    res_sz = subprocess.run([size_bin, harn_elf], capture_output=True, text=True)
    harn_t, harn_d, _ = parse_size_output(res_sz.stdout)
    harn_flash = harn_t + harn_d

    transitive_flash = harn_flash - base_flash

    return {
        "name": cfg["name"],
        "lib_flash": lib_flash,
        "transitive_flash": transitive_flash,
        "harness_flash": harn_flash,
        "base_flash": base_flash,
        "sections": sections
    }

def print_markdown_tables(results):
    print("\n### Formatted Markdown Tables for Documentation:\n")

    # Default Configuration Table
    print("#### Default Configuration\n")
    print("| Target Architecture | Library-Only Flash (`.text` + `.rodata`) | Transitive Flash (Library + `libm` Delta) |")
    print("| :--- | :---: | :---: |")
    if "stm32_default" in results:
        r = results["stm32_default"]
        print(f"| **STM32G4** (Cortex-M4F) | **{r['lib_flash']:,} B** (~{r['lib_flash']/1024:.1f} KB) | **{r['transitive_flash']:,} B** (~{r['transitive_flash']/1024:.1f} KB) |")
    if "esp32s3_default" in results:
        r = results["esp32s3_default"]
        print(f"| **ESP32-S3** (Xtensa LX7) | **{r['lib_flash']:,} B** (~{r['lib_flash']/1024:.1f} KB) | **{r['transitive_flash']:,} B** (~{r['transitive_flash']/1024:.1f} KB) |")

    # Fast Exp Math Table
    print("\n#### Fast Exp Math (`-DWICKED_USE_FAST_EXP_MATH`)\n")
    print("| Target Architecture | Library-Only Flash (`.text` + `.rodata`) | Transitive Flash (Library + `libm` Delta) | Transitive Savings vs. Default |")
    print("| :--- | :---: | :---: | :---: |")
    if "stm32_fast_exp" in results:
        r = results["stm32_fast_exp"]
        def_r = results.get("stm32_default")
        savings = f"{def_r['transitive_flash'] - r['transitive_flash']:,} B (~{(def_r['transitive_flash'] - r['transitive_flash'])/1024:.1f} KB)" if def_r else "—"
        print(f"| **STM32G4** (Cortex-M4F) | **{r['lib_flash']:,} B** (~{r['lib_flash']/1024:.1f} KB) | **{r['transitive_flash']:,} B** (~{r['transitive_flash']/1024:.1f} KB) | **{savings}** |")
    if "esp32s3_fast_exp" in results:
        r = results["esp32s3_fast_exp"]
        def_r = results.get("esp32s3_default")
        savings = f"{def_r['transitive_flash'] - r['transitive_flash']:,} B (~{(def_r['transitive_flash'] - r['transitive_flash'])/1024:.1f} KB)" if def_r else "—"
        print(f"| **ESP32-S3** (Xtensa LX7) | **{r['lib_flash']:,} B** (~{r['lib_flash']/1024:.1f} KB) | **{r['transitive_flash']:,} B** (~{r['transitive_flash']/1024:.1f} KB) | **{savings}** |")

    # Hardware CORDIC Table (with Fast Exp)
    if "stm32_cordic" in results:
        print("\n#### Hardware CORDIC (`-DWICKED_USE_FAST_EXP_MATH -DWICKED_USE_STM32_CORDIC`)\n")
        print("| Target Architecture | Library-Only Flash (`.text` + `.rodata`) | Transitive Flash (Library + `libm` Delta) | Transitive Savings vs. Default |")
        print("| :--- | :---: | :---: | :---: |")
        r = results["stm32_cordic"]
        def_r = results.get("stm32_default")
        savings = f"{def_r['transitive_flash'] - r['transitive_flash']:,} B (~{(def_r['transitive_flash'] - r['transitive_flash'])/1024:.1f} KB)" if def_r else "—"
        print(f"| **STM32G4** (Cortex-M4F) | **{r['lib_flash']:,} B** (~{r['lib_flash']/1024:.1f} KB) | **{r['transitive_flash']:,} B** (~{r['transitive_flash']/1024:.1f} KB) | **{savings}** |")

    # CMSIS-DSP Table (with Fast Exp)
    if "stm32_cmsis_dsp" in results:
        print("\n#### CMSIS-DSP (`-DWICKED_USE_FAST_EXP_MATH -DWICKED_USE_CMSIS_DSP`)\n")
        print("| Target Architecture | Library-Only Flash (`.text` + `.rodata`) | Transitive Flash (Library + CMSIS-DSP + `libm` Delta) | Transitive Savings vs. Default |")
        print("| :--- | :---: | :---: | :---: |")
        r = results["stm32_cmsis_dsp"]
        def_r = results.get("stm32_default")
        savings = f"{def_r['transitive_flash'] - r['transitive_flash']:,} B (~{(def_r['transitive_flash'] - r['transitive_flash'])/1024:.1f} KB)" if def_r else "—"
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

    if args.protocol_root:
        protocol_root = os.path.abspath(args.protocol_root)
    else:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        protocol_root = os.path.abspath(os.path.join(script_dir, ".."))

    include_dir = os.path.join(protocol_root, "cpp", "include")
    src_postfix = os.path.join(protocol_root, "cpp", "src", "WickedPostfix.cc")
    src_math = os.path.join(protocol_root, "cpp", "src", "WickedMath.cc")

    if not os.path.isfile(src_postfix):
        print(f"Error: WickedPostfix.cc not found at {src_postfix}", file=sys.stderr)
        sys.exit(1)
    if not os.path.isfile(src_math):
        print(f"Error: WickedMath.cc not found at {src_math}", file=sys.stderr)
        sys.exit(1)

    arm_cxx, arm_size = discover_arm_toolchain(args.arm_cxx, args.arm_size)
    xtensa_cxx, xtensa_size = discover_xtensa_toolchain(args.xtensa_cxx, args.xtensa_size)
    cube_fw = discover_stm32cube_firmware(args.stm32cube_fw)

    print("=" * 70)
    print("WickedPostfix Flash Memory Footprint Benchmark")
    print("=" * 70)
    print(f"Protocol Root:    {protocol_root}")
    print(f"ARM Toolchain:    {arm_cxx} (size: {arm_size})")
    print(f"Xtensa Toolchain: {xtensa_cxx} (size: {xtensa_size})")
    print(f"STM32Cube FW:     {cube_fw}")
    print("=" * 70)

    if not (arm_cxx and arm_size) and not (xtensa_cxx and xtensa_size):
        print("Error: Neither ARM nor Xtensa toolchains were discovered. Install STM32CubeIDE / ESP-IDF or pass CLI paths.", file=sys.stderr)
        sys.exit(1)

    results = {}
    base_src = BASELINE_SRC
    harn_src = HARNESS_SRC

    with tempfile.TemporaryDirectory() as td:
        arm_base_flash = 0
        xt_base_flash = 0

        if arm_cxx and arm_size:
            base_elf = os.path.join(td, "base_arm.elf")
            base_src_path = os.path.join(td, "base_arm.cc")
            with open(base_src_path, "w") as f:
                f.write(base_src)
            cmd = [arm_cxx, "-mcpu=cortex-m4", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard", "-mthumb",
                   "-std=c++20", "-Os", "-fno-exceptions", "-fno-rtti", "-ffunction-sections", "-fdata-sections",
                   base_src_path, "-o", base_elf, "--specs=nano.specs", "--specs=nosys.specs", "-Wl,--gc-sections", "-lm"]
            subprocess.run(cmd, check=True)
            res_sz = subprocess.run([arm_size, base_elf], capture_output=True, text=True)
            t, d, _ = parse_size_output(res_sz.stdout)
            arm_base_flash = t + d

        if xtensa_cxx and xtensa_size:
            base_elf = os.path.join(td, "base_xt.elf")
            base_src_path = os.path.join(td, "base_xt.cc")
            with open(base_src_path, "w") as f:
                f.write(base_src)
            cmd = [xtensa_cxx, "-mlongcalls", "-std=c++20", "-Os", "-fno-exceptions", "-fno-rtti",
                   "-ffunction-sections", "-fdata-sections", base_src_path, "-o", base_elf,
                   "--specs=nosys.specs", "-Wl,--gc-sections", "-lm"]
            subprocess.run(cmd, check=True)
            res_sz = subprocess.run([xtensa_size, base_elf], capture_output=True, text=True)
            t, d, _ = parse_size_output(res_sz.stdout)
            xt_base_flash = t + d

        # Configure targets
        configs = []
        if arm_cxx and arm_size:
            configs.append({
                "key": "stm32_default",
                "name": "STM32G4 Default",
                "cxx": arm_cxx, "size": arm_size, "base_flash": arm_base_flash,
                "cflags": ["-mcpu=cortex-m4", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard", "-mthumb",
                           "-std=c++20", "-Os", "-fno-exceptions", "-fno-rtti", "-ffunction-sections", "-fdata-sections",
                           f"-I{include_dir}"],
                "ldflags": ["--specs=nano.specs", "--specs=nosys.specs", "-Wl,--gc-sections", "-lm"]
            })
            configs.append({
                "key": "stm32_fast_exp",
                "name": "STM32G4 + Fast Exp Math",
                "cxx": arm_cxx, "size": arm_size, "base_flash": arm_base_flash,
                "cflags": ["-mcpu=cortex-m4", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard", "-mthumb",
                           "-std=c++20", "-Os", "-fno-exceptions", "-fno-rtti", "-ffunction-sections", "-fdata-sections",
                           "-DWICKED_USE_FAST_EXP_MATH", f"-I{include_dir}"],
                "ldflags": ["--specs=nano.specs", "--specs=nosys.specs", "-Wl,--gc-sections", "-lm"]
            })

            if cube_fw:
                cmsis_dsp_inc = os.path.join(cube_fw, "Drivers", "CMSIS", "DSP", "Include")
                cmsis_core_inc = os.path.join(cube_fw, "Drivers", "CMSIS", "Include")
                cmsis_lib_dir = os.path.join(cube_fw, "Drivers", "CMSIS", "DSP", "Lib", "GCC")
                stm32g4_dev_inc = os.path.join(cube_fw, "Drivers", "CMSIS", "Device", "ST", "STM32G4xx", "Include")

                if os.path.isdir(cmsis_dsp_inc) and os.path.isdir(cmsis_lib_dir):
                    configs.append({
                        "key": "stm32_cmsis_dsp",
                        "name": "STM32G4 + Fast Exp + CMSIS-DSP",
                        "cxx": arm_cxx, "size": arm_size, "base_flash": arm_base_flash,
                        "cflags": ["-mcpu=cortex-m4", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard", "-mthumb",
                                   "-std=c++20", "-Os", "-fno-exceptions", "-fno-rtti", "-ffunction-sections", "-fdata-sections",
                                   "-fpermissive", "-DARM_MATH_CM4", "-DWICKED_USE_FAST_EXP_MATH", "-DWICKED_USE_CMSIS_DSP",
                                   f"-I{include_dir}", f"-I{cmsis_dsp_inc}", f"-I{cmsis_core_inc}"],
                        "ldflags": ["--specs=nano.specs", "--specs=nosys.specs", "-Wl,--gc-sections",
                                    f"-L{cmsis_lib_dir}", "-larm_cortexM4lf_math", "-lm"]
                    })
                else:
                    print(f"Note: CMSIS-DSP include/lib not found in {cube_fw}; skipping CMSIS-DSP benchmark.")

                if os.path.isdir(stm32g4_dev_inc):
                    configs.append({
                        "key": "stm32_cordic",
                        "name": "STM32G4 + Fast Exp + CORDIC",
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
                "key": "esp32s3_default",
                "name": "ESP32-S3 Default",
                "cxx": xtensa_cxx, "size": xtensa_size, "base_flash": xt_base_flash,
                "cflags": ["-mlongcalls", "-std=c++20", "-Os", "-fno-exceptions", "-fno-rtti",
                           "-ffunction-sections", "-fdata-sections", f"-I{include_dir}"],
                "ldflags": ["--specs=nosys.specs", "-Wl,--gc-sections", "-lm"]
            })
            configs.append({
                "key": "esp32s3_fast_exp",
                "name": "ESP32-S3 + Fast Exp Math",
                "cxx": xtensa_cxx, "size": xtensa_size, "base_flash": xt_base_flash,
                "cflags": ["-mlongcalls", "-std=c++20", "-Os", "-fno-exceptions", "-fno-rtti",
                           "-ffunction-sections", "-fdata-sections", "-DWICKED_USE_FAST_EXP_MATH", f"-I{include_dir}"],
                "ldflags": ["--specs=nosys.specs", "-Wl,--gc-sections", "-lm"]
            })

        for cfg in configs:
            print(f"Compiling & linking {cfg['name']}...")
            res = run_benchmark_for_config(cfg, td, src_postfix, src_math, base_src, harn_src, verbose=args.verbose)
            results[cfg["key"]] = res

    # Print summary table
    print("\n" + "=" * 80)
    print("BENCHMARK SUMMARY")
    print("=" * 80)
    print(f"{'Configuration':<32} | {'Library-Only Flash':<18} | {'Transitive Flash':<18} | {'Notes'}")
    print("-" * 80)
    for key, r in results.items():
        lib_str = f"{r['lib_flash']:,} B (~{r['lib_flash']/1024:.1f} KB)"
        trans_str = f"{r['transitive_flash']:,} B (~{r['transitive_flash']/1024:.1f} KB)"
        notes = f"Harness: {r['harness_flash']}, Baseline: {r['base_flash']}"
        print(f"{r['name']:<32} | {lib_str:<18} | {trans_str:<18} | {notes}")
    print("=" * 80)

    # Print section breakdown for STM32 Default
    if "stm32_default" in results:
        secs = results["stm32_default"]["sections"]
        val_sz = sum(v for k, v in secs.items() if "WickedPostfixValidate" in k)
        eval_sz = sum(v for k, v in secs.items() if "WickedPostfixEvaluate" in k)
        read_sz = sum(v for k, v in secs.items() if "WickedPostfixRead" in k or "copy_n" in k)
        str_sz = sum(v for k, v in secs.items() if "WickedPostfixOpToString" in k or "CSWTCH" in k or "str1.1" in k)
        print("\nSection Breakdown for STM32G4 Default (Thumb-2):")
        print(f"  * WickedPostfixValidate:    ~{val_sz} B")
        print(f"  * WickedPostfixEvaluate:    ~{eval_sz} B")
        print(f"  * WickedPostfixRead/copy_n: ~{read_sz} B")
        print(f"  * OpToString / Switch:      ~{str_sz} B")

    # Math helper functions in WickedMath.h
    target_key = "stm32_cordic" if "stm32_cordic" in results else ("stm32_fast_exp" if "stm32_fast_exp" in results else None)
    if target_key:
        secs = results[target_key]["sections"]
        fast_log_sz = sum(v for k, v in secs.items() if "wicked_fast_log" in k)
        fast_exp_sz = sum(v for k, v in secs.items() if "wicked_fast_exp" in k)
        cordic_sincos_sz = sum(v for k, v in secs.items() if "wicked_cordic_sincos" in k)
        cordic_atan2_sz = sum(v for k, v in secs.items() if "wicked_cordic_atan2" in k)
        print("\nHelper Math Functions (WickedMath.h, Thumb-2):")
        if fast_exp_sz:
            print(f"  * wicked_fast_expf:         ~{fast_exp_sz} B")
        if fast_log_sz:
            print(f"  * wicked_fast_logf:         ~{fast_log_sz} B")
        if cordic_sincos_sz:
            print(f"  * wicked_cordic_sincosf:    ~{cordic_sincos_sz} B")
        if cordic_atan2_sz:
            print(f"  * wicked_cordic_atan2f:     ~{cordic_atan2_sz} B")

    print_markdown_tables(results)
    return results

if __name__ == "__main__":
    main()
