#pragma once

#include "WickedPostfix.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <span>
#include <vector>

// CRTP base class providing all opcode builder and emission methods.
// Derived classes must implement:
//   void EmitByte(uint8_t b);
template <typename Derived>
class WickedPostfixEmitter {
 public:
  Derived& derived() { return static_cast<Derived&>(*this); }
  const Derived& derived() const { return static_cast<const Derived&>(*this); }

  // Emits an operation opcode byte into the bytecode stream.
  void EmitOp(WickedPostfixOp_t op) {
    derived().EmitByte(static_cast<uint8_t>(op));
  }

  // Emits a 32-bit float literal into the bytecode stream.
  void EmitFloat(float f) {
    uint8_t bytes[sizeof(float)];
    std::memcpy(bytes, &f, sizeof(float));
    for (uint8_t b : bytes) derived().EmitByte(b);
  }

  // Pushes a single float literal onto the stack ( -> value).
  void Push(float value) {
    EmitOp(WickedPostfixOp_Push);
    EmitFloat(value);
  }

  // Duplicates the top value on the stack (a -> a a).
  void Dup() { EmitOp(WickedPostfixOp_Dup); }

  // Copies the second value to the top (a b -> a b a).
  void Over() { EmitOp(WickedPostfixOp_Over); }

  // Inserts copy of top value under second value (a b -> b a b).
  void Tuck() { EmitOp(WickedPostfixOp_Tuck); }

  // Drops the top value from the stack (a -> ).
  void Drop() { EmitOp(WickedPostfixOp_Drop); }

  // Drops the second value from the top (a b -> b).
  void Nip() { EmitOp(WickedPostfixOp_Nip); }

  // Swaps the top two values on the stack (a b -> b a).
  void Swap() { EmitOp(WickedPostfixOp_Swap); }

  // Rotates third value to the top (a b c -> b c a).
  void Rot() { EmitOp(WickedPostfixOp_Rot); }

  // Moves / rotates a subvector from stack depth offset to the top (... v ... -> ... ... v).
  void Move(uint8_t offset, uint8_t size = 1) {
    assert(offset <= 31);
    assert(size >= 1 && size <= 8);
    EmitOp(WickedPostfixOp_Move);
    derived().EmitByte((offset & 0x1F) | ((size - 1) << 5));
  }

  // Copies a subvector from stack depth offset to the top (... v ... -> ... v ... v).
  void Copy(uint8_t offset, uint8_t size = 1) {
    assert(offset <= 31);
    assert(size >= 1 && size <= 8);
    EmitOp(WickedPostfixOp_Copy);
    derived().EmitByte((offset & 0x1F) | ((size - 1) << 5));
  }

  // Copies a subvector from bottom index to the top (... v ... -> ... v ... v).
  void CopyBase(uint8_t index, uint8_t size = 1) {
    assert(index <= 31);
    assert(size >= 1 && size <= 8);
    EmitOp(WickedPostfixOp_CopyBase);
    derived().EmitByte((index & 0x1F) | ((size - 1) << 5));
  }

  // Keeps top `keep` elements and drops `drop` elements directly beneath them.
  void Slide(uint8_t keep, uint8_t drop) {
    assert(keep >= 1 && keep <= 16);
    assert(drop >= 1 && drop <= 16);
    EmitOp(WickedPostfixOp_Slide);
    derived().EmitByte(((keep - 1) & 0x0F) | (((drop - 1) & 0x0F) << 4));
  }

  // Pops two values and pushes their sum (a b -> a + b).
  void Add() { EmitOp(WickedPostfixOp_Add); }

  // Pops rhs and lhs and pushes lhs - rhs (a b -> a - b).
  void Sub() { EmitOp(WickedPostfixOp_Sub); }

  // Pops two values and pushes their product (a b -> a * b).
  void Mul() { EmitOp(WickedPostfixOp_Mul); }

  // Pops rhs and lhs and pushes lhs / rhs (a b -> a / b).
  void Div() { EmitOp(WickedPostfixOp_Div); }

  // Pops c, b, a and pushes a*b + c (a b c -> a * b + c).
  void MulAdd() { EmitOp(WickedPostfixOp_MulAdd); }

  // Pops c, b, a and pushes a*b - c (a b c -> a * b - c).
  void MulSub() { EmitOp(WickedPostfixOp_MulSub); }

  // Pops b, a, c and pushes c - a*b (c a b -> c - a * b).
  void SubMul() { EmitOp(WickedPostfixOp_SubMul); }

  // Negates the top value (a -> -a).
  void Neg() { EmitOp(WickedPostfixOp_Neg); }

  // Absolute value of top value (a -> |a|).
  void Abs() { EmitOp(WickedPostfixOp_Abs); }

  // Inverts top value (a -> 1 / a).
  void Inv() { EmitOp(WickedPostfixOp_Inv); }

  // Square root of top value (a -> sqrt(a)).
  void Sqrt() { EmitOp(WickedPostfixOp_Sqrt); }

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

  // Pops y and x and pushes atan2(y, x) (x y -> atan2(y, x)).
  void Atan2() { EmitOp(WickedPostfixOp_Atan2); }

  // Replaces t with cos(t) and pushes sin(t) (t -> cos(t) sin(t)).
  void CosSin() { EmitOp(WickedPostfixOp_CosSin); }

  // Transforms a 3D line-of-sight vector [v0, v1, v2] into [pan, tilt] (v[3] -> pan tilt).
  void PanTilt() { EmitOp(WickedPostfixOp_PanTilt); }

  // Raises the top value to the integer exponent exp (a -> a ^ exp).
  void Ipow(int8_t exp) {
    EmitOp(WickedPostfixOp_Ipow);
    derived().EmitByte(static_cast<uint8_t>(exp));
  }

  // Pops rhs and lhs and pushes lhs^rhs (a b -> a ^ b).
  void Pow() { EmitOp(WickedPostfixOp_Pow); }

  // Exponential e^a of top value (a -> exp(a)).
  void Exp() { EmitOp(WickedPostfixOp_Exp); }

  // Natural logarithm ln(a) of top value (a -> ln(a)).
  void Ln() { EmitOp(WickedPostfixOp_Ln); }

  // Pushes multiple float literals onto the stack ( -> values...).
  // Operand constraints:
  //   - values.size(): 1 to 255
  void PushVec(std::span<const float> values) {
    assert(values.size() >= 1 && values.size() <= 255);
    EmitOp(WickedPostfixOp_PushVec);
    derived().EmitByte(static_cast<uint8_t>(values.size() - 1));
    for (float value : values) EmitFloat(value);
  }

  // Pushes multiple float literals onto the stack ( -> values...).
  void PushVec(std::initializer_list<float> values) {
    PushVec(std::span<const float>(values));
  }

  // Adds two vectors componentwise (u[N] v[N] -> (u + v)[N]).
  void AddVec(uint8_t size) {
    assert(size > 0);
    EmitOp(WickedPostfixOp_AddVec);
    derived().EmitByte(size - 1);
  }

  // Subtracts two vectors componentwise (u[N] v[N] -> (u - v)[N]).
  void SubVec(uint8_t size) {
    assert(size > 0);
    EmitOp(WickedPostfixOp_SubVec);
    derived().EmitByte(size - 1);
  }

  // Multiplies two vectors componentwise (u[N] v[N] -> (u * v)[N]).
  void MulVec(uint8_t size) {
    assert(size > 0);
    EmitOp(WickedPostfixOp_MulVec);
    derived().EmitByte(size - 1);
  }

  // Divides two vectors componentwise (u[N] v[N] -> (u / v)[N]).
  void DivVec(uint8_t size) {
    assert(size > 0);
    EmitOp(WickedPostfixOp_DivVec);
    derived().EmitByte(size - 1);
  }

  // Componentwise fused multiply-add (a[N] b[N] c[N] -> (a * b + c)[N]).
  void MulAddVec(uint8_t size) {
    assert(size > 0);
    EmitOp(WickedPostfixOp_MulAddVec);
    derived().EmitByte(size - 1);
  }

  // Componentwise fused multiply-subtract (a[N] b[N] c[N] -> (a * b - c)[N]).
  void MulSubVec(uint8_t size) {
    assert(size > 0);
    EmitOp(WickedPostfixOp_MulSubVec);
    derived().EmitByte(size - 1);
  }

  // Componentwise fused subtract-multiply (c[N] a[N] b[N] -> (c - a * b)[N]).
  void SubMulVec(uint8_t size) {
    assert(size > 0);
    EmitOp(WickedPostfixOp_SubMulVec);
    derived().EmitByte(size - 1);
  }

  // Negates each component of vector (v[N] -> (-v)[N]).
  void NegVec(uint8_t size) {
    assert(size > 0);
    EmitOp(WickedPostfixOp_NegVec);
    derived().EmitByte(size - 1);
  }

  // Absolute value of each component of vector (v[N] -> |v|[N]).
  void AbsVec(uint8_t size) {
    assert(size > 0);
    EmitOp(WickedPostfixOp_AbsVec);
    derived().EmitByte(size - 1);
  }

  // Inverts each component of vector (v[N] -> (1 / v)[N]).
  void InvVec(uint8_t size) {
    assert(size > 0);
    EmitOp(WickedPostfixOp_InvVec);
    derived().EmitByte(size - 1);
  }

  // Pushes scalar dot product of two vectors (u[N] v[N] -> (u · v)).
  void DotVec(uint8_t size) {
    assert(size > 0);
    EmitOp(WickedPostfixOp_DotVec);
    derived().EmitByte(size - 1);
  }

  // Pushes scalar squared Euclidean norm ||v||^2 (v[N] -> ||v||^2).
  void NormSqVec(uint8_t size) {
    assert(size > 0);
    EmitOp(WickedPostfixOp_NormSqVec);
    derived().EmitByte(size - 1);
  }

  // Pushes scalar sum of vector components (v[N] -> sum(v)).
  void SumVec(uint8_t size) {
    assert(size > 0);
    EmitOp(WickedPostfixOp_SumVec);
    derived().EmitByte(size - 1);
  }

  // Scales vector by scalar (v[N] s -> (s * v)[N]).
  void ScaleVec(uint8_t size) {
    assert(size > 0);
    EmitOp(WickedPostfixOp_ScaleVec);
    derived().EmitByte(size - 1);
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
    derived().EmitByte(arg);
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
    derived().EmitByte(arg);
    for (size_t i = rows; i > 0; --i) {
      size_t row_offset = (i - 1) * cols;
      for (size_t j = 0; j < cols; ++j) {
        EmitFloat(coeffs[row_offset + j]);
      }
    }
  }

  void Poly(uint8_t rows, uint8_t cols, std::initializer_list<float> coeffs) {
    Poly(rows, cols, std::span<const float>(coeffs));
  }

  // Evaluates a 1D polynomial (t -> P(t)).
  // Coefficients must be provided in INCREASING degree order:
  // coeffs[0] is degree 0 (constant), coeffs[1] is degree 1, ..., coeffs[N-1] is degree N-1.
  void Poly(std::span<const float> coeffs) {
    Poly(static_cast<uint8_t>(coeffs.size()), 1, coeffs);
  }

  void Poly(std::initializer_list<float> coeffs) {
    Poly(std::span<const float>(coeffs));
  }

  // Convenience aliases (expecting coefficients in INCREASING degree order).
  void PolyVec(std::span<const float> coeffs) { Poly(coeffs); }
  void PolyVec(std::initializer_list<float> coeffs) { Poly(coeffs); }
  void PolyMat(uint8_t rows, uint8_t cols, std::span<const float> coeffs) { Poly(rows, cols, coeffs); }
  void PolyMat(uint8_t rows, uint8_t cols, std::initializer_list<float> coeffs) { Poly(rows, cols, coeffs); }

  // Terminates postfix evaluation (returns from program).
  void Ret() { EmitOp(WickedPostfixOp_Ret); }
};

// In-place, zero-allocation, bounds-checked streaming writer.
// Writes serialized postfix bytecode directly into a caller-supplied buffer or span.
class WickedPostfixStreamWriter : public WickedPostfixEmitter<WickedPostfixStreamWriter> {
 public:
  // Constructs a streaming writer targeting a buffer of given capacity.
  WickedPostfixStreamWriter(uint8_t* dest, size_t capacity)
      : dest_(dest), capacity_(capacity), offset_(0), overflow_(dest == nullptr && capacity > 0) {}

  // Constructs a streaming writer targeting a std::span buffer.
  explicit WickedPostfixStreamWriter(std::span<uint8_t> buffer)
      : WickedPostfixStreamWriter(buffer.data(), buffer.size()) {}

  // Emits a raw byte into the stream with strict bounds checking.
  void EmitByte(uint8_t b) {
    if (offset_ < capacity_) {
      dest_[offset_] = b;
    } else {
      overflow_ = true;
    }
    offset_++;
  }

  // Finalizes the bytecode stream: appends Ret (if not already emitted) and validates bounds.
  // Returns true on success, false if buffer overflowed or exceeds 65535 bytes.
  bool Finalize() {
    if (overflow_) return false;
    if (offset_ == 0 || dest_[offset_ - 1] != static_cast<uint8_t>(WickedPostfixOp_Ret)) {
      EmitByte(static_cast<uint8_t>(WickedPostfixOp_Ret));
      if (overflow_) return false;
    }
    if (offset_ > UINT16_MAX) return false;
    return true;
  }

  // Returns true if no overflow occurred.
  bool ok() const { return !overflow_; }

  // Returns true if write operations exceeded capacity.
  bool overflow() const { return overflow_; }

  // Total bytes successfully written so far. Returns 0 on overflow.
  size_t bytes_written() const { return overflow_ ? 0 : offset_; }

  // Total bytes required for bytecode (tracks total needed even after overflow).
  size_t bytes_needed() const { return offset_; }

  // Size of bytecode payload in bytes.
  uint16_t prog_size() const {
    return static_cast<uint16_t>(overflow_ ? 0 : offset_);
  }

  // Pointer to bytecode payload.
  const uint8_t* prog_data() const { return dest_; }
  uint8_t* prog_data() { return dest_; }

  // Pointer to entire serialized buffer.
  const uint8_t* data() const { return dest_; }
  uint8_t* data() { return dest_; }

  // Total capacity of the underlying buffer.
  size_t capacity() const { return capacity_; }

 private:
  uint8_t* dest_ = nullptr;
  size_t capacity_ = 0;
  size_t offset_ = 0;
  bool overflow_ = false;
};

// Builds serialized postfix bytecode programs dynamically using std::vector.
class WickedPostfixWriter : public WickedPostfixEmitter<WickedPostfixWriter> {
 public:
  // Pointer to bytecode data buffer.
  uint8_t* prog_data() { return prog_.data(); }
  // Const pointer to bytecode data buffer.
  const uint8_t* prog_data() const { return prog_.data(); }
  // Bytecode size in bytes.
  uint16_t prog_size() const { return static_cast<uint16_t>(prog_.size()); }
  // Total serialized size including trailing Ret if not already present.
  uint16_t data_size() const {
    return static_cast<uint16_t>(prog_.size() + (ends_with_ret() ? 0 : 1));
  }

  // Serializes bytecode into the provided buffer, ensuring trailing Ret.
  bool Write(uint8_t* data, size_t size) const {
    if (size < data_size()) return false;
    if (!prog_.empty()) {
      std::memcpy(data, prog_.data(), prog_.size());
    }
    if (!ends_with_ret()) {
      data[prog_.size()] = static_cast<uint8_t>(WickedPostfixOp_Ret);
    }
    return true;
  }

  // Serializes bytecode into a newly allocated vector, ensuring trailing Ret.
  std::vector<uint8_t> Write() const {
    std::vector<uint8_t> buffer(data_size());
    Write(buffer.data(), buffer.size());
    return buffer;
  }

  // Clears the bytecode buffer.
  void clear() { prog_.clear(); }

  // Emits a raw byte into the bytecode stream.
  void EmitByte(uint8_t b) { prog_.push_back(b); }

 private:
  bool ends_with_ret() const {
    return !prog_.empty() && prog_.back() == static_cast<uint8_t>(WickedPostfixOp_Ret);
  }
  std::vector<uint8_t> prog_;
};
