#pragma once

#include "WickedEvalStatus.h"
#include "WickedPostfix.h"

#include <stdbool.h>
#include <stdint.h>

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
typedef struct WICKED_PACKED WickedPathHeader {
  uint16_t segment_size;
  uint16_t flags;
} WickedPathHeader_t;

typedef struct WICKED_PACKED WickedPathSegmentHeader {
  uint32_t start_time;
  uint16_t offset;
  uint16_t size;
} WickedPathSegmentHeader_t;
#pragma pack(pop)

#if defined(__cplusplus)
static_assert(sizeof(WickedPathHeader_t) == 4, "WickedPathHeader_t must be 4 bytes");
static_assert(alignof(WickedPathHeader_t) == 1, "WickedPathHeader_t must be packed");
static_assert(sizeof(WickedPathSegmentHeader_t) == 8, "WickedPathSegmentHeader_t must be 8 bytes");
static_assert(alignof(WickedPathSegmentHeader_t) == 1, "WickedPathSegmentHeader_t must be packed");
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(WickedPathHeader_t) == 4, "WickedPathHeader_t must be 4 bytes");
_Static_assert(_Alignof(WickedPathHeader_t) == 1, "WickedPathHeader_t must be packed");
_Static_assert(sizeof(WickedPathSegmentHeader_t) == 8, "WickedPathSegmentHeader_t must be 8 bytes");
_Static_assert(_Alignof(WickedPathSegmentHeader_t) == 1, "WickedPathSegmentHeader_t must be packed");
#endif

typedef struct WickedPathSegmentDescriptor {
  uint32_t start_time;
  const uint8_t* prog_data;
} WickedPathSegmentDescriptor_t;

WickedEvalStatus_t WickedPathValidate(const uint8_t* path_data, size_t path_size, uint16_t stack_capacity);
bool WickedPathSegmentAt(const uint8_t* path_data, uint32_t t, WickedPathSegmentDescriptor_t* descriptor);
WickedEvalStatus_t WickedPathEvaluate(const uint8_t* path_data, uint32_t t, float* stack_data);

#ifdef __cplusplus
}

#include "WickedPostfixWriter.h"
#include <span>
#include <vector>

struct WickedPathSegmentWriter {
  uint32_t start_time;
  WickedPostfixWriter expr;
};

class WickedPathWriter {
public:
  uint16_t data_size() const;
	bool Write(uint8_t* data, size_t size) const;
  std::vector<uint8_t> Write() const {
    std::vector<uint8_t> buffer(data_size());
    Write(buffer.data(), buffer.size());
    return buffer;
  }

  WickedPathSegmentWriter* add_segments() { return &segments_.emplace_back(); }
  std::span<WickedPathSegmentWriter> segments() { return segments_; }
  std::span<const WickedPathSegmentWriter> segments() const { return segments_; }

private:
  std::vector<WickedPathSegmentWriter> segments_;
};

#endif
