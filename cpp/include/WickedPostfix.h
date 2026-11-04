#pragma once

#include "WickedEvalStatus.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// Macro for trapping on invalid bytecode execution (e.g. unmapped opcodes in unvalidated programs).
// Users can override this by defining WICKED_CUSTOM_TRAP() before including this header.
#ifndef WICKED_CUSTOM_TRAP
#if defined(__GNUC__) || defined(__clang__)
#define WICKED_CUSTOM_TRAP() __builtin_trap()
#elif defined(_MSC_VER)
#define WICKED_CUSTOM_TRAP() __debugbreak()
#else
#include <stdlib.h>
#define WICKED_CUSTOM_TRAP() abort()
#endif
#endif

// Bytecode operations for the postfix evaluation machine.
typedef enum WickedPostfixOp {
  WickedPostfixOp_Undefined = 0,
  WickedPostfixOp_Ret       = 1,
  WickedPostfixOp_Call      = 2,
  WickedPostfixOp_Piecewise = 3,
  WickedPostfixOp_Push      = 4,
  WickedPostfixOp_Dup       = 5,
  WickedPostfixOp_Over      = 6,
  WickedPostfixOp_Tuck      = 7,
  WickedPostfixOp_Drop      = 8,
  WickedPostfixOp_Nip       = 9,
  WickedPostfixOp_Swap      = 10,
  WickedPostfixOp_Rot       = 11,
  WickedPostfixOp_Move      = 12,
  WickedPostfixOp_Copy      = 13,
  WickedPostfixOp_CopyBase  = 14,
  WickedPostfixOp_Slide     = 15,
  WickedPostfixOp_Add       = 16,
  WickedPostfixOp_Sub       = 17,
  WickedPostfixOp_Mul       = 18,
  WickedPostfixOp_Div       = 19,
  WickedPostfixOp_MulAdd    = 20,
  WickedPostfixOp_MulSub    = 21,
  WickedPostfixOp_SubMul    = 22,
  WickedPostfixOp_Neg       = 23,
  WickedPostfixOp_Abs       = 24,
  WickedPostfixOp_Inv       = 25,
  WickedPostfixOp_Sqrt      = 26,
  WickedPostfixOp_Sin       = 27,
  WickedPostfixOp_Cos       = 28,
  WickedPostfixOp_Tan       = 29,
  WickedPostfixOp_Asin      = 30,
  WickedPostfixOp_Acos      = 31,
  WickedPostfixOp_Atan2     = 32,
  WickedPostfixOp_CosSin    = 33,
  WickedPostfixOp_PanTilt   = 34,
  WickedPostfixOp_Ipow      = 35,
  WickedPostfixOp_Pow       = 36,
  WickedPostfixOp_Exp       = 37,
  WickedPostfixOp_Ln        = 38,
  WickedPostfixOp_PushVec   = 39,
  WickedPostfixOp_AddVec    = 40,
  WickedPostfixOp_SubVec    = 41,
  WickedPostfixOp_MulVec    = 42,
  WickedPostfixOp_DivVec    = 43,
  WickedPostfixOp_MulAddVec = 44,
  WickedPostfixOp_MulSubVec = 45,
  WickedPostfixOp_SubMulVec = 46,
  WickedPostfixOp_NegVec    = 47,
  WickedPostfixOp_AbsVec    = 48,
  WickedPostfixOp_InvVec    = 49,
  WickedPostfixOp_DotVec    = 50,
  WickedPostfixOp_NormSqVec = 51,
  WickedPostfixOp_SumVec    = 52,
  WickedPostfixOp_ScaleVec  = 53,
  WickedPostfixOp_MulMat    = 54,
  WickedPostfixOp_Poly      = 55,
} WickedPostfixOp_t;

// Returns the string representation of an opcode.
const char* WickedPostfixOpToString(WickedPostfixOp_t op);

// Validates bytecode stack requirements within buffer_size and returns status, minimum stack capacity, and final stack size.
// The program must terminate with WickedPostfixOp_Ret before the buffer boundary is exceeded.
WickedEvalStatus_t WickedPostfixValidate(const uint8_t* prog_data, size_t buffer_size, uint16_t initial_stack_size, uint16_t* minimum_stack_capacity, uint16_t* final_stack_size);

// Evaluates bytecode starting at entry_pc within root_prog, using initial stack pointer sp
// and stack boundary stack_base. Execution continues in-place without bounds checking until
// WickedPostfixOp_Ret is reached. Returns the updated stack pointer upon return.
float* WickedPostfixEvaluateInternal(const uint8_t* root_prog, const uint8_t* entry_pc, float* sp, float* stack_base);

// Evaluates bytecode on stack_base in-place without bounds checking until WickedPostfixOp_Ret is reached.
// Returns the updated stack pointer upon return.
static inline float* WickedPostfixEvaluate(const uint8_t* prog_data, uint16_t initial_stack_size, float* stack_base) {
  float* sp = stack_base + initial_stack_size;
  return WickedPostfixEvaluateInternal(prog_data, prog_data, sp, stack_base);
}

#ifdef __cplusplus
}
#endif
