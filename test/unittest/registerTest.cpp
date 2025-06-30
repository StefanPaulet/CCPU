//
// Created by stefan on 2/13/25.
//

#include <catch2/catch_test_macros.hpp>
#include <functional>
#include <register/Register.hpp>
#include "Utils.hpp"

namespace {
using ccpu::Register;

template <auto Value>
constexpr auto regWithValue() {
  auto reg = Register{};
  switch (sizeof(Value)) {
    case 1: { reg.storeByte(static_cast<uint8_t>(Value)); break; }
    case 2: { reg.storeWord(static_cast<uint16_t>(Value)); break; }
    case 4: { reg.storeDWord(static_cast<uint32_t>(Value)); break; }
    default: { assert(false && "Should not load a value with a different size than 1, 2 or 4"); }
  }
  return reg;
}

template <typename Update, typename Check> struct CallableContainer {};

template <typename Supplier, typename Callable>
constexpr auto registerWithUpdate(Supplier&& supplier) -> std::tuple<Register, std::invoke_result_t<Callable, Register>> {
  Register reg = std::invoke(std::forward<Supplier>(supplier));
  return {reg, std::invoke(Callable{}, reg)};
}

template <typename... Callables>
constexpr auto registerSequence() {
  std::array<Register, sizeof...(Callables) + 1> arr{};
  std::array<bool, sizeof...(Callables)> results{};
  arr[0] = Register{};
  size_t idx {1};

  auto call = [&arr, &results, &idx]<typename Update, typename Check>(CallableContainer<Update, Check>) {
    arr[idx] = arr[idx - 1];
    Update{}(arr[idx]);
    results[idx - 1] = Check{}(arr[idx]);
    ++idx;
  };

  (call(Callables{}), ...);
  return results;
}

template <std::array array, std::size_t... indices>
constexpr auto validate(std::index_sequence<indices...>) {
  constexpr auto check = []<std::size_t idx> {
    ASSERT(array[idx]);
  };
  (check.template operator()<indices>(), ...);
}

template <typename... Callables>
constexpr auto validateSequence() {
  constexpr auto array = registerSequence<Callables...>();
  validate<array>(std::make_integer_sequence<std::size_t, array.size()>());
}
} // namespace

TEST_CASE("Registers can store and load values") {
  CONSTEXPR auto al = regWithValue<uint8_t{4}>();
  ASSERT(al.loadByte() == 4);

  CONSTEXPR auto ax = regWithValue<uint16_t{5}>();
  ASSERT(ax.loadWord() == 5);

  CONSTEXPR auto eax = regWithValue<uint32_t{6}>();
  ASSERT(eax.loadDWord() == 6);
}

TEST_CASE("Registers can be partially updated") {
  CONSTEXPR auto eax = [] {
    Register eax = regWithValue<uint32_t{0x12000000}>();
    eax.storeWord(0x3400);
    eax.storeByte(0x56);
    return eax;
  }();
  ASSERT(eax.loadByte() == 0x56);
  ASSERT(eax.loadWord() == 0x3456);
  ASSERT(eax.loadDWord() == 0x12003456);
}

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
