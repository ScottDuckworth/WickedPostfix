#pragma once

#include <cmath>

#if defined(WICKED_USE_STM32_CORDIC)
#include "stm32g4xx.h"

// Hardware CORDIC coprocessor driver for STM32G4.
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

#ifndef WICKED_CUSTOM_SIN
#define WICKED_CUSTOM_SIN(t) wicked_cordic_sinf(t)
#endif
#ifndef WICKED_CUSTOM_COS
#define WICKED_CUSTOM_COS(t) wicked_cordic_cosf(t)
#endif
#ifndef WICKED_CUSTOM_SINCOS
#define WICKED_CUSTOM_SINCOS(t, s, c) wicked_cordic_sincosf((t), (s), (c))
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
