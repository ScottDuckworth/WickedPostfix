#include "WickedPath.h"
#include "WickedPostfix.h"

#include <array>
#include <span>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::FloatEq;
using ::testing::Pointwise;

namespace {

struct TestEval {
  uint16_t stack_size = 1;
  uint16_t stack_capacity = sizeof(stack_buffer) / sizeof(float);
  float stack_buffer[8];

  std::span<float> stack() { return std::span<float>(stack_buffer, stack_size); }
};

TEST(PathEvalTest, Empty) {
  TestEval eval;
  EXPECT_EQ(WickedPathValidate(nullptr, 3, eval.stack_capacity), WickedEvalStatus_IllegalOperation);

  WickedPathWriter writer;
  auto buffer = writer.Write();
  EXPECT_EQ(WickedPathValidate(buffer.data(), buffer.size(), 0), WickedEvalStatus_StackOverflow);
  EXPECT_EQ(WickedPathValidate(buffer.data(), buffer.size(), eval.stack_capacity), WickedEvalStatus_Ok);

  EXPECT_EQ(WickedPathEvaluate(buffer.data(), 0, eval.stack_buffer), WickedEvalStatus_UndefinedOperation);
}

TEST(PathEvalTest, NoStack) {
  WickedPathWriter writer;
  WickedPathSegmentWriter* segment;
  segment = writer.add_segments();
  segment->start_time = 0;
  auto buffer = writer.Write();
  EXPECT_EQ(WickedPathValidate(buffer.data(), buffer.size(), 0), WickedEvalStatus_StackOverflow);
  EXPECT_EQ(WickedPathValidate(buffer.data(), buffer.size(), 1), WickedEvalStatus_Ok);

  float dummy = 0;
  EXPECT_EQ(WickedPathEvaluate(buffer.data(), 0, &dummy), WickedEvalStatus_Ok);
}

TEST(PathEvalTest, Eval) {
  WickedPathWriter writer;
  WickedPathSegmentWriter* segment;
  segment = writer.add_segments();
  segment->start_time = 1000;
  segment = writer.add_segments();
  segment->start_time = 2000;
  segment->expr.Drop();
  segment->expr.Push(99.0f);
  auto buffer = writer.Write();

  TestEval eval;
  EXPECT_EQ(WickedPathValidate(buffer.data(), buffer.size(), 0), WickedEvalStatus_StackOverflow);
  EXPECT_EQ(WickedPathValidate(buffer.data(), buffer.size(), eval.stack_capacity), WickedEvalStatus_Ok);

  EXPECT_EQ(WickedPathEvaluate(buffer.data(), 1000, eval.stack_buffer), WickedEvalStatus_Ok);
  EXPECT_THAT(eval.stack(), Pointwise(FloatEq(), {0}));

  EXPECT_EQ(WickedPathEvaluate(buffer.data(), 1750, eval.stack_buffer), WickedEvalStatus_Ok);
  EXPECT_THAT(eval.stack(), Pointwise(FloatEq(), {750}));

  EXPECT_EQ(WickedPathEvaluate(buffer.data(), 2000, eval.stack_buffer), WickedEvalStatus_Ok);
  EXPECT_THAT(eval.stack(), Pointwise(FloatEq(), {99}));

  EXPECT_EQ(WickedPathEvaluate(buffer.data(), 2100, eval.stack_buffer), WickedEvalStatus_Ok);
  EXPECT_THAT(eval.stack(), Pointwise(FloatEq(), {99}));
}

TEST(PathEvalTest, Wraparound) {
  WickedPathWriter writer;
  WickedPathSegmentWriter* segment;
  segment = writer.add_segments();
  segment->start_time = -1000;
  segment = writer.add_segments();
  segment->start_time = 1000;

  auto buffer = writer.Write();

  TestEval eval;
  EXPECT_EQ(WickedPathValidate(buffer.data(), buffer.size(), 0), WickedEvalStatus_StackOverflow);
  EXPECT_EQ(WickedPathValidate(buffer.data(), buffer.size(), eval.stack_capacity), WickedEvalStatus_Ok);

  EXPECT_EQ(WickedPathEvaluate(buffer.data(), -1000, eval.stack_buffer), WickedEvalStatus_Ok);
  EXPECT_THAT(eval.stack(), Pointwise(FloatEq(), {0}));

  EXPECT_EQ(WickedPathEvaluate(buffer.data(), -500, eval.stack_buffer), WickedEvalStatus_Ok);
  EXPECT_THAT(eval.stack(), Pointwise(FloatEq(), {500}));

  EXPECT_EQ(WickedPathEvaluate(buffer.data(), 0, eval.stack_buffer), WickedEvalStatus_Ok);
  EXPECT_THAT(eval.stack(), Pointwise(FloatEq(), {1000}));

  EXPECT_EQ(WickedPathEvaluate(buffer.data(), 500, eval.stack_buffer), WickedEvalStatus_Ok);
  EXPECT_THAT(eval.stack(), Pointwise(FloatEq(), {1500}));

  EXPECT_EQ(WickedPathEvaluate(buffer.data(), 1000, eval.stack_buffer), WickedEvalStatus_Ok);
  EXPECT_THAT(eval.stack(), Pointwise(FloatEq(), {0}));

  EXPECT_EQ(WickedPathEvaluate(buffer.data(), 1500, eval.stack_buffer), WickedEvalStatus_Ok);
  EXPECT_THAT(eval.stack(), Pointwise(FloatEq(), {500}));
}

TEST(PathEvalTest, UnexpectedStackSize) {
  WickedPathWriter writer;
  WickedPathSegmentWriter* segment = writer.add_segments();
  segment->start_time = 0;
  // Initial stack is 1, push 2 more -> final stack is 3 (expected 1)
  segment->expr.PushVec(std::array<float, 2>{1, 2});
  auto buffer = writer.Write();
  EXPECT_EQ(WickedPathValidate(buffer.data(), buffer.size(), 8), WickedEvalStatus_UnexpectedStackSize);
}

TEST(PathEvalTest, InsufficientStackCapacity) {
  WickedPathWriter writer;
  WickedPathSegmentWriter* segment = writer.add_segments();
  segment->start_time = 0;
  // Initial stack is 1, push 2 floats (depth 3), then drop 2 floats (depth back to 1)
  segment->expr.PushVec(std::array<float, 2>{1, 2});
  segment->expr.Drop();
  segment->expr.Drop();
  auto buffer = writer.Write();

  EXPECT_EQ(WickedPathValidate(buffer.data(), buffer.size(), 2), WickedEvalStatus_StackOverflow);
  EXPECT_EQ(WickedPathValidate(buffer.data(), buffer.size(), 3), WickedEvalStatus_Ok);

  float stack_data[3];
  EXPECT_EQ(WickedPathEvaluate(buffer.data(), 0, stack_data), WickedEvalStatus_Ok);
  EXPECT_EQ(stack_data[0], 0);
}

TEST(PathEvalTest, SegmentAt) {
  WickedPathWriter empty_writer;
  auto empty_buffer = empty_writer.Write();
  WickedPathSegmentDescriptor_t descriptor;
  EXPECT_FALSE(WickedPathSegmentAt(empty_buffer.data(), 500, &descriptor));

  WickedPathWriter writer;
  WickedPathSegmentWriter* segment;
  segment = writer.add_segments();
  segment->start_time = 1000;
  segment->expr.Drop();
  segment->expr.Push(42.0f);

  segment = writer.add_segments();
  segment->start_time = 2000;
  segment->expr.Drop();
  segment->expr.Push(99.0f);

  auto buffer = writer.Write();
  EXPECT_EQ(WickedPathValidate(buffer.data(), buffer.size(), 8), WickedEvalStatus_Ok);

  EXPECT_TRUE(WickedPathSegmentAt(buffer.data(), 1000, nullptr));

  // At 1000: segment 0
  EXPECT_TRUE(WickedPathSegmentAt(buffer.data(), 1000, &descriptor));
  EXPECT_EQ(descriptor.start_time, 1000);
  EXPECT_NE(descriptor.prog_data, nullptr);

  // At 1999: still segment 0
  EXPECT_TRUE(WickedPathSegmentAt(buffer.data(), 1999, &descriptor));
  EXPECT_EQ(descriptor.start_time, 1000);

  // At 2000: segment 1
  EXPECT_TRUE(WickedPathSegmentAt(buffer.data(), 2000, &descriptor));
  EXPECT_EQ(descriptor.start_time, 2000);
  EXPECT_NE(descriptor.prog_data, nullptr);

  // At 500: wraps around to segment 1 (since 1000 is start of segment 0)
  EXPECT_TRUE(WickedPathSegmentAt(buffer.data(), 500, &descriptor));
  EXPECT_EQ(descriptor.start_time, 2000);
}

TEST(PathEvalTest, PostfixErrors) {
  // Stack underflow
  {
    WickedPathWriter writer;
    WickedPathSegmentWriter* segment = writer.add_segments();
    segment->start_time = 0;
    segment->expr.Drop(); // Dropping 2 from stack of 1
    segment->expr.Drop();
    auto buffer = writer.Write();
    EXPECT_EQ(WickedPathValidate(buffer.data(), buffer.size(), 8), WickedEvalStatus_StackUnderflow);
  }

  // Undefined opcode
  {
    WickedPathWriter writer;
    WickedPathSegmentWriter* segment = writer.add_segments();
    segment->start_time = 0;
    segment->expr.EmitOp(static_cast<WickedPostfixOp_t>(254)); // Undefined opcode
    auto buffer = writer.Write();
    EXPECT_EQ(WickedPathValidate(buffer.data(), buffer.size(), 8), WickedEvalStatus_UndefinedOperation);
  }

  // Truncated buffer
  {
    WickedPathWriter writer;
    WickedPathSegmentWriter* segment = writer.add_segments();
    segment->start_time = 0;
    auto buffer = writer.Write();
    EXPECT_EQ(WickedPathValidate(buffer.data(), sizeof(WickedPathHeader_t), 8), WickedEvalStatus_IllegalOperation);
    EXPECT_EQ(WickedPathValidate(buffer.data(), sizeof(WickedPathHeader_t) + sizeof(WickedPathSegmentHeader_t), 8), WickedEvalStatus_IllegalOperation);
  }
}

TEST(PathEvalTest, HeadersPacked) {
  EXPECT_EQ(sizeof(WickedPathHeader_t), 4);
  EXPECT_EQ(alignof(WickedPathHeader_t), 1);
  EXPECT_EQ(sizeof(WickedPathSegmentHeader_t), 8);
  EXPECT_EQ(alignof(WickedPathSegmentHeader_t), 1);
}

}
