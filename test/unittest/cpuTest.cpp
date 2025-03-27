//
// Created by stefan on 3/27/25.
//


#include <catch2/catch_test_macros.hpp>
#include <cpu/CentralProcessingUnit.hpp>

namespace {
using ccpu::Instruction;
using ccpu::Cpu;

using MemoryEntry = std::tuple<uint32_t, uint32_t>;
using RAM =  ccpu::RandomAccessMemory<Cpu::RamSize>;

class InstructionBuilder {
private:
  using enum ccpu::InstructionType;

public:
  constexpr InstructionBuilder() {
    reset();
  }

  [[nodiscard]] constexpr auto& withCondition(ccpu::Condition condition) noexcept {
    _instr.val &= ~(static_cast<uint8_t>(15) << 28);
    _instr.val |= static_cast<uint8_t>(condition) << 28;
    return *this;
  }

  [[nodiscard]] constexpr auto& withType(ccpu::InstructionType instruction) noexcept {
    _instr.val &= ~(static_cast<uint8_t>(15) << 25);
    _instr.val |= static_cast<uint8_t>(instruction) << 25;
    return *this;
  }

  [[nodiscard]] constexpr auto& withOffset(
    ccpu::OffsetBasedInstruction::ShiftType shift, uint8_t amount, uint8_t reg
  ) noexcept {
    assert(_instr.type() == MemoryTransfer || _instr.type() == AluOperation);
    _instr.val &= ~(1 << 24);
    _instr.val |= static_cast<uint8_t>(shift) << 10;
    _instr.val |= static_cast<uint8_t>(amount) << 4;
    _instr.val |= static_cast<uint8_t>(reg);
    return *this;
  }

  [[nodiscard]] constexpr auto& withOffset(uint16_t immediate) noexcept {
    assert(_instr.type() == MemoryTransfer || _instr.type() == AluOperation);
    _instr.val |= 1 << 24;
    assert(immediate < (1 << 13));
    _instr.val |= static_cast<uint8_t>(immediate);
    return *this;
  }

  [[nodiscard]] constexpr auto& withTransferSize(ccpu::MemoryTransferInstruction::TransferSize size) noexcept {
    assert(_instr.type() == MemoryTransfer);
    _instr.val |= static_cast<uint8_t>(size) << 22;
    return *this;
  }
  [[nodiscard]] constexpr auto& withStore(bool store) noexcept {
    assert(_instr.type() == MemoryTransfer);
    _instr.val |= static_cast<uint8_t>(store) << 21;
    return *this;
  }
  [[nodiscard]] constexpr auto& withSubtract(bool subtract) noexcept {
    assert(_instr.type() == MemoryTransfer);
    _instr.val |= static_cast<uint8_t>(subtract) << 20;
    return *this;
  }
  [[nodiscard]] constexpr auto& withSourceAndBase(uint8_t source, uint8_t base) noexcept {
    assert(_instr.type() == MemoryTransfer || _instr.type() == AluOperation);
    _instr.val |= source << 16;
    _instr.val |= base << 12;
    return *this;
  }

  [[nodiscard]] constexpr auto& withOpcode(ccpu::AluInstruction::OpCode opCode) noexcept {
    assert(_instr.type() == AluOperation);
    _instr.val |= static_cast<uint8_t>(opCode) << 20;
    return *this;
  }

  [[nodiscard]] constexpr auto& withLink(bool link) noexcept {
    assert(_instr.type() == Branch);
    _instr.val |= static_cast<uint8_t>(link) << 24;
    return *this;
  }
  [[nodiscard]] constexpr auto& withImmediate(uint32_t offset) noexcept {
    assert(_instr.type() == Branch);
    assert(offset < (1 << 24));
    _instr.val |= offset;
    return *this;
  }

  [[nodiscard]] constexpr auto& withOpcode(ccpu::StackInstruction::OpCode opCode) noexcept {
    assert(_instr.type() == StackOperation);
    _instr.val |= static_cast<uint8_t>(opCode) << 24;
    return *this;
  }
  [[nodiscard]] constexpr auto& withRegisters(std::array<bool, 16> registers) noexcept {
    assert(_instr.type() == StackOperation);
    for (auto idx = 0; idx < registers.size(); ++idx) {
      if (registers[idx]) {
        _instr.val |= (1 << (7 + idx));
      } else {
        _instr.val &= ~(1 << (7 + idx));
      }
    }
    return *this;
  }

  [[nodiscard]] constexpr auto get() noexcept {
    auto instr = _instr;
    reset();
    return instr;
  }

private:
  constexpr auto reset() -> void {
    using enum ccpu::Condition;
    _instr.val = static_cast<uint8_t>(AL) << 28; //set always condition by default
    _instr.val |= 7 << 25; //set invalid type by default
  }
  Instruction _instr;
};
using IB = InstructionBuilder;
using Cond = ccpu::Condition;
using InType = ccpu::InstructionType;
using Shift = ccpu::OffsetBasedInstruction::ShiftType;
using TransSize = ccpu::MemoryTransferInstruction::TransferSize;
using AluCode = ccpu::AluInstruction::OpCode;
using StackCode = ccpu::StackInstruction::OpCode;

class RamFactory {
public:
  [[nodiscard]] constexpr auto produce(std::vector<MemoryEntry> entries, std::vector<Instruction> instructions) {
    for (auto [idx, value] : entries) {
      assert(_ram.writeDWord(idx, value).has_value());
    }
    auto idx = Cpu::CodeSectionBegin;
    for (auto instr : instructions) {
      assert(_ram.writeDWord(idx, instr.val).has_value());
      idx += 4;
    }
    static_assert(IB().withType(InType::Branch).get().type() == InType::Branch);
    assert(_ram.writeDWord(idx, IB().withType(InType::Branch).withImmediate(Cpu::FinalInstruction).get().val).has_value());
    return _ram;
  }
private:
  RAM _ram{};
};

constexpr auto cpuWithRam(RAM ram) -> std::tuple<Cpu, std::expected<void, ccpu::FaultyInstruction>> {
  auto cpu = Cpu{ram};
  auto val = cpu.run();
  return {cpu, val};
}
} // namespace

TEST_CASE("Cpu without instructions should leave all registers in the expected state") {
  constexpr auto ram = RamFactory().produce({}, {});
  constexpr auto cpuWithStatus = cpuWithRam(ram);
  STATIC_CHECK(get<1>(cpuWithStatus).has_value());
  STATIC_CHECK(get<0>(cpuWithStatus).readRegister(15) == Cpu::FinalInstruction);
}