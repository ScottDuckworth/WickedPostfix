#include "WickedPostfix.h"
#include "WickedMath.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <iterator>
#include <limits>
#include <span>

static_assert(sizeof(float) == 4, "Host float must be 32-bit IEEE 754");
static_assert(std::numeric_limits<float>::is_iec559, "Host float must adhere to IEC 60559 / IEEE 754");

#ifndef WICKED_ALWAYS_INLINE
#if defined(_MSC_VER)
#define WICKED_ALWAYS_INLINE __forceinline
#define WICKED_RESTRICT __restrict
#elif defined(__GNUC__) || defined(__clang__)
#define WICKED_ALWAYS_INLINE inline __attribute__((always_inline))
#define WICKED_RESTRICT __restrict__
#else
#define WICKED_ALWAYS_INLINE inline
#define WICKED_RESTRICT
#endif
#endif

extern "C" const char* WickedPostfixOpToString(WickedPostfixOp_t op) {
  switch (op) {
    case WickedPostfixOp_Undefined: return "Undefined";
    case WickedPostfixOp_Ret:       return "Ret";
    case WickedPostfixOp_Call:      return "Call";
    case WickedPostfixOp_Piecewise: return "Piecewise";
    case WickedPostfixOp_Push:      return "Push";
    case WickedPostfixOp_Dup:       return "Dup";
    case WickedPostfixOp_Over:      return "Over";
    case WickedPostfixOp_Tuck:      return "Tuck";
    case WickedPostfixOp_Drop:      return "Drop";
    case WickedPostfixOp_Nip:       return "Nip";
    case WickedPostfixOp_Swap:      return "Swap";
    case WickedPostfixOp_Rot:       return "Rot";
    case WickedPostfixOp_Move:      return "Move";
    case WickedPostfixOp_Copy:      return "Copy";
    case WickedPostfixOp_CopyBase:  return "CopyBase";
    case WickedPostfixOp_Slide:     return "Slide";
    case WickedPostfixOp_Add:       return "Add";
    case WickedPostfixOp_Sub:       return "Sub";
    case WickedPostfixOp_Mul:       return "Mul";
    case WickedPostfixOp_Div:       return "Div";
    case WickedPostfixOp_MulAdd:    return "MulAdd";
    case WickedPostfixOp_MulSub:    return "MulSub";
    case WickedPostfixOp_SubMul:    return "SubMul";
    case WickedPostfixOp_Neg:       return "Neg";
    case WickedPostfixOp_Abs:       return "Abs";
    case WickedPostfixOp_Inv:       return "Inv";
    case WickedPostfixOp_Sqrt:      return "Sqrt";
    case WickedPostfixOp_Sin:       return "Sin";
    case WickedPostfixOp_Cos:       return "Cos";
    case WickedPostfixOp_Tan:       return "Tan";
    case WickedPostfixOp_Asin:      return "Asin";
    case WickedPostfixOp_Acos:      return "Acos";
    case WickedPostfixOp_Atan2:     return "Atan2";
    case WickedPostfixOp_CosSin:    return "CosSin";
    case WickedPostfixOp_PanTilt:   return "PanTilt";
    case WickedPostfixOp_Ipow:      return "Ipow";
    case WickedPostfixOp_Pow:       return "Pow";
    case WickedPostfixOp_Exp:       return "Exp";
    case WickedPostfixOp_Ln:        return "Ln";
    case WickedPostfixOp_PushVec:   return "PushVec";
    case WickedPostfixOp_AddVec:    return "AddVec";
    case WickedPostfixOp_SubVec:    return "SubVec";
    case WickedPostfixOp_MulVec:    return "MulVec";
    case WickedPostfixOp_DivVec:    return "DivVec";
    case WickedPostfixOp_MulAddVec: return "MulAddVec";
    case WickedPostfixOp_MulSubVec: return "MulSubVec";
    case WickedPostfixOp_SubMulVec: return "SubMulVec";
    case WickedPostfixOp_NegVec:    return "NegVec";
    case WickedPostfixOp_AbsVec:    return "AbsVec";
    case WickedPostfixOp_InvVec:    return "InvVec";
    case WickedPostfixOp_DotVec:    return "DotVec";
    case WickedPostfixOp_NormSqVec: return "NormSqVec";
    case WickedPostfixOp_SumVec:    return "SumVec";
    case WickedPostfixOp_ScaleVec:  return "ScaleVec";
    case WickedPostfixOp_MulMat:    return "MulMat";
    case WickedPostfixOp_Poly:      return "Poly";
    default:
      return "UNKNOWN";
  }
}

namespace {

static constexpr size_t WIRE_FLOAT_SIZE = 4;
static constexpr size_t WIRE_UINT16_SIZE = 2;

WICKED_ALWAYS_INLINE uint16_t read_wire_uint16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

WICKED_ALWAYS_INLINE float read_wire_float(const uint8_t* p) {
  if constexpr (std::endian::native == std::endian::little) {
    float f;
    std::memcpy(&f, p, WIRE_FLOAT_SIZE);
    return f;
  } else {
    uint32_t bits = static_cast<uint32_t>(p[0]) |
                    (static_cast<uint32_t>(p[1]) << 8) |
                    (static_cast<uint32_t>(p[2]) << 16) |
                    (static_cast<uint32_t>(p[3]) << 24);
    float f;
    std::memcpy(&f, &bits, WIRE_FLOAT_SIZE);
    return f;
  }
}

struct ValidationContext {
  const uint8_t* const prog_data;
  const uint8_t* const prog_end;
  const uint8_t* pc;
  int32_t stack_depth;
  int32_t max_stack_depth;
  uint8_t call_depth;

  WICKED_ALWAYS_INLINE void update_max(int32_t depth) {
    if (depth > max_stack_depth) {
      max_stack_depth = depth;
    }
  }

  WICKED_ALWAYS_INLINE bool has_bytes(uint16_t n) const {
    return static_cast<size_t>(prog_end - pc) >= n;
  }

  WICKED_ALWAYS_INLINE uint8_t fetch_byte() {
    return *pc++;
  }

  WICKED_ALWAYS_INLINE uint16_t fetch_uint16() {
    uint16_t u = read_wire_uint16(pc);
    pc += WIRE_UINT16_SIZE;
    return u;
  }

  WICKED_ALWAYS_INLINE float fetch_float() {
    float f = read_wire_float(pc);
    pc += WIRE_FLOAT_SIZE;
    return f;
  }

  WICKED_ALWAYS_INLINE void skip(uint16_t n) {
    pc += n;
  }
};

static constexpr uint8_t MAX_CALL_DEPTH = 4;

static WickedEvalStatus_t ValidateInternal(
    const uint8_t* prog_data,
    const uint8_t* prog_end,
    const uint8_t* entry_pc,
    int32_t initial_stack_size,
    uint8_t call_depth,
    int32_t* out_max_depth,
    int32_t* out_final_depth);

static WickedEvalStatus_t ValidateSubroutine(
    const uint8_t* prog_data,
    const uint8_t* prog_end,
    uint16_t target_offset,
    int32_t initial_stack_size,
    uint8_t call_depth,
    int32_t* out_max_depth,
    int32_t* out_final_depth) {
  if (target_offset >= static_cast<size_t>(prog_end - prog_data)) {
    return WickedEvalStatus_IllegalOperation;
  }
  if (call_depth >= MAX_CALL_DEPTH) {
    return WickedEvalStatus_StackOverflow;
  }
  return ValidateInternal(
      prog_data, prog_end, prog_data + target_offset, initial_stack_size, call_depth, out_max_depth, out_final_depth);
}

namespace validate {

WICKED_ALWAYS_INLINE WickedEvalStatus_t Push(ValidationContext& ctx) {
  if (!ctx.has_bytes(WIRE_FLOAT_SIZE)) return WickedEvalStatus_IllegalOperation;
  ctx.skip(WIRE_FLOAT_SIZE);
  ctx.stack_depth += 1;
  ctx.update_max(ctx.stack_depth);
  return WickedEvalStatus_Ok;
}

WICKED_ALWAYS_INLINE WickedEvalStatus_t PushVec(ValidationContext& ctx) {
  if (!ctx.has_bytes(1)) return WickedEvalStatus_IllegalOperation;
  uint8_t size = ctx.fetch_byte() + 1;
  uint16_t byte_count = uint16_t(size) * WIRE_FLOAT_SIZE;
  if (!ctx.has_bytes(byte_count)) return WickedEvalStatus_IllegalOperation;
  ctx.skip(byte_count);
  ctx.stack_depth += size;
  ctx.update_max(ctx.stack_depth);
  return WickedEvalStatus_Ok;
}

WICKED_ALWAYS_INLINE WickedEvalStatus_t Drop(ValidationContext& ctx) {
  if (ctx.stack_depth < 1) return WickedEvalStatus_StackUnderflow;
  ctx.stack_depth -= 1;
  return WickedEvalStatus_Ok;
}

WICKED_ALWAYS_INLINE WickedEvalStatus_t Dup(ValidationContext& ctx) {
  if (ctx.stack_depth < 1) return WickedEvalStatus_StackUnderflow;
  ctx.stack_depth += 1;
  ctx.update_max(ctx.stack_depth);
  return WickedEvalStatus_Ok;
}

WICKED_ALWAYS_INLINE WickedEvalStatus_t Swap(ValidationContext& ctx) {
  if (ctx.stack_depth < 2) return WickedEvalStatus_StackUnderflow;
  return WickedEvalStatus_Ok;
}

WICKED_ALWAYS_INLINE WickedEvalStatus_t Over(ValidationContext& ctx) {
  if (ctx.stack_depth < 2) return WickedEvalStatus_StackUnderflow;
  ctx.stack_depth += 1;
  ctx.update_max(ctx.stack_depth);
  return WickedEvalStatus_Ok;
}

WICKED_ALWAYS_INLINE WickedEvalStatus_t Tuck(ValidationContext& ctx) {
  if (ctx.stack_depth < 2) return WickedEvalStatus_StackUnderflow;
  ctx.stack_depth += 1;
  ctx.update_max(ctx.stack_depth);
  return WickedEvalStatus_Ok;
}

WICKED_ALWAYS_INLINE WickedEvalStatus_t Nip(ValidationContext& ctx) {
  if (ctx.stack_depth < 2) return WickedEvalStatus_StackUnderflow;
  ctx.stack_depth -= 1;
  return WickedEvalStatus_Ok;
}

WICKED_ALWAYS_INLINE WickedEvalStatus_t Rot(ValidationContext& ctx) {
  if (ctx.stack_depth < 3) return WickedEvalStatus_StackUnderflow;
  return WickedEvalStatus_Ok;
}

WICKED_ALWAYS_INLINE WickedEvalStatus_t Copy(ValidationContext& ctx) {
  if (!ctx.has_bytes(1)) return WickedEvalStatus_IllegalOperation;
  uint8_t arg = ctx.fetch_byte();
  uint8_t offset = arg & 0x1F;
  uint8_t size = (arg >> 5) + 1;
  if (ctx.stack_depth < int32_t(offset) + size) return WickedEvalStatus_StackUnderflow;
  ctx.stack_depth += size;
  ctx.update_max(ctx.stack_depth);
  return WickedEvalStatus_Ok;
}

WICKED_ALWAYS_INLINE WickedEvalStatus_t CopyBase(ValidationContext& ctx) {
  if (!ctx.has_bytes(1)) return WickedEvalStatus_IllegalOperation;
  uint8_t arg = ctx.fetch_byte();
  uint8_t index = arg & 0x1F;
  uint8_t size = (arg >> 5) + 1;
  if (ctx.stack_depth < int32_t(index) + size) return WickedEvalStatus_StackUnderflow;
  ctx.stack_depth += size;
  ctx.update_max(ctx.stack_depth);
  return WickedEvalStatus_Ok;
}

WICKED_ALWAYS_INLINE WickedEvalStatus_t Move(ValidationContext& ctx) {
  if (!ctx.has_bytes(1)) return WickedEvalStatus_IllegalOperation;
  uint8_t arg = ctx.fetch_byte();
  uint8_t offset = arg & 0x1F;
  uint8_t size = (arg >> 5) + 1;
  if (ctx.stack_depth < int32_t(offset) + size) return WickedEvalStatus_StackUnderflow;
  return WickedEvalStatus_Ok;
}

WICKED_ALWAYS_INLINE WickedEvalStatus_t Slide(ValidationContext& ctx) {
  if (!ctx.has_bytes(1)) return WickedEvalStatus_IllegalOperation;
  uint8_t arg = ctx.fetch_byte();
  uint8_t keep = (arg & 0x0F) + 1;
  uint8_t drop = (arg >> 4) + 1;
  if (ctx.stack_depth < int32_t(keep) + drop) return WickedEvalStatus_StackUnderflow;
  ctx.stack_depth -= drop;
  return WickedEvalStatus_Ok;
}

WICKED_ALWAYS_INLINE WickedEvalStatus_t BinaryOp(ValidationContext& ctx) {
  if (ctx.stack_depth < 2) return WickedEvalStatus_StackUnderflow;
  ctx.stack_depth -= 1;
  return WickedEvalStatus_Ok;
}

WICKED_ALWAYS_INLINE WickedEvalStatus_t TernaryOp(ValidationContext& ctx) {
  if (ctx.stack_depth < 3) return WickedEvalStatus_StackUnderflow;
  ctx.stack_depth -= 2;
  return WickedEvalStatus_Ok;
}

WICKED_ALWAYS_INLINE WickedEvalStatus_t UnaryOp(ValidationContext& ctx) {
  if (ctx.stack_depth < 1) return WickedEvalStatus_StackUnderflow;
  return WickedEvalStatus_Ok;
}

WICKED_ALWAYS_INLINE WickedEvalStatus_t CosSin(ValidationContext& ctx) {
  if (ctx.stack_depth < 1) return WickedEvalStatus_StackUnderflow;
  ctx.stack_depth += 1;
  ctx.update_max(ctx.stack_depth);
  return WickedEvalStatus_Ok;
}

WICKED_ALWAYS_INLINE WickedEvalStatus_t PanTilt(ValidationContext& ctx) {
  if (ctx.stack_depth < 3) return WickedEvalStatus_StackUnderflow;
  ctx.stack_depth -= 1;
  return WickedEvalStatus_Ok;
}

WICKED_ALWAYS_INLINE WickedEvalStatus_t Ipow(ValidationContext& ctx) {
  if (!ctx.has_bytes(1)) return WickedEvalStatus_IllegalOperation;
  ctx.skip(1);
  if (ctx.stack_depth < 1) return WickedEvalStatus_StackUnderflow;
  return WickedEvalStatus_Ok;
}

WICKED_ALWAYS_INLINE WickedEvalStatus_t BinaryVec(ValidationContext& ctx) {
  if (!ctx.has_bytes(1)) return WickedEvalStatus_IllegalOperation;
  uint8_t size = ctx.fetch_byte() + 1;
  if (ctx.stack_depth < int32_t(size) * 2) return WickedEvalStatus_StackUnderflow;
  ctx.stack_depth -= size;
  return WickedEvalStatus_Ok;
}

WICKED_ALWAYS_INLINE WickedEvalStatus_t TernaryVec(ValidationContext& ctx) {
  if (!ctx.has_bytes(1)) return WickedEvalStatus_IllegalOperation;
  uint8_t size = ctx.fetch_byte() + 1;
  if (ctx.stack_depth < int32_t(size) * 3) return WickedEvalStatus_StackUnderflow;
  ctx.stack_depth -= int32_t(size) * 2;
  return WickedEvalStatus_Ok;
}

WICKED_ALWAYS_INLINE WickedEvalStatus_t UnaryVec(ValidationContext& ctx) {
  if (!ctx.has_bytes(1)) return WickedEvalStatus_IllegalOperation;
  uint8_t size = ctx.fetch_byte() + 1;
  if (ctx.stack_depth < size) return WickedEvalStatus_StackUnderflow;
  return WickedEvalStatus_Ok;
}

WICKED_ALWAYS_INLINE WickedEvalStatus_t DotVec(ValidationContext& ctx) {
  if (!ctx.has_bytes(1)) return WickedEvalStatus_IllegalOperation;
  uint8_t size = ctx.fetch_byte() + 1;
  if (ctx.stack_depth < int32_t(size) * 2) return WickedEvalStatus_StackUnderflow;
  ctx.stack_depth -= (int32_t(size) * 2 - 1);
  return WickedEvalStatus_Ok;
}

WICKED_ALWAYS_INLINE WickedEvalStatus_t ReduceVec(ValidationContext& ctx) {
  if (!ctx.has_bytes(1)) return WickedEvalStatus_IllegalOperation;
  uint8_t size = ctx.fetch_byte() + 1;
  if (ctx.stack_depth < size) return WickedEvalStatus_StackUnderflow;
  ctx.stack_depth -= (size - 1);
  return WickedEvalStatus_Ok;
}

WICKED_ALWAYS_INLINE WickedEvalStatus_t ScaleVec(ValidationContext& ctx) {
  if (!ctx.has_bytes(1)) return WickedEvalStatus_IllegalOperation;
  uint8_t size = ctx.fetch_byte() + 1;
  if (ctx.stack_depth < int32_t(size) + 1) return WickedEvalStatus_StackUnderflow;
  ctx.stack_depth -= 1;
  return WickedEvalStatus_Ok;
}

WICKED_ALWAYS_INLINE WickedEvalStatus_t MulMat(ValidationContext& ctx) {
  if (!ctx.has_bytes(1)) return WickedEvalStatus_IllegalOperation;
  uint8_t arg = ctx.fetch_byte();
  if ((arg & 0x0C) != 0) return WickedEvalStatus_IllegalOperation;
  uint8_t rows = (arg & 0x03) + 1;
  uint8_t cols = ((arg >> 4) & 0x0F) + 1;
  uint16_t byte_count = uint16_t(rows) * cols * WIRE_FLOAT_SIZE;
  if (!ctx.has_bytes(byte_count)) return WickedEvalStatus_IllegalOperation;
  ctx.skip(byte_count);
  if (ctx.stack_depth < rows) return WickedEvalStatus_StackUnderflow;
  ctx.stack_depth -= rows;
  ctx.update_max(ctx.stack_depth + cols);
  ctx.stack_depth += cols;
  return WickedEvalStatus_Ok;
}

WICKED_ALWAYS_INLINE WickedEvalStatus_t Poly(ValidationContext& ctx) {
  if (!ctx.has_bytes(1)) return WickedEvalStatus_IllegalOperation;
  uint8_t arg = ctx.fetch_byte();
  uint8_t rows = (arg & 0x0F) + 1;
  uint8_t cols = ((arg >> 4) & 0x0F) + 1;
  uint16_t coeff_size = uint16_t(rows) * cols;
  uint16_t byte_count = coeff_size * WIRE_FLOAT_SIZE;
  if (!ctx.has_bytes(byte_count)) return WickedEvalStatus_IllegalOperation;
  ctx.skip(byte_count);
  if (ctx.stack_depth < 1) return WickedEvalStatus_StackUnderflow;
  ctx.stack_depth = ctx.stack_depth - 1 + cols;
  ctx.update_max(ctx.stack_depth);
  return WickedEvalStatus_Ok;
}

WICKED_ALWAYS_INLINE WickedEvalStatus_t Call(ValidationContext& ctx) {
  if (!ctx.has_bytes(WIRE_UINT16_SIZE)) {
    return WickedEvalStatus_IllegalOperation;
  }
  uint16_t target_offset = ctx.fetch_uint16();
  int32_t sub_max = 0, sub_final = 0;
  WickedEvalStatus_t status = ValidateSubroutine(
      ctx.prog_data, ctx.prog_end, target_offset, ctx.stack_depth, ctx.call_depth + 1, &sub_max, &sub_final);
  if (status != WickedEvalStatus_Ok) return status;
  ctx.update_max(sub_max);
  ctx.stack_depth = sub_final;
  ctx.update_max(ctx.stack_depth);
  return WickedEvalStatus_Ok;
}

WICKED_ALWAYS_INLINE WickedEvalStatus_t Piecewise(ValidationContext& ctx) {
  if (ctx.stack_depth < 1) {
    return WickedEvalStatus_StackUnderflow;
  }
  if (!ctx.has_bytes(1)) {
    return WickedEvalStatus_IllegalOperation;
  }
  uint8_t count_byte = ctx.fetch_byte();
  uint8_t num_knots = count_byte & 0x7F;
  if (num_knots == 0) {
    return WickedEvalStatus_IllegalOperation;
  }
  size_t req_bytes = static_cast<size_t>(num_knots) * WIRE_FLOAT_SIZE + static_cast<size_t>(num_knots + 1) * WIRE_UINT16_SIZE;
  if (!ctx.has_bytes(static_cast<uint16_t>(req_bytes))) {
    return WickedEvalStatus_IllegalOperation;
  }
  float prev_k = -std::numeric_limits<float>::infinity();
  for (uint8_t i = 0; i < num_knots; ++i) {
    float k = ctx.fetch_float();
    if (std::isnan(k) || (i > 0 && k <= prev_k)) {
      return WickedEvalStatus_IllegalOperation;
    }
    prev_k = k;
  }
  const uint8_t* offsets_ptr = ctx.pc;
  ctx.skip(static_cast<uint16_t>((num_knots + 1) * WIRE_UINT16_SIZE));
  int32_t expected_final = 0;
  for (uint8_t i = 0; i <= num_knots; ++i) {
    uint16_t branch_offset = read_wire_uint16(offsets_ptr + i * WIRE_UINT16_SIZE);
    int32_t branch_max = 0, branch_final = 0;
    WickedEvalStatus_t status = ValidateSubroutine(
        ctx.prog_data, ctx.prog_end, branch_offset, ctx.stack_depth, ctx.call_depth + 1, &branch_max, &branch_final);
    if (status != WickedEvalStatus_Ok) return status;
    if (i == 0) {
      expected_final = branch_final;
    } else if (branch_final != expected_final) {
      return WickedEvalStatus_UnexpectedStackSize;
    }
    ctx.update_max(branch_max);
  }
  ctx.stack_depth = expected_final;
  ctx.update_max(ctx.stack_depth);
  return WickedEvalStatus_Ok;
}

}  // namespace validate

static WickedEvalStatus_t ValidateInternal(
    const uint8_t* prog_data,
    const uint8_t* prog_end,
    const uint8_t* entry_pc,
    int32_t initial_stack_size,
    uint8_t call_depth,
    int32_t* out_max_depth,
    int32_t* out_final_depth) {
  ValidationContext ctx{
    .prog_data = prog_data,
    .prog_end = prog_end,
    .pc = entry_pc,
    .stack_depth = initial_stack_size,
    .max_stack_depth = initial_stack_size,
    .call_depth = call_depth,
  };

  while (ctx.pc < ctx.prog_end) {
    uint8_t op = ctx.fetch_byte();
    WickedEvalStatus_t status = WickedEvalStatus_Ok;
    switch (op) {
      case WickedPostfixOp_Ret:
        if (out_max_depth) {
          *out_max_depth = ctx.max_stack_depth;
        }
        if (out_final_depth) {
          *out_final_depth = ctx.stack_depth;
        }
        return WickedEvalStatus_Ok;
      case WickedPostfixOp_Call:      status = validate::Call(ctx); break;
      case WickedPostfixOp_Piecewise: status = validate::Piecewise(ctx); break;
      case WickedPostfixOp_Push:      status = validate::Push(ctx); break;
      case WickedPostfixOp_Dup:       status = validate::Dup(ctx); break;
      case WickedPostfixOp_Over:      status = validate::Over(ctx); break;
      case WickedPostfixOp_Tuck:      status = validate::Tuck(ctx); break;
      case WickedPostfixOp_Drop:      status = validate::Drop(ctx); break;
      case WickedPostfixOp_Nip:       status = validate::Nip(ctx); break;
      case WickedPostfixOp_Swap:      status = validate::Swap(ctx); break;
      case WickedPostfixOp_Rot:       status = validate::Rot(ctx); break;
      case WickedPostfixOp_Move:      status = validate::Move(ctx); break;
      case WickedPostfixOp_Copy:      status = validate::Copy(ctx); break;
      case WickedPostfixOp_CopyBase:  status = validate::CopyBase(ctx); break;
      case WickedPostfixOp_Slide:     status = validate::Slide(ctx); break;
      case WickedPostfixOp_Add:
      case WickedPostfixOp_Sub:
      case WickedPostfixOp_Mul:
      case WickedPostfixOp_Div:       status = validate::BinaryOp(ctx); break;
      case WickedPostfixOp_MulAdd:
      case WickedPostfixOp_MulSub:
      case WickedPostfixOp_SubMul:    status = validate::TernaryOp(ctx); break;
      case WickedPostfixOp_Neg:
      case WickedPostfixOp_Abs:
      case WickedPostfixOp_Inv:
      case WickedPostfixOp_Sqrt:
      case WickedPostfixOp_Sin:
      case WickedPostfixOp_Cos:
      case WickedPostfixOp_Tan:
      case WickedPostfixOp_Asin:
      case WickedPostfixOp_Acos:      status = validate::UnaryOp(ctx); break;
      case WickedPostfixOp_Atan2:     status = validate::BinaryOp(ctx); break;
      case WickedPostfixOp_CosSin:    status = validate::CosSin(ctx); break;
      case WickedPostfixOp_PanTilt:   status = validate::PanTilt(ctx); break;
      case WickedPostfixOp_Ipow:      status = validate::Ipow(ctx); break;
      case WickedPostfixOp_Pow:       status = validate::BinaryOp(ctx); break;
      case WickedPostfixOp_Exp:
      case WickedPostfixOp_Ln:        status = validate::UnaryOp(ctx); break;
      case WickedPostfixOp_PushVec:   status = validate::PushVec(ctx); break;
      case WickedPostfixOp_AddVec:
      case WickedPostfixOp_SubVec:
      case WickedPostfixOp_MulVec:
      case WickedPostfixOp_DivVec:    status = validate::BinaryVec(ctx); break;
      case WickedPostfixOp_MulAddVec:
      case WickedPostfixOp_MulSubVec:
      case WickedPostfixOp_SubMulVec: status = validate::TernaryVec(ctx); break;
      case WickedPostfixOp_NegVec:
      case WickedPostfixOp_AbsVec:
      case WickedPostfixOp_InvVec:    status = validate::UnaryVec(ctx); break;
      case WickedPostfixOp_DotVec:    status = validate::DotVec(ctx); break;
      case WickedPostfixOp_NormSqVec:
      case WickedPostfixOp_SumVec:    status = validate::ReduceVec(ctx); break;
      case WickedPostfixOp_ScaleVec:  status = validate::ScaleVec(ctx); break;
      case WickedPostfixOp_MulMat:    status = validate::MulMat(ctx); break;
      case WickedPostfixOp_Poly:      status = validate::Poly(ctx); break;
      default:
        return WickedEvalStatus_UndefinedOperation;
    }

    if (status != WickedEvalStatus_Ok) {
      return status;
    }
  }
  return WickedEvalStatus_IllegalOperation;
}

}  // namespace

extern "C" WickedEvalStatus_t WickedPostfixValidate(const uint8_t* prog_data, size_t buffer_size, uint16_t initial_stack_size, uint16_t* minimum_stack_capacity, uint16_t* final_stack_size) {
  if (!prog_data || buffer_size == 0) {
    return WickedEvalStatus_IllegalOperation;
  }

  int32_t max_depth = 0;
  int32_t final_depth = 0;
  WickedEvalStatus_t status = ValidateInternal(
      prog_data, prog_data + buffer_size, prog_data, initial_stack_size, 0, &max_depth, &final_depth);
  if (status != WickedEvalStatus_Ok) {
    return status;
  }

  if (max_depth > UINT16_MAX) {
    return WickedEvalStatus_StackOverflow;
  }

  if (minimum_stack_capacity) {
    *minimum_stack_capacity = static_cast<uint16_t>(max_depth);
  }
  if (final_stack_size) {
    *final_stack_size = static_cast<uint16_t>(final_depth);
  }
  return WickedEvalStatus_Ok;
}

namespace {

struct EvalContext {
  const uint8_t* pc;
  float* sp;
  float* const stack_base;
  const uint8_t* const root_prog;

  WICKED_ALWAYS_INLINE uint8_t fetch_byte() {
    return *pc++;
  }

  WICKED_ALWAYS_INLINE uint16_t fetch_uint16() {
    uint16_t u = read_wire_uint16(pc);
    pc += WIRE_UINT16_SIZE;
    return u;
  }

  WICKED_ALWAYS_INLINE float fetch_float() {
    float f = read_wire_float(pc);
    pc += WIRE_FLOAT_SIZE;
    return f;
  }

  WICKED_ALWAYS_INLINE void skip(uint16_t n) {
    pc += n;
  }

  WICKED_ALWAYS_INLINE void push(float v) {
    *sp++ = v;
  }

  WICKED_ALWAYS_INLINE float pop() {
    return *--sp;
  }

  WICKED_ALWAYS_INLINE std::span<float> popv(uint8_t n) {
    sp -= n;
    return std::span<float>(sp, n);
  }

  WICKED_ALWAYS_INLINE float& peek() {
    return sp[-1];
  }

  WICKED_ALWAYS_INLINE std::span<float> peekv(uint8_t n) {
    return std::span<float>(sp - n, n);
  }

  WICKED_ALWAYS_INLINE std::span<float> allocv(uint8_t n) {
    std::span<float> s(sp, n);
    sp += n;
    return s;
  }
};

WICKED_ALWAYS_INLINE static float ipow(float base, int exp) {
  if (exp == 0) return 1.0f;
  if (exp < 0) {
    base = 1.0f / base;
    exp = -exp;
  }
  float result = 1.0f;
  while (exp > 0) {
    if (exp & 1) {
      result *= base;
    }
    base *= base;
    exp >>= 1;
  }
  return result;
}

namespace ops {

WICKED_ALWAYS_INLINE void Push(EvalContext& ctx) {
  ctx.push(ctx.fetch_float());
}

WICKED_ALWAYS_INLINE void Dup(EvalContext& ctx) {
  ctx.push(ctx.peek());
}

WICKED_ALWAYS_INLINE void Over(EvalContext& ctx) {
  ctx.push(ctx.sp[-2]);
}

WICKED_ALWAYS_INLINE void Tuck(EvalContext& ctx) {
  float top = ctx.sp[-1];
  ctx.sp[-1] = ctx.sp[-2];
  ctx.sp[-2] = top;
  ctx.push(top);
}

WICKED_ALWAYS_INLINE void Drop(EvalContext& ctx) {
  --ctx.sp;
}

WICKED_ALWAYS_INLINE void Nip(EvalContext& ctx) {
  ctx.sp[-2] = ctx.sp[-1];
  --ctx.sp;
}

WICKED_ALWAYS_INLINE void Swap(EvalContext& ctx) {
  std::swap(ctx.sp[-1], ctx.sp[-2]);
}

WICKED_ALWAYS_INLINE void Rot(EvalContext& ctx) {
  float b = ctx.sp[-3];
  ctx.sp[-3] = ctx.sp[-2];
  ctx.sp[-2] = ctx.sp[-1];
  ctx.sp[-1] = b;
}

WICKED_ALWAYS_INLINE void Move(EvalContext& ctx) {
  uint8_t arg = ctx.fetch_byte();
  uint8_t offset = arg & 0x1F;
  uint8_t size = (arg >> 5) + 1;
  std::rotate(ctx.sp - offset - size, ctx.sp - offset, ctx.sp);
}

WICKED_ALWAYS_INLINE void Copy(EvalContext& ctx) {
  uint8_t arg = ctx.fetch_byte();
  uint8_t offset = arg & 0x1F;
  uint8_t size = (arg >> 5) + 1;
  std::copy_n(ctx.sp - offset - size, size, ctx.sp);
  ctx.sp += size;
}

WICKED_ALWAYS_INLINE void CopyBase(EvalContext& ctx) {
  uint8_t arg = ctx.fetch_byte();
  uint8_t index = arg & 0x1F;
  uint8_t size = (arg >> 5) + 1;
  std::copy_n(ctx.stack_base + index, size, ctx.sp);
  ctx.sp += size;
}

WICKED_ALWAYS_INLINE void Slide(EvalContext& ctx) {
  uint8_t arg = ctx.fetch_byte();
  uint8_t keep = (arg & 0x0F) + 1;
  uint8_t drop = (arg >> 4) + 1;
  std::memmove(ctx.sp - keep - drop, ctx.sp - keep, keep * sizeof(float));
  ctx.sp -= drop;
}

WICKED_ALWAYS_INLINE void Add(EvalContext& ctx) {
  float b = ctx.pop();
  ctx.peek() += b;
}

WICKED_ALWAYS_INLINE void Sub(EvalContext& ctx) {
  float b = ctx.pop();
  ctx.peek() -= b;
}

WICKED_ALWAYS_INLINE void Mul(EvalContext& ctx) {
  float b = ctx.pop();
  ctx.peek() *= b;
}

WICKED_ALWAYS_INLINE void Div(EvalContext& ctx) {
  float b = ctx.pop();
  ctx.peek() /= b;
}

WICKED_ALWAYS_INLINE void MulAdd(EvalContext& ctx) {
  float c = ctx.pop();
  float b = ctx.pop();
  float& top = ctx.peek();
  top = std::fma(top, b, c);
}

WICKED_ALWAYS_INLINE void MulSub(EvalContext& ctx) {
  float c = ctx.pop();
  float b = ctx.pop();
  float& top = ctx.peek();
  top = std::fma(top, b, -c);
}

WICKED_ALWAYS_INLINE void SubMul(EvalContext& ctx) {
  float b = ctx.pop();
  float a = ctx.pop();
  float& top = ctx.peek();
  top = std::fma(-a, b, top);
}

WICKED_ALWAYS_INLINE void Neg(EvalContext& ctx) {
  ctx.peek() = -ctx.peek();
}

WICKED_ALWAYS_INLINE void Abs(EvalContext& ctx) {
  ctx.peek() = std::abs(ctx.peek());
}

WICKED_ALWAYS_INLINE void Inv(EvalContext& ctx) {
  ctx.peek() = 1.0f / ctx.peek();
}

WICKED_ALWAYS_INLINE void Sqrt(EvalContext& ctx) {
  ctx.peek() = std::sqrt(ctx.peek());
}

WICKED_ALWAYS_INLINE void Sin(EvalContext& ctx) {
  ctx.peek() = wicked_sinf(ctx.peek());
}

WICKED_ALWAYS_INLINE void Cos(EvalContext& ctx) {
  ctx.peek() = wicked_cosf(ctx.peek());
}

// Derived via wicked_sincosf and wicked_atan2f to dead-strip libm's tanf/asinf/acosf
// and automatically leverage hardware CORDIC / DSP acceleration when available.
WICKED_ALWAYS_INLINE void Tan(EvalContext& ctx) {
  float s, c;
  wicked_sincosf(ctx.peek(), &s, &c);
  ctx.peek() = s / c;
}

WICKED_ALWAYS_INLINE void Asin(EvalContext& ctx) {
  float x = ctx.peek();
  ctx.peek() = wicked_atan2f(x, std::sqrt(1.0f - x * x));
}

WICKED_ALWAYS_INLINE void Acos(EvalContext& ctx) {
  float x = ctx.peek();
  ctx.peek() = wicked_atan2f(std::sqrt(1.0f - x * x), x);
}

WICKED_ALWAYS_INLINE void Atan2(EvalContext& ctx) {
  float y = ctx.pop();
  ctx.peek() = wicked_atan2f(y, ctx.peek());
}

WICKED_ALWAYS_INLINE void CosSin(EvalContext& ctx) {
  float s, c;
  wicked_sincosf(ctx.peek(), &s, &c);
  ctx.peek() = c;
  ctx.push(s);
}

WICKED_ALWAYS_INLINE void PanTilt(EvalContext& ctx) {
  float v2 = ctx.sp[-1];
  float v1 = ctx.sp[-2];
  float v0 = ctx.sp[-3];
  float r = std::sqrt(v0 * v0 + v1 * v1);
  ctx.sp[-3] = wicked_atan2f(v1, v0);
  ctx.sp[-2] = wicked_atan2f(v2, r);
  --ctx.sp;
}

WICKED_ALWAYS_INLINE void Ipow(EvalContext& ctx) {
  int8_t exp = static_cast<int8_t>(ctx.fetch_byte());
  ctx.peek() = ipow(ctx.peek(), exp);
}

WICKED_ALWAYS_INLINE void Pow(EvalContext& ctx) {
  float b = ctx.pop();
  ctx.peek() = wicked_powf(ctx.peek(), b);
}

WICKED_ALWAYS_INLINE void Exp(EvalContext& ctx) {
  ctx.peek() = wicked_expf(ctx.peek());
}

WICKED_ALWAYS_INLINE void Ln(EvalContext& ctx) {
  ctx.peek() = wicked_logf(ctx.peek());
}

WICKED_ALWAYS_INLINE void PushVec(EvalContext& ctx) {
  uint8_t size = ctx.fetch_byte() + 1;
  if constexpr (std::endian::native == std::endian::little) {
    size_t bytes = size * WIRE_FLOAT_SIZE;
    std::memcpy(ctx.sp, ctx.pc, bytes);
    ctx.sp += size;
    ctx.pc += bytes;
  } else {
    for (uint8_t i = 0; i < size; ++i) {
      *ctx.sp++ = ctx.fetch_float();
    }
  }
}

WICKED_ALWAYS_INLINE void AddVec(EvalContext& ctx) {
  uint8_t size = ctx.fetch_byte() + 1;
  std::span<float> rhs = ctx.popv(size);
  std::span<float> lhs = ctx.peekv(size);
  for (uint8_t i = 0; i < size; ++i) {
    lhs[i] += rhs[i];
  }
}

WICKED_ALWAYS_INLINE void SubVec(EvalContext& ctx) {
  uint8_t size = ctx.fetch_byte() + 1;
  std::span<float> rhs = ctx.popv(size);
  std::span<float> lhs = ctx.peekv(size);
  for (uint8_t i = 0; i < size; ++i) {
    lhs[i] -= rhs[i];
  }
}

WICKED_ALWAYS_INLINE void MulVec(EvalContext& ctx) {
  uint8_t size = ctx.fetch_byte() + 1;
  std::span<float> rhs = ctx.popv(size);
  std::span<float> lhs = ctx.peekv(size);
  for (uint8_t i = 0; i < size; ++i) {
    lhs[i] *= rhs[i];
  }
}

WICKED_ALWAYS_INLINE void DivVec(EvalContext& ctx) {
  uint8_t size = ctx.fetch_byte() + 1;
  std::span<float> rhs = ctx.popv(size);
  std::span<float> lhs = ctx.peekv(size);
  for (uint8_t i = 0; i < size; ++i) {
    lhs[i] /= rhs[i];
  }
}

WICKED_ALWAYS_INLINE void MulAddVec(EvalContext& ctx) {
  uint8_t size = ctx.fetch_byte() + 1;
  std::span<float> c = ctx.popv(size);
  std::span<float> b = ctx.popv(size);
  std::span<float> a = ctx.peekv(size);
  for (uint8_t i = 0; i < size; ++i) {
    a[i] = std::fma(a[i], b[i], c[i]);
  }
}

WICKED_ALWAYS_INLINE void MulSubVec(EvalContext& ctx) {
  uint8_t size = ctx.fetch_byte() + 1;
  std::span<float> c = ctx.popv(size);
  std::span<float> b = ctx.popv(size);
  std::span<float> a = ctx.peekv(size);
  for (uint8_t i = 0; i < size; ++i) {
    a[i] = std::fma(a[i], b[i], -c[i]);
  }
}

WICKED_ALWAYS_INLINE void SubMulVec(EvalContext& ctx) {
  uint8_t size = ctx.fetch_byte() + 1;
  std::span<float> b = ctx.popv(size);
  std::span<float> a = ctx.popv(size);
  std::span<float> c = ctx.peekv(size);
  for (uint8_t i = 0; i < size; ++i) {
    c[i] = std::fma(-a[i], b[i], c[i]);
  }
}

WICKED_ALWAYS_INLINE void NegVec(EvalContext& ctx) {
  uint8_t size = ctx.fetch_byte() + 1;
  std::span<float> v = ctx.peekv(size);
  for (uint8_t i = 0; i < size; ++i) {
    v[i] = -v[i];
  }
}

WICKED_ALWAYS_INLINE void AbsVec(EvalContext& ctx) {
  uint8_t size = ctx.fetch_byte() + 1;
  std::span<float> v = ctx.peekv(size);
  for (uint8_t i = 0; i < size; ++i) {
    v[i] = std::fabs(v[i]);
  }
}

WICKED_ALWAYS_INLINE void InvVec(EvalContext& ctx) {
  uint8_t size = ctx.fetch_byte() + 1;
  std::span<float> v = ctx.peekv(size);
  for (uint8_t i = 0; i < size; ++i) {
    v[i] = 1.0f / v[i];
  }
}

WICKED_ALWAYS_INLINE void DotVec(EvalContext& ctx) {
  uint8_t size = ctx.fetch_byte() + 1;
  std::span<float> rhs = ctx.popv(size);
  std::span<float> lhs = ctx.popv(size);
  const float* WICKED_RESTRICT p_lhs = lhs.data();
  const float* WICKED_RESTRICT p_rhs = rhs.data();
  float s0 = 0.0f;
  float s1 = 0.0f;
  while (size >= 2) {
    s0 = std::fma(p_lhs[0], p_rhs[0], s0);
    s1 = std::fma(p_lhs[1], p_rhs[1], s1);
    p_lhs += 2;
    p_rhs += 2;
    size -= 2;
  }
  if (size) {
    s0 = std::fma(p_lhs[0], p_rhs[0], s0);
  }
  ctx.push(s0 + s1);
}

WICKED_ALWAYS_INLINE void NormSqVec(EvalContext& ctx) {
  uint8_t size = ctx.fetch_byte() + 1;
  std::span<float> v = ctx.popv(size);
  const float* WICKED_RESTRICT p = v.data();
  float s0 = 0.0f;
  float s1 = 0.0f;
  while (size >= 2) {
    s0 = std::fma(p[0], p[0], s0);
    s1 = std::fma(p[1], p[1], s1);
    p += 2;
    size -= 2;
  }
  if (size) {
    s0 = std::fma(p[0], p[0], s0);
  }
  ctx.push(s0 + s1);
}

WICKED_ALWAYS_INLINE void SumVec(EvalContext& ctx) {
  uint8_t size = ctx.fetch_byte() + 1;
  std::span<float> v = ctx.popv(size);
  const float* WICKED_RESTRICT p = v.data();
  float s0 = 0.0f;
  float s1 = 0.0f;
  while (size >= 2) {
    s0 += p[0];
    s1 += p[1];
    p += 2;
    size -= 2;
  }
  if (size) {
    s0 += p[0];
  }
  ctx.push(s0 + s1);
}

WICKED_ALWAYS_INLINE void ScaleVec(EvalContext& ctx) {
  uint8_t size = ctx.fetch_byte() + 1;
  float scalar = ctx.pop();
  std::span<float> v = ctx.peekv(size);
  for (uint8_t i = 0; i < size; ++i) {
    v[i] *= scalar;
  }
}

WICKED_ALWAYS_INLINE void MulMat(EvalContext& ctx) {
  uint8_t arg = ctx.fetch_byte();
  uint8_t rows = (arg & 0x03) + 1;
  uint8_t cols = ((arg >> 4) & 0x0F) + 1;
  std::span<float> v = ctx.popv(rows);

  switch (rows) {
    case 1: {
      float v0 = v[0];
      std::span<float> c = ctx.allocv(cols);
      for (uint8_t j = 0; j < cols; ++j) {
        c[j] = v0 * ctx.fetch_float();
      }
      return;
    }
    case 2: {
      float v0 = v[0], v1 = v[1];
      std::span<float> c = ctx.allocv(cols);
      for (uint8_t j = 0; j < cols; ++j) {
        float m0 = ctx.fetch_float();
        float m1 = ctx.fetch_float();
        c[j] = std::fma(v0, m0, v1 * m1);
      }
      return;
    }
    case 3: {
      float v0 = v[0], v1 = v[1], v2 = v[2];
      std::span<float> c = ctx.allocv(cols);
      for (uint8_t j = 0; j < cols; ++j) {
        float m0 = ctx.fetch_float();
        float m1 = ctx.fetch_float();
        float m2 = ctx.fetch_float();
        c[j] = std::fma(v0, m0, std::fma(v1, m1, v2 * m2));
      }
      return;
    }
    case 4:
    default: {
      float v0 = v[0], v1 = v[1], v2 = v[2], v3 = v[3];
      std::span<float> c = ctx.allocv(cols);
      for (uint8_t j = 0; j < cols; ++j) {
        float m0 = ctx.fetch_float();
        float m1 = ctx.fetch_float();
        float m2 = ctx.fetch_float();
        float m3 = ctx.fetch_float();
        c[j] = std::fma(v0, m0, std::fma(v1, m1, std::fma(v2, m2, v3 * m3)));
      }
      return;
    }
  }
}

WICKED_ALWAYS_INLINE void Poly(EvalContext& ctx) {
  uint8_t arg = ctx.fetch_byte();
  uint8_t rows = (arg & 0x0F) + 1;
  uint8_t cols = ((arg >> 4) & 0x0F) + 1;
  float t = ctx.pop();
  std::span<float> result = ctx.allocv(cols);
  switch (cols) {
    case 1: {
      float r = ctx.fetch_float();
      for (uint8_t i = 1; i < rows; ++i) {
        r = std::fma(r, t, ctx.fetch_float());
      }
      result[0] = r;
      return;
    }
    case 2: {
      float r0 = ctx.fetch_float();
      float r1 = ctx.fetch_float();
      for (uint8_t i = 1; i < rows; ++i) {
        r0 = std::fma(r0, t, ctx.fetch_float());
        r1 = std::fma(r1, t, ctx.fetch_float());
      }
      result[0] = r0;
      result[1] = r1;
      return;
    }
    case 3: {
      float r0 = ctx.fetch_float();
      float r1 = ctx.fetch_float();
      float r2 = ctx.fetch_float();
      for (uint8_t i = 1; i < rows; ++i) {
        r0 = std::fma(r0, t, ctx.fetch_float());
        r1 = std::fma(r1, t, ctx.fetch_float());
        r2 = std::fma(r2, t, ctx.fetch_float());
      }
      result[0] = r0;
      result[1] = r1;
      result[2] = r2;
      return;
    }
    default: {
      for (uint8_t j = 0; j < cols; ++j) {
        result[j] = ctx.fetch_float();
      }
      for (uint8_t i = 1; i < rows; ++i) {
        for (uint8_t j = 0; j < cols; ++j) {
          result[j] = std::fma(result[j], t, ctx.fetch_float());
        }
      }
      return;
    }
  }
}

WICKED_ALWAYS_INLINE void Call(EvalContext& ctx) {
  uint16_t target_offset = ctx.fetch_uint16();
  ctx.sp = WickedPostfixEvaluateInternal(ctx.root_prog, ctx.root_prog + target_offset, ctx.sp, ctx.stack_base);
}

WICKED_ALWAYS_INLINE void Piecewise(EvalContext& ctx) {
  float x = ctx.peek();
  uint8_t count_byte = ctx.fetch_byte();
  bool is_relative = (count_byte & 0x80) != 0;
  uint8_t num_knots = count_byte & 0x7F;

  uint8_t low = 0;
  uint8_t high = num_knots;
  while (low < high) {
    uint8_t mid = low + (high - low) / 2;
    float k = read_wire_float(ctx.pc + mid * WIRE_FLOAT_SIZE);
    if (x >= k) {
      low = mid + 1;
    } else {
      high = mid;
    }
  }
  uint8_t branch_idx = low;

  if (is_relative) {
    float x_ref = (branch_idx == 0)
        ? read_wire_float(ctx.pc)
        : read_wire_float(ctx.pc + (branch_idx - 1) * WIRE_FLOAT_SIZE);
    ctx.peek() = x - x_ref;
  }

  const uint8_t* offsets_ptr = ctx.pc + num_knots * WIRE_FLOAT_SIZE;
  uint16_t target_offset = read_wire_uint16(offsets_ptr + branch_idx * WIRE_UINT16_SIZE);

  ctx.pc = offsets_ptr + (num_knots + 1) * WIRE_UINT16_SIZE;
  ctx.sp = WickedPostfixEvaluateInternal(ctx.root_prog, ctx.root_prog + target_offset, ctx.sp, ctx.stack_base);
}

}  // namespace ops
}  // namespace

// Evaluates bytecode starting at entry_pc within root_prog, using initial stack pointer sp
// and stack boundary stack_base. Execution continues in-place without bounds checking until
// WickedPostfixOp_Ret is reached. Returns the updated stack pointer upon return.
extern "C" float* WickedPostfixEvaluateInternal(const uint8_t* root_prog, const uint8_t* entry_pc, float* sp, float* stack_base) {
  EvalContext ctx{
    .pc = entry_pc,
    .sp = sp,
    .stack_base = stack_base,
    .root_prog = root_prog,
  };

  while (true) {
    const uint8_t op = ctx.fetch_byte();
    switch (op) {
      case WickedPostfixOp_Ret:
        return ctx.sp;
      case WickedPostfixOp_Call:      ops::Call(ctx); break;
      case WickedPostfixOp_Piecewise: ops::Piecewise(ctx); break;
      case WickedPostfixOp_Push:      ops::Push(ctx); break;
      case WickedPostfixOp_Dup:       ops::Dup(ctx); break;
      case WickedPostfixOp_Over:      ops::Over(ctx); break;
      case WickedPostfixOp_Tuck:      ops::Tuck(ctx); break;
      case WickedPostfixOp_Drop:      ops::Drop(ctx); break;
      case WickedPostfixOp_Nip:       ops::Nip(ctx); break;
      case WickedPostfixOp_Swap:      ops::Swap(ctx); break;
      case WickedPostfixOp_Rot:       ops::Rot(ctx); break;
      case WickedPostfixOp_Move:      ops::Move(ctx); break;
      case WickedPostfixOp_Copy:      ops::Copy(ctx); break;
      case WickedPostfixOp_CopyBase:  ops::CopyBase(ctx); break;
      case WickedPostfixOp_Slide:     ops::Slide(ctx); break;
      case WickedPostfixOp_Add:       ops::Add(ctx); break;
      case WickedPostfixOp_Sub:       ops::Sub(ctx); break;
      case WickedPostfixOp_Mul:       ops::Mul(ctx); break;
      case WickedPostfixOp_Div:       ops::Div(ctx); break;
      case WickedPostfixOp_MulAdd:    ops::MulAdd(ctx); break;
      case WickedPostfixOp_MulSub:    ops::MulSub(ctx); break;
      case WickedPostfixOp_SubMul:    ops::SubMul(ctx); break;
      case WickedPostfixOp_Neg:       ops::Neg(ctx); break;
      case WickedPostfixOp_Abs:       ops::Abs(ctx); break;
      case WickedPostfixOp_Inv:       ops::Inv(ctx); break;
      case WickedPostfixOp_Sqrt:      ops::Sqrt(ctx); break;
      case WickedPostfixOp_Sin:       ops::Sin(ctx); break;
      case WickedPostfixOp_Cos:       ops::Cos(ctx); break;
      case WickedPostfixOp_Tan:       ops::Tan(ctx); break;
      case WickedPostfixOp_Asin:      ops::Asin(ctx); break;
      case WickedPostfixOp_Acos:      ops::Acos(ctx); break;
      case WickedPostfixOp_Atan2:     ops::Atan2(ctx); break;
      case WickedPostfixOp_CosSin:    ops::CosSin(ctx); break;
      case WickedPostfixOp_PanTilt:   ops::PanTilt(ctx); break;
      case WickedPostfixOp_Ipow:      ops::Ipow(ctx); break;
      case WickedPostfixOp_Pow:       ops::Pow(ctx); break;
      case WickedPostfixOp_Exp:       ops::Exp(ctx); break;
      case WickedPostfixOp_Ln:        ops::Ln(ctx); break;
      case WickedPostfixOp_PushVec:   ops::PushVec(ctx); break;
      case WickedPostfixOp_AddVec:    ops::AddVec(ctx); break;
      case WickedPostfixOp_SubVec:    ops::SubVec(ctx); break;
      case WickedPostfixOp_MulVec:    ops::MulVec(ctx); break;
      case WickedPostfixOp_DivVec:    ops::DivVec(ctx); break;
      case WickedPostfixOp_MulAddVec: ops::MulAddVec(ctx); break;
      case WickedPostfixOp_MulSubVec: ops::MulSubVec(ctx); break;
      case WickedPostfixOp_SubMulVec: ops::SubMulVec(ctx); break;
      case WickedPostfixOp_NegVec:    ops::NegVec(ctx); break;
      case WickedPostfixOp_AbsVec:    ops::AbsVec(ctx); break;
      case WickedPostfixOp_InvVec:    ops::InvVec(ctx); break;
      case WickedPostfixOp_DotVec:    ops::DotVec(ctx); break;
      case WickedPostfixOp_NormSqVec: ops::NormSqVec(ctx); break;
      case WickedPostfixOp_SumVec:    ops::SumVec(ctx); break;
      case WickedPostfixOp_ScaleVec:  ops::ScaleVec(ctx); break;
      case WickedPostfixOp_MulMat:    ops::MulMat(ctx); break;
      case WickedPostfixOp_Poly:      ops::Poly(ctx); break;
      default:
        WICKED_CUSTOM_TRAP();
        return ctx.sp;
    }
  }
}
