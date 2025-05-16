//
// Created by stefan on 2/14/25.
//

#include <catch2/catch_test_macros.hpp>
#include <alu/ArithmeticUnit.hpp>
#include "Utils.hpp"

namespace {
using ccpu::ArithmeticUnit;
using OpType = ArithmeticUnit::OpType;
using enum OpType;

template <uint32_t in1, uint32_t in2, OpType op>
constexpr auto getAlu() {
  ArithmeticUnit alu;
  alu.loadIn1(in1);
  alu.loadIn2(in2);
  alu.compute(op);
  return alu;
}
} // namespace

TEST_CASE("ALU should perform move") {
  CONSTEXPR auto alu1 = getAlu<32, 21, Move>();
  ASSERT(alu1.res() == 21);

  CONSTEXPR auto alu2 = getAlu<32, static_cast<uint32_t>(-43), Move>();
  ASSERT(alu2.res() == -43);
}

TEST_CASE("ALU should perform addition") {
  CONSTEXPR auto alu1 = getAlu<32, 21, Addition>();
  ASSERT(alu1.res() == 53);

  CONSTEXPR auto alu2 = getAlu<static_cast<uint32_t>(-3), 12, Addition>();
  ASSERT(alu2.res() == 9);

  CONSTEXPR auto alu3 = getAlu<9, static_cast<uint32_t>(-12), Addition>();
  ASSERT(alu3.res() == -3);

  CONSTEXPR auto alu4 = getAlu<static_cast<uint32_t>(-2), static_cast<uint32_t>(-12), Addition>();
  ASSERT(alu4.res() == -14);
}

TEST_CASE("ALU should perform subtraction") {
  CONSTEXPR auto alu1 = getAlu<32, 21, Subtraction>();
  ASSERT(alu1.res() == 11);

  CONSTEXPR auto alu2 = getAlu<9, 21, Subtraction>();
  ASSERT(alu2.res() == -12);

  CONSTEXPR auto alu3 = getAlu<static_cast<uint32_t>(-9), 8, Subtraction>();
  ASSERT(alu3.res() == -17);

  CONSTEXPR auto alu4 = getAlu<6, static_cast<uint32_t>(-9), Subtraction>();
  ASSERT(alu4.res() == 15);

  CONSTEXPR auto alu5 = getAlu<static_cast<uint32_t>(-4), static_cast<uint32_t>(-2), Subtraction>();
  ASSERT(alu5.res() == -2);
}

TEST_CASE("ALU should set carry flag") {
  CONSTEXPR auto alu1 = getAlu<0xFFFFFFFF, 1, Addition>();
  ASSERT(alu1.flags().carry());

  CONSTEXPR auto alu2 = getAlu<0xFFFFFFFE, 1, Addition>();
  ASSERT(!alu2.flags().carry());

  CONSTEXPR auto alu3 = getAlu<0, 1, Subtraction>();
  ASSERT(alu3.flags().carry());

  CONSTEXPR auto alu4 = getAlu<1, 0, Subtraction>();
  ASSERT(!alu4.flags().carry());
}

TEST_CASE("ALU should set sign flag") {
  CONSTEXPR auto alu1 = getAlu<0x0FFFFFFF, 0xF0000000, Addition>();
  ASSERT(alu1.flags().sign());

  CONSTEXPR auto alu2 = getAlu<0x0FFFFFFF, 0x70000000, Addition>();
  ASSERT(!alu2.flags().sign());

  CONSTEXPR auto alu3 = getAlu<0, 1, Subtraction>();
  ASSERT(alu3.flags().sign());

  CONSTEXPR auto alu4 = getAlu<static_cast<uint32_t>(-3), static_cast<uint32_t>(-4), Subtraction>();
  ASSERT(!alu4.flags().sign());
}

TEST_CASE("ALU should set zero flag") {
  CONSTEXPR auto alu1 = getAlu<9, static_cast<uint32_t>(-9), Addition>();
  ASSERT(alu1.flags().zero());

  CONSTEXPR auto alu2 = getAlu<9, 8, Addition>();
  ASSERT(!alu2.flags().zero());

  CONSTEXPR auto alu3 = getAlu<12, 12, Subtraction>();
  ASSERT(alu3.flags().zero());

  CONSTEXPR auto alu4 = getAlu<13, static_cast<uint32_t>(-12), Subtraction>();
  ASSERT(!alu4.flags().zero());

  CONSTEXPR auto alu5 = getAlu<0xFFFFFFFF, 1, Addition>();
  ASSERT(alu5.flags().zero());
}
