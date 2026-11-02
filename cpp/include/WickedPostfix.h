#pragma once

#include "WickedEvalStatus.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// Postfix engine math optimizations:
// - WICKED_USE_FAST_EXP_MATH: enables compact IEEE-754 bit-trick and Horner polynomial
//   approximations for Exp, Ln, and Pow, eliminating Newlib libm exponential dependencies.
// - Custom overrides: WICKED_CUSTOM_EXP(x), WICKED_CUSTOM_LN(x), WICKED_CUSTOM_POW(x, y)
//   allow binding custom vendor DSP or hardware math accelerators.
// - Note: Pow is derived from Exp and Ln (x^y = exp(y * ln(x))),
//   dead-stripping Newlib's heavy powf from libm by default.

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
  WickedPostfixOp_MulAdd    = 17,
  WickedPostfixOp_MulSub    = 18,
  WickedPostfixOp_SubMul    = 19,
  WickedPostfixOp_Neg       = 20,
  WickedPostfixOp_Abs       = 21,
  WickedPostfixOp_Inv       = 22,
  WickedPostfixOp_Sqrt      = 23,
  WickedPostfixOp_Sin       = 24,
  WickedPostfixOp_Cos       = 25,
  WickedPostfixOp_Tan       = 26,
  WickedPostfixOp_Asin      = 27,
  WickedPostfixOp_Acos      = 28,
  WickedPostfixOp_Atan2     = 29,
  WickedPostfixOp_CosSin    = 30,
  WickedPostfixOp_PanTilt   = 31,
  WickedPostfixOp_Ipow      = 32,
  WickedPostfixOp_Pow       = 33,
  WickedPostfixOp_Exp       = 34,
  WickedPostfixOp_Ln        = 35,
  WickedPostfixOp_PushVec   = 36,
  WickedPostfixOp_AddVec    = 37,
  WickedPostfixOp_SubVec    = 38,
  WickedPostfixOp_MulVec    = 39,
  WickedPostfixOp_DivVec    = 40,
  WickedPostfixOp_MulAddVec = 41,
  WickedPostfixOp_MulSubVec = 42,
  WickedPostfixOp_SubMulVec = 43,
  WickedPostfixOp_NegVec    = 44,
  WickedPostfixOp_AbsVec    = 45,
  WickedPostfixOp_InvVec    = 46,
  WickedPostfixOp_DotVec    = 47,
  WickedPostfixOp_NormSqVec = 48,
  WickedPostfixOp_SumVec    = 49,
  WickedPostfixOp_ScaleVec  = 50,
  WickedPostfixOp_MulMat    = 51,
  WickedPostfixOp_Poly      = 52,
} WickedPostfixOp_t;

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

#include <ostream>

// Formats opcode name to output stream.
inline std::ostream& operator<<(std::ostream& out, WickedPostfixOp_t op) {
  return out << WickedPostfixOpToString(op);
}

#endif
