# WickedWinchProtocol

WickedWinchProtocol defines the wire protocols and bytecode evaluation engine for the Wicked Winch robotic system.

## WickedPostfix Bytecode

`WickedPostfix` is a compact, deterministic, zero-allocation postfix bytecode evaluation machine designed for real-time trajectory generation, coordinate transformations, and polynomial evaluation on embedded motor controllers and central supervisors.

### Wire Format & Header

Every serialized `WickedPostfix` program begins with a 2-byte header:

```
+-----------------------------+
|  ProgSize (16-bit uint, LE) |
+-----------------------------+
|  Bytecode bytes (ProgSize)  |
+-----------------------------+
```

### Operation Set

Stack transformations show stack states ordered from deepest to top of stack (`before` → `after`, where the rightmost item is the top of the stack).

| Opcode | Operation Name | Stack Transformation | Description | Operand Bit Fields |
|:---|:---|:---|:---|:---|
| 0 | `Undefined` | — | Undefined opcode (halts validation with illegal operation error) | *None* |
| 1 | `Push` | ` ` → `v` | Push scalar float literal onto stack | **Followed by**: 4 bytes (IEEE 754 32-bit float literal) |
| 2 | `Dup` | `a` → `a a` | Duplicate top stack value | *None* |
| 3 | `Over` | `a b` → `a b a` | Copy second stack value to top | *None* |
| 4 | `Tuck` | `a b` → `b a b` | Insert copy of top value beneath second value | *None* |
| 5 | `Drop` | `a` → ` ` | Discard top stack value | *None* |
| 6 | `Nip` | `a b` → `b` | Discard second value from top | *None* |
| 7 | `Swap` | `a b` → `b a` | Swap top two stack values | *None* |
| 8 | `Rot` | `a b c` → `b c a` | Rotate third value to top | *None* |
| 9 | `Move` | `... v[N] ...` → `... ... v[N]` | Move / rotate element or subvector from depth to top | **1 byte**:<br>• `[4:0]`: `offset` (0..31 depth below top)<br>• `[7:5]`: `size - 1` (0..7 for vector size 1..8) |
| 10 | `Copy` | `... v[N] ...` → `... v[N] ... v[N]` | Copy element or subvector from depth to top | **1 byte**:<br>• `[4:0]`: `offset` (0..31 depth below top)<br>• `[7:5]`: `size - 1` (0..7 for vector size 1..8) |
| 11 | `CopyBase` | `... v[N] ...` → `... v[N] ... v[N]` | Copy element or subvector from bottom index to top of stack | **1 byte**:<br>• `[4:0]`: `index` (0..31 index from bottom of stack)<br>• `[7:5]`: `size - 1` (0..7 for vector size 1..8) |
| 12 | `Slide` | `... d[D] k[K]` → `... k[K]` | Keep top K elements and drop D elements directly beneath them | **1 byte**:<br>• `[3:0]`: `keep - 1` (0..15 for 1..16 elements to keep)<br>• `[7:4]`: `drop - 1` (0..15 for 1..16 elements to drop) |
| 13 | `Add` | `a b` → `a + b` | Scalar addition | *None* |
| 14 | `Sub` | `a b` → `a - b` | Scalar subtraction | *None* |
| 15 | `Mul` | `a b` → `a * b` | Scalar multiplication | *None* |
| 16 | `Div` | `a b` → `a / b` | Scalar division | *None* |
| 17 | `Pow` | `a b` → `a ^ b` | Scalar power | *None* |
| 18 | `Atan2` | `y x` → `atan2(y, x)` | Four-quadrant inverse tangent | *None* |
| 19 | `Neg` | `a` → `-a` | Scalar negation | *None* |
| 20 | `Abs` | `a` → `\|a\|` | Scalar absolute value | *None* |
| 21 | `Inv` | `a` → `1 / a` | Scalar reciprocal | *None* |
| 22 | `Sqrt` | `a` → `sqrt(a)` | Square root | *None* |
| 23 | `Exp` | `a` → `exp(a)` | Exponential | *None* |
| 24 | `Ln` | `a` → `ln(a)` | Natural logarithm | *None* |
| 25 | `Sin` | `a` → `sin(a)` | Sine | *None* |
| 26 | `Cos` | `a` → `cos(a)` | Cosine | *None* |
| 27 | `Tan` | `a` → `tan(a)` | Tangent | *None* |
| 28 | `Asin` | `a` → `asin(a)` | Arc sine | *None* |
| 29 | `Acos` | `a` → `acos(a)` | Arc cosine | *None* |
| 30 | `Ipow` | `a` → `a ^ exp` | Integer exponentiation | **1 byte**:<br>• `[7:0]`: `exp` (two's complement signed 8-bit int, -128..127) |
| 31 | `MulAdd` | `a b c` → `a * b + c` | Fused multiply-add | *None* |
| 32 | `MulSub` | `a b c` → `a * b - c` | Fused multiply-subtract | *None* |
| 33 | `SubMul` | `c a b` → `c - a * b` | Fused subtract-multiply | *None* |
| 34 | `PushVec` | ` ` → `v[N]` | Push vector float literals onto stack | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$)<br>**Followed by**: `size * 4` bytes (IEEE 754 32-bit float literals) |
| 35 | `AddVec` | `u[N] v[N]` → `(u + v)[N]` | Componentwise vector addition | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 36 | `SubVec` | `u[N] v[N]` → `(u - v)[N]` | Componentwise vector subtraction | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 37 | `MulVec` | `u[N] v[N]` → `(u * v)[N]` | Componentwise vector multiplication | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 38 | `DivVec` | `u[N] v[N]` → `(u / v)[N]` | Componentwise vector division | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 39 | `NegVec` | `v[N]` → `(-v)[N]` | Componentwise vector negation | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 40 | `InvVec` | `v[N]` → `(1 / v)[N]` | Componentwise vector reciprocal | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 41 | `DotVec` | `u[N] v[N]` → `(u · v)` | Vector dot product | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 42 | `NormSqVec` | `v[N]` → `\|\|v\|\|^2` | Squared Euclidean norm $\|v\|^2$ | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 43 | `SumVec` | `v[N]` → `sum(v)` | Sum of vector components | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 44 | `MulAddVec` | `a[N] b[N] c[N]` → `(a * b + c)[N]` | Componentwise fused multiply-add | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 45 | `MulSubVec` | `a[N] b[N] c[N]` → `(a * b - c)[N]` | Componentwise fused multiply-subtract | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 46 | `SubMulVec` | `c[N] a[N] b[N]` → `(c - a * b)[N]` | Componentwise fused subtract-multiply | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 47 | `ScaleVec` | `v[N] s` → `(s * v)[N]` | Vector scalar multiplication | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 48 | `LerpVec` | `t` → `((1 - t)*v0 + t*v1)[N]` | Linear interpolation between two immediate vectors for scalar $t$ | **1 byte dimension**:<br>• `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$)<br>**Followed by**: `2 * size * 4` bytes of IEEE 754 32-bit floats (interleaved component pairs: $v_{0,0}, v_{1,0}, v_{0,1}, v_{1,1}, \dots, v_{0,N-1}, v_{1,N-1}$) |
| 49 | `MulMat` | `v[rows]` → `(v * M)[cols]` | Vector-matrix multiplication $w = v \cdot M$ with immediate matrix $M$ | **1 byte dimension**:<br>• `[1:0]`: `rows - 1` (0..3 for 1..4 input dimensions / rows in $M$)<br>• `[3:2]`: Reserved (must be 0)<br>• `[7:4]`: `cols - 1` (0..15 for 1..16 output dimensions / cols in $M$)<br>**Followed by**: `rows * cols * 4` bytes of IEEE 754 32-bit floats ($M$ in column-major order: column by column) |
| 50 | `Poly` | `t` → `P(t)[cols]` | Polynomial evaluation for scalar $t$ with immediate coefficients | **1 byte dimension**:<br>• `[3:0]`: `rows - 1` (0..15 for 1..16 coefficients per column / degree + 1)<br>• `[7:4]`: `cols - 1` (0..15 for 1..16 output dimensions)<br>**Followed by**: `rows * cols * 4` bytes of IEEE 754 32-bit float coefficients (degree by degree from highest order down to constant term: for each degree, column 0 to cols-1) |
| 51 | `CosSin` | `t` → `cos(t) sin(t)` | Simultaneous scalar cosine and sine evaluation | *None* |
| 52 | `PanTilt` | `v[3]` → `pan tilt` | Pan and tilt spherical angles from 3D line-of-sight vector $[v_0, v_1, v_2]$: $\text{pan} = \text{atan2}(v_1, v_0)$, $\text{tilt} = \text{atan2}(v_2, \sqrt{v_0^2 + v_1^2})$ | *None* |

### Embedded Memory & Acceleration Flags (C++)

For flash-constrained microcontrollers (e.g., STM32 with 32 KB or 64 KB flash), compile-time preprocessor flags allow reducing flash consumption either by disabling unneeded opcodes to dead-strip standard library functions, or by routing active opcodes to hardware accelerators:

#### 1. Opcode Disabling Flags (Feature Stripping)
These flags omit specific non-core operations from the evaluator, allowing the linker (`-Wl,--gc-sections`) to dead-strip their underlying `libm` implementations:

* **`-DWICKED_POSTFIX_SLIM`**: Disables both non-core exponential/power math (`Pow`, `Exp`, `Ln`) and extended trigonometry (`Tan`, `Asin`, `Acos`).
* **`-DWICKED_POSTFIX_NO_EXP_MATH`**: Disables `Pow`, `Exp`, and `Ln` (eliminating `powf`, `expf`, and `logf`).
* **`-DWICKED_POSTFIX_NO_EXTENDED_TRIG`**: Disables `Tan`, `Asin`, and `Acos` (eliminating `asinf`, `acosf`, and `tanf`).

When opcodes are disabled via these flags, programs attempting to evaluate them are rejected during validation (`WickedPostfixValidate`) with `WickedEvalStatus_UndefinedOperation`.

#### 2. Trigonometric Hardware Acceleration & Alternative Backends
These flags **do not disable any opcodes**; all operations (`Sin`, `Cos`, `CosSin`, and `Atan2`) remain fully supported and validated. Instead, they replace Newlib's ~3.1 KB software range-reduction machinery and ~0.8 KB arctangent engine with lightweight hardware or DSP alternatives:

* **`-DWICKED_USE_STM32_CORDIC`**: Routes `Sin`, `Cos`, `CosSin`, and `Atan2` (along with `PanTilt`) to the hardware CORDIC co-processor on STM32 microcontrollers (STM32G4, STM32H5, STM32U5, STM32H7R/S). Automatically discovers family headers (`stm32g4xx.h`, `stm32h5xx.h`, `stm32u5xx.h`, etc.) or accepts custom overrides via `-DWICKED_STM32_HEADER="<header.h>"`. Computes trigonometric functions in 4–8 CPU cycles with **0 bytes of lookup tables**, capturing **~4.8 KB** of flash savings vs. standard Slim and completely dead-stripping all Newlib trigonometric code.
* **`-DWICKED_USE_CMSIS_DSP`**: Routes `Sin`, `Cos`, and `CosSin` to ARM CMSIS-DSP (`arm_sin_f32`, `arm_cos_f32`, `arm_sin_cos_f32`), eliminating Newlib's ~3.1 KB range-reduction engine.
* **Custom Backends**: Firmware targeting other hardware (e.g., TI TMU, NXP PowerQuad) can provide custom overrides directly:
  * `WICKED_CUSTOM_SIN(t)`
  * `WICKED_CUSTOM_COS(t)`
  * `WICKED_CUSTOM_SINCOS(t, s, c)`
  * `WICKED_CUSTOM_ATAN2(y, x)`

### Flash Memory Footprint (Reading & Evaluation Suite)

The tables below report measured flash memory consumption compiled with `-Os` for the execution and reading suite (`WickedPostfixValidate`, `WickedPostfixEvaluate`, `WickedPostfixRead`, and `WickedPostfixOpToString`), excluding `WickedPostfixWriter`.

* **Library-Only Flash**: Code and constant rodata generated directly by `WickedPostfix` functions in `WickedPostfix.cc`.
* **Transitive Flash**: Net flash delta added to a linked firmware binary with linker dead-stripping (`-Wl,--gc-sections`), capturing the library code plus all transitive standard library (`libm` / Newlib) dependencies.

#### Full Profile (Default: All 52 Opcodes)

| Target Architecture | Library-Only Flash (`.text` + `.rodata`) | Transitive Flash (Library + `libm` Delta) |
| :--- | :---: | :---: |
| **STM32G4** (Cortex-M4F) | **3,828 B** (~3.7 KB) | **14,384 B** (~14.0 KB) |
| **ESP32-S3** (Xtensa LX7) | **4,989 B** (~4.9 KB) | **15,996 B** (~15.6 KB) |

#### Slim Profile (`-DWICKED_POSTFIX_SLIM`)

| Target Architecture | Library-Only Flash (`.text` + `.rodata`) | Transitive Flash (Library + `libm` Delta) | Transitive Savings vs. Full |
| :--- | :---: | :---: | :---: |
| **STM32G4** (Cortex-M4F) | **3,760 B** (~3.7 KB) | **9,276 B** (~9.1 KB) | **5,108 B (~5.0 KB)** |
| **ESP32-S3** (Xtensa LX7) | **4,865 B** (~4.8 KB) | **10,656 B** (~10.4 KB) | **5,340 B (~5.2 KB)** |

#### Slim Profile with CMSIS-DSP (`-DWICKED_POSTFIX_SLIM -DWICKED_USE_CMSIS_DSP`)

Replaces Newlib's standard trigonometry with ARM CMSIS-DSP table-interpolated functions (`arm_sin_f32`, `arm_cos_f32`, `arm_sin_cos_f32`), evaluating only the required function and completely dead-stripping Newlib's ~3.1 KB Payne-Hanek range reduction subsystem:

| Target Architecture | Library-Only Flash (`.text` + `.rodata`) | Transitive Flash (Library + CMSIS-DSP + `libm` Delta) | Transitive Savings vs. Slim |
| :--- | :---: | :---: | :---: |
| **STM32G4** (Cortex-M4F) | **3,760 B** (~3.7 KB) | **7,592 B** (~7.4 KB) | **1,684 B (~1.6 KB)** |

#### Slim Profile with Hardware CORDIC (`-DWICKED_POSTFIX_SLIM -DWICKED_USE_STM32_CORDIC`)

Leverages STM32's on-chip CORDIC hardware accelerator (STM32G4, STM32H5, STM32U5, STM32H7R/S) for trigonometric evaluation (`Sin`, `Cos`, `CosSin`, `Atan2`, and `PanTilt`). Because CORDIC executes in hardware registers with zero lookup tables, it completely dead-strips all Newlib trigonometric code and tables (`__kernel_rem_pio2f`, `two_over_pi`, `atan2f`, `atanf`), eliminating `libm` trigonometry from the binary:

| Target Architecture | Library-Only Flash (`.text` + `.rodata`) | Transitive Flash (Library + `libm` Delta) | Transitive Savings vs. Slim |
| :--- | :---: | :---: | :---: |
| **STM32G4** (Cortex-M4F) | **4,028 B** (~3.9 KB) | **4,400 B** (~4.3 KB) | **4,876 B (~4.8 KB)** |

> [!NOTE]
> **Firmware Considerations for CORDIC:**
> * **Clock Enable:** The firmware must enable the CORDIC peripheral clock during system initialization before executing expressions (e.g., `__HAL_RCC_CORDIC_CLK_ENABLE()` or `LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_CORDIC)`). Accessing CORDIC registers with the peripheral clock disabled triggers a hardware fault.
> * **Re-entrancy & Concurrency:** The CORDIC co-processor is stateful (`CSR`, `WDATA`, `RDATA`). If expressions are evaluated concurrently across multiple RTOS threads or inside high-priority interrupts while other firmware tasks use CORDIC, register access must be guarded by a mutex/critical section or dedicated to the expression engine.
> * **Range & Scale Normalization:** The embedded CORDIC helper automatically maps arbitrary angles in radians to $[-1.0, 1.0)$ for $\sin$/$\cos$, and scales Cartesian inputs by $1 / \max(|x|, |y|)$ for $\text{atan2}$, requiring zero table lookups or standard library calls.

#### Section Breakdown (STM32G4, Thumb-2)
* **Bytecode Validation (`WickedPostfixValidate`)**: ~664 B `.text`
* **Bytecode Evaluation (`WickedPostfixEvaluate`)**: ~2,476 B (Slim) / ~2,544 B (Full) `.text`
* **Reader & Utilities (`WickedPostfixRead`, `copy_n`)**: ~58 B `.text`
* **Opcode Names & Tables (`WickedPostfixOpToString`, rodata switch tables)**: ~562 B `.rodata`

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


