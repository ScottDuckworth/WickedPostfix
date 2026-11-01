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
| 32 | `PushVec` | ` ` → `v[N]` | Push vector float literals onto stack | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$)<br>**Followed by**: `size * 4` bytes (IEEE 754 32-bit float literals) |
| 33 | `AddVec` | `u[N] v[N]` → `(u + v)[N]` | Componentwise vector addition | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 34 | `SubVec` | `u[N] v[N]` → `(u - v)[N]` | Componentwise vector subtraction | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 35 | `MulVec` | `u[N] v[N]` → `(u * v)[N]` | Componentwise vector multiplication | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 36 | `DivVec` | `u[N] v[N]` → `(u / v)[N]` | Componentwise vector division | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 37 | `NegVec` | `v[N]` → `(-v)[N]` | Componentwise vector negation | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 38 | `InvVec` | `v[N]` → `(1 / v)[N]` | Componentwise vector reciprocal | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 39 | `NormSqVec` | `v[N]` → `\|\|v\|\|^2` | Squared Euclidean norm $\|v\|^2$ | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 40 | `SumVec` | `v[N]` → `sum(v)` | Sum of vector components | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 41 | `MulAddVec` | `a[N] b[N] c[N]` → `(a * b + c)[N]` | Componentwise fused multiply-add | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 42 | `ScaleVec` | `v[N] s` → `(s * v)[N]` | Vector scalar multiplication | **1 byte**: `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$) |
| 43 | `LerpVec` | `t` → `((1 - t)*v0 + t*v1)[N]` | Linear interpolation between two immediate vectors for scalar $t$ | **1 byte dimension**:<br>• `[7:0]`: `size - 1` (0..254 for vector dimension $N = 1..255$)<br>**Followed by**: `2 * size * 4` bytes of IEEE 754 32-bit floats (interleaved component pairs: $v_{0,0}, v_{1,0}, v_{0,1}, v_{1,1}, \dots, v_{0,N-1}, v_{1,N-1}$) |
| 44 | `MulMat` | `v[rows]` → `(v * M)[cols]` | Vector-matrix multiplication $w = v \cdot M$ with immediate matrix $M$ | **1 byte dimension**:<br>• `[1:0]`: `rows - 1` (0..3 for 1..4 input dimensions / rows in $M$)<br>• `[3:2]`: Reserved (must be 0)<br>• `[7:4]`: `cols - 1` (0..15 for 1..16 output dimensions / cols in $M$)<br>**Followed by**: `rows * cols * 4` bytes of IEEE 754 32-bit floats ($M$ in column-major order: column by column) |
| 45 | `Poly` | `t` → `P(t)[cols]` | Polynomial evaluation for scalar $t$ with immediate coefficients | **1 byte dimension**:<br>• `[3:0]`: `rows - 1` (0..15 for 1..16 coefficients per column / degree + 1)<br>• `[7:4]`: `cols - 1` (0..15 for 1..16 output dimensions)<br>**Followed by**: `rows * cols * 4` bytes of IEEE 754 32-bit float coefficients (degree by degree from highest order down to constant term: for each degree, column 0 to cols-1) |
| 46 | `CosSin` | `t` → `cos(t) sin(t)` | Simultaneous scalar cosine and sine evaluation | *None* |
| 47 | `PanTilt` | `v[3]` → `pan tilt` | Pan and tilt spherical angles from 3D line-of-sight vector $[v_0, v_1, v_2]$: $\text{pan} = \text{atan2}(v_1, v_0)$, $\text{tilt} = \text{atan2}(v_2, \sqrt{v_0^2 + v_1^2})$ | *None* |

### Embedded Memory Profiles (C++)

For flash-constrained microcontrollers (e.g., STM32 with 32 KB or 64 KB flash), compile-time preprocessor flags allow dead-stripping standard library `libm` math functions:

* **`-DWICKED_POSTFIX_SLIM`**: Disables both non-core exponential/power math (`Pow`, `Exp`, `Ln`) and extended trigonometry (`Tan`, `Asin`, `Acos`), saving ~1.6 KB to 2.2 KB of flash `.text`.
* **`-DWICKED_POSTFIX_NO_EXP_MATH`**: Disables `Pow`, `Exp`, `Ln` (eliminates `powf`, `expf`, and `logf`, saving ~1.2 KB to 1.6 KB flash).
* **`-DWICKED_POSTFIX_NO_EXTENDED_TRIG`**: Disables `Tan`, `Asin`, `Acos` (eliminates `asinf`, `acosf`, and `tanf`, saving ~400 B to 600 B flash).

When disabled, programs attempting to evaluate these opcodes are rejected during validation with `WickedEvalStatus_UndefinedOperation`.

