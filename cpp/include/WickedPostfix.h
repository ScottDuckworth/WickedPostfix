#pragma once

#include "WickedEvalStatus.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WICKED_PACKED
#if defined(__GNUC__) || defined(__clang__)
#define WICKED_PACKED __attribute__((__packed__))
#else
#define WICKED_PACKED
#endif
#endif

#pragma pack(push, 1)
// Header preceding the postfix bytecode program, specifying program byte size.
typedef struct WICKED_PACKED WickedPostfixHeader {
	uint16_t prog_size;
} WickedPostfixHeader_t;
#pragma pack(pop)

#if defined(__cplusplus)
static_assert(sizeof(WickedPostfixHeader_t) == 2, "WickedPostfixHeader_t must be 2 bytes");
static_assert(alignof(WickedPostfixHeader_t) == 1, "WickedPostfixHeader_t must be packed");
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(WickedPostfixHeader_t) == 2, "WickedPostfixHeader_t must be 2 bytes");
_Static_assert(_Alignof(WickedPostfixHeader_t) == 1, "WickedPostfixHeader_t must be packed");
#endif

// Bytecode operations for the postfix evaluation machine.
typedef enum WickedPostfixOp {
  WickedPostfixOp_Undefined = 0,
  WickedPostfixOp_Push      = 1,
  WickedPostfixOp_Dup       = 2,
  WickedPostfixOp_Over      = 3,
  WickedPostfixOp_Tuck      = 4,
  WickedPostfixOp_Drop      = 5,
  WickedPostfixOp_Nip       = 6,
  WickedPostfixOp_Swap      = 7,
  WickedPostfixOp_Rot       = 8,
  WickedPostfixOp_Move      = 9,
  WickedPostfixOp_Copy      = 10,
  WickedPostfixOp_CopyBase  = 11,
  WickedPostfixOp_Slide     = 12,
  WickedPostfixOp_Add       = 13,
  WickedPostfixOp_Sub       = 14,
  WickedPostfixOp_Mul       = 15,
  WickedPostfixOp_Div       = 16,
  WickedPostfixOp_Pow       = 17,
  WickedPostfixOp_Atan2     = 18,
  WickedPostfixOp_Neg       = 19,
  WickedPostfixOp_Abs       = 20,
  WickedPostfixOp_Inv       = 21,
  WickedPostfixOp_Sqrt      = 22,
  WickedPostfixOp_Exp       = 23,
  WickedPostfixOp_Ln        = 24,
  WickedPostfixOp_Sin       = 25,
  WickedPostfixOp_Cos       = 26,
  WickedPostfixOp_Tan       = 27,
  WickedPostfixOp_Asin      = 28,
  WickedPostfixOp_Acos      = 29,
  WickedPostfixOp_Ipow      = 30,
  WickedPostfixOp_MulAdd    = 31,
  WickedPostfixOp_PushVec   = 32,
  WickedPostfixOp_AddVec    = 33,
  WickedPostfixOp_SubVec    = 34,
  WickedPostfixOp_MulVec    = 35,
  WickedPostfixOp_DivVec    = 36,
  WickedPostfixOp_NegVec    = 37,
  WickedPostfixOp_InvVec    = 38,
  WickedPostfixOp_NormSqVec = 39,
  WickedPostfixOp_SumVec    = 40,
  WickedPostfixOp_MulAddVec = 41,
  WickedPostfixOp_ScaleVec  = 42,
  WickedPostfixOp_LerpVec   = 43,
  WickedPostfixOp_MulMat    = 44,
  WickedPostfixOp_Poly      = 45,
  WickedPostfixOp_CosSin    = 46,
} WickedPostfixOp_t;

// Returns true if op is a binary scalar operation (13..18).
static inline bool WickedPostfixOpIsBinaryScalar(WickedPostfixOp_t op) {
  return op >= WickedPostfixOp_Add && op <= WickedPostfixOp_Atan2;
}

// Returns true if op is a unary scalar operation (19..29).
static inline bool WickedPostfixOpIsUnaryScalar(WickedPostfixOp_t op) {
  return op >= WickedPostfixOp_Neg && op <= WickedPostfixOp_Acos;
}

// Returns true if op is any scalar operation (13..31).
static inline bool WickedPostfixOpIsScalar(WickedPostfixOp_t op) {
  return op >= WickedPostfixOp_Add && op <= WickedPostfixOp_MulAdd;
}

// Returns true if op is a binary vector operation (33..36).
static inline bool WickedPostfixOpIsBinaryVec(WickedPostfixOp_t op) {
  return op >= WickedPostfixOp_AddVec && op <= WickedPostfixOp_DivVec;
}

// Returns true if op is a unary vector operation (37..38).
static inline bool WickedPostfixOpIsUnaryVec(WickedPostfixOp_t op) {
  return op >= WickedPostfixOp_NegVec && op <= WickedPostfixOp_InvVec;
}

// Returns true if op is a vector reduction operation (39..40).
static inline bool WickedPostfixOpIsVectorReduction(WickedPostfixOp_t op) {
  return op >= WickedPostfixOp_NormSqVec && op <= WickedPostfixOp_SumVec;
}

// Returns true if op is a vector operation (32..43).
static inline bool WickedPostfixOpIsVector(WickedPostfixOp_t op) {
  return op >= WickedPostfixOp_PushVec && op <= WickedPostfixOp_LerpVec;
}

// Returns true if op takes a 1-byte size operand (32..43).
static inline bool WickedPostfixOpHasSizeOperand(WickedPostfixOp_t op) {
  return op >= WickedPostfixOp_PushVec && op <= WickedPostfixOp_LerpVec;
}

// Returns the string representation of an opcode.
const char* WickedPostfixOpToString(WickedPostfixOp_t op);

// Parses and validates the header from data, returning pointers to prog_data and prog_size.
bool WickedPostfixRead(const uint8_t* data, size_t size, const uint8_t** prog_data, uint16_t* prog_size);

// Validates bytecode stack requirements and returns status, minimum stack capacity, and final stack size.
WickedEvalStatus_t WickedPostfixValidate(const uint8_t* prog_data, uint16_t prog_size, uint16_t initial_stack_size, uint16_t* minimum_stack_capacity, uint16_t* final_stack_size);

// Evaluates bytecode on stack_data in-place without bounds checking.
void WickedPostfixEvaluate(const uint8_t* prog_data, uint16_t prog_size, uint16_t initial_stack_size, float* stack_data);

// Unsafe inline reader that extracts prog_data and prog_size without bounds checking.
static inline void WickedPostfixReadUnsafe(const uint8_t* data, const uint8_t** prog_data, uint16_t* prog_size) {
  if (prog_data) *prog_data = data + sizeof(WickedPostfixHeader_t);
  if (prog_size) {
    uint16_t size;
    memcpy(&size, data, sizeof(uint16_t));
    *prog_size = size;
  }
}

#ifdef __cplusplus
}

#include <cassert>
#include <cstring>
#include <initializer_list>
#include <ostream>
#include <span>
#include <vector>

// Formats opcode name to output stream.
inline std::ostream& operator<<(std::ostream& out, WickedPostfixOp_t op) {
  return out << WickedPostfixOpToString(op);
}

// Builds serialized postfix bytecode programs.
class WickedPostfixWriter {
 public:
  // Pointer to bytecode data buffer.
  uint8_t* prog_data() { return prog_.data(); }
  // Const pointer to bytecode data buffer.
  const uint8_t* prog_data() const { return prog_.data(); }
  // Bytecode size in bytes.
  uint16_t prog_size() const { return static_cast<uint16_t>(prog_.size()); }
  // Total serialized size including header.
  uint16_t data_size() const { return prog_offset() + prog_size(); }

  // Serializes header and bytecode into the provided buffer.
  bool Write(uint8_t* data, size_t size) const;
  // Serializes header and bytecode into a newly allocated vector.
  std::vector<uint8_t> Write() const {
    std::vector<uint8_t> buffer(data_size());
    Write(buffer.data(), buffer.size());
    return buffer;
  }

  // Clears the bytecode buffer.
  void clear() {
    prog_.clear();
  }

  // Emits a raw byte into the bytecode stream.
  void EmitByte(uint8_t b) { prog_.push_back(b); }

  // Emits an operation opcode byte into the bytecode stream.
  void EmitOp(WickedPostfixOp_t op) { EmitByte(static_cast<uint8_t>(op)); }

  // Emits a 32-bit float literal into the bytecode stream.
  void EmitFloat(float f) {
    uint8_t bytes[sizeof(float)];
    memcpy(bytes, &f, sizeof(float));
    for (uint8_t b : bytes) prog_.push_back(b);
  }

  // Pushes a single float literal onto the stack ( -> value).
  void Push(float value) {
    EmitOp(WickedPostfixOp_Push);
    EmitFloat(value);
  }

  // Pushes multiple float literals onto the stack ( -> values...).
  // Operand constraints:
  //   - values.size(): 1 to 255
  void PushVec(std::span<const float> values) {
    assert(values.size() >= 1 && values.size() <= 255);
    EmitOp(WickedPostfixOp_PushVec);
    EmitByte(static_cast<uint8_t>(values.size() - 1));
    for (float value : values) EmitFloat(value);
  }

  // Pushes multiple float literals onto the stack ( -> values...).
  void PushVec(std::initializer_list<float> values) {
    PushVec(std::span<const float>(values));
  }

  // Drops the top value from the stack (a -> ).
  void Drop() {
    EmitOp(WickedPostfixOp_Drop);
  }

  // Duplicates the top value on the stack (a -> a a).
  void Dup() {
    EmitOp(WickedPostfixOp_Dup);
  }

  // Swaps the top two values on the stack (a b -> b a).
  void Swap() {
    EmitOp(WickedPostfixOp_Swap);
  }

  // Copies the second value to the top (a b -> a b a).
  void Over() {
    EmitOp(WickedPostfixOp_Over);
  }

  // Inserts copy of top value under second value (a b -> b a b).
  void Tuck() {
    EmitOp(WickedPostfixOp_Tuck);
  }

  // Drops the second value from the top (a b -> b).
  void Nip() {
    EmitOp(WickedPostfixOp_Nip);
  }

  // Rotates third value to the top (a b c -> b c a).
  void Rot() {
    EmitOp(WickedPostfixOp_Rot);
  }

  // Copies a subvector from stack depth offset to the top (... v ... -> ... v ... v).
  void Copy(uint8_t offset, uint8_t size = 1) {
    assert(offset <= 31);
    assert(size >= 1 && size <= 8);
    EmitOp(WickedPostfixOp_Copy);
    EmitByte((offset & 0x1F) | ((size - 1) << 5));
  }

  // Moves / rotates a subvector from stack depth offset to the top (... v ... -> ... ... v).
  void Move(uint8_t offset, uint8_t size = 1) {
    assert(offset <= 31);
    assert(size >= 1 && size <= 8);
    EmitOp(WickedPostfixOp_Move);
    EmitByte((offset & 0x1F) | ((size - 1) << 5));
  }

  // Copies a subvector from bottom index to the top (... v ... -> ... v ... v).
  void CopyBase(uint8_t index, uint8_t size = 1) {
    assert(index <= 31);
    assert(size >= 1 && size <= 8);
    EmitOp(WickedPostfixOp_CopyBase);
    EmitByte((index & 0x1F) | ((size - 1) << 5));
  }

  // Keeps top `keep` elements and drops `drop` elements directly beneath them.
  void Slide(uint8_t keep, uint8_t drop) {
    assert(keep >= 1 && keep <= 16);
    assert(drop >= 1 && drop <= 16);
    EmitOp(WickedPostfixOp_Slide);
    EmitByte(((keep - 1) & 0x0F) | (((drop - 1) & 0x0F) << 4));
  }

  // Pops two values and pushes their sum (a b -> a + b).
  void Add() { EmitOp(WickedPostfixOp_Add); }

  // Pops rhs and lhs and pushes lhs - rhs (a b -> a - b).
  void Sub() { EmitOp(WickedPostfixOp_Sub); }

  // Pops two values and pushes their product (a b -> a * b).
  void Mul() { EmitOp(WickedPostfixOp_Mul); }

  // Pops rhs and lhs and pushes lhs / rhs (a b -> a / b).
  void Div() { EmitOp(WickedPostfixOp_Div); }

  // Pops rhs and lhs and pushes lhs^rhs (a b -> a ^ b).
  void Pow() { EmitOp(WickedPostfixOp_Pow); }

  // Pops x and y and pushes atan2(y, x) (y x -> atan2(y, x)).
  void Atan2() { EmitOp(WickedPostfixOp_Atan2); }

  // Negates the top value (a -> -a).
  void Neg() { EmitOp(WickedPostfixOp_Neg); }

  // Absolute value of top value (a -> |a|).
  void Abs() { EmitOp(WickedPostfixOp_Abs); }

  // Inverts top value (a -> 1 / a).
  void Inv() { EmitOp(WickedPostfixOp_Inv); }

  // Square root of top value (a -> sqrt(a)).
  void Sqrt() { EmitOp(WickedPostfixOp_Sqrt); }

  // Exponential e^a of top value (a -> exp(a)).
  void Exp() { EmitOp(WickedPostfixOp_Exp); }

  // Natural logarithm ln(a) of top value (a -> ln(a)).
  void Ln() { EmitOp(WickedPostfixOp_Ln); }

  // Sine of top value (a -> sin(a)).
  void Sin() { EmitOp(WickedPostfixOp_Sin); }

  // Cosine of top value (a -> cos(a)).
  void Cos() { EmitOp(WickedPostfixOp_Cos); }

  // Tangent of top value (a -> tan(a)).
  void Tan() { EmitOp(WickedPostfixOp_Tan); }

  // Arc sine of top value (a -> asin(a)).
  void Asin() { EmitOp(WickedPostfixOp_Asin); }

  // Arc cosine of top value (a -> acos(a)).
  void Acos() { EmitOp(WickedPostfixOp_Acos); }

  // Replaces t with cos(t) and pushes sin(t) (t -> cos(t) sin(t)).
  void CosSin() { EmitOp(WickedPostfixOp_CosSin); }

  // Raises the top value to the integer exponent exp (a -> a ^ exp).
  void Ipow(int8_t exp) {
    EmitOp(WickedPostfixOp_Ipow);
    EmitByte(static_cast<uint8_t>(exp));
  }

  // Pops c, b, a and pushes a*b + c (a b c -> a * b + c).
  void MulAdd() { EmitOp(WickedPostfixOp_MulAdd); }

  // Adds two vectors componentwise (u[N] v[N] -> (u + v)[N]).
  void AddVec(uint8_t size) {
    assert(size > 0);
    EmitOp(WickedPostfixOp_AddVec);
    EmitByte(size - 1);
  }

  // Subtracts two vectors componentwise (u[N] v[N] -> (u - v)[N]).
  void SubVec(uint8_t size) {
    assert(size > 0);
    EmitOp(WickedPostfixOp_SubVec);
    EmitByte(size - 1);
  }

  // Multiplies two vectors componentwise (u[N] v[N] -> (u * v)[N]).
  void MulVec(uint8_t size) {
    assert(size > 0);
    EmitOp(WickedPostfixOp_MulVec);
    EmitByte(size - 1);
  }

  // Divides two vectors componentwise (u[N] v[N] -> (u / v)[N]).
  void DivVec(uint8_t size) {
    assert(size > 0);
    EmitOp(WickedPostfixOp_DivVec);
    EmitByte(size - 1);
  }

  // Negates each component of vector (v[N] -> (-v)[N]).
  void NegVec(uint8_t size) {
    assert(size > 0);
    EmitOp(WickedPostfixOp_NegVec);
    EmitByte(size - 1);
  }

  // Inverts each component of vector (v[N] -> (1 / v)[N]).
  void InvVec(uint8_t size) {
    assert(size > 0);
    EmitOp(WickedPostfixOp_InvVec);
    EmitByte(size - 1);
  }

  // Pushes scalar squared Euclidean norm ||v||^2 (v[N] -> ||v||^2).
  void NormSqVec(uint8_t size) {
    assert(size > 0);
    EmitOp(WickedPostfixOp_NormSqVec);
    EmitByte(size - 1);
  }

  // Pushes scalar sum of vector components (v[N] -> sum(v)).
  void SumVec(uint8_t size) {
    assert(size > 0);
    EmitOp(WickedPostfixOp_SumVec);
    EmitByte(size - 1);
  }

  // Componentwise fused multiply-add (a[N] b[N] c[N] -> (a * b + c)[N]).
  void MulAddVec(uint8_t size) {
    assert(size > 0);
    EmitOp(WickedPostfixOp_MulAddVec);
    EmitByte(size - 1);
  }

  // Scales vector by scalar (v[N] s -> (s * v)[N]).
  void ScaleVec(uint8_t size) {
    assert(size > 0);
    EmitOp(WickedPostfixOp_ScaleVec);
    EmitByte(size - 1);
  }

  // Linear interpolation between two immediate vectors for scalar t (t -> ((1 - t)*v0 + t*v1)[N]).
  // Operand constraints:
  //   - v0.size() == v1.size()
  //   - size: 1 to 255
  void LerpVec(std::span<const float> v0, std::span<const float> v1) {
    assert(v0.size() == v1.size());
    assert(v0.size() >= 1 && v0.size() <= 255);
    EmitOp(WickedPostfixOp_LerpVec);
    EmitByte(static_cast<uint8_t>(v0.size() - 1));
    for (size_t i = 0; i < v0.size(); ++i) {
      EmitFloat(v0[i]);
      EmitFloat(v1[i]);
    }
  }

  void LerpVec(std::initializer_list<float> v0, std::initializer_list<float> v1) {
    LerpVec(std::span<const float>(v0), std::span<const float>(v1));
  }

  // Vector-matrix multiplication w = v * M with immediate column-major matrix M (v[rows] -> (v * M)[cols]).
  // Operand constraints:
  //   - rows: 1 to 4 (input vector dimension)
  //   - cols: 1 to 16 (output vector dimension)
  //   - matrix: elements in column-major order (column by column)
  //   - matrix.size() == rows * cols
  void MulMat(uint8_t rows, uint8_t cols, std::span<const float> matrix) {
    assert(rows >= 1 && rows <= 4);
    assert(cols >= 1 && cols <= 16);
    assert(matrix.size() == static_cast<size_t>(rows) * cols);
    EmitOp(WickedPostfixOp_MulMat);
    uint8_t arg = ((rows - 1) & 0x03) | (((cols - 1) & 0x0F) << 4);
    EmitByte(arg);
    for (float f : matrix) {
      EmitFloat(f);
    }
  }

  void MulMat(uint8_t rows, uint8_t cols, std::initializer_list<float> matrix) {
    MulMat(rows, cols, std::span<const float>(matrix));
  }

  // Evaluates a polynomial or matrix of polynomials for scalar t (t -> P(t)[cols]).
  //
  // Coefficients must be provided in row-major order with INCREASING degree order:
  //   - Row 0 (coeffs[0 : cols]): degree 0 (constant terms)
  //   - Row 1 (coeffs[cols : 2*cols]): degree 1 (linear terms)
  //   - ...
  //   - Row rows-1 (coeffs[(rows-1)*cols : rows*cols]): highest degree (degree = rows - 1)
  //
  // For dimension j (0 <= j < cols):
  //   P_j(t) = coeffs[0*cols + j] + coeffs[1*cols + j]*t + ... + coeffs[(rows-1)*cols + j]*t^(rows-1)
  //
  // Note: When serializing to bytecode, Poly automatically reverses the row order
  // into decreasing degree order (highest degree first) as required by Horner's method
  // during bytecode evaluation.
  //
  // Operand constraints:
  //   - rows: 1 to 16 (number of polynomial coefficients per column, i.e. degree + 1)
  //   - cols: 1 to 16 (number of dimensions / columns)
  //   - coeffs.size() == rows * cols
  void Poly(uint8_t rows, uint8_t cols, std::span<const float> coeffs) {
    assert(rows >= 1 && rows <= 16);
    assert(cols >= 1 && cols <= 16);
    assert(coeffs.size() == static_cast<size_t>(rows) * cols);
    EmitOp(WickedPostfixOp_Poly);
    uint8_t arg = ((rows - 1) & 0x0F) | (((cols - 1) & 0x0F) << 4);
    EmitByte(arg);
    for (size_t i = rows; i > 0; --i) {
      size_t row_offset = (i - 1) * cols;
      for (size_t j = 0; j < cols; ++j) {
        EmitFloat(coeffs[row_offset + j]);
      }
    }
  }

  // Evaluates a polynomial or matrix of polynomials (t -> P(t)[cols]).
  void Poly(uint8_t rows, uint8_t cols, std::initializer_list<float> coeffs) {
    Poly(rows, cols, std::span<const float>(coeffs));
  }

  // Evaluates a 1D polynomial (t -> P(t)).
  // Coefficients must be provided in INCREASING degree order:
  // coeffs[0] is degree 0 (constant), coeffs[1] is degree 1, ..., coeffs[N-1] is degree N-1.
  void Poly(std::span<const float> coeffs) {
    Poly(static_cast<uint8_t>(coeffs.size()), 1, coeffs);
  }

  // Evaluates a 1D polynomial (t -> P(t)).
  // Coefficients must be provided in INCREASING degree order:
  // coeffs[0] is degree 0 (constant), coeffs[1] is degree 1, ..., coeffs[N-1] is degree N-1.
  void Poly(std::initializer_list<float> coeffs) {
    Poly(std::span<const float>(coeffs));
  }

  // Convenience aliases (expecting coefficients in INCREASING degree order).
  void PolyVec(std::span<const float> coeffs) { Poly(coeffs); }
  void PolyVec(std::initializer_list<float> coeffs) { Poly(coeffs); }
  void PolyMat(uint8_t rows, uint8_t cols, std::span<const float> coeffs) { Poly(rows, cols, coeffs); }
  void PolyMat(uint8_t rows, uint8_t cols, std::initializer_list<float> coeffs) { Poly(rows, cols, coeffs); }

 private:
  constexpr uint16_t prog_offset() const { return sizeof(WickedPostfixHeader); }

  std::vector<uint8_t> prog_;
};

#endif
