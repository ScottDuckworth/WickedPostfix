#pragma once

#include "WickedPostfix.h"

#include <bit>
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

  // Emits a 32-bit float literal into the bytecode stream (little-endian).
  void EmitFloat(float f) {
    if constexpr (std::endian::native == std::endian::little) {
      uint8_t bytes[4];
      std::memcpy(bytes, &f, 4);
      for (uint8_t b : bytes) derived().EmitByte(b);
    } else {
      uint32_t bits;
      std::memcpy(&bits, &f, 4);
      derived().EmitByte(static_cast<uint8_t>(bits & 0xFF));
      derived().EmitByte(static_cast<uint8_t>((bits >> 8) & 0xFF));
      derived().EmitByte(static_cast<uint8_t>((bits >> 16) & 0xFF));
      derived().EmitByte(static_cast<uint8_t>((bits >> 24) & 0xFF));
    }
  }

  // Emits a 16-bit unsigned integer into the bytecode stream (little-endian).
  void EmitUint16(uint16_t u) {
    derived().EmitByte(static_cast<uint8_t>(u & 0xFF));
    derived().EmitByte(static_cast<uint8_t>((u >> 8) & 0xFF));
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

  // Unconditional subroutine call to target byte offset.
  void Call(uint16_t target_offset) {
    EmitOp(WickedPostfixOp_Call);
    EmitUint16(target_offset);
  }

  // Evaluates a piecewise function over scalar x with explicit branch offsets.
  void Piecewise(bool relative, std::span<const float> knots, std::span<const uint16_t> offsets) {
    assert(knots.size() >= 1 && knots.size() <= 127);
    assert(offsets.size() == knots.size() + 1);
    EmitOp(WickedPostfixOp_Piecewise);
    uint8_t count_byte = static_cast<uint8_t>(knots.size() | (relative ? 0x80 : 0x00));
    derived().EmitByte(count_byte);
    for (float k : knots) {
      EmitFloat(k);
    }
    for (uint16_t off : offsets) {
      EmitUint16(off);
    }
  }

  void Piecewise(std::span<const float> knots, std::span<const uint16_t> offsets) {
    Piecewise(false, knots, offsets);
  }

  void Piecewise(std::initializer_list<float> knots, std::initializer_list<uint16_t> offsets) {
    Piecewise(false, std::span<const float>(knots.begin(), knots.size()),
              std::span<const uint16_t>(offsets.begin(), offsets.size()));
  }

  void PiecewiseRelative(std::span<const float> knots, std::span<const uint16_t> offsets) {
    Piecewise(true, knots, offsets);
  }

  void PiecewiseRelative(std::initializer_list<float> knots, std::initializer_list<uint16_t> offsets) {
    Piecewise(true, std::span<const float>(knots.begin(), knots.size()),
              std::span<const uint16_t>(offsets.begin(), offsets.size()));
  }

  // Terminates postfix evaluation (returns from program).
  void Ret() {
    EmitOp(WickedPostfixOp_Ret);
    derived().set_ends_with_ret(true);
  }
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

  void set_ends_with_ret(bool val) { ends_with_ret_ = val; }

  // Emits a raw byte into the stream with strict bounds checking.
  void EmitByte(uint8_t b) {
    if (offset_ < capacity_) {
      dest_[offset_] = b;
    } else {
      overflow_ = true;
    }
    offset_++;
    ends_with_ret_ = false;
  }

  // Finalizes the bytecode stream: appends Ret (if not already emitted) and validates bounds.
  // Returns true on success, false if buffer overflowed or exceeds 65535 bytes.
  bool Finalize() {
    if (overflow_) return false;
    if (!ends_with_ret_) {
      EmitByte(static_cast<uint8_t>(WickedPostfixOp_Ret));
      ends_with_ret_ = true;
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
  bool ends_with_ret_ = false;
};

// Handle referencing a defined subroutine in WickedPostfixWriter.
struct SubroutineHandle {
  size_t index = 0;
};

// Builds serialized postfix bytecode programs dynamically using std::vector.
class WickedPostfixWriter : public WickedPostfixEmitter<WickedPostfixWriter> {
 public:
  // Defines a subroutine that will be appended after the main program upon serialization.
  // Ensures the subroutine terminates with Ret.
  SubroutineHandle DefineSubroutine(std::vector<uint8_t> bytecode) {
    if (bytecode.empty() || bytecode.back() != static_cast<uint8_t>(WickedPostfixOp_Ret)) {
      bytecode.push_back(static_cast<uint8_t>(WickedPostfixOp_Ret));
    }
    size_t idx = subroutines_.size();
    subroutines_.push_back(std::move(bytecode));
    return SubroutineHandle{idx};
  }

  SubroutineHandle DefineSubroutine(const WickedPostfixWriter& sub) {
    size_t idx = subroutines_.size();
    std::vector<uint8_t> prog = sub.prog_;
    if (!sub.ends_with_ret()) {
      prog.push_back(static_cast<uint8_t>(WickedPostfixOp_Ret));
    }
    subroutines_.push_back(std::move(prog));

    // Absorb sub's call_patches
    for (const auto& cp : sub.call_patches_) {
      call_patches_.push_back(CallPatch{
        .in_subroutine_index = idx,
        .patch_offset = cp.patch_offset,
        .target_subroutine_index = cp.target_subroutine_index,
      });
    }

    // Absorb sub's piecewise_patches
    for (const auto& pp : sub.piecewise_patches_) {
      piecewise_patches_.push_back(PiecewisePatch{
        .in_subroutine_index = idx,
        .patch_offset = pp.patch_offset,
        .subroutine_indices = pp.subroutine_indices,
      });
    }

    return SubroutineHandle{idx};
  }

  // Appends a subroutine to the writer immediately and returns its byte offset.
  uint16_t AppendSubroutine(const WickedPostfixWriter& sub) {
    uint16_t offset = static_cast<uint16_t>(prog_.size());
    auto bytes = sub.Write();
    prog_.insert(prog_.end(), bytes.begin(), bytes.end());
    return offset;
  }

  // Invokes a defined subroutine via automatic backpatching.
  void Call(SubroutineHandle sub) {
    EmitOp(WickedPostfixOp_Call);
    call_patches_.push_back(CallPatch{
      .in_subroutine_index = static_cast<size_t>(-1),
      .patch_offset = prog_.size(),
      .target_subroutine_index = sub.index,
    });
    EmitUint16(0);  // placeholder
  }

  using WickedPostfixEmitter<WickedPostfixWriter>::Call;

  // Emits Piecewise using defined subroutines and automatic backpatching.
  void Piecewise(bool relative, std::span<const float> knots, std::span<const SubroutineHandle> branches) {
    assert(knots.size() >= 1 && knots.size() <= 127);
    assert(branches.size() == knots.size() + 1);
    EmitOp(WickedPostfixOp_Piecewise);
    uint8_t count_byte = static_cast<uint8_t>(knots.size() | (relative ? 0x80 : 0x00));
    EmitByte(count_byte);
    for (float k : knots) {
      EmitFloat(k);
    }
    size_t patch_start = prog_.size();
    std::vector<size_t> branch_indices;
    branch_indices.reserve(branches.size());
    for (const auto& h : branches) {
      branch_indices.push_back(h.index);
      EmitUint16(0);  // placeholder
    }
    piecewise_patches_.push_back(PiecewisePatch{
      .in_subroutine_index = static_cast<size_t>(-1),
      .patch_offset = patch_start,
      .subroutine_indices = std::move(branch_indices),
    });
  }

  void Piecewise(std::span<const float> knots, std::span<const SubroutineHandle> branches) {
    Piecewise(false, knots, branches);
  }

  void Piecewise(std::initializer_list<float> knots, std::initializer_list<SubroutineHandle> branches) {
    Piecewise(false, std::span<const float>(knots.begin(), knots.size()),
              std::span<const SubroutineHandle>(branches.begin(), branches.size()));
  }

  void PiecewiseRelative(std::span<const float> knots, std::span<const SubroutineHandle> branches) {
    Piecewise(true, knots, branches);
  }

  void PiecewiseRelative(std::initializer_list<float> knots, std::initializer_list<SubroutineHandle> branches) {
    Piecewise(true, std::span<const float>(knots.begin(), knots.size()),
              std::span<const SubroutineHandle>(branches.begin(), branches.size()));
  }

  using WickedPostfixEmitter<WickedPostfixWriter>::Piecewise;
  using WickedPostfixEmitter<WickedPostfixWriter>::PiecewiseRelative;

  // Pointer to bytecode data buffer.
  uint8_t* prog_data() { return prog_.data(); }
  // Const pointer to bytecode data buffer.
  const uint8_t* prog_data() const { return prog_.data(); }
  // Bytecode size in bytes.
  uint16_t prog_size() const { return static_cast<uint16_t>(prog_.size()); }
  // Total serialized size including trailing Ret and any defined subroutines.
  uint16_t data_size() const {
    size_t sz = prog_.size() + (ends_with_ret() ? 0 : 1);
    for (const auto& sub : subroutines_) {
      sz += sub.size();
    }
    return static_cast<uint16_t>(sz);
  }

  // Serializes bytecode into the provided buffer, ensuring trailing Ret and backpatching subroutines.
  bool Write(uint8_t* data, size_t size) const {
    if (size < data_size()) return false;
    size_t cur = 0;
    if (!prog_.empty()) {
      std::memcpy(data, prog_.data(), prog_.size());
      cur += prog_.size();
    }
    if (!ends_with_ret()) {
      data[cur++] = static_cast<uint8_t>(WickedPostfixOp_Ret);
    }

    std::vector<uint16_t> sub_offsets;
    sub_offsets.reserve(subroutines_.size());
    for (const auto& sub : subroutines_) {
      sub_offsets.push_back(static_cast<uint16_t>(cur));
      if (!sub.empty()) {
        std::memcpy(data + cur, sub.data(), sub.size());
        cur += sub.size();
      }
    }

    // Backpatch Call offsets
    for (const auto& cp : call_patches_) {
      assert(cp.target_subroutine_index < sub_offsets.size());
      size_t base_pos = (cp.in_subroutine_index == static_cast<size_t>(-1))
                            ? 0
                            : sub_offsets[cp.in_subroutine_index];
      uint16_t target = sub_offsets[cp.target_subroutine_index];
      data[base_pos + cp.patch_offset] = static_cast<uint8_t>(target & 0xFF);
      data[base_pos + cp.patch_offset + 1] = static_cast<uint8_t>((target >> 8) & 0xFF);
    }

    // Backpatch Piecewise branch offsets
    for (const auto& pp : piecewise_patches_) {
      size_t base_pos = (pp.in_subroutine_index == static_cast<size_t>(-1))
                            ? 0
                            : sub_offsets[pp.in_subroutine_index];
      for (size_t b = 0; b < pp.subroutine_indices.size(); ++b) {
        assert(pp.subroutine_indices[b] < sub_offsets.size());
        uint16_t target = sub_offsets[pp.subroutine_indices[b]];
        size_t patch_pos = base_pos + pp.patch_offset + b * 2;
        data[patch_pos] = static_cast<uint8_t>(target & 0xFF);
        data[patch_pos + 1] = static_cast<uint8_t>((target >> 8) & 0xFF);
      }
    }

    return true;
  }

  // Serializes bytecode into a newly allocated vector, ensuring trailing Ret.
  std::vector<uint8_t> Write() const {
    std::vector<uint8_t> buffer(data_size());
    Write(buffer.data(), buffer.size());
    return buffer;
  }

  void set_ends_with_ret(bool val) { ends_with_ret_ = val; }

  // Clears the bytecode buffer and pending subroutines.
  void clear() {
    prog_.clear();
    subroutines_.clear();
    call_patches_.clear();
    piecewise_patches_.clear();
    ends_with_ret_ = false;
  }

  // Emits a raw byte into the bytecode stream.
  void EmitByte(uint8_t b) {
    prog_.push_back(b);
    ends_with_ret_ = false;
  }

  bool ends_with_ret() const {
    return ends_with_ret_;
  }

 private:
  bool ends_with_ret_ = false;

  struct CallPatch {
    size_t in_subroutine_index;
    size_t patch_offset;
    size_t target_subroutine_index;
  };
  struct PiecewisePatch {
    size_t in_subroutine_index;
    size_t patch_offset;
    std::vector<size_t> subroutine_indices;
  };

  std::vector<uint8_t> prog_;
  std::vector<std::vector<uint8_t>> subroutines_;
  std::vector<CallPatch> call_patches_;
  std::vector<PiecewisePatch> piecewise_patches_;
};
