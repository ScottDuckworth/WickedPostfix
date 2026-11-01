#define WICKED_POSTFIX_SLIM 1

#include "WickedPostfix.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <cmath>
#include <span>

using ::testing::ElementsAre;

namespace {

struct TestEval {
  const uint8_t* prog_data = nullptr;
  uint16_t prog_size = 0;
  float* stack_data = nullptr;
  uint16_t stack_size = 0;
  uint16_t stack_capacity = 0;
  uint16_t min_capacity = 0;
  uint16_t final_stack_size = 0;

  TestEval(uint16_t capacity, std::initializer_list<float> stack_values) {
    stack_data = new float[capacity];
    stack_size = static_cast<uint16_t>(stack_values.size());
    stack_capacity = capacity;

    float* v = stack_data;
    for (float value : stack_values) {
      *v++ = value;
    }
  }

  ~TestEval() {
    delete[] stack_data;
  }

  std::span<float> stack() { return std::span<float>(stack_data, stack_size); }
};

bool WickedPostfixRead(const uint8_t* data, size_t size, TestEval* eval) {
  return ::WickedPostfixRead(data, size, &eval->prog_data, &eval->prog_size);
}

WickedEvalStatus_t WickedPostfixValidate(TestEval* eval, uint16_t expected_final_stack_size) {
  WickedEvalStatus_t status = ::WickedPostfixValidate(
      eval->prog_data, eval->prog_size, eval->stack_size,
      &eval->min_capacity, &eval->final_stack_size);
  if (status != WickedEvalStatus_Ok) {
    return status;
  }
  if (eval->min_capacity > eval->stack_capacity) {
    return WickedEvalStatus_StackOverflow;
  }
  if (eval->final_stack_size != expected_final_stack_size) {
    return WickedEvalStatus_UnexpectedStackSize;
  }
  return WickedEvalStatus_Ok;
}

void WickedPostfixEvaluate(TestEval* eval) {
  ::WickedPostfixEvaluate(eval->prog_data, eval->prog_size, eval->stack_size, eval->stack_data);
  eval->stack_size = eval->final_stack_size;
}

}  // namespace

TEST(SlimProfileTest, CoreOperationsWork) {
  // Sin, Cos, CosSin, Atan2, PanTilt, Sqrt, Ipow all still work in slim profile
  WickedPostfixWriter writer;
  writer.Push(0.5f);
  writer.CosSin();
  auto buffer = writer.Write();

  TestEval eval(4, {});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(std::cos(0.5f), std::sin(0.5f)));
}

TEST(SlimProfileTest, PanTiltWorks) {
  WickedPostfixWriter writer;
  writer.PanTilt();
  auto buffer = writer.Write();

  TestEval eval(4, {3.0f, 4.0f, 5.0f});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(std::atan2(4.0f, 3.0f), std::atan2(5.0f, 5.0f)));
}

TEST(SlimProfileTest, PowDisabled) {
  WickedPostfixWriter writer;
  writer.Pow();
  auto buffer = writer.Write();

  TestEval eval(4, {2.0f, 3.0f});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 1), WickedEvalStatus_UndefinedOperation);
}

TEST(SlimProfileTest, ExpDisabled) {
  WickedPostfixWriter writer;
  writer.Exp();
  auto buffer = writer.Write();

  TestEval eval(4, {1.0f});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 1), WickedEvalStatus_UndefinedOperation);
}

TEST(SlimProfileTest, LnDisabled) {
  WickedPostfixWriter writer;
  writer.Ln();
  auto buffer = writer.Write();

  TestEval eval(4, {1.0f});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 1), WickedEvalStatus_UndefinedOperation);
}

TEST(SlimProfileTest, TanDisabled) {
  WickedPostfixWriter writer;
  writer.Tan();
  auto buffer = writer.Write();

  TestEval eval(4, {0.5f});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 1), WickedEvalStatus_UndefinedOperation);
}

TEST(SlimProfileTest, AsinDisabled) {
  WickedPostfixWriter writer;
  writer.Asin();
  auto buffer = writer.Write();

  TestEval eval(4, {0.5f});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 1), WickedEvalStatus_UndefinedOperation);
}

TEST(SlimProfileTest, AcosDisabled) {
  WickedPostfixWriter writer;
  writer.Acos();
  auto buffer = writer.Write();

  TestEval eval(4, {0.5f});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 1), WickedEvalStatus_UndefinedOperation);
}
