//
// Created by stefan on 2/14/25.
//

#include <catch2/catch_test_macros.hpp>
#include <alu/ArithmeticUnit.hpp>

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

TEST_CASE("ALU should perform addition") {
  constexpr auto alu1 = getAlu<32, 21, Addition>();
  STATIC_CHECK(alu1.res() == 53);

  constexpr auto alu2 = getAlu<static_cast<uint32_t>(-3), 12, Addition>();
  STATIC_CHECK(alu2.res() == 9);

  constexpr auto alu3 = getAlu<9, static_cast<uint32_t>(-12), Addition>();
  STATIC_CHECK(alu3.res() == -3);

  constexpr auto alu4 = getAlu<static_cast<uint32_t>(-2), static_cast<uint32_t>(-12), Addition>();
  STATIC_CHECK(alu4.res() == -14);
}

TEST_CASE("ALU should perform subtraction") {
  constexpr auto alu1 = getAlu<32, 21, Subtraction>();
  STATIC_CHECK(alu1.res() == 11);

  constexpr auto alu2 = getAlu<9, 21, Subtraction>();
  STATIC_CHECK(alu2.res() == -12);

  constexpr auto alu3 = getAlu<static_cast<uint32_t>(-9), 8, Subtraction>();
  STATIC_CHECK(alu3.res() == -17);

  constexpr auto alu4 = getAlu<6, static_cast<uint32_t>(-9), Subtraction>();
  STATIC_CHECK(alu4.res() == 15);

  constexpr auto alu5 = getAlu<static_cast<uint32_t>(-4), static_cast<uint32_t>(-2), Subtraction>();
  STATIC_CHECK(alu5.res() == -2);
}

TEST_CASE("ALU should set carry flag") {
  constexpr auto alu1 = getAlu<0xFFFFFFFF, 1, Addition>();
  STATIC_CHECK(alu1.flags().carry());

  constexpr auto alu2 = getAlu<0xFFFFFFFE, 1, Addition>();
  STATIC_CHECK(!alu2.flags().carry());

  constexpr auto alu3 = getAlu<0, 1, Subtraction>();
  STATIC_CHECK(alu3.flags().carry());

  constexpr auto alu4 = getAlu<1, 0, Subtraction>();
  STATIC_CHECK(!alu4.flags().carry());
}

TEST_CASE("ALU should set sign flag") {
  constexpr auto alu1 = getAlu<0x0FFFFFFF, 0xF0000000, Addition>();
  STATIC_CHECK(alu1.flags().sign());

  constexpr auto alu2 = getAlu<0x0FFFFFFF, 0x70000000, Addition>();
  STATIC_CHECK(!alu2.flags().sign());

  constexpr auto alu3 = getAlu<0, 1, Subtraction>();
  STATIC_CHECK(alu3.flags().sign());

  constexpr auto alu4 = getAlu<static_cast<uint32_t>(-3), static_cast<uint32_t>(-4), Subtraction>();
  STATIC_CHECK(!alu4.flags().sign());
}

TEST_CASE("ALU should set zero flag") {
  constexpr auto alu1 = getAlu<9, static_cast<uint32_t>(-9), Addition>();
  STATIC_CHECK(alu1.flags().zero());

  constexpr auto alu2 = getAlu<9, 8, Addition>();
  STATIC_CHECK(!alu2.flags().zero());

  constexpr auto alu3 = getAlu<12, 12, Subtraction>();
  STATIC_CHECK(alu3.flags().zero());

  constexpr auto alu4 = getAlu<13, static_cast<uint32_t>(-12), Subtraction>();
  STATIC_CHECK(!alu4.flags().zero());

  constexpr auto alu5 = getAlu<0xFFFFFFFF, 1, Addition>();
  STATIC_CHECK(alu5.flags().zero());
}
