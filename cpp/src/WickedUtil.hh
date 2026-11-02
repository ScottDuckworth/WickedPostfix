#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

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

// --- FAST EXPONENTIAL MATH IMPLEMENTATIONS ---

// Tableless IEEE-754 bit-trick + Horner polynomial approximation of expf(x).
// Precision: < 5e-6 relative error across the entire float range.
static inline float wicked_fast_expf(float x) {
  if (std::isnan(x)) return x;
  if (x > 88.722839f) return std::numeric_limits<float>::infinity();
  if (x < -87.33654f) return 0.0f;

  // 2^(x * log2(e)) = 2^(k + f)
  constexpr float LOG2E = 1.4426950408889634f;
  float z = x * LOG2E;
  int32_t k = static_cast<int32_t>(std::floor(z + 0.5f));
  float f = z - static_cast<float>(k);

  // Polynomial approximation of 2^f for f in [-0.5, 0.5]
  // 2^f ≈ 1 + a1*f + a2*f^2 + a3*f^3 + a4*f^4 + a5*f^5
  constexpr float a1 = 0.69314718f;
  constexpr float a2 = 0.24022651f;
  constexpr float a3 = 0.05550411f;
  constexpr float a4 = 0.00961813f;
  constexpr float a5 = 0.00133336f;
  float poly = 1.0f + f * (a1 + f * (a2 + f * (a3 + f * (a4 + f * a5))));

  // Synthesize 2^k via IEEE-754 exponent field
  uint32_t k_bits = static_cast<uint32_t>(k + 127) << 23;
  float p2k;
  std::memcpy(&p2k, &k_bits, sizeof(float));
  return p2k * poly;
}

// Tableless IEEE-754 bit-trick + Horner polynomial approximation of logf(x).
// Precision: < 3e-6 relative error across the entire positive float range.
static inline float wicked_fast_logf(float x) {
  if (x <= 0.0f) {
    if (x == 0.0f) return -std::numeric_limits<float>::infinity();
    return std::numeric_limits<float>::quiet_NaN();
  }

  int32_t exp = 0;
  // Handle subnormal floats
  if (x < 1.17549435e-38f) {
    x *= 16777216.0f;  // 2^24
    exp -= 24;
  }

  uint32_t bits;
  std::memcpy(&bits, &x, sizeof(float));
  exp += static_cast<int32_t>(bits >> 23) - 127;
  bits = (bits & 0x007FFFFFu) | 0x3F800000u;
  float m;
  std::memcpy(&m, &bits, sizeof(float));

  // Range reduce m from [1.0, 2.0) to [1/sqrt(2), sqrt(2)) ≈ [0.7071, 1.4142]
  if (m > 1.41421356f) {
    m *= 0.5f;
    exp += 1;
  }

  // u = (m - 1) / (m + 1), |u| <= 0.17157
  float u = (m - 1.0f) / (m + 1.0f);
  float u2 = u * u;
  // ln(m) = 2 * u * (1 + u^2/3 + u^4/5 + u^6/7)
  float log_m = 2.0f * u * (1.0f + u2 * (0.333333333f + u2 * (0.2f + u2 * 0.142857143f)));

  constexpr float LN2 = 0.6931471805599453f;
  return log_m + static_cast<float>(exp) * LN2;
}

// Tableless fast powf(x, y) = fast_exp(y * fast_log(x)) with full edge cases.
static inline float wicked_fast_powf(float x, float y) {
  if (y == 0.0f) {
    return 1.0f;
  }
  if (x == 0.0f) {
    return (y > 0.0f) ? 0.0f : std::numeric_limits<float>::infinity();
  }
  if (x < 0.0f) {
    if (std::floor(y) == y) {
      bool is_odd = (std::abs(y) < 16777216.0f) && (static_cast<int32_t>(y) & 1);
      float res = wicked_fast_expf(y * wicked_fast_logf(-x));
      return is_odd ? -res : res;
    }
    return std::numeric_limits<float>::quiet_NaN();
  }
  return wicked_fast_expf(y * wicked_fast_logf(x));
}

// --- EXPONENTIAL / POWER MACRO DISPATCH ---

#if defined(WICKED_CUSTOM_EXP)
#define wicked_expf(x) WICKED_CUSTOM_EXP(x)
#elif defined(WICKED_USE_FAST_EXP_MATH)
#define wicked_expf(x) wicked_fast_expf(x)
#else
#define wicked_expf(x) std::exp(x)
#endif

#if defined(WICKED_CUSTOM_LN)
#define wicked_logf(x) WICKED_CUSTOM_LN(x)
#elif defined(WICKED_USE_FAST_EXP_MATH)
#define wicked_logf(x) wicked_fast_logf(x)
#else
#define wicked_logf(x) std::log(x)
#endif

// Derived powf: x^y = exp(y * ln(x))
// Dead-strips Newlib's heavy powf from libm while dispatching through wicked_expf / wicked_logf.
static inline float wicked_derived_powf(float x, float y) {
  if (y == 0.0f) {
    return 1.0f;
  }
  if (x == 0.0f) {
    return (y > 0.0f) ? 0.0f : std::numeric_limits<float>::infinity();
  }
  if (x < 0.0f) {
    if (std::floor(y) == y) {
      bool is_odd = (std::abs(y) < 16777216.0f) && (static_cast<int32_t>(y) & 1);
      float res = wicked_expf(y * wicked_logf(-x));
      return is_odd ? -res : res;
    }
    return std::numeric_limits<float>::quiet_NaN();
  }
  return wicked_expf(y * wicked_logf(x));
}

#if defined(WICKED_CUSTOM_POW)
#define wicked_powf(x, y) WICKED_CUSTOM_POW(x, y)
#elif defined(WICKED_USE_FAST_EXP_MATH)
#define wicked_powf(x, y) wicked_fast_powf(x, y)
#else
#define wicked_powf(x, y) wicked_derived_powf(x, y)
#endif

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
