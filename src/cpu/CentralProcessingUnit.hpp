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

  constexpr auto run() noexcept -> expected<void, FaultyInstruction> {
    setup();
    while (hasOperations()) {
      auto returnVal = process(nextOperation());
      if (!returnVal) {
        return returnVal;
      }
    }
    return {};
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
    LR().storeDWord(FinalInstruction);
    SP().storeDWord(RamSize - 4);
  }

  template <typename T>
  [[nodiscard]] constexpr auto handleFaultyMemoryReturn(expected<T, MemoryAccessViolation> value)
    -> expected<void, FaultyInstruction> {
    using enum MemoryAccessViolation;
    using enum FaultyInstruction;
    if (!value) {
      switch (value.error()) {
        case InvalidWrite: unexpected<FaultyInstruction>{InvalidMemoryWrite};
        case InvalidRead: unexpected<FaultyInstruction>{InvalidMemoryRead};
      }
    }
    return {};
  }

  [[nodiscard]] constexpr auto process(Instruction instruction) -> expected<void, FaultyInstruction> {
    switch (instruction.type()) {
      using enum InstructionType;
      case MemoryTransfer: {
        return process(MemoryTransferInstruction(instruction));
      }
      default: assert(false && "Unimplemented instruction type");
    }
  }

  [[nodiscard]] constexpr auto process(MemoryTransferInstruction instruction) -> expected<void, FaultyInstruction> {
    if (instruction.store()) {
      auto valueToStore = _registers[instruction.source()].loadDWord();
      auto memoryAddress = _registers[instruction.base()].loadDWord() +
          (instruction.subtract() ? (-1) : 1) * computeOffset(instruction, instruction.immediate());
      switch (instruction.transferSize()) {
        using enum MemoryTransferInstruction::TransferSize;
        case Byte: { return handleFaultyMemoryReturn(_ram.writeByte(memoryAddress, valueToStore)); }
        case Word: { return handleFaultyMemoryReturn(_ram.writeWord(memoryAddress, valueToStore)); }
        case DWord: { return handleFaultyMemoryReturn(_ram.writeDWord(memoryAddress, valueToStore)); }
        default: { assert(false && "Unhandled memory transfer size"); }
      }
    } else {

      auto performStore = [this, &instruction](
          auto readInstruction, auto storeInstruction
        ) -> expected<void, FaultyInstruction> {
        auto memoryAddress = _registers[instruction.base()].loadDWord() +
          (instruction.subtract() ? (-1) : 1) * computeOffset(instruction, instruction.immediate());
        auto& targetRegister = _registers[instruction.source()];

        auto value = (_ram.*readInstruction)(memoryAddress);
        if (!value) {
          return handleFaultyMemoryReturn(value);
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

  [[nodiscard]] constexpr auto process(AluInstruction instruction) -> expected<void, FaultyInstruction> {
    auto lhs = _registers[instruction.source()].loadDWord();
    auto& destination = _registers[instruction.source()];
    auto rhs = computeOffset(instruction, instruction.immediate());

    _alu.loadIn1(lhs);
    _alu.loadIn2(rhs);
    switch (instruction.opcode()) {
      using enum AluInstruction::OpCode;
      using enum ArithmeticUnit::OpType;
      case ADD: { _alu.compute(Addition); break; }
      case SUB: { _alu.compute(Subtraction); break; }
      default: { assert(false && "Unimplemented ALU instruction"); }
    }
    auto result = _alu.res();
    destination.storeDWord(result);

    auto aluFlags = _alu.flags();
    _cpsr.nFlag(aluFlags.sign());
    _cpsr.zFlag(aluFlags.zero());
    _cpsr.cFlag(aluFlags.carry());
    _cpsr.vFlag([](uint32_t lhs, uint32_t rhs, uint32_t result) -> bool {
      auto lhsSign = lhs & (1u << 31);
      auto rhsSign = lhs & (1u << 31);
      auto resSign = lhs & (1u << 31);
      return (lhsSign == rhsSign) && (resSign != lhsSign);
    }(lhs, rhs, result));

    return {};
  }

  [[nodiscard]] constexpr auto process(BranchInstruction instruction) -> expected<void, FaultyInstruction> {
    if (instruction.link()) {
      LR().storeDWord(PC().loadDWord());
    }
    PC().storeDWord(instruction.offset());
    return {};
  }

  [[nodiscard]] constexpr auto process(StackInstruction instruction) -> expected<void, FaultyInstruction> {
    auto registerList = instruction.registerList();

    auto runStackOperation = [](uint16_t registerList, auto callable) -> expected<void, FaultyInstruction> {
      for (auto idx = 0; registerList; registerList >>= 1, ++idx) {
        if (registerList & 1) {
          if (auto retVal = callable(idx); !retVal) {
            return retVal;
          }
        }
      }
      return {};
    };

    switch (instruction.opcode()) {
      using enum StackInstruction::OpCode;
      case POP: {
        return runStackOperation(registerList, [this](uint8_t idx) -> expected<void, FaultyInstruction> {
          auto currentSP = SP().loadDWord();
          auto memoryVal = _ram.readDWord(currentSP);
          if (!memoryVal) {
            return handleFaultyMemoryReturn(memoryVal);
          }
          _registers[idx].storeDWord(memoryVal.value());
          SP().storeDWord(currentSP + 4);
          return {};
        });
      }
      case PUSH: {
        return runStackOperation(registerList, [this](uint8_t idx) -> expected<void, FaultyInstruction> {
          auto value = _registers[idx].loadDWord();
          auto currentSP = SP().loadDWord();
          if (auto retVal = _ram.writeDWord(currentSP, value); !retVal) {
            return handleFaultyMemoryReturn(retVal);
          }
          SP().storeDWord(currentSP - 4);
          return {};
        });
      }
      default: assert(false && "Unimplemented stack operation");
    }
    return {};
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
