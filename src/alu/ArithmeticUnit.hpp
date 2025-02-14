//
// Created by stefan on 2/14/25.
//

#pragma once

#include <bitset>
#include <cassert>
#include <cstdint>

namespace ccpu {
using std::bitset;

class ArithmeticUnit {
private:
  class OpFlags {
  public:
    [[nodiscard]] constexpr auto carry() const noexcept -> bool { return flags.test(static_cast<uint8_t>(Flags::Carry)); }
    [[nodiscard]] constexpr auto sign() const noexcept -> bool { return flags.test(static_cast<uint8_t>(Flags::Sign)); }
    [[nodiscard]] constexpr auto zero() const noexcept -> bool { return flags.test(static_cast<uint8_t>(Flags::Zero)); }

  private:
    friend class ArithmeticUnit;

    enum struct Flags : uint8_t {
      Carry = 0b0000,
      Sign  = 0b0001,
      Zero  = 0b0010
    };

    bitset<3> flags{};
  };

public:
  enum struct OpType {
    Addition, Subtraction
  };

  constexpr ArithmeticUnit() = default;
  constexpr ArithmeticUnit(ArithmeticUnit const&) = default;
  constexpr ArithmeticUnit(ArithmeticUnit&&) noexcept = default;

  constexpr auto compute(OpType opType) noexcept {
    switch (opType) {
      using enum OpType;
      case Addition: {
        add();
        break;
      }
      case Subtraction: {
        sub();
        break;
      }
      default: {
        assert(false && "Unhandled ALU operation type");
      }
    }

    {
      using enum OpFlags::Flags;
      _flags.flags.set(static_cast<uint8_t>(Carry), _res & carryBit);
      _flags.flags.set(static_cast<uint8_t>(Sign), _res & signBit);
      _flags.flags.set(static_cast<uint8_t>(Zero), (_res & maxVal) == 0);
    }
  }
  constexpr auto loadIn1(uint32_t val) noexcept { _in1 = val; }
  constexpr auto loadIn2(uint32_t val) noexcept { _in2 = val; }
  [[nodiscard]] constexpr auto res() const noexcept -> uint32_t { return _res & maxVal; }
  [[nodiscard]] constexpr auto flags() const noexcept -> OpFlags { return _flags; }

private:
  constexpr auto add() noexcept -> void { _res = static_cast<uint64_t>(_in1) + _in2; }
  constexpr auto sub() noexcept -> void { _res = static_cast<uint64_t>(_in1) - _in2; }

  static constexpr uint32_t maxVal = 0xFFFFFFFF;
  static constexpr uint32_t signBit = (1u << 31);
  static constexpr uint64_t carryBit = (1ul << 32);

  uint32_t _in1{};
  uint32_t _in2{};
  uint64_t _res{};
  OpFlags _flags{};
};
} // namespace ccpu
