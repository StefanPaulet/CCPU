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
  [[nodiscard]] constexpr auto& withStore() noexcept {
    assert(_instr.type() == MemoryTransfer);
    _instr.val |= 1 << 21;
    return *this;
  }
  [[nodiscard]] constexpr auto& withSubtract(bool subtract) noexcept {
    assert(_instr.type() == MemoryTransfer);
    _instr.val |= static_cast<uint8_t>(subtract) << 20;
    return *this;
  }
  [[nodiscard]] constexpr auto& withSourceAndBase(uint8_t source, uint8_t base) noexcept {
    assert(_instr.type() == MemoryTransfer);
    _instr.val |= source << 16;
    _instr.val |= base << 12;
    return *this;
  }
  [[nodiscard]] constexpr auto& withSourceAndDestination(uint8_t source, uint8_t base) noexcept {
    assert(_instr.type() == AluOperation);
    _instr.val |= source << 16;
    _instr.val |= base << 12;
    return *this;
  }

  [[nodiscard]] constexpr auto& withOpcode(ccpu::AluInstruction::OpCode opCode) noexcept {
    assert(_instr.type() == AluOperation);
    _instr.val |= static_cast<uint8_t>(opCode) << 20;
    return *this;
  }

  [[nodiscard]] constexpr auto& withLink() noexcept {
    assert(_instr.type() == Branch);
    _instr.val |= 1 << 24;
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
        _instr.val |= (1 << (8 + idx));
      } else {
        _instr.val &= ~(1 << (8 + idx));
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
    assert(_ram.writeDWord(idx, IB().withType(InType::Branch).withImmediate(Cpu::FinalInstruction - 4).get().val).has_value());
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

using enum ccpu::InstructionType;
using enum ccpu::MemoryTransferInstruction::TransferSize;
using enum ccpu::OffsetBasedInstruction::ShiftType;
using enum ccpu::AluInstruction::OpCode;
using enum ccpu::StackInstruction::OpCode;
} // namespace

TEST_CASE("Cpu without instructions should leave all registers in the expected state") {
  constexpr auto ram = RamFactory().produce({}, {});
  constexpr auto cpuWithStatus = cpuWithRam(ram);
  STATIC_CHECK(get<1>(cpuWithStatus).has_value());
  STATIC_CHECK(get<0>(cpuWithStatus).readRegister(15) == Cpu::FinalInstruction);
}

TEST_CASE("Cpu with memory loads should work as expected") {
  constexpr auto ram = RamFactory().produce(
    {
      MemoryEntry{64, 128},
      MemoryEntry{65, 10},
      MemoryEntry{66, 13},
      MemoryEntry{67, 25}
    },
    {
      IB().withType(MemoryTransfer).withSourceAndBase(1, 0).withOffset(64).get(),
      IB().withType(MemoryTransfer).withSourceAndBase(2, 0).withOffset(64).withTransferSize(Word).get(),
      IB().withType(MemoryTransfer).withSourceAndBase(3, 0).withOffset(64).withTransferSize(DWord).get(),
    }
 );
  constexpr auto cpuWithStatus = cpuWithRam(ram);
  STATIC_CHECK(get<1>(cpuWithStatus).has_value());
  STATIC_CHECK(get<0>(cpuWithStatus).readRegister(15) == Cpu::FinalInstruction);
  STATIC_CHECK(get<0>(cpuWithStatus).readRegister(1) == 128);
  STATIC_CHECK(get<0>(cpuWithStatus).readRegister(2) == 128 + (10 << 8));
  STATIC_CHECK(get<0>(cpuWithStatus).readRegister(3) == 128 + (10 << 8) + (13 << 16) + (25 << 24));
}

TEST_CASE("Cpu with memory stores should work as expected") {
  constexpr auto ram = RamFactory().produce(
    {
      MemoryEntry{64, 128},
      MemoryEntry{68, 0x1234}
    },
    {
      IB().withType(MemoryTransfer).withSourceAndBase(1, 0).withOffset(64).get(),
      IB().withType(MemoryTransfer).withSourceAndBase(2, 0).withOffset(68).withTransferSize(DWord).get(),
      IB().withType(MemoryTransfer).withStore().withSourceAndBase(2, 1).withOffset(0).withTransferSize(Byte).get(),
      IB().withType(MemoryTransfer).withStore().withSourceAndBase(2, 1).withOffset(16).withTransferSize(DWord).get(),
    }
  );
  constexpr auto cpuWithStatus = cpuWithRam(ram);
  STATIC_CHECK(get<1>(cpuWithStatus).has_value());
  STATIC_CHECK(get<0>(cpuWithStatus).readRegister(15) == Cpu::FinalInstruction);
  STATIC_CHECK(get<0>(cpuWithStatus).readMem(128).value() == 0x34);
  STATIC_CHECK(get<0>(cpuWithStatus).readMem(144).value() == 0x1234);
}

TEST_CASE("Cpu with alu operations should work as expected") {
  constexpr auto ram = RamFactory().produce(
    {
      MemoryEntry{64, 5},
      MemoryEntry{68, 12}
    },
    {
      IB().withType(MemoryTransfer).withSourceAndBase(1, 0).withOffset(64).get(),
      IB().withType(MemoryTransfer).withSourceAndBase(2, 0).withOffset(68).get(),
      IB().withType(AluOperation).withOpcode(ADD).withSourceAndDestination(1, 2).withOffset(ArithmeticLeft, 0, 2).get(),
      IB().withType(AluOperation).withOpcode(ADD).withSourceAndDestination(1, 3).withOffset(ArithmeticLeft, 1, 2).get(),
      IB().withType(AluOperation).withOpcode(ADD).withSourceAndDestination(1, 4).withOffset(ArithmeticRight, 1, 2).get(),
    }
  );
  constexpr auto cpuWithStatus = cpuWithRam(ram);
  STATIC_CHECK(get<1>(cpuWithStatus).has_value());
  STATIC_CHECK(get<0>(cpuWithStatus).readRegister(15) == Cpu::FinalInstruction);
  STATIC_CHECK(get<0>(cpuWithStatus).readRegister(2) == 17);
  STATIC_CHECK(get<0>(cpuWithStatus).readRegister(3) == 39);
  STATIC_CHECK(get<0>(cpuWithStatus).readRegister(4) == 13);
}

TEST_CASE("Cpu with simple branch operations should work as expected") {
  constexpr auto ram = RamFactory().produce(
    {
      MemoryEntry{64, 5},
      MemoryEntry{256, IB().withType(MemoryTransfer).withSourceAndBase(1, 0).withOffset(64).get().val},
      MemoryEntry{260, IB().withType(InType::Branch).withImmediate(Cpu::FinalInstruction - 4).get().val}
    },
    {
      IB().withType(Branch).withImmediate(256).get(),
    }
  );
  constexpr auto cpuWithStatus = cpuWithRam(ram);
  STATIC_CHECK(get<1>(cpuWithStatus).has_value());
  STATIC_CHECK(get<0>(cpuWithStatus).readRegister(15) == Cpu::FinalInstruction);
  STATIC_CHECK(get<0>(cpuWithStatus).readRegister(1) == 5);
}

TEST_CASE("Cpu with linked branch operations should work as expected") {
  constexpr auto ram = RamFactory().produce(
    {
      MemoryEntry{64, 5},
      MemoryEntry{256, IB().withType(MemoryTransfer).withSourceAndBase(1, 0).withOffset(64).get().val},
      MemoryEntry{260, IB().withType(AluOperation).withSourceAndDestination(0, 15).withOffset(LogicalLeft, 0, 14).withOpcode(MOV).get().val}
    },
    {
      IB().withType(Branch).withLink().withImmediate(256).get(),
    }
  );
  constexpr auto cpuWithStatus = cpuWithRam(ram);
  STATIC_CHECK(get<1>(cpuWithStatus).has_value());
  STATIC_CHECK(get<0>(cpuWithStatus).readRegister(15) == Cpu::FinalInstruction);
  STATIC_CHECK(get<0>(cpuWithStatus).readRegister(1) == 5);
}

TEST_CASE("Cpu with push stack operations should work as expected") {
  constexpr auto ram = RamFactory().produce(
    {},
    {
      IB().withType(StackOperation).withOpcode(PUSH).withRegisters({1, 1}).get(),
    }
  );
  constexpr auto cpuWithStatus = cpuWithRam(ram);
  STATIC_CHECK(get<1>(cpuWithStatus).has_value());
  STATIC_CHECK(get<0>(cpuWithStatus).readRegister(15) == Cpu::FinalInstruction);
  STATIC_CHECK(get<0>(cpuWithStatus).readRegister(13) == Cpu::RamSize - 12);
}
