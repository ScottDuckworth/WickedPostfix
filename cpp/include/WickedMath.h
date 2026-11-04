#ifndef WICKED_MATH_H_
#define WICKED_MATH_H_

#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// Hardware CORDIC coprocessor driver routines for STM32 (STM32G4, STM32H5, STM32U5, STM32H7R/S)
#if defined(WICKED_USE_STM32_CORDIC)
void wicked_cordic_sincosf(float t, float* s, float* c);
float wicked_cordic_atan2f(float y, float x);

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
#endif

// --- TRIGONOMETRIC DISPATCH CONFIGURATION ---

#if defined(WICKED_USE_STM32_CORDIC)
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
#define WICKED_CUSTOM_SIN(t) sinf(t)
#endif

#ifndef WICKED_CUSTOM_COS
#define WICKED_CUSTOM_COS(t) cosf(t)
#endif

#ifndef WICKED_CUSTOM_SINCOS
#if defined(__GNUC__) || defined(__clang__)
#define WICKED_CUSTOM_SINCOS(t, s, c) __builtin_sincosf((t), (s), (c))
#else
#define WICKED_CUSTOM_SINCOS(t, s, c) \
  do {                                \
    *(s) = sinf(t);                   \
    *(c) = cosf(t);                   \
  } while (0)
#endif
#endif

#ifndef WICKED_CUSTOM_ATAN2
#define WICKED_CUSTOM_ATAN2(y, x) atan2f((y), (x))
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

// --- EXPONENTIAL / POWER MACRO DISPATCH ---

// Fast exponential math routines (< 5e-6 relative error, tableless)
float wicked_fast_expf(float x);
float wicked_fast_logf(float x);

// Derived powf: x^y = exp(y * ln(x))
// Dead-strips Newlib's heavy powf from libm while dispatching through wicked_expf / wicked_logf.
float wicked_derived_powf(float x, float y);

#if defined(WICKED_CUSTOM_EXP)
#define wicked_expf(x) WICKED_CUSTOM_EXP(x)
#elif defined(WICKED_USE_FAST_EXP_MATH)
#define wicked_expf(x) wicked_fast_expf(x)
#else
#define wicked_expf(x) expf(x)
#endif

#if defined(WICKED_CUSTOM_LN)
#define wicked_logf(x) WICKED_CUSTOM_LN(x)
#elif defined(WICKED_USE_FAST_EXP_MATH)
#define wicked_logf(x) wicked_fast_logf(x)
#else
#define wicked_logf(x) logf(x)
#endif

#if defined(WICKED_CUSTOM_POW)
#define wicked_powf(x, y) WICKED_CUSTOM_POW(x, y)
#else
#define wicked_powf(x, y) wicked_derived_powf(x, y)
#endif

#ifdef __cplusplus
}
#endif

#endif  // WICKED_MATH_H_

