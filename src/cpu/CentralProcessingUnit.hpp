//
// Created by stefan on 2/25/25.
//

#pragma once

#include <array>
#include "../alu/ArithmeticUnit.hpp"
#include "../instruction/Instruction.hpp"
#include "../memory/RAM.hpp"
#include "../register/Register.hpp"

namespace ccpu {
using std::array;
using std::expected;

enum struct FaultyInstruction {
  InvalidMemoryRead, InvalidMemoryWrite
};

class Cpu {
private:
  static constexpr auto RamSize = 4 * 1024 * 1024;
  static constexpr auto FinalInstruction = RamSize - 1;
  static constexpr auto CodeSectionBegin = 4;
  using RAM = RandomAccessMemory<RamSize>;

public:
  constexpr Cpu() = default;
  constexpr Cpu(Cpu const&) = default;
  constexpr Cpu(Cpu&&) noexcept = default;

  explicit constexpr Cpu(array<uint8_t, RamSize> ram) : _ram {ram} {}

  constexpr auto run() noexcept -> void {
    setup();
    while (hasOperations()) {
      [[maybe_unused]] auto returnVal = process(nextOperation());
    }
  }

private:
  class CPSR {
  public:
    [[nodiscard]] constexpr auto nFlag() const noexcept -> bool { return _val & (1 << 31); }
    [[nodiscard]] constexpr auto zFlag() const noexcept -> bool { return _val & (1 << 30); }
    [[nodiscard]] constexpr auto cFlag() const noexcept -> bool { return _val & (1 << 29); }
    [[nodiscard]] constexpr auto vFlag() const noexcept -> bool { return _val & (1 << 28); }

    constexpr auto nFlag(bool set) noexcept -> void { setOrClearFlag(31, set); }
    constexpr auto zFlag(bool set) noexcept -> void { setOrClearFlag(30, set); }
    constexpr auto cFlag(bool set) noexcept -> void { setOrClearFlag(29, set); }
    constexpr auto vFlag(bool set) noexcept -> void { setOrClearFlag(28, set); }

  private:
    constexpr auto setOrClearFlag(uint8_t position, bool set) noexcept -> void {
      if (set) {
        _val |= (1 << position);
      } else {
        _val &= ~(1 << position);
      }
    }
    uint32_t _val;
  };

  constexpr auto setup() -> void {
    PC().storeDWord(CodeSectionBegin);
    SP().storeDWord(FinalInstruction);
    LR().storeDWord(RamSize - 4);
  }

  constexpr auto process(Instruction instruction) -> expected<void, FaultyInstruction> {
    switch (instruction.type()) {
      using enum InstructionType;
      case MemoryTransfer: {
        return process(MemoryTransferInstruction(instruction));
      }
      default: assert(false && "Unimplemented instruction type");
    }
  }

  constexpr auto process(MemoryTransferInstruction instruction) -> expected<void, FaultyInstruction> {
    constexpr auto handleMemoryOpReturn =
      []<typename T>(expected<T, MemoryAccessViolation> retVal)  -> expected<void, FaultyInstruction> {
      using enum MemoryAccessViolation;
      using enum FaultyInstruction;
      if (!retVal) {
        switch (retVal.error()) {
          case InvalidWrite: unexpected<FaultyInstruction>{InvalidMemoryWrite};
          case InvalidRead: unexpected<FaultyInstruction>{InvalidMemoryRead};
        }
      }
      return {};
    };

    if (instruction.store()) {
      auto valueToStore = _registers[instruction.source()].loadDWord();
      auto memoryAddress = _registers[instruction.base()].loadDWord() +
          (instruction.subtract() ? (-1) : 1) * computeOffset(instruction, instruction.immediate());
      switch (instruction.transferSize()) {
        using enum MemoryTransferInstruction::TransferSize;
        case Byte: { return handleMemoryOpReturn(_ram.writeByte(memoryAddress, valueToStore)); }
        case Word: { return handleMemoryOpReturn(_ram.writeWord(memoryAddress, valueToStore)); }
        case DWord: { return handleMemoryOpReturn(_ram.writeDWord(memoryAddress, valueToStore)); }
        default: { assert(false && "Unhandled memory transfer size"); }
      }
    } else {

      auto performStore = [this, &instruction, &handleMemoryOpReturn](
          auto readInstruction, auto storeInstruction
        ) -> expected<void, FaultyInstruction> {
        auto memoryAddress = _registers[instruction.base()].loadDWord() +
          (instruction.subtract() ? (-1) : 1) * computeOffset(instruction, instruction.immediate());
        auto& targetRegister = _registers[instruction.source()];

        auto value = (_ram.*readInstruction)(memoryAddress);
        if (!value) {
          return handleMemoryOpReturn(value);
        }
        (targetRegister.*storeInstruction)(value.value());
        return {};
      };

      switch (instruction.transferSize()) {
        using enum MemoryTransferInstruction::TransferSize;
        case Byte: { return performStore(&RAM::readByte, &Register::storeByte); }
        case Word: { return performStore(&RAM::readWord, &Register::storeWord); }
        case DWord: { return performStore(&RAM::readDWord, &Register::storeDWord); }
      }
    }
  }

  [[nodiscard]] constexpr auto computeOffset(OffsetBasedInstruction instruction, bool immediate) -> uint32_t {
    if (immediate) {
      return instruction.offset();
    }
    auto baseRegisterValue = _registers[instruction.offsetRegister()].loadDWord();
    switch (instruction.shiftType()) {
      using enum OffsetBasedInstruction::ShiftType;
      case LogicalLeft: { return baseRegisterValue << instruction.shiftAmount(); }
      case LogicalRight: { return baseRegisterValue >> instruction.shiftAmount(); }
      case ArithmeticLeft: { return static_cast<int32_t>(baseRegisterValue) >> instruction.shiftAmount(); }
      case ArithmeticRight: { return static_cast<int32_t>(baseRegisterValue) >> instruction.shiftAmount(); }
    }
  }

  [[nodiscard]] constexpr auto hasOperations() const noexcept -> bool { return PC().loadDWord() != FinalInstruction; }
  [[nodiscard]] constexpr auto nextOperation() noexcept -> Instruction {
    auto currentPC = PC().loadDWord();
    auto instruction = Instruction{currentPC - 4};
    PC().storeDWord(currentPC + 4);
    return instruction;
  }

  [[nodiscard]] constexpr auto SP() -> Register& { return _registers[13]; }
  [[nodiscard]] constexpr auto SP() const  -> Register const& { return _registers[13]; }

  [[nodiscard]] constexpr auto LR() -> Register& { return _registers[14]; }
  [[nodiscard]] constexpr auto LR() const -> Register const& { return _registers[14]; }

  [[nodiscard]] constexpr auto PC() -> Register& { return _registers[15]; }
  [[nodiscard]] constexpr auto PC() const -> Register const& { return _registers[15]; }

  /*
   * 16 registers:
   *  -reg15 = program counter
   *  -reg14 = link register (return address)
   *  -reg13 = stack pointer
   * 1 arithmetic logical unit
   * 1 main memory
   * 1 CPSR register
   */
  array<Register, 16> _registers {};
  ArithmeticUnit _alu {};
  RAM _ram {};
  [[maybe_unused]] CPSR _cpsr {};
};
} // namespace ccpu
