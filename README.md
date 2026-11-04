# WickedPostfix

`WickedPostfix` is a compact, deterministic, zero-allocation postfix bytecode evaluation machine designed for real-time trajectory generation, coordinate transformations, and polynomial evaluation on embedded motor controllers and central supervisors.

### Wire Format & Termination

`WickedPostfix` bytecode programs are headerless and stream-terminated with an explicit return opcode (`Ret` / `1`). Evaluation traps immediately on invalid or out-of-range opcodes, preventing runaway execution on corrupted or unprogrammed memory.

```
+------------------------------------+
|  Bytecode instructions ...         |
+------------------------------------+
|  Ret (Opcode 1)                    |
+------------------------------------+
```

### Operation Set

Stack transformations show stack states ordered from deepest to top of stack (`before` → `after`, where the rightmost item is the top of the stack).

| Opcode | Operation Name | Stack Transformation | Description | Operand Bit Fields |
|:---|:---|:---|:---|:---|
| 0 | `Undefined` | — | Undefined opcode (halts validation with illegal operation error) | *None* |
| 1 | `Ret` | — | End of program or return from subroutine | *None* |
| 2 | `Call` | `...` → `...` | Subroutine call | **2 bytes**: `uint16_t` target byte offset from program start |
| 3 | `Piecewise` | `... x` → `...` | Piecewise function evaluation over scalar $x$ | **1 byte count**:<br>• `[6:0]`: `num_knots` $N$ ($1 \le N \le 127$)<br>• `[7]`: Mode bit ($0 = \text{Absolute } x, 1 = \text{Relative } x - x_{\text{ref}}$)<br>**Followed by**:<br>• $N \times 4$ bytes of sorted IEEE 754 32-bit float knots ($x_0 < x_1 < \dots < x_{N-1}$)<br>• $(N + 1) \times 2$ bytes of `uint16_t` branch target offsets |
| 4 | `Push` | ` ` → `v` | Push scalar float literal onto stack | **Followed by**: 4 bytes (IEEE 754 32-bit float literal) |
| 5 | `Dup` | `a` → `a a` | Duplicate top stack value | *None* |
| 6 | `Over` | `a b` → `a b a` | Copy second stack value to top | *None* |
| 7 | `Tuck` | `a b` → `b a b` | Insert copy of top value beneath second value | *None* |
| 8 | `Drop` | `a` → ` ` | Discard top stack value | *None* |
| 9 | `Nip` | `a b` → `b` | Discard second value from top | *None* |
| 10 | `Swap` | `a b` → `b a` | Swap top two stack values | *None* |
| 11 | `Rot` | `a b c` → `b c a` | Rotate third value to top | *None* |
| 12 | `Move` | `... v[N] ...` → `... ... v[N]` | Move / rotate element or subvector from depth to top | **1 byte**:<br>• `[4:0]`: `offset` (0..31 depth below top)<br>• `[7:5]`: `size - 1` (0..7 for vector size 1..8) |
| 13 | `Copy` | `... v[N] ...` → `... v[N] ... v[N]` | Copy element or subvector from depth to top | **1 byte**:<br>• `[4:0]`: `offset` (0..31 depth below top)<br>• `[7:5]`: `size - 1` (0..7 for vector size 1..8) |
| 14 | `CopyBase` | `... v[N] ...` → `... v[N] ... v[N]` | Copy element or subvector from bottom index to top of stack | **1 byte**:<br>• `[4:0]`: `index` (0..31 index from bottom of stack)<br>• `[7:5]`: `size - 1` (0..7 for vector size 1..8) |
| 15 | `Slide` | `... d[D] k[K]` → `... k[K]` | Keep top K elements and drop D elements directly beneath them | **1 byte**:<br>• `[3:0]`: `keep - 1` (0..15 for 1..16 elements to keep)<br>• `[7:4]`: `drop - 1` (0..15 for 1..16 elements to drop) |
| 16 | `Add` | `a b` → `a + b` | Scalar addition | *None* |
| 17 | `Sub` | `a b` → `a - b` | Scalar subtraction | *None* |
| 18 | `Mul` | `a b` → `a * b` | Scalar multiplication | *None* |
| 19 | `Div` | `a b` → `a / b` | Scalar division | *None* |
| 20 | `MulAdd` | `a b c` → `a * b + c` | Fused multiply-add | *None* |
| 21 | `MulSub` | `a b c` → `a * b - c` | Fused multiply-subtract | *None* |
| 22 | `SubMul` | `c a b` → `c - a * b` | Fused subtract-multiply | *None* |
| 23 | `Neg` | `a` → `-a` | Scalar negation | *None* |
| 24 | `Abs` | `a` → `\|a\|` | Scalar absolute value | *None* |
| 25 | `Inv` | `a` → `1 / a` | Scalar reciprocal | *None* |
| 26 | `Sqrt` | `a` → `sqrt(a)` | Square root | *None* |
| 27 | `Sin` | `a` → `sin(a)` | Sine | *None* |
| 28 | `Cos` | `a` → `cos(a)` | Cosine | *None* |
| 29 | `Tan` | `a` → `tan(a)` | Tangent | *None* |
| 30 | `Asin` | `a` → `asin(a)` | Arc sine | *None* |
| 31 | `Acos` | `a` → `acos(a)` | Arc cosine | *None* |
| 32 | `Atan2` | `x y` → `atan2(y, x)` | Four-quadrant inverse tangent | *None* |
| 33 | `CosSin` | `t` → `cos(t) sin(t)` | Simultaneous scalar cosine and sine evaluation | *None* |
| 34 | `PanTilt` | `v[3]` → `pan tilt` | Pan and tilt spherical angles from 3D line-of-sight vector $[v_0, v_1, v_2]$: $\text{pan} = \text{atan2}(v_1, v_0)$, $\text{tilt} = \text{atan2}(v_2, \sqrt{v_0^2 + v_1^2})$ | *None* |
| 35 | `Ipow` | `a` → `a ^ exp` | Integer exponentiation | **1 byte**:<br>• `[7:0]`: `exp` (two's complement signed 8-bit int, -128..127) |
| 36 | `Pow` | `a b` → `a ^ b` | Scalar power | *None* |
| 37 | `Exp` | `a` → `exp(a)` | Exponential | *None* |
| 38 | `Ln` | `a` → `ln(a)` | Natural logarithm | *None* |
| 39 | `PushVec` | ` ` → `v[N]` | Push vector float literals onto stack | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$)<br>**Followed by**: `size * 4` bytes (IEEE 754 32-bit float literals) |
| 40 | `AddVec` | `u[N] v[N]` → `(u + v)[N]` | Componentwise vector addition | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 41 | `SubVec` | `u[N] v[N]` → `(u - v)[N]` | Componentwise vector subtraction | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 42 | `MulVec` | `u[N] v[N]` → `(u * v)[N]` | Componentwise vector multiplication | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 43 | `DivVec` | `u[N] v[N]` → `(u / v)[N]` | Componentwise vector division | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 44 | `MulAddVec` | `a[N] b[N] c[N]` → `(a * b + c)[N]` | Componentwise fused multiply-add | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 45 | `MulSubVec` | `a[N] b[N] c[N]` → `(a * b - c)[N]` | Componentwise fused multiply-subtract | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 46 | `SubMulVec` | `c[N] a[N] b[N]` → `(c - a * b)[N]` | Componentwise fused subtract-multiply | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 47 | `NegVec` | `v[N]` → `(-v)[N]` | Componentwise vector negation | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 48 | `AbsVec` | `v[N]` → `\|v\|[N]` | Componentwise vector absolute value | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 49 | `InvVec` | `v[N]` → `(1 / v)[N]` | Componentwise vector reciprocal | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 50 | `DotVec` | `u[N] v[N]` → `(u · v)` | Vector dot product | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 51 | `NormSqVec` | `v[N]` → `\|\|v\|\|^2` | Squared Euclidean norm $\|v\|^2$ | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 52 | `SumVec` | `v[N]` → `sum(v)` | Sum of vector components | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 53 | `ScaleVec` | `v[N] s` → `(s * v)[N]` | Vector scalar multiplication | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 54 | `MulMat` | `v[rows]` → `(v * M)[cols]` | Vector-matrix multiplication $w = v \cdot M$ with immediate matrix $M$ | **1 byte dimension**:<br>• `[1:0]`: `rows - 1` (0..3 for 1..4 input dimensions / rows in $M$)<br>• `[3:2]`: Reserved (must be 0)<br>• `[7:4]`: `cols - 1` (0..15 for 1..16 output dimensions / cols in $M$)<br>**Followed by**: `rows * cols * 4` bytes of IEEE 754 32-bit floats ($M$ in column-major order: column by column) |
| 55 | `Poly` | `t` → `P(t)[cols]` | Polynomial evaluation for scalar $t$ with immediate coefficients | **1 byte dimension**:<br>• `[3:0]`: `rows - 1` (0..15 for 1..16 coefficients per column / degree + 1)<br>• `[7:4]`: `cols - 1` (0..15 for 1..16 output dimensions)<br>**Followed by**: `rows * cols * 4` bytes of IEEE 754 32-bit float coefficients (degree by degree from highest order down to constant term: for each degree, column 0 to cols-1) |

### Embedded Memory & Acceleration Flags (C++)

For flash-constrained microcontrollers (e.g., STM32 with 32 KB or 64 KB flash), compile-time preprocessor flags allow reducing flash consumption by routing transcendental operations to hardware accelerators or lightweight polynomial approximations:

#### 1. Trigonometric Hardware Acceleration & Alternative Backends
Replaces Newlib's ~3.1 KB software range-reduction machinery and ~0.8 KB arctangent engine with lightweight hardware or DSP alternatives:

* **`-DWICKED_USE_STM32_CORDIC`**: Routes `Sin`, `Cos`, `CosSin`, and `Atan2` (along with `PanTilt`) to the hardware CORDIC co-processor on STM32 microcontrollers (STM32G4, STM32H5, STM32U5, STM32H7R/S). Automatically discovers family headers (`stm32g4xx.h`, `stm32h5xx.h`, `stm32u5xx.h`, etc.) or accepts custom overrides via `-DWICKED_STM32_HEADER="<header.h>"`. Computes trigonometric functions in 4–8 CPU cycles with **0 bytes of lookup tables**, capturing **~5.5 KB** of flash savings and completely dead-stripping all Newlib trigonometric code.
* **`-DWICKED_USE_CMSIS_DSP`**: Routes `Sin`, `Cos`, and `CosSin` to ARM CMSIS-DSP (`arm_sin_f32`, `arm_cos_f32`, `arm_sin_cos_f32`), eliminating Newlib's ~3.1 KB range-reduction engine.
* **Custom Backends**: Firmware targeting other hardware (e.g., TI TMU, NXP PowerQuad) can provide custom overrides directly:
  * `WICKED_CUSTOM_SIN(t)`
  * `WICKED_CUSTOM_COS(t)`
  * `WICKED_CUSTOM_SINCOS(t, s, c)`
  * `WICKED_CUSTOM_ATAN2(y, x)`

*(Note: Extended trigonometric functions `Tan`, `Asin`, and `Acos` are derived algebraically directly from `Sin`/`Cos` and `Atan2`, dead-stripping `libm`'s `tanf`, `asinf`, and `acosf` automatically without requiring any flags).*

#### 2. Exponential Math Optimization & Fast Math Backends
* **Default Case (`Pow` Derivation)**: `Pow` is mathematically derived from `Exp` and `Ln` ($x^y = \exp(y \ln x)$). This dead-strips Newlib's large `powf` function from `libm.a` by default.
* **`-DWICKED_USE_FAST_EXP_MATH`**: Enables lightweight, table-less IEEE-754 bit-trick and Horner polynomial approximations for `Exp`, `Ln`, and `Pow` with $< 5 \times 10^{-6}$ relative error across the entire float range. Completely eliminates `libm` exponential dependencies, saving **~0.9 KB** of flash across STM32 and ESP32.
* **Custom Overrides**: Pluggable hooks allow binding vendor DSP or hardware math accelerators:
  * `WICKED_CUSTOM_EXP(x)`
  * `WICKED_CUSTOM_LN(x)`
  * `WICKED_CUSTOM_POW(x, y)`

### Flash Memory Footprint (Reading & Evaluation Suite)

The tables below report measured flash memory consumption compiled with `-Os` for the execution and validation suite (`WickedPostfixValidate`, `WickedPostfixEvaluate`, and `WickedPostfixOpToString`), excluding `WickedPostfixWriter`.

* **Library-Only Flash**: Code and constant rodata generated directly by the protocol library (`WickedPostfix.cc` + `WickedMath.cc`).
* **Transitive Flash**: Net flash delta added to a linked firmware binary with linker dead-stripping (`-Wl,--gc-sections`), capturing the library code plus all transitive standard library (`libm` / Newlib) dependencies.

#### Default Configuration

| Target Architecture | Library-Only Flash (`.text` + `.rodata`) | Transitive Flash (Library + `libm` Delta) |
| :--- | :---: | :---: |
| **STM32G4** (Cortex-M4F) | **4,954 B** (~4.8 KB) | **11,376 B** (~11.1 KB) |
| **ESP32-S3** (Xtensa LX7) | **6,290 B** (~6.1 KB) | **12,848 B** (~12.5 KB) |

#### Fast Exp Math (`-DWICKED_USE_FAST_EXP_MATH`)

| Target Architecture | Library-Only Flash (`.text` + `.rodata`) | Transitive Flash (Library + `libm` Delta) | Transitive Savings vs. Default |
| :--- | :---: | :---: | :---: |
| **STM32G4** (Cortex-M4F) | **4,954 B** (~4.8 KB) | **10,448 B** (~10.2 KB) | **928 B (~0.9 KB)** |
| **ESP32-S3** (Xtensa LX7) | **6,290 B** (~6.1 KB) | **11,952 B** (~11.7 KB) | **896 B (~0.9 KB)** |

#### Hardware CORDIC (`-DWICKED_USE_FAST_EXP_MATH -DWICKED_USE_STM32_CORDIC`)

Leverages STM32's on-chip CORDIC hardware accelerator (STM32G4, STM32H5, STM32U5, STM32H7R/S) for trigonometric evaluation (`Sin`, `Cos`, `CosSin`, `Atan2`, and `PanTilt`) together with Fast Exp Math. Because CORDIC executes in hardware registers with zero lookup tables and Fast Exp Math replaces software exponentials, this completely dead-strips all Newlib trigonometric and exponential runtime routines:

| Target Architecture | Library-Only Flash (`.text` + `.rodata`) | Transitive Flash (Library + `libm` Delta) | Transitive Savings vs. Default |
| :--- | :---: | :---: | :---: |
| **STM32G4** (Cortex-M4F) | **5,222 B** (~5.1 KB) | **5,720 B** (~5.6 KB) | **5,656 B (~5.5 KB)** |

#### CMSIS-DSP (`-DWICKED_USE_FAST_EXP_MATH -DWICKED_USE_CMSIS_DSP`)

Replaces Newlib's standard trigonometry with ARM CMSIS-DSP table-interpolated functions (`arm_sin_f32`, `arm_cos_f32`, `arm_sin_cos_f32`) alongside Fast Exp Math:

| Target Architecture | Library-Only Flash (`.text` + `.rodata`) | Transitive Flash (Library + CMSIS-DSP + `libm` Delta) | Transitive Savings vs. Default |
| :--- | :---: | :---: | :---: |
| **STM32G4** (Cortex-M4F) | **4,954 B** (~4.8 KB) | **8,912 B** (~8.7 KB) | **2,464 B (~2.4 KB)** |

> [!NOTE]
> **Firmware Considerations for CORDIC:**
> * **Clock Enable:** The firmware must enable the CORDIC peripheral clock during system initialization before executing expressions (e.g., `__HAL_RCC_CORDIC_CLK_ENABLE()` or `LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_CORDIC)`). Accessing CORDIC registers with the peripheral clock disabled triggers a hardware fault.
> * **Re-entrancy & Concurrency:** The CORDIC co-processor is stateful (`CSR`, `WDATA`, `RDATA`). If expressions are evaluated concurrently across multiple RTOS threads or inside high-priority interrupts while other firmware tasks use CORDIC, register access must be guarded by a mutex/critical section or dedicated to the expression engine.
> * **Range & Scale Normalization:** The embedded CORDIC helper automatically maps arbitrary angles in radians to $[-1.0, 1.0)$ for $\sin$/$\cos$, and scales Cartesian inputs by $1 / \max(|x|, |y|)$ for $\text{atan2}$, requiring zero table lookups or standard library calls.

#### Section Breakdown (STM32G4, Thumb-2)
* **Bytecode Validation (`ValidateInternal`, `ValidateSubroutine`, `WickedPostfixValidate`)**: ~1,024 B `.text`
* **Bytecode Evaluation (`WickedPostfixEvaluate`, `WickedPostfixEvaluateInternal`)**: ~2,692 B `.text`
* **Opcode Names & Tables (`WickedPostfixOpToString`, rodata switch tables)**: ~592 B `.rodata`

#### Helper Math Functions (`WickedMath.h`, Thumb-2)
* **Fast Exponential (`wicked_fast_expf`)**: ~200 B `.text`
* **Fast Natural Logarithm (`wicked_fast_logf`)**: ~220 B `.text`
* **Hardware CORDIC Sine/Cosine (`wicked_cordic_sincosf`)**: ~140 B `.text`
* **Hardware CORDIC Arctangent (`wicked_cordic_atan2f`)**: ~116 B `.text`

#### Benchmark Compilation Flags
* **STM32G4** (GNU Tools for STM32 14.3.1 / `arm-none-eabi-g++`):
  `-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -std=c++20 -Os -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections -lm`
* **ESP32-S3** (Espressif Xtensa GCC 14.2.0 / `xtensa-esp32s3-elf-g++`):
  `-mlongcalls -std=c++20 -Os -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections --specs=nosys.specs -Wl,--gc-sections -lm`

#### Automated Benchmark Suite

The repository includes an automated, path-agnostic benchmark suite:
* **Via CMake**: `cmake --build build --target flash_benchmark`
* **Via Python**: `python scripts/benchmark_flash.py`
  *(Accepts optional `--arm-cxx`, `--arm-size`, `--xtensa-cxx`, `--xtensa-size`, and `--stm32cube-fw` flags, or auto-detects toolchains dynamically from `PATH`, environment variables, and standard installation roots)*


