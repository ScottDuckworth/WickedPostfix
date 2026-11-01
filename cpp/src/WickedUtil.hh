#pragma once

#include <cmath>

#if defined(WICKED_USE_STM32_CORDIC)
#if defined(CORDIC)
// CORDIC peripheral already defined by previously included device header.
#elif defined(WICKED_STM32_HEADER)
#include WICKED_STM32_HEADER
#elif defined(__has_include)
#if __has_include("stm32g4xx.h")
#include "stm32g4xx.h"
#elif __has_include("stm32h5xx.h")
#include "stm32h5xx.h"
#elif __has_include("stm32u5xx.h")
#include "stm32u5xx.h"
#elif __has_include("stm32h7rsxx.h")
#include "stm32h7rsxx.h"
#else
#include "stm32g4xx.h"
#endif
#elif defined(STM32H5) || defined(STM32H5xx)
#include "stm32h5xx.h"
#elif defined(STM32U5) || defined(STM32U5xx)
#include "stm32u5xx.h"
#elif defined(STM32H7RS) || defined(STM32H7RSxx)
#include "stm32h7rsxx.h"
#else
#include "stm32g4xx.h"
#endif

// Hardware CORDIC coprocessor driver for STM32 (STM32G4, STM32H5, STM32U5, STM32H7R/S).
// Computes sine and cosine in hardware with 0 bytes of lookup tables.
static inline void wicked_cordic_sincosf(float t, float* s, float* c) {
  // Normalize t in radians to [-1.0f, 1.0f) representing [-pi, pi]
  float turns = t * 0.15915494309189535f;  // t / (2 * pi)
  turns -= static_cast<int32_t>(turns);
  float scaled = turns * 2.0f;
  if (scaled >= 1.0f) {
    scaled -= 2.0f;
  } else if (scaled < -1.0f) {
    scaled += 2.0f;
  }
  int32_t q31 = static_cast<int32_t>(scaled * 2147483648.0f);

  // Cosine with NRES=1 (2 results: Cosine then Sine in Q1.31)
  // Precision 6 cycles (24 iterations) for single-precision mantissa accuracy
  CORDIC->CSR = CORDIC_CSR_NRES | (6U << CORDIC_CSR_PRECISION_Pos);
  CORDIC->WDATA = static_cast<uint32_t>(q31);

  constexpr float kQ31ToFloat = 1.0f / 2147483648.0f;
  *c = static_cast<int32_t>(CORDIC->RDATA) * kQ31ToFloat;
  *s = static_cast<int32_t>(CORDIC->RDATA) * kQ31ToFloat;
}

static inline float wicked_cordic_sinf(float t) {
  float s, c;
  wicked_cordic_sincosf(t, &s, &c);
  return s;
}

static inline float wicked_cordic_cosf(float t) {
  float s, c;
  wicked_cordic_sincosf(t, &s, &c);
  return c;
}

static inline float wicked_cordic_atan2f(float y, float x) {
  float ax = std::abs(x);
  float ay = std::abs(y);
  float max_val = ax > ay ? ax : ay;
  if (max_val == 0.0f) {
    return 0.0f;
  }
  // Scale to [-0.5, 0.5] to strictly avoid Q1.31 overflow at +1.0
  float scale = 1073741824.0f / max_val;
  int32_t q31_x = static_cast<int32_t>(x * scale);
  int32_t q31_y = static_cast<int32_t>(y * scale);

  // Phase function (atan2): ARG1 = x, ARG2 = y. NARGS = 1 (2 arguments), NRES = 0 (1 result)
  CORDIC->CSR = CORDIC_CSR_FUNC_1 | CORDIC_CSR_NARGS | (6U << CORDIC_CSR_PRECISION_Pos);
  CORDIC->WDATA = static_cast<uint32_t>(q31_x);
  CORDIC->WDATA = static_cast<uint32_t>(q31_y);

  constexpr float kQ31ToPi = 3.14159265358979323846f / 2147483648.0f;
  return static_cast<int32_t>(CORDIC->RDATA) * kQ31ToPi;
}

#ifndef WICKED_CUSTOM_SIN
#define WICKED_CUSTOM_SIN(t) wicked_cordic_sinf(t)
#endif
#ifndef WICKED_CUSTOM_COS
#define WICKED_CUSTOM_COS(t) wicked_cordic_cosf(t)
#endif
#ifndef WICKED_CUSTOM_SINCOS
#define WICKED_CUSTOM_SINCOS(t, s, c) wicked_cordic_sincosf((t), (s), (c))
#endif
#ifndef WICKED_CUSTOM_ATAN2
#define WICKED_CUSTOM_ATAN2(y, x) wicked_cordic_atan2f((y), (x))
#endif
#elif defined(WICKED_USE_CMSIS_DSP)
#include "arm_math.h"
#ifndef WICKED_CUSTOM_SIN
#define WICKED_CUSTOM_SIN(t) arm_sin_f32(t)
#endif
#ifndef WICKED_CUSTOM_COS
#define WICKED_CUSTOM_COS(t) arm_cos_f32(t)
#endif
#ifndef WICKED_CUSTOM_SINCOS
#define WICKED_CUSTOM_SINCOS(t, s, c) arm_sin_cos_f32((t), (s), (c))
#endif
#endif

#ifndef WICKED_CUSTOM_SIN
#define WICKED_CUSTOM_SIN(t) std::sin(t)
#endif

#ifndef WICKED_CUSTOM_COS
#define WICKED_CUSTOM_COS(t) std::cos(t)
#endif

#ifndef WICKED_CUSTOM_SINCOS
#if defined(__GNUC__) || defined(__clang__)
#define WICKED_CUSTOM_SINCOS(t, s, c) __builtin_sincosf((t), (s), (c))
#else
#define WICKED_CUSTOM_SINCOS(t, s, c) \
  do {                                \
    *(s) = std::sin(t);               \
    *(c) = std::cos(t);               \
  } while (0)
#endif
#endif

#ifndef WICKED_CUSTOM_ATAN2
#define WICKED_CUSTOM_ATAN2(y, x) std::atan2((y), (x))
#endif

// Computes sine.
static inline float wicked_sinf(float t) {
  return WICKED_CUSTOM_SIN(t);
}

// Computes cosine.
static inline float wicked_cosf(float t) {
  return WICKED_CUSTOM_COS(t);
}

// Computes sine and cosine simultaneously.
static inline void wicked_sincosf(float t, float* s, float* c) {
  WICKED_CUSTOM_SINCOS(t, s, c);
}

// Computes 2-argument arctangent atan2(y, x).
static inline float wicked_atan2f(float y, float x) {
  return WICKED_CUSTOM_ATAN2(y, x);
}

// Return the smallest index i in [first, last) at which pred(i) is true.
// Returns last if not found.
template <typename Idx, typename Pred>
Idx wicked_upper_bound(Idx first, Idx last, const Pred& pred) {
  Idx i = first;
  Idx n = last - first;
  while (n > 0) {
    Idx h = n / 2;
    if (pred(i + h)) {
      n = h;
    } else {
      i += h + 1;
      n -= h + 1;
    }
  }
  return i;
}
