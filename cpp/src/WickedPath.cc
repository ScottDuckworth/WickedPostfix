#include "WickedPath.h"

namespace {

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

}  // namespace

static_assert(sizeof(WickedPathHeader_t) == 4);
static_assert(alignof(WickedPathHeader_t) == 1);
static_assert(sizeof(WickedPathSegmentHeader_t) == 8);
static_assert(alignof(WickedPathSegmentHeader_t) == 1);

extern "C" WickedEvalStatus_t WickedPathValidate(const uint8_t* path_data, size_t path_size, uint16_t stack_capacity) {
  if (stack_capacity < 1) return WickedEvalStatus_StackOverflow;
  if (!path_data || path_size < sizeof(WickedPathHeader_t)) return WickedEvalStatus_IllegalOperation;

  auto* const header = reinterpret_cast<const WickedPathHeader_t*>(path_data);
  auto* const segments = reinterpret_cast<const WickedPathSegmentHeader_t*>(path_data + sizeof(WickedPathHeader_t));

  if (path_size < sizeof(WickedPathHeader_t) + header->segment_size * sizeof(WickedPathSegmentHeader_t)) return WickedEvalStatus_IllegalOperation;

  for (uint16_t i = 0; i < header->segment_size; ++i) {
    const WickedPathSegmentHeader_t& segment = segments[i];
    if (path_size < segment.offset + segment.size) return WickedEvalStatus_IllegalOperation;
    uint16_t min_capacity;
    uint16_t final_size;
    WickedEvalStatus_t status = WickedPostfixValidate(path_data + segment.offset, segment.size, 1, &min_capacity, &final_size);
    if (status != WickedEvalStatus_Ok) return status;
    if (final_size != 1) return WickedEvalStatus_UnexpectedStackSize;
    if (min_capacity > stack_capacity) return WickedEvalStatus_StackOverflow;
  }
  return WickedEvalStatus_Ok;
}

bool WickedPathSegmentAt(const uint8_t* path_data, uint32_t t, WickedPathSegmentDescriptor_t* descriptor) {
  auto* const header = reinterpret_cast<const WickedPathHeader_t*>(path_data);
  auto* const segments = reinterpret_cast<const WickedPathSegmentHeader_t*>(path_data + sizeof(WickedPathHeader_t));

  uint16_t i = wicked_upper_bound(uint16_t(0), header->segment_size, [segments, t](uint16_t i) {
    return t - segments[0].start_time < segments[i].start_time - segments[0].start_time;
  });
  if (i == 0) return false;
  const WickedPathSegmentHeader_t& segment = segments[i - 1];

  if (descriptor) {
    descriptor->start_time = segment.start_time;
    descriptor->prog_data = path_data + segment.offset;
  }
  return true;
}

extern "C" WickedEvalStatus_t WickedPathEvaluate(const uint8_t* path_data, uint32_t t, float* stack_data) {
  WickedPathSegmentDescriptor_t descriptor;
  if (!WickedPathSegmentAt(path_data, t, &descriptor)) return WickedEvalStatus_UndefinedOperation;
  stack_data[0] = static_cast<float>(t - descriptor.start_time);
  WickedPostfixEvaluate(descriptor.prog_data, 1, stack_data);
  return WickedEvalStatus_Ok;
}

uint16_t WickedPathWriter::data_size() const {
  size_t size = sizeof(WickedPathHeader_t) + segments_.size() * sizeof(WickedPathSegmentHeader_t);
  for (const WickedPathSegmentWriter& segment : segments_) {
    size += (segment.expr.data_size() + 3) & ~3;
  }
  return uint16_t(size);
}

bool WickedPathWriter::Write(uint8_t* data, size_t size) const {
  const size_t headers_size = sizeof(WickedPathHeader_t) + segments_.size() * sizeof(WickedPathSegmentHeader_t);
  if (size < headers_size) return false;

  auto* header = reinterpret_cast<WickedPathHeader_t*>(data);
  header->segment_size = uint8_t(segments_.size());
  header->flags = 0;

  auto* segment_header = reinterpret_cast<WickedPathSegmentHeader_t*>(data + sizeof(WickedPathHeader_t));
  uint16_t offset = uint16_t(headers_size);
  for (const WickedPathSegmentWriter& segment : segments_) {
    segment_header->start_time = segment.start_time;
    segment_header->offset = offset;
    segment_header->size = segment.expr.data_size();
    if (!segment.expr.Write(data + offset, size - offset)) return false;
    offset += uint16_t((segment_header->size + 3) & ~3);
    ++segment_header;
  }
  return true;
}
