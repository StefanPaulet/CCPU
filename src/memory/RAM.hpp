//
// Created by stefan on 2/16/25.
//

#pragma once

#include <array>
#include <expected>
#include <span>
#include <utility>

namespace ccpu {
using std::array;
using std::expected;
using std::remove_reference_t;
using std::span;

using std::declval;
using std::unexpected;
using std::forward_like;

template <uint8_t typeSize> struct SizedType;
template <> struct SizedType<1> { using Type = uint8_t; };
template <> struct SizedType<2> { using Type = uint16_t; };
template <> struct SizedType<4> { using Type = uint32_t; };

template <uint8_t count>
concept IsAddressableSize = requires { count == 1 || count == 2 || count == 4; };

enum struct MemoryAccessViolation {
  InvalidRead, InvalidWrite
};

template <uint32_t size> class RandomAccessMemory {
public:
  constexpr RandomAccessMemory() = default;
  constexpr RandomAccessMemory(RandomAccessMemory const&) = default;
  constexpr RandomAccessMemory(RandomAccessMemory&&) noexcept = default;
  explicit constexpr RandomAccessMemory(array<uint8_t, size> data) : _data {data} {}

  [[nodiscard]] constexpr auto readByte(uint32_t const address) const noexcept { return read<1>(address); }
  [[nodiscard]] constexpr auto readWord(uint32_t const address) const noexcept { return read<2>(address); }
  [[nodiscard]] constexpr auto readDWord(uint32_t const address) const noexcept { return read<4>(address); }

  constexpr auto writeByte(uint32_t const address, uint8_t value) noexcept { return write<1>(address, value); }
  constexpr auto writeWord(uint32_t const address, uint16_t value) noexcept { return write<2>(address, value); }
  constexpr auto writeDWord(uint32_t const address, uint32_t value) noexcept { return write<4>(address, value); }

private:
  template <uint8_t count> requires IsAddressableSize<count>
  [[nodiscard]] constexpr auto read(uint32_t const address) const noexcept -> expected<typename SizedType<count>::Type, MemoryAccessViolation> {
    auto data = this->getOffsetOfAddress<count>(address);
    if (data.empty()) {
      return unexpected(MemoryAccessViolation::InvalidRead);
    }

    typename SizedType<count>::Type result{0};
    for (auto byte = data.rbegin(); byte != data.rend(); ++byte) {
      result = result << 8 | *byte;
    }
    return result;
  }

  template <uint8_t count> requires IsAddressableSize<count>
  constexpr auto write(uint32_t const address, typename SizedType<count>::Type value) noexcept -> expected<void, MemoryAccessViolation> {
    auto data = this->getOffsetOfAddress<count>(address);
    if (data.empty()) {
      return unexpected(MemoryAccessViolation::InvalidWrite);
    }

    for (auto& byte : data) {
      byte = value & 0xFF;
      if constexpr (sizeof(value) >= 1) {
        value = value >> 8;
      }
    }
    return {};
  }

  template <uint8_t count, typename Self> requires IsAddressableSize<count>
  [[nodiscard]] constexpr auto getOffsetOfAddress(this Self&& self, uint32_t const address) noexcept
      -> span<remove_reference_t<decltype(forward_like<decltype(self)>(declval<uint8_t>()))>> {
    if (address + count > size) {
      return {self._data.end(), 0};
    }
    return {&self._data[address], count};
  }

  array<uint8_t, size> _data{};
};

} // namespace ccpu
