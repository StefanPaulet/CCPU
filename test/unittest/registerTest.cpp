//
// Created by stefan on 2/13/25.
//

#include <catch2/catch_test_macros.hpp>
#include <register/Register.hpp>

namespace {
using ccpu::Register;

template <auto Value>
constexpr auto regWithValue() {
  auto reg = Register{};
  switch (sizeof(Value)) {
    case 1: { reg.storeByte(Value); break; }
    case 2: { reg.storeWord(Value); break; }
    case 4: { reg.storeDWord(Value); break; }
    default: { assert(false && "Should not load a value with a different size than 1, 2 or 4"); }
  }
  return reg;
}

template <typename Update, typename Check> struct CallableContainer {};

template <typename... Callables>
constexpr auto validateSequence() {
  Register reg {};
  auto call = [&reg]<typename Update, typename Check>(CallableContainer<Update, Check>) {
    Update{}(reg);
    CHECK(Check{}(reg));
  };
  (call(Callables{}), ...);
}
} // namespace

TEST_CASE("Registers can store and load values") {
  constexpr Register al = regWithValue<uint8_t{4}>();
  STATIC_CHECK(al.loadByte() == 4);

  constexpr Register ax = regWithValue<uint16_t{5}>();
  STATIC_CHECK(ax.loadWord() == 5);

  constexpr Register eax = regWithValue<uint32_t{6}>();
  STATIC_CHECK(eax.loadDWord() == 6);
}

TEST_CASE("Registers can be partially updated") {
  constexpr auto eax = [] {
    Register eax = regWithValue<uint32_t{0x12000000}>();
    eax.storeWord(0x3400);
    eax.storeByte(0x56);
    return eax;
  }();
  STATIC_CHECK(eax.loadByte() == 0x56);
  STATIC_CHECK(eax.loadWord() == 0x3456);
  STATIC_CHECK(eax.loadDWord() == 0x12003456);
}

//TODO can this test be made constexpr?
TEST_CASE("Registers maintain intermediate states") {
  validateSequence<
    CallableContainer<
      decltype([](Register& reg) constexpr{ reg.storeDWord(0x12000000); }),
      decltype([](Register const& reg) constexpr { return reg.loadDWord() == 0x12000000; })
      >,
    CallableContainer<
      decltype([](Register& reg) constexpr{ reg.storeWord(0x3400); }),
      decltype([](Register const& reg) constexpr { return reg.loadDWord() == 0x12003400; })
      >,
    CallableContainer<
      decltype([](Register& reg) constexpr{ reg.storeByte(0x56); }),
      decltype([](Register const& reg) constexpr { return reg.loadDWord() == 0x12003456; })
      >
  >();
}
