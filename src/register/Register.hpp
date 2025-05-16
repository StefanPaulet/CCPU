//
// Created by stefan on 2/13/25.
//

#pragma once

namespace ccpu {
class Register {
public:
  constexpr Register() = default;
  constexpr Register(Register const&) = default;
  constexpr Register(Register&&) noexcept = default;

  constexpr auto operator=(Register const&) -> Register& = default;
  constexpr auto operator=(Register&&) noexcept -> Register& = default;

  [[nodiscard]] constexpr auto loadByte() const noexcept -> uint8_t { return _data; }
  [[nodiscard]] constexpr auto loadWord() const noexcept -> uint16_t { return _data; }
  [[nodiscard]] constexpr auto loadDWord() const noexcept -> uint32_t { return _data; }

  constexpr auto storeByte(uint8_t value) noexcept -> void { _data = _data | (0xFF & value); }
  constexpr auto storeWord(uint16_t value) noexcept -> void { _data = _data | (0xFFFF & value); }
  constexpr auto storeDWord(uint32_t value) noexcept -> void { _data = value; }

private:

  uint32_t _data {};
};
} // namespace ccpu
