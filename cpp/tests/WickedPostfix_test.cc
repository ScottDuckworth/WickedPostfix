#include "WickedPostfix.h"

#include <cmath>
#include <span>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

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
    assert(capacity >= stack_values.size());

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

TEST(EvalTest, Empty) {
  WickedPostfixWriter writer;
  auto buffer = writer.Write();

  TestEval eval(4, {42});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 1), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(42));
}

TEST(EvalTest, Push) {
  WickedPostfixWriter writer;
  writer.Push(42.0f);
  EXPECT_EQ(writer.prog_size(), 5);
  auto buffer = writer.Write();

  TestEval eval(4, {1});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(1, 42));
}

TEST(EvalTest, PushVec) {
  WickedPostfixWriter writer;
  writer.PushVec({1, 2});
  EXPECT_EQ(writer.prog_size(), 10);
  auto buffer = writer.Write();

  TestEval eval(4, {42});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 3), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(42, 1, 2));
}

TEST(EvalTest, PushMany) {
  WickedPostfixWriter writer;
  writer.PushVec({1, 2});
  writer.Push(3.0f);
  EXPECT_EQ(writer.prog_size(), 15);
  auto buffer = writer.Write();

  TestEval eval(4, {42});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 4), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(42, 1, 2, 3));
}

TEST(EvalTest, PushIntUnderflow) {
  WickedPostfixWriter writer;
  writer.EmitOp(WickedPostfixOp_Push);
  auto buffer = writer.Write();

  TestEval eval(4, {42});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_IllegalOperation);
}

TEST(EvalTest, PushVecIntUnderflow) {
  WickedPostfixWriter writer;
  writer.EmitOp(WickedPostfixOp_PushVec);
  auto buffer = writer.Write();

  TestEval eval(4, {42});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_IllegalOperation);
}

TEST(EvalTest, PushVecDataUnderflow) {
  WickedPostfixWriter writer;
  writer.EmitOp(WickedPostfixOp_PushVec);
  writer.EmitByte(1);  // size = 2, needs 8 bytes
  writer.EmitFloat(1);  // only 4 bytes provided
  auto buffer = writer.Write();

  TestEval eval(4, {42});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_IllegalOperation);
}
TEST(EvalTest, Drop) {
  WickedPostfixWriter writer;
  writer.Drop();
  auto buffer = writer.Write();

  TestEval eval(4, {1, 2, 3});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(1, 2));
}

TEST(EvalTest, DropStackUnderflow) {
  WickedPostfixWriter writer;
  writer.Drop();
  auto buffer = writer.Write();

  TestEval eval(4, {});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, Dup) {
  WickedPostfixWriter writer;
  writer.Dup();
  auto buffer = writer.Write();

  TestEval eval(4, {1, 2, 3});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 4), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(1, 2, 3, 3));
}

TEST(EvalTest, DupStackUnderflow) {
  WickedPostfixWriter writer;
  writer.Dup();
  auto buffer = writer.Write();

  TestEval eval(4, {});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, Swap) {
  WickedPostfixWriter writer;
  writer.Swap();
  auto buffer = writer.Write();

  TestEval eval(4, {1, 2, 3});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 3), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(1, 3, 2));
}

TEST(EvalTest, SwapStackUnderflow) {
  WickedPostfixWriter writer;
  writer.Swap();
  auto buffer = writer.Write();

  TestEval eval(4, {1});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, Over) {
  WickedPostfixWriter writer;
  writer.Over();
  auto buffer = writer.Write();

  TestEval eval(4, {1, 2});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 3), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(1, 2, 1));
}

TEST(EvalTest, OverStackUnderflow) {
  WickedPostfixWriter writer;
  writer.Over();
  auto buffer = writer.Write();

  TestEval eval(4, {1});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, Tuck) {
  WickedPostfixWriter writer;
  writer.Tuck();
  auto buffer = writer.Write();

  TestEval eval(4, {1, 2});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 3), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(2, 1, 2));
}

TEST(EvalTest, TuckStackUnderflow) {
  WickedPostfixWriter writer;
  writer.Tuck();
  auto buffer = writer.Write();

  TestEval eval(4, {1});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, Nip) {
  WickedPostfixWriter writer;
  writer.Nip();
  auto buffer = writer.Write();

  TestEval eval(4, {1, 2, 3});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(1, 3));
}

TEST(EvalTest, NipStackUnderflow) {
  WickedPostfixWriter writer;
  writer.Nip();
  auto buffer = writer.Write();

  TestEval eval(4, {1});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, Rot) {
  WickedPostfixWriter writer;
  writer.Rot();
  auto buffer = writer.Write();

  TestEval eval(4, {1, 2, 3, 4});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 4), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(1, 3, 4, 2));
}

TEST(EvalTest, RotStackUnderflow) {
  WickedPostfixWriter writer;
  writer.Rot();
  auto buffer = writer.Write();

  TestEval eval(4, {1, 2});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, CopyScalar) {
  WickedPostfixWriter writer;
  writer.Copy(1, 1);
  auto buffer = writer.Write();

  TestEval eval(4, {10, 20, 30});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 4), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(10, 20, 30, 20));
}

TEST(EvalTest, CopyVector) {
  WickedPostfixWriter writer;
  writer.Copy(1, 2);
  auto buffer = writer.Write();

  TestEval eval(6, {10, 20, 30, 40});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 6), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(10, 20, 30, 40, 20, 30));
}

TEST(EvalTest, CopyMaxSize) {
  WickedPostfixWriter writer;
  writer.Copy(0, 8);
  auto buffer = writer.Write();

  TestEval eval(16, {1, 2, 3, 4, 5, 6, 7, 8});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 16), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8));
}

TEST(EvalTest, MoveScalar) {
  WickedPostfixWriter writer;
  writer.Move(2, 1);
  auto buffer = writer.Write();

  TestEval eval(4, {10, 20, 30, 40});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 4), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(10, 30, 40, 20));
}

TEST(EvalTest, MoveVector) {
  WickedPostfixWriter writer;
  writer.Move(1, 2);
  auto buffer = writer.Write();

  TestEval eval(5, {10, 20, 30, 40});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 4), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(10, 40, 20, 30));
}

TEST(EvalTest, MoveMaxSize) {
  WickedPostfixWriter writer;
  writer.Move(1, 8);
  auto buffer = writer.Write();

  TestEval eval(10, {1, 2, 3, 4, 5, 6, 7, 8, 9});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 9), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(9, 1, 2, 3, 4, 5, 6, 7, 8));
}

TEST(EvalTest, CopyStackUnderflow) {
  WickedPostfixWriter writer;
  writer.Copy(2, 2);
  auto buffer = writer.Write();

  TestEval eval(4, {10, 20, 30});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, CopyIntUnderflow) {
  WickedPostfixWriter writer;
  writer.EmitOp(WickedPostfixOp_Copy);
  auto buffer = writer.Write();

  TestEval eval(4, {10, 20});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_IllegalOperation);
}

TEST(EvalTest, MoveStackUnderflow) {
  WickedPostfixWriter writer;
  writer.Move(2, 2);
  auto buffer = writer.Write();

  TestEval eval(4, {10, 20, 30});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, MoveIntUnderflow) {
  WickedPostfixWriter writer;
  writer.EmitOp(WickedPostfixOp_Move);
  auto buffer = writer.Write();

  TestEval eval(4, {10, 20});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_IllegalOperation);
}

TEST(EvalTest, CopyBaseScalar) {
  WickedPostfixWriter writer;
  writer.CopyBase(0, 1);
  auto buffer = writer.Write();

  TestEval eval(5, {10, 20, 30});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 4), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(10, 20, 30, 10));
}

TEST(EvalTest, CopyBaseVector) {
  WickedPostfixWriter writer;
  writer.CopyBase(1, 2);
  auto buffer = writer.Write();

  TestEval eval(7, {10, 20, 30, 40});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 6), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(10, 20, 30, 40, 20, 30));
}

TEST(EvalTest, CopyBaseMaxSize) {
  WickedPostfixWriter writer;
  writer.CopyBase(0, 8);
  auto buffer = writer.Write();

  TestEval eval(16, {1, 2, 3, 4, 5, 6, 7, 8});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 16), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8));
}

TEST(EvalTest, CopyBaseStackUnderflow) {
  WickedPostfixWriter writer;
  writer.CopyBase(2, 2);
  auto buffer = writer.Write();

  TestEval eval(4, {10, 20, 30});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, CopyBaseIntUnderflow) {
  WickedPostfixWriter writer;
  writer.EmitOp(WickedPostfixOp_CopyBase);
  auto buffer = writer.Write();

  TestEval eval(4, {10, 20});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_IllegalOperation);
}

TEST(EvalTest, Slide) {
  WickedPostfixWriter writer;
  writer.Slide(2, 3);
  auto buffer = writer.Write();

  TestEval eval(5, {10, 20, 30, 40, 50});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(40, 50));
}

TEST(EvalTest, SlideScalar) {
  WickedPostfixWriter writer;
  writer.Slide(1, 1);
  auto buffer = writer.Write();

  TestEval eval(2, {10, 20});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 1), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(20));
}

TEST(EvalTest, SlideUnderflow) {
  WickedPostfixWriter writer;
  writer.Slide(2, 2);
  auto buffer = writer.Write();

  TestEval eval(3, {10, 20, 30});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, SlideIntUnderflow) {
  WickedPostfixWriter writer;
  writer.EmitOp(WickedPostfixOp_Slide);
  auto buffer = writer.Write();

  TestEval eval(4, {10, 20});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_IllegalOperation);
}

TEST(EvalTest, Add) {
  WickedPostfixWriter writer;
  writer.Add();
  auto buffer = writer.Write();

  TestEval eval(4, {0, 1, 2});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, 3));
}

TEST(EvalTest, AddStackUnderflow) {
  WickedPostfixWriter writer;
  writer.Add();
  auto buffer = writer.Write();

  TestEval eval(4, {1});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, Sub) {
  WickedPostfixWriter writer;
  writer.Sub();
  auto buffer = writer.Write();

  TestEval eval(4, {0, 1, 2});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, -1));
}

TEST(EvalTest, SubStackUnderflow) {
  WickedPostfixWriter writer;
  writer.Sub();
  auto buffer = writer.Write();

  TestEval eval(4, {1});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, Mul) {
  WickedPostfixWriter writer;
  writer.Mul();
  auto buffer = writer.Write();

  TestEval eval(4, {0, 2, 3});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, 6));
}

TEST(EvalTest, MulStackUnderflow) {
  WickedPostfixWriter writer;
  writer.Mul();
  auto buffer = writer.Write();

  TestEval eval(4, {1});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, Div) {
  WickedPostfixWriter writer;
  writer.Div();
  auto buffer = writer.Write();

  TestEval eval(4, {0, 1, 2});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, 0.5));
}

TEST(EvalTest, DivStackUnderflow) {
  WickedPostfixWriter writer;
  writer.Div();
  auto buffer = writer.Write();

  TestEval eval(4, {1});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, Pow) {
  WickedPostfixWriter writer;
  writer.Pow();
  auto buffer = writer.Write();

  TestEval eval(4, {0, 2, 3});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, 8));
}

TEST(EvalTest, PowStackUnderflow) {
  WickedPostfixWriter writer;
  writer.Pow();
  auto buffer = writer.Write();

  TestEval eval(4, {1});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, Atan2) {
  WickedPostfixWriter writer;
  writer.Atan2();
  auto buffer = writer.Write();

  TestEval eval(4, {0, 4, 5});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, std::atan2(5, 4)));
}

TEST(EvalTest, Atan2StackUnderflow) {
  WickedPostfixWriter writer;
  writer.Atan2();
  auto buffer = writer.Write();

  TestEval eval(4, {5});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, Neg) {
  WickedPostfixWriter writer;
  writer.Neg();
  auto buffer = writer.Write();

  TestEval eval(4, {0, 2});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, -2));
}

TEST(EvalTest, NegStackUnderflow) {
  WickedPostfixWriter writer;
  writer.Neg();
  auto buffer = writer.Write();

  TestEval eval(4, {});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, Abs) {
  WickedPostfixWriter writer;
  writer.Abs();
  auto buffer = writer.Write();

  TestEval eval(4, {0, -2});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, 2));
}

TEST(EvalTest, AbsStackUnderflow) {
  WickedPostfixWriter writer;
  writer.Abs();
  auto buffer = writer.Write();

  TestEval eval(4, {});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, Inv) {
  WickedPostfixWriter writer;
  writer.Inv();
  auto buffer = writer.Write();

  TestEval eval(4, {0, 2});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, 0.5));
}

TEST(EvalTest, InvStackUnderflow) {
  WickedPostfixWriter writer;
  writer.Inv();
  auto buffer = writer.Write();

  TestEval eval(4, {});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, Sqrt) {
  WickedPostfixWriter writer;
  writer.Sqrt();
  auto buffer = writer.Write();

  TestEval eval(4, {0, 7});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, std::sqrt(7)));
}

TEST(EvalTest, SqrtStackUnderflow) {
  WickedPostfixWriter writer;
  writer.Sqrt();
  auto buffer = writer.Write();

  TestEval eval(4, {});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, Exp) {
  WickedPostfixWriter writer;
  writer.Exp();
  auto buffer = writer.Write();

  TestEval eval(4, {0, 4});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, std::exp(4)));
}

TEST(EvalTest, ExpStackUnderflow) {
  WickedPostfixWriter writer;
  writer.Exp();
  auto buffer = writer.Write();

  TestEval eval(4, {});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, Ln) {
  WickedPostfixWriter writer;
  writer.Ln();
  auto buffer = writer.Write();

  TestEval eval(4, {0, 5});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, std::log(5)));
}

TEST(EvalTest, LnStackUnderflow) {
  WickedPostfixWriter writer;
  writer.Ln();
  auto buffer = writer.Write();

  TestEval eval(4, {});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, Sin) {
  WickedPostfixWriter writer;
  writer.Sin();
  auto buffer = writer.Write();

  TestEval eval(4, {0, 5});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, std::sin(5)));
}

TEST(EvalTest, SinStackUnderflow) {
  WickedPostfixWriter writer;
  writer.Sin();
  auto buffer = writer.Write();

  TestEval eval(4, {});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, Cos) {
  WickedPostfixWriter writer;
  writer.Cos();
  auto buffer = writer.Write();

  TestEval eval(4, {0, 5});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, std::cos(5)));
}

TEST(EvalTest, CosStackUnderflow) {
  WickedPostfixWriter writer;
  writer.Cos();
  auto buffer = writer.Write();

  TestEval eval(4, {});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, Tan) {
  WickedPostfixWriter writer;
  writer.Tan();
  auto buffer = writer.Write();

  TestEval eval(4, {0, 5});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, std::tan(5)));
}

TEST(EvalTest, TanStackUnderflow) {
  WickedPostfixWriter writer;
  writer.Tan();
  auto buffer = writer.Write();

  TestEval eval(4, {});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, Asin) {
  WickedPostfixWriter writer;
  writer.Asin();
  auto buffer = writer.Write();

  TestEval eval(4, {0, 0.5});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, std::asin(0.5)));
}

TEST(EvalTest, AsinStackUnderflow) {
  WickedPostfixWriter writer;
  writer.Asin();
  auto buffer = writer.Write();

  TestEval eval(4, {});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, Acos) {
  WickedPostfixWriter writer;
  writer.Acos();
  auto buffer = writer.Write();

  TestEval eval(4, {0, 0.5});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, std::acos(0.5)));
}

TEST(EvalTest, AcosStackUnderflow) {
  WickedPostfixWriter writer;
  writer.Acos();
  auto buffer = writer.Write();

  TestEval eval(4, {});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, CosSin) {
  WickedPostfixWriter writer;
  writer.CosSin();
  auto buffer = writer.Write();

  TestEval eval(4, {0, 0.5f});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 3), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, std::cos(0.5f), std::sin(0.5f)));
}

TEST(EvalTest, CosSinStackUnderflow) {
  WickedPostfixWriter writer;
  writer.CosSin();
  auto buffer = writer.Write();

  TestEval eval(4, {});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, PanTilt) {
  WickedPostfixWriter writer;
  writer.PanTilt();
  auto buffer = writer.Write();

  TestEval eval(4, {0.0f, 3.0f, 4.0f, 5.0f});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 3), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  float expected_pan = std::atan2(4.0f, 3.0f);
  float expected_tilt = std::atan2(5.0f, 5.0f);
  EXPECT_THAT(eval.stack(), ElementsAre(0.0f, expected_pan, expected_tilt));
}

TEST(EvalTest, PanTiltStackUnderflow) {
  WickedPostfixWriter writer;
  writer.PanTilt();
  auto buffer = writer.Write();

  TestEval eval(4, {1.0f, 2.0f});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, Ipow) {
  WickedPostfixWriter writer;
  writer.Ipow(3);
  auto buffer = writer.Write();

  TestEval eval(4, {0, 2});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, 8));
}

TEST(EvalTest, IpowNegative) {
  WickedPostfixWriter writer;
  writer.Ipow(-2);
  auto buffer = writer.Write();

  TestEval eval(4, {0, 2});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, 0.25f));
}

TEST(EvalTest, IpowZero) {
  WickedPostfixWriter writer;
  writer.Ipow(0);
  auto buffer = writer.Write();

  TestEval eval(4, {0, 5});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, 1));
}

TEST(EvalTest, IpowStackUnderflow) {
  WickedPostfixWriter writer;
  writer.Ipow(2);
  auto buffer = writer.Write();

  TestEval eval(4, {});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, IpowIllegalTruncated) {
  uint8_t raw[] = {1, 0, static_cast<uint8_t>(WickedPostfixOp_Ipow)};
  TestEval eval(4, {2});
  EXPECT_TRUE(WickedPostfixRead(raw, sizeof(raw), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 1), WickedEvalStatus_IllegalOperation);
}

TEST(EvalTest, MulAdd) {
  WickedPostfixWriter writer;
  writer.MulAdd();
  auto buffer = writer.Write();

  TestEval eval(4, {0, 3, 2, 1});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, 7));
}

TEST(EvalTest, MulAddStackUnderflow) {
  WickedPostfixWriter writer;
  writer.MulAdd();
  auto buffer = writer.Write();

  TestEval eval(4, {1, 2});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, AddVec) {
  WickedPostfixWriter writer;
  writer.AddVec(3);
  auto buffer = writer.Write();

  TestEval eval(8, {0, 1, 2, 3, 4, 5, 6});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 4), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, 5, 7, 9));
}

TEST(EvalTest, AddVecStackUnderflow) {
  WickedPostfixWriter writer;
  writer.AddVec(3);
  auto buffer = writer.Write();

  TestEval eval(8, {1, 2, 3, 4, 5});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, AddVecIntUnderflow) {
  WickedPostfixWriter writer;
  writer.EmitOp(WickedPostfixOp_AddVec);
  auto buffer = writer.Write();

  TestEval eval(8, {0, 1, 2, 3, 4, 5, 6});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_IllegalOperation);
}

TEST(EvalTest, SubVec) {
  WickedPostfixWriter writer;
  writer.SubVec(3);
  auto buffer = writer.Write();

  TestEval eval(8, {0, 1, 2, 3, 4, 2, 1});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 4), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, -3, 0, 2));
}

TEST(EvalTest, SubVecStackUnderflow) {
  WickedPostfixWriter writer;
  writer.SubVec(3);
  auto buffer = writer.Write();

  TestEval eval(8, {1, 2, 3, 4, 5});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, SubVecIntUnderflow) {
  WickedPostfixWriter writer;
  writer.EmitOp(WickedPostfixOp_SubVec);
  auto buffer = writer.Write();

  TestEval eval(8, {0, 1, 2, 3, 4, 5, 6});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_IllegalOperation);
}

TEST(EvalTest, MulVec) {
  WickedPostfixWriter writer;
  writer.MulVec(3);
  auto buffer = writer.Write();

  TestEval eval(8, {0, 1, 2, 3, 4, 3, -1});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 4), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, 4, 6, -3));
}

TEST(EvalTest, MulVecStackUnderflow) {
  WickedPostfixWriter writer;
  writer.MulVec(3);
  auto buffer = writer.Write();

  TestEval eval(8, {1, 2, 3, 4, 5});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, MulVecIntUnderflow) {
  WickedPostfixWriter writer;
  writer.EmitOp(WickedPostfixOp_MulVec);
  auto buffer = writer.Write();

  TestEval eval(8, {0, 1, 2, 3, 4, 5, 6});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_IllegalOperation);
}

TEST(EvalTest, DivVec) {
  WickedPostfixWriter writer;
  writer.DivVec(3);
  auto buffer = writer.Write();

  TestEval eval(8, {0, 6, 8, -10, 2, 4, 5});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 4), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, 3, 2, -2));
}

TEST(EvalTest, DivVecStackUnderflow) {
  WickedPostfixWriter writer;
  writer.DivVec(3);
  auto buffer = writer.Write();

  TestEval eval(8, {1, 2, 3, 4, 5});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, DivVecIntUnderflow) {
  WickedPostfixWriter writer;
  writer.EmitOp(WickedPostfixOp_DivVec);
  auto buffer = writer.Write();

  TestEval eval(8, {0, 1, 2, 3, 4, 5, 6});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_IllegalOperation);
}

TEST(EvalTest, NegVec) {
  WickedPostfixWriter writer;
  writer.NegVec(3);
  auto buffer = writer.Write();

  TestEval eval(8, {0, 3, 4, -2});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 4), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, -3, -4, 2));
}

TEST(EvalTest, NegVecStackUnderflow) {
  WickedPostfixWriter writer;
  writer.NegVec(3);
  auto buffer = writer.Write();

  TestEval eval(8, {1, 2});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, NegVecIntUnderflow) {
  WickedPostfixWriter writer;
  writer.EmitOp(WickedPostfixOp_NegVec);
  auto buffer = writer.Write();

  TestEval eval(8, {0, 1, 2, 3});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_IllegalOperation);
}

TEST(EvalTest, InvVec) {
  WickedPostfixWriter writer;
  writer.InvVec(3);
  auto buffer = writer.Write();

  TestEval eval(8, {0, 2, 4, -0.5f});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 4), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, 0.5f, 0.25f, -2.0f));
}

TEST(EvalTest, InvVecStackUnderflow) {
  WickedPostfixWriter writer;
  writer.InvVec(3);
  auto buffer = writer.Write();

  TestEval eval(8, {1, 2});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, InvVecIntUnderflow) {
  WickedPostfixWriter writer;
  writer.EmitOp(WickedPostfixOp_InvVec);
  auto buffer = writer.Write();

  TestEval eval(8, {0, 1, 2, 3});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_IllegalOperation);
}

TEST(EvalTest, DotVec) {
  // Size 1: scalar product
  {
    WickedPostfixWriter writer;
    writer.DotVec(1);
    auto buffer = writer.Write();

    TestEval eval(8, {0, 3.0f, 7.0f});
    EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
    EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
    WickedPostfixEvaluate(&eval);
    EXPECT_THAT(eval.stack(), ElementsAre(0, 21.0f));
  }
  // Size 2: unrolled loop (even, no remainder)
  {
    WickedPostfixWriter writer;
    writer.DotVec(2);
    auto buffer = writer.Write();

    TestEval eval(8, {0, 2.0f, 3.0f, 4.0f, 5.0f});
    EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
    EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
    WickedPostfixEvaluate(&eval);
    EXPECT_THAT(eval.stack(), ElementsAre(0, 2.0f * 4.0f + 3.0f * 5.0f));
  }
  // Size 3: unrolled loop + tail remainder
  {
    WickedPostfixWriter writer;
    writer.DotVec(3);
    auto buffer = writer.Write();

    TestEval eval(8, {0, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f});
    EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
    EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
    WickedPostfixEvaluate(&eval);
    EXPECT_THAT(eval.stack(), ElementsAre(0, 1.0f * 4.0f + 2.0f * 5.0f + 3.0f * 6.0f));
  }
  // Size 4: 2 unrolled iterations
  {
    WickedPostfixWriter writer;
    writer.DotVec(4);
    auto buffer = writer.Write();

    TestEval eval(10, {0, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f});
    EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
    EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
    WickedPostfixEvaluate(&eval);
    EXPECT_THAT(eval.stack(), ElementsAre(0, 1.0f * 5.0f + 2.0f * 6.0f + 3.0f * 7.0f + 4.0f * 8.0f));
  }
  // Size 5: 2 unrolled iterations + 1 tail
  {
    WickedPostfixWriter writer;
    writer.DotVec(5);
    auto buffer = writer.Write();

    TestEval eval(12, {0, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f});
    EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
    EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
    WickedPostfixEvaluate(&eval);
    EXPECT_THAT(eval.stack(), ElementsAre(0, 1.0f * 2.0f + 2.0f * 3.0f + 3.0f * 4.0f + 4.0f * 5.0f + 5.0f * 6.0f));
  }
}

TEST(EvalTest, DotVecStackUnderflow) {
  WickedPostfixWriter writer;
  writer.DotVec(3);
  auto buffer = writer.Write();

  TestEval eval(8, {1, 2, 3, 4, 5});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, DotVecIntUnderflow) {
  WickedPostfixWriter writer;
  writer.EmitOp(WickedPostfixOp_DotVec);
  auto buffer = writer.Write();

  TestEval eval(8, {0, 1, 2, 3, 4, 5, 6});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_IllegalOperation);
}

TEST(EvalTest, NormSqVec) {
  WickedPostfixWriter writer;
  writer.NormSqVec(3);
  auto buffer = writer.Write();

  TestEval eval(8, {0, 2, 3, 4});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, 2 * 2 + 3 * 3 + 4 * 4));
}

TEST(EvalTest, NormSqVecAndSqrt) {
  WickedPostfixWriter writer;
  writer.NormSqVec(3);
  writer.Sqrt();
  auto buffer = writer.Write();

  TestEval eval(8, {0, 2, 3, 4});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, std::sqrt(2 * 2 + 3 * 3 + 4 * 4)));
}

TEST(EvalTest, NormSqVecStackUnderflow) {
  WickedPostfixWriter writer;
  writer.NormSqVec(3);
  auto buffer = writer.Write();

  TestEval eval(8, {1, 2});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, NormSqVecIntUnderflow) {
  WickedPostfixWriter writer;
  writer.EmitOp(WickedPostfixOp_NormSqVec);
  auto buffer = writer.Write();

  TestEval eval(8, {0, 2, 3, 4});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_IllegalOperation);
}

TEST(EvalTest, SumVec) {
  WickedPostfixWriter writer;
  writer.SumVec(3);
  auto buffer = writer.Write();

  TestEval eval(8, {0, 1.0f, 2.5f, -0.5f});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, 3.0f));
}

TEST(EvalTest, SumVecStackUnderflow) {
  WickedPostfixWriter writer;
  writer.SumVec(3);
  auto buffer = writer.Write();

  TestEval eval(8, {1, 2});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, SumVecIntUnderflow) {
  WickedPostfixWriter writer;
  writer.EmitOp(WickedPostfixOp_SumVec);
  auto buffer = writer.Write();

  TestEval eval(8, {0, 2, 3, 4});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_IllegalOperation);
}

TEST(EvalTest, MulAddVec) {
  WickedPostfixWriter writer;
  writer.MulAddVec(3);
  auto buffer = writer.Write();

  TestEval eval(12, {0, 1, 2, 3, 4, 3, -1, 0, 1, 2});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 4), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, 1 * 4 + 0, 2 * 3 + 1, 3 * -1 + 2));
}

TEST(EvalTest, MulAddVecStackUnderflow) {
  WickedPostfixWriter writer;
  writer.MulAddVec(3);
  auto buffer = writer.Write();

  TestEval eval(12, {1, 2, 3, 4, 5, 6, 7, 8});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, MulAddVecIntUnderflow) {
  WickedPostfixWriter writer;
  writer.EmitOp(WickedPostfixOp_MulAddVec);
  auto buffer = writer.Write();

  TestEval eval(12, {0, 1, 2, 3, 4, 5, 6, 7, 8});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_IllegalOperation);
}

TEST(EvalTest, ScaleVec) {
  WickedPostfixWriter writer;
  writer.ScaleVec(3);
  auto buffer = writer.Write();

  TestEval eval(8, {0, 3, 4, -2, 2});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 4), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, 6, 8, -4));
}

TEST(EvalTest, ScaleVecStackUnderflow) {
  WickedPostfixWriter writer;
  writer.ScaleVec(3);
  auto buffer = writer.Write();

  TestEval eval(8, {1, 2, 3});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, ScaleVecIntUnderflow) {
  WickedPostfixWriter writer;
  writer.EmitOp(WickedPostfixOp_ScaleVec);
  auto buffer = writer.Write();

  TestEval eval(8, {0, 1, 2, 3, 4});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_IllegalOperation);
}

TEST(EvalTest, LerpVec) {
  WickedPostfixWriter writer;
  writer.LerpVec({2.0f, 3.0f, 4.0f}, {6.0f, 7.0f, 8.0f});
  auto buffer = writer.Write();

  TestEval eval(12, {0, 0.25f});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 4), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, 3, 4, 5));
}

TEST(EvalTest, LerpVecStackUnderflow) {
  WickedPostfixWriter writer;
  writer.LerpVec({2.0f, 3.0f, 4.0f}, {6.0f, 7.0f, 8.0f});
  auto buffer = writer.Write();

  TestEval eval(8, {});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, LerpVecIntUnderflow) {
  WickedPostfixWriter writer;
  writer.EmitOp(WickedPostfixOp_LerpVec);
  auto buffer = writer.Write();

  TestEval eval(12, {0, 0.25f});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_IllegalOperation);
}

TEST(EvalTest, LerpVecDataUnderflow) {
  WickedPostfixWriter writer;
  writer.EmitOp(WickedPostfixOp_LerpVec);
  writer.EmitByte(2);  // size = 3, needs 6 floats = 24 bytes
  writer.EmitFloat(1.0f);  // only 1 float provided
  auto buffer = writer.Write();

  TestEval eval(12, {0, 0.25f});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_IllegalOperation);
}

TEST(EvalTest, MulMat) {
  WickedPostfixWriter writer;
  writer.MulMat(3, 4, {
      1, 5, 9,     // Col 0
      2, 6, 10,    // Col 1
      3, 7, 11,    // Col 2
      4, 8, 12,    // Col 3
  });
  auto buffer = writer.Write();

  TestEval eval(32, {0, 1, 2, 3});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 5), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(
                                0,
                                1 * 1 + 2 * 5 + 3 * 9,
                                1 * 2 + 2 * 6 + 3 * 10,
                                1 * 3 + 2 * 7 + 3 * 11,
                                1 * 4 + 2 * 8 + 3 * 12));
}

TEST(EvalTest, MulMatSquare) {
  WickedPostfixWriter writer;
  writer.MulMat(3, 3, {
      1, 0, 0,
      0, 1, 0,
      0, 0, 1,
  });
  auto buffer = writer.Write();

  TestEval eval(16, {0, 1, 2, 3});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 4), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, 1, 2, 3));
}

TEST(EvalTest, MulMat2D) {
  WickedPostfixWriter writer;
  writer.MulMat(2, 2, {
      1, 3,  // Col 0
      2, 4,  // Col 1
  });
  auto buffer = writer.Write();

  TestEval eval(16, {0, 3, 4});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 3), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, 3 * 1 + 4 * 3, 3 * 2 + 4 * 4));
}

TEST(EvalTest, MulMat4D) {
  WickedPostfixWriter writer;
  writer.MulMat(4, 4, {
      1, 0, 0, 0,
      0, 2, 0, 0,
      0, 0, 3, 0,
      0, 0, 0, 4,
  });
  auto buffer = writer.Write();

  TestEval eval(32, {0, 1, 2, 3, 4});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 5), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, 1, 4, 9, 16));
}

TEST(EvalTest, MulMatProjectionDown) {
  WickedPostfixWriter writer;
  writer.MulMat(3, 2, {
      1, 3, 5,  // Col 0
      2, 4, 6,  // Col 1
  });
  auto buffer = writer.Write();

  TestEval eval(16, {0, 1, 2, 3});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 3), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, 1 * 1 + 2 * 3 + 3 * 5, 1 * 2 + 2 * 4 + 3 * 6));
}

TEST(EvalTest, MulMatDotProduct) {
  WickedPostfixWriter writer;
  writer.MulMat(3, 1, {4, 5, 6});
  auto buffer = writer.Write();

  TestEval eval(16, {0, 1, 2, 3});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, 1 * 4 + 2 * 5 + 3 * 6));
}

TEST(EvalTest, MulMatTightCapacity) {
  WickedPostfixWriter writer;
  writer.MulMat(3, 3, {
      1, 0, 0,
      0, 1, 0,
      0, 0, 1,
  });
  auto buffer = writer.Write();

  // Tight capacity: exactly 3 stack slots for 3 elements.
  TestEval eval(3, {10, 20, 30});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 3), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(10, 20, 30));
}

TEST(EvalTest, MulMatArbitraryProjection) {
  WickedPostfixWriter writer;
  writer.MulMat(4, 2, {
      1, 3, 5, 7,  // Col 0
      2, 4, 6, 8,  // Col 1
  });
  auto buffer = writer.Write();

  TestEval eval(16, {0, 1, 1, 1, 1});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 3), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, 1 + 3 + 5 + 7, 2 + 4 + 6 + 8));
}

TEST(EvalTest, MulMatStackUnderflow) {
  WickedPostfixWriter writer;
  writer.MulMat(3, 4, {
      1, 5, 9,
      2, 6, 10,
      3, 7, 11,
      4, 8, 12,
  });
  auto buffer = writer.Write();

  TestEval eval(32, {1, 2});  // needs 3, only 2 provided
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, MulMatIntUnderflow) {
  WickedPostfixWriter writer;
  writer.EmitOp(WickedPostfixOp_MulMat);
  auto buffer = writer.Write();

  TestEval eval(32, {0, 1, 2, 3});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_IllegalOperation);
}

TEST(EvalTest, MulMatDataUnderflow) {
  WickedPostfixWriter writer;
  writer.EmitOp(WickedPostfixOp_MulMat);
  // rows=3, cols=3 -> (3-1) | ((3-1) << 4) = 2 | 0x20 = 0x22
  writer.EmitByte(0x22);
  writer.EmitFloat(1.0f);  // needs 9 floats, only has 1
  auto buffer = writer.Write();

  TestEval eval(32, {0, 1, 2, 3});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_IllegalOperation);
}

TEST(EvalTest, MulMatReservedBitsNonZero) {
  WickedPostfixWriter writer;
  writer.EmitOp(WickedPostfixOp_MulMat);
  // rows=1 (0), cols=1 (0), but reserved bit 2 is set (0x04)
  writer.EmitByte(0x04);
  writer.EmitFloat(1.0f);
  auto buffer = writer.Write();

  TestEval eval(32, {0, 1});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_IllegalOperation);
}

TEST(EvalTest, Poly1D) {
  WickedPostfixWriter writer;
  writer.Poly({3, 4, 5, 6});
  auto buffer = writer.Write();

  TestEval eval(4, {0, 2});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, 79));
}

TEST(EvalTest, Poly1DStackUnderflow) {
  WickedPostfixWriter writer;
  writer.Poly({3, 4, 5});
  auto buffer = writer.Write();

  TestEval eval(4, {});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, PolyMatrix) {
  WickedPostfixWriter writer;
  writer.Poly(4, 2, {3, 4, 5, 6, 7, 8, 9, 10});
  auto buffer = writer.Write();

  TestEval eval(4, {0, 2});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 3), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, 3 + 2 * 5 + 4 * 7 + 8 * 9, 4 + 2 * 6 + 4 * 8 + 8 * 10));
}

TEST(EvalTest, Poly3D) {
  WickedPostfixWriter writer;
  writer.Poly(3, 3, {
      1, 2, 3,
      4, 5, 6,
      7, 8, 9,
  });
  auto buffer = writer.Write();

  TestEval eval(8, {0, 2.0f});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 4), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, 37.0f, 44.0f, 51.0f));
}

TEST(EvalTest, Poly4DGeneric) {
  WickedPostfixWriter writer;
  writer.Poly(2, 4, {
      1, 2, 3, 4,
      5, 6, 7, 8,
  });
  auto buffer = writer.Write();

  TestEval eval(8, {0, 3.0f});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 5), WickedEvalStatus_Ok);
  WickedPostfixEvaluate(&eval);
  EXPECT_THAT(eval.stack(), ElementsAre(0, 16.0f, 20.0f, 24.0f, 28.0f));
}

TEST(EvalTest, PolyStackUnderflow) {
  WickedPostfixWriter writer;
  writer.Poly(4, 2, {3, 4, 5, 6, 7, 8, 9, 10});
  auto buffer = writer.Write();

  TestEval eval(4, {});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_StackUnderflow);
}

TEST(EvalTest, PolyIntUnderflow) {
  WickedPostfixWriter writer;
  writer.EmitOp(WickedPostfixOp_Poly);
  auto buffer = writer.Write();

  TestEval eval(4, {0, 2});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_IllegalOperation);
}

TEST(EvalTest, PolyDataUnderflow) {
  WickedPostfixWriter writer;
  writer.EmitOp(WickedPostfixOp_Poly);
  writer.EmitByte(0x13);  // rows = 4, cols = 2 (needs 8 floats)
  writer.EmitFloat(1.0f);
  auto buffer = writer.Write();

  TestEval eval(4, {0, 2});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  EXPECT_EQ(WickedPostfixValidate(&eval, 0), WickedEvalStatus_IllegalOperation);
}

TEST(EvalTest, UnexpectedStackSize) {
  WickedPostfixWriter writer;
  writer.PushVec({1, 2});
  auto buffer = writer.Write();

  TestEval eval(4, {42});
  EXPECT_TRUE(WickedPostfixRead(buffer.data(), buffer.size(), &eval));
  // Initial stack has 1 element, pushes 2 -> final size is 3.
  EXPECT_EQ(WickedPostfixValidate(&eval, 2), WickedEvalStatus_UnexpectedStackSize);
  EXPECT_EQ(WickedPostfixValidate(&eval, 3), WickedEvalStatus_Ok);
}

TEST(EvalTest, DirectValidateAndEvaluate) {
  WickedPostfixWriter writer;
  writer.PushVec({1, 2});
  auto buffer = writer.Write();

  const uint8_t* prog_data = nullptr;
  uint16_t prog_size = 0;
  EXPECT_TRUE(::WickedPostfixRead(buffer.data(), buffer.size(), &prog_data, &prog_size));

  uint16_t min_capacity = 0;
  uint16_t final_stack_size = 0;
  EXPECT_EQ(::WickedPostfixValidate(prog_data, prog_size, 1, &min_capacity, &final_stack_size), WickedEvalStatus_Ok);
  EXPECT_EQ(min_capacity, 3);
  EXPECT_EQ(final_stack_size, 3);

  float stack[4] = {42, 0, 0, 0};
  ::WickedPostfixEvaluate(prog_data, prog_size, 1, stack);
  EXPECT_THAT(std::span<float>(stack, final_stack_size), ElementsAre(42, 1, 2));
}

TEST(EvalTest, StackCapacityExceedsUint16) {
  WickedPostfixWriter writer;
  writer.Dup();
  auto buffer = writer.Write();

  const uint8_t* prog_data = nullptr;
  uint16_t prog_size = 0;
  EXPECT_TRUE(::WickedPostfixRead(buffer.data(), buffer.size(), &prog_data, &prog_size));

  uint16_t min_capacity = 0;
  uint16_t final_stack_size = 0;
  EXPECT_EQ(::WickedPostfixValidate(prog_data, prog_size, 65535, &min_capacity, &final_stack_size), WickedEvalStatus_StackOverflow);
}

TEST(EvalTest, ReadUnsafe) {
  WickedPostfixWriter writer;
  writer.PushVec({1.0f, 2.0f});
  auto buffer = writer.Write();

  const uint8_t* safe_prog_data = nullptr;
  uint16_t safe_prog_size = 0;
  EXPECT_TRUE(::WickedPostfixRead(buffer.data(), buffer.size(), &safe_prog_data, &safe_prog_size));

  const uint8_t* unsafe_prog_data = nullptr;
  uint16_t unsafe_prog_size = 0;
  WickedPostfixReadUnsafe(buffer.data(), &unsafe_prog_data, &unsafe_prog_size);

  EXPECT_EQ(safe_prog_data, unsafe_prog_data);
  EXPECT_EQ(safe_prog_size, unsafe_prog_size);
  EXPECT_NE(unsafe_prog_data, nullptr);
  EXPECT_GT(unsafe_prog_size, 0);
}

TEST(EvalTest, ReadUnaligned) {
  WickedPostfixWriter writer;
  writer.PushVec({1.0f, 2.0f});
  auto buffer = writer.Write();

  std::vector<uint8_t> unaligned_buffer(buffer.size() + 1);
  unaligned_buffer[0] = 0xAA;
  memcpy(unaligned_buffer.data() + 1, buffer.data(), buffer.size());

  const uint8_t* unaligned_data = unaligned_buffer.data() + 1;
  const uint8_t* prog_data = nullptr;
  uint16_t prog_size = 0;

  EXPECT_TRUE(::WickedPostfixRead(unaligned_data, buffer.size(), &prog_data, &prog_size));
  EXPECT_EQ(prog_data, unaligned_data + sizeof(WickedPostfixHeader_t));
  EXPECT_GT(prog_size, 0);

  const uint8_t* unsafe_prog_data = nullptr;
  uint16_t unsafe_prog_size = 0;
  WickedPostfixReadUnsafe(unaligned_data, &unsafe_prog_data, &unsafe_prog_size);
  EXPECT_EQ(unsafe_prog_data, prog_data);
  EXPECT_EQ(unsafe_prog_size, prog_size);
}

TEST(EvalTest, HeaderPacked) {
  EXPECT_EQ(sizeof(WickedPostfixHeader_t), 2);
  EXPECT_EQ(alignof(WickedPostfixHeader_t), 1);
}

TEST(EvalTest, OpToString) {
  EXPECT_STREQ(WickedPostfixOpToString(WickedPostfixOp_Push), "Push");
  EXPECT_STREQ(WickedPostfixOpToString(WickedPostfixOp_Copy), "Copy");
  EXPECT_STREQ(WickedPostfixOpToString(WickedPostfixOp_Move), "Move");
  EXPECT_STREQ(WickedPostfixOpToString(WickedPostfixOp_PushVec), "PushVec");
  EXPECT_STREQ(WickedPostfixOpToString(WickedPostfixOp_DotVec), "DotVec");
  EXPECT_STREQ(WickedPostfixOpToString(WickedPostfixOp_Ipow), "Ipow");
  EXPECT_STREQ(WickedPostfixOpToString(WickedPostfixOp_Poly), "Poly");
  EXPECT_STREQ(WickedPostfixOpToString(WickedPostfixOp_CosSin), "CosSin");
  EXPECT_STREQ(WickedPostfixOpToString(WickedPostfixOp_PanTilt), "PanTilt");
}

TEST(EvalTest, Predicates) {
  // Binary scalar: 13..18 (Add..Atan2)
  EXPECT_TRUE(WickedPostfixOpIsBinaryScalar(WickedPostfixOp_Add));
  EXPECT_TRUE(WickedPostfixOpIsBinaryScalar(WickedPostfixOp_Pow));
  EXPECT_TRUE(WickedPostfixOpIsBinaryScalar(WickedPostfixOp_Atan2));
  EXPECT_FALSE(WickedPostfixOpIsBinaryScalar(WickedPostfixOp_Move));
  EXPECT_FALSE(WickedPostfixOpIsBinaryScalar(WickedPostfixOp_Neg));

  // Unary scalar: 19..29 (Neg..Acos)
  EXPECT_TRUE(WickedPostfixOpIsUnaryScalar(WickedPostfixOp_Neg));
  EXPECT_TRUE(WickedPostfixOpIsUnaryScalar(WickedPostfixOp_Sqrt));
  EXPECT_TRUE(WickedPostfixOpIsUnaryScalar(WickedPostfixOp_Acos));
  EXPECT_FALSE(WickedPostfixOpIsUnaryScalar(WickedPostfixOp_Atan2));
  EXPECT_FALSE(WickedPostfixOpIsUnaryScalar(WickedPostfixOp_Ipow));

  // Scalar: 13..31 (Add..MulAdd)
  EXPECT_TRUE(WickedPostfixOpIsScalar(WickedPostfixOp_Add));
  EXPECT_TRUE(WickedPostfixOpIsScalar(WickedPostfixOp_Ipow));
  EXPECT_TRUE(WickedPostfixOpIsScalar(WickedPostfixOp_MulAdd));
  EXPECT_FALSE(WickedPostfixOpIsScalar(WickedPostfixOp_Copy));
  EXPECT_FALSE(WickedPostfixOpIsScalar(WickedPostfixOp_AddVec));

  // Binary vector: 33..36 (AddVec..DivVec)
  EXPECT_TRUE(WickedPostfixOpIsBinaryVec(WickedPostfixOp_AddVec));
  EXPECT_TRUE(WickedPostfixOpIsBinaryVec(WickedPostfixOp_DivVec));
  EXPECT_FALSE(WickedPostfixOpIsBinaryVec(WickedPostfixOp_MulAdd));
  EXPECT_FALSE(WickedPostfixOpIsBinaryVec(WickedPostfixOp_DotVec));
  EXPECT_FALSE(WickedPostfixOpIsBinaryVec(WickedPostfixOp_NegVec));

  // Unary vector: 37..38 (NegVec..InvVec)
  EXPECT_TRUE(WickedPostfixOpIsUnaryVec(WickedPostfixOp_NegVec));
  EXPECT_TRUE(WickedPostfixOpIsUnaryVec(WickedPostfixOp_InvVec));
  EXPECT_FALSE(WickedPostfixOpIsUnaryVec(WickedPostfixOp_DivVec));
  EXPECT_FALSE(WickedPostfixOpIsUnaryVec(WickedPostfixOp_DotVec));
  EXPECT_FALSE(WickedPostfixOpIsUnaryVec(WickedPostfixOp_NormSqVec));

  // Vector reduction: 40..41 (NormSqVec..SumVec)
  EXPECT_TRUE(WickedPostfixOpIsVectorReduction(WickedPostfixOp_NormSqVec));
  EXPECT_TRUE(WickedPostfixOpIsVectorReduction(WickedPostfixOp_SumVec));
  EXPECT_FALSE(WickedPostfixOpIsVectorReduction(WickedPostfixOp_InvVec));
  EXPECT_FALSE(WickedPostfixOpIsVectorReduction(WickedPostfixOp_DotVec));
  EXPECT_FALSE(WickedPostfixOpIsVectorReduction(WickedPostfixOp_MulAddVec));

  // Vector: 32..44 (PushVec..LerpVec)
  EXPECT_TRUE(WickedPostfixOpIsVector(WickedPostfixOp_PushVec));
  EXPECT_TRUE(WickedPostfixOpIsVector(WickedPostfixOp_AddVec));
  EXPECT_TRUE(WickedPostfixOpIsVector(WickedPostfixOp_DotVec));
  EXPECT_TRUE(WickedPostfixOpIsVector(WickedPostfixOp_LerpVec));
  EXPECT_FALSE(WickedPostfixOpIsVector(WickedPostfixOp_MulAdd));
  EXPECT_FALSE(WickedPostfixOpIsVector(WickedPostfixOp_MulMat));

  // Size operand: 32..44 (PushVec..LerpVec)
  EXPECT_TRUE(WickedPostfixOpHasSizeOperand(WickedPostfixOp_PushVec));
  EXPECT_TRUE(WickedPostfixOpHasSizeOperand(WickedPostfixOp_AddVec));
  EXPECT_TRUE(WickedPostfixOpHasSizeOperand(WickedPostfixOp_DotVec));
  EXPECT_TRUE(WickedPostfixOpHasSizeOperand(WickedPostfixOp_LerpVec));
  EXPECT_FALSE(WickedPostfixOpHasSizeOperand(WickedPostfixOp_MulMat));
}

TEST(EvalTest, VectorEncoding) {
  WickedPostfixWriter writer;
  writer.AddVec(1);
  writer.SubVec(3);
  writer.MulVec(255);

  auto prog = writer.prog_data();
  EXPECT_EQ(writer.prog_size(), 6);
  // AddVec(1) -> [Op_AddVec, 0]
  EXPECT_EQ(prog[0], WickedPostfixOp_AddVec);
  EXPECT_EQ(prog[1], 0);
  // SubVec(3) -> [Op_SubVec, 2]
  EXPECT_EQ(prog[2], WickedPostfixOp_SubVec);
  EXPECT_EQ(prog[3], 2);
  // MulVec(255) -> [Op_MulVec, 254]
  EXPECT_EQ(prog[4], WickedPostfixOp_MulVec);
  EXPECT_EQ(prog[5], 254);
}

}  // namespace