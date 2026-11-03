#pragma once

#include "WickedEvalStatus.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
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
  WickedPostfixOp_Call      = 253,
  WickedPostfixOp_Piecewise = 254,
  WickedPostfixOp_Ret       = 255,
} WickedPostfixOp_t;

// Returns the string representation of an opcode.
const char* WickedPostfixOpToString(WickedPostfixOp_t op);

// Validates bytecode stack requirements within buffer_size and returns status, minimum stack capacity, and final stack size.
// The program must terminate with WickedPostfixOp_Ret before the buffer boundary is exceeded.
WickedEvalStatus_t WickedPostfixValidate(const uint8_t* prog_data, size_t buffer_size, uint16_t initial_stack_size, uint16_t* minimum_stack_capacity, uint16_t* final_stack_size);

void WickedPostfixEvaluateInternal(const uint8_t* root_prog, const uint8_t* entry_pc, float*& sp, float* base);

// Evaluates bytecode on stack_data in-place without bounds checking until WickedPostfixOp_Ret is reached.
static inline void WickedPostfixEvaluate(const uint8_t* prog_data, uint16_t initial_stack_size, float* stack_data) {
  float* sp = stack_data + initial_stack_size;
  WickedPostfixEvaluateInternal(prog_data, prog_data, sp, stack_data);
}

#ifdef __cplusplus
}
#endif
