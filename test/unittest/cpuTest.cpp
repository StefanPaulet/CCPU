//
// Created by stefan on 3/27/25.
//


#include <catch2/catch_test_macros.hpp>
#include <cpu/CentralProcessingUnit.hpp>
#include "Utils.hpp"

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
    _instr.val &= ~(static_cast<uint8_t>(0xF) << 28);
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
  [[nodiscard]] constexpr auto& withSubtract() noexcept {
    assert(_instr.type() == MemoryTransfer);
    _instr.val |= 1 << 20;
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
  Instruction _instr{};
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
  CONSTEXPR auto ram = RamFactory().produce({}, {});
  CONSTEXPR auto cpuWithStatus = cpuWithRam(ram);
  ASSERT(get<1>(cpuWithStatus).has_value());
  ASSERT(get<0>(cpuWithStatus).readRegister(15) == Cpu::FinalInstruction);
}

TEST_CASE("Cpu with memory loads should work as expected") {
  CONSTEXPR auto ram = RamFactory().produce(
    {
      MemoryEntry{64, 128},
      MemoryEntry{65, 10},
      MemoryEntry{66, 13},
      MemoryEntry{67, 25}
    },
    {
      IB().withType(MemoryTransfer).withSourceAndBase(1, 0).withOffset(64).withTransferSize(Byte).get(),
      IB().withType(MemoryTransfer).withSourceAndBase(2, 0).withOffset(64).withTransferSize(Word).get(),
      IB().withType(MemoryTransfer).withSourceAndBase(3, 0).withOffset(64).withTransferSize(DWord).get(),
    }
 );
  CONSTEXPR auto cpuWithStatus = cpuWithRam(ram);
  ASSERT(get<1>(cpuWithStatus).has_value());
  ASSERT(get<0>(cpuWithStatus).readRegister(15) == Cpu::FinalInstruction);
  ASSERT(get<0>(cpuWithStatus).readRegister(1) == 128);
  ASSERT(get<0>(cpuWithStatus).readRegister(2) == 128 + (10 << 8));
  ASSERT(get<0>(cpuWithStatus).readRegister(3) == 128 + (10 << 8) + (13 << 16) + (25 << 24));
}

TEST_CASE("Cpu with memory stores should work as expected") {
  CONSTEXPR auto ram = RamFactory().produce(
    {
      MemoryEntry{64, 128},
      MemoryEntry{68, 0x12341234}
    },
    {
      IB().withType(MemoryTransfer).withSourceAndBase(1, 0).withOffset(64).get(),
      IB().withType(MemoryTransfer).withSourceAndBase(2, 0).withOffset(68).withTransferSize(DWord).get(),
      IB().withType(MemoryTransfer).withStore().withSourceAndBase(2, 1).withOffset(0).withTransferSize(Byte).get(),
      IB().withType(MemoryTransfer).withStore().withSourceAndBase(2, 1).withOffset(16).withTransferSize(Word).get(),
      IB().withType(MemoryTransfer).withStore().withSourceAndBase(2, 1).withOffset(32).withTransferSize(DWord).get(),
    }
  );
  CONSTEXPR auto cpuWithStatus = cpuWithRam(ram);
  ASSERT(get<1>(cpuWithStatus).has_value());
  ASSERT(get<0>(cpuWithStatus).readRegister(15) == Cpu::FinalInstruction);
  ASSERT(get<0>(cpuWithStatus).readMem(128).value() == 0x34);
  ASSERT(get<0>(cpuWithStatus).readMem(144).value() == 0x1234);
  ASSERT(get<0>(cpuWithStatus).readMem(160).value() == 0x12341234);
}

TEST_CASE("Cpu with memory operations with subtraction from base should work as expected") {
  CONSTEXPR auto ram = RamFactory().produce(
    {
      MemoryEntry{64, 128},
      MemoryEntry{124, 64}
    },
    {
      IB().withType(MemoryTransfer).withSourceAndBase(1, 0).withOffset(64).get(),
      IB().withType(MemoryTransfer).withSourceAndBase(2, 1).withSubtract().withOffset(4).withTransferSize(Byte).get(),
      IB().withType(MemoryTransfer).withSourceAndBase(2, 1).withStore().withSubtract().withOffset(8).withTransferSize(Byte).get(),
    }
  );
  CONSTEXPR auto cpuWithStatus = cpuWithRam(ram);
  ASSERT(get<1>(cpuWithStatus).has_value());
  ASSERT(get<0>(cpuWithStatus).readRegister(15) == Cpu::FinalInstruction);
  ASSERT(get<0>(cpuWithStatus).readRegister(2) == 64);
  ASSERT(get<0>(cpuWithStatus).readMem(120).value() == 64);
}

TEST_CASE("Cpu with alu ADD operations should work as expected") {
  CONSTEXPR auto ram = RamFactory().produce(
    {
      MemoryEntry{64, 5},
      MemoryEntry{68, 12}
    },
    {
      IB().withType(MemoryTransfer).withSourceAndBase(1, 0).withOffset(64).get(),
      IB().withType(MemoryTransfer).withSourceAndBase(2, 0).withOffset(68).get(),
      IB().withType(AluOperation).withOpcode(ADD).withSourceAndDestination(1, 3).withOffset(ArithmeticLeft, 0, 2).get(),
    }
  );
  CONSTEXPR auto cpuWithStatus = cpuWithRam(ram);
  ASSERT(get<1>(cpuWithStatus).has_value());
  ASSERT(get<0>(cpuWithStatus).readRegister(15) == Cpu::FinalInstruction);
  ASSERT(get<0>(cpuWithStatus).readRegister(3) == 17);
}

TEST_CASE("Cpu with alu SUB operations should work as expected") {
  CONSTEXPR auto ram = RamFactory().produce(
    {
      MemoryEntry{64, 5},
      MemoryEntry{68, 12}
    },
    {
      IB().withType(MemoryTransfer).withSourceAndBase(1, 0).withOffset(64).get(),
      IB().withType(MemoryTransfer).withSourceAndBase(2, 0).withOffset(68).get(),
      IB().withType(AluOperation).withOpcode(SUB).withSourceAndDestination(2, 3).withOffset(ArithmeticLeft, 0, 1).get(),
    }
  );
  CONSTEXPR auto cpuWithStatus = cpuWithRam(ram);
  ASSERT(get<1>(cpuWithStatus).has_value());
  ASSERT(get<0>(cpuWithStatus).readRegister(15) == Cpu::FinalInstruction);
  ASSERT(get<0>(cpuWithStatus).readRegister(3) == 7);
}

TEST_CASE("Cpu with alu MOV operations should work as expected") {
  CONSTEXPR auto ram = RamFactory().produce(
    {
      MemoryEntry{64, 5},
      MemoryEntry{68, 12}
    },
    {
      IB().withType(MemoryTransfer).withSourceAndBase(1, 0).withOffset(64).get(),
      IB().withType(MemoryTransfer).withSourceAndBase(2, 0).withOffset(68).get(),
      IB().withType(AluOperation).withOpcode(MOV).withSourceAndDestination(2, 3).withOffset(ArithmeticLeft, 0, 1).get(),
    }
  );
  CONSTEXPR auto cpuWithStatus = cpuWithRam(ram);
  ASSERT(get<1>(cpuWithStatus).has_value());
  ASSERT(get<0>(cpuWithStatus).readRegister(15) == Cpu::FinalInstruction);
  ASSERT(get<0>(cpuWithStatus).readRegister(3) == 5);
}

TEST_CASE("Cpu with alu operations should work properly with all shifts") {
  CONSTEXPR auto ram = RamFactory().produce(
    {
      MemoryEntry{64, 0b10111111'11111111'11111111'11011110},
    },
    {
      IB().withType(MemoryTransfer).withSourceAndBase(1, 0).withOffset(64).withTransferSize(DWord).get(),
      IB().withType(AluOperation).withOpcode(MOV).withSourceAndDestination(0, 3).withOffset(ArithmeticLeft, 1, 1).get(),
      IB().withType(AluOperation).withOpcode(MOV).withSourceAndDestination(0, 4).withOffset(ArithmeticRight, 1, 1).get(),
      IB().withType(AluOperation).withOpcode(MOV).withSourceAndDestination(0, 5).withOffset(LogicalLeft, 1, 1).get(),
      IB().withType(AluOperation).withOpcode(MOV).withSourceAndDestination(0, 6).withOffset(LogicalRight, 1, 1).get(),
    }
  );
  CONSTEXPR auto cpuWithStatus = cpuWithRam(ram);
  ASSERT(get<1>(cpuWithStatus).has_value());
  ASSERT(get<0>(cpuWithStatus).readRegister(15) == Cpu::FinalInstruction);
  ASSERT(get<0>(cpuWithStatus).readRegister(3) == 0b01111111'11111111'11111111'10111100);
  ASSERT(get<0>(cpuWithStatus).readRegister(4) == 0b11011111'11111111'11111111'11101111);
  ASSERT(get<0>(cpuWithStatus).readRegister(5) == 0b01111111'11111111'11111111'10111100);
  ASSERT(get<0>(cpuWithStatus).readRegister(6) == 0b01011111'11111111'11111111'11101111);
}

TEST_CASE("Cpu with simple branch operations should work as expected") {
  CONSTEXPR auto ram = RamFactory().produce(
    {
      MemoryEntry{64, 5},
      MemoryEntry{256, IB().withType(MemoryTransfer).withSourceAndBase(1, 0).withOffset(64).get().val},
      MemoryEntry{260, IB().withType(InType::Branch).withImmediate(Cpu::FinalInstruction - 4).get().val}
    },
    {
      IB().withType(Branch).withImmediate(256).get(),
    }
  );
  CONSTEXPR auto cpuWithStatus = cpuWithRam(ram);
  ASSERT(get<1>(cpuWithStatus).has_value());
  ASSERT(get<0>(cpuWithStatus).readRegister(15) == Cpu::FinalInstruction);
  ASSERT(get<0>(cpuWithStatus).readRegister(1) == 5);
}

TEST_CASE("Cpu with linked branch operations should work as expected") {
  CONSTEXPR auto ram = RamFactory().produce(
    {
      MemoryEntry{64, 5},
      MemoryEntry{256, IB().withType(MemoryTransfer).withSourceAndBase(1, 0).withOffset(64).get().val},
      MemoryEntry{260, IB().withType(AluOperation).withSourceAndDestination(0, 15).withOffset(LogicalLeft, 0, 14).withOpcode(MOV).get().val}
    },
    {
      IB().withType(Branch).withLink().withImmediate(256).get(),
    }
  );
  CONSTEXPR auto cpuWithStatus = cpuWithRam(ram);
  ASSERT(get<1>(cpuWithStatus).has_value());
  ASSERT(get<0>(cpuWithStatus).readRegister(15) == Cpu::FinalInstruction);
  ASSERT(get<0>(cpuWithStatus).readRegister(1) == 5);
}

TEST_CASE("Cpu with push stack operations should work as expected") {
  CONSTEXPR auto ram = RamFactory().produce(
    {},
    {
      IB().withType(StackOperation).withOpcode(PUSH).withRegisters({1, 1}).get(),
    }
  );
  CONSTEXPR auto cpuWithStatus = cpuWithRam(ram);
  ASSERT(get<1>(cpuWithStatus).has_value());
  ASSERT(get<0>(cpuWithStatus).readRegister(15) == Cpu::FinalInstruction);
  ASSERT(get<0>(cpuWithStatus).readRegister(13) == Cpu::RamSize - 12);
}


TEST_CASE("Cpu with pop stack operations should work as expected") {
  CONSTEXPR auto ram = RamFactory().produce(
    {
      MemoryEntry{64, 35},
    },
    {
      IB().withType(MemoryTransfer).withSourceAndBase(1, 0).withOffset(64).get(),
      IB().withType(StackOperation).withOpcode(PUSH).withRegisters({0, 1}).get(),
      IB().withType(StackOperation).withOpcode(POP).withRegisters({1}).get(),
    }
  );
  CONSTEXPR auto cpuWithStatus = cpuWithRam(ram);
  ASSERT(get<1>(cpuWithStatus).has_value());
  ASSERT(get<0>(cpuWithStatus).readRegister(15) == Cpu::FinalInstruction);
  ASSERT(get<0>(cpuWithStatus).readRegister(13) == Cpu::RamSize - 4);
  ASSERT(get<0>(cpuWithStatus).readRegister(0) == 35);
}

TEST_CASE("If PC points outside of RAM boundaries, the CPU exits with failure") {
  using enum ccpu::FaultyInstruction;
  CONSTEXPR auto ram = RamFactory().produce(
    {
      MemoryEntry{64, 4098},
    },
    {
      IB().withType(MemoryTransfer).withSourceAndBase(15, 0).withOffset(64).withTransferSize(DWord).get(),
    }
  );
  CONSTEXPR auto cpuWithStatus = cpuWithRam(ram);
  ASSERT(!get<1>(cpuWithStatus).has_value());
  ASSERT(get<1>(cpuWithStatus).error() == InvalidMemoryRead);
}

TEST_CASE("If memory writes are outside RAM boundary, the CPU exits with failure") {
  using enum ccpu::FaultyInstruction;

  constexpr auto ramProducer = []<ccpu::MemoryTransferInstruction::TransferSize size> {
    return RamFactory().produce(
      {
        MemoryEntry{64, 4098},
      },
      {
        IB().withType(MemoryTransfer).withSourceAndBase(1, 0).withOffset(64).withTransferSize(DWord).get(),
        IB().withType(MemoryTransfer).withStore().withSourceAndBase(2, 1).withTransferSize(size).get(),
      }
    );
  };
  CONSTEXPR auto byteCpuWithStatus = cpuWithRam(ramProducer.operator()<Byte>());
  ASSERT(!get<1>(byteCpuWithStatus).has_value());
  ASSERT(get<1>(byteCpuWithStatus).error() == InvalidMemoryWrite);


  CONSTEXPR auto wordCpuWithStatus = cpuWithRam(ramProducer.operator()<Word>());
  ASSERT(!get<1>(wordCpuWithStatus).has_value());
  ASSERT(get<1>(wordCpuWithStatus).error() == InvalidMemoryWrite);


  CONSTEXPR auto dwordCpuWithStatus = cpuWithRam(ramProducer.operator()<DWord>());
  ASSERT(!get<1>(dwordCpuWithStatus).has_value());
  ASSERT(get<1>(dwordCpuWithStatus).error() == InvalidMemoryWrite);
}

TEST_CASE("If memory reads are outside RAM boundary, the CPU exits with failure") {
  using enum ccpu::FaultyInstruction;

  constexpr auto ramProducer = []<ccpu::MemoryTransferInstruction::TransferSize size> {
    return RamFactory().produce(
      {
        MemoryEntry{64, 4098},
      },
      {
        IB().withType(MemoryTransfer).withSourceAndBase(1, 0).withOffset(64).withTransferSize(DWord).get(),
        IB().withType(MemoryTransfer).withSourceAndBase(2, 1).withTransferSize(size).get(),
      }
    );
  };
  CONSTEXPR auto byteCpuWithStatus = cpuWithRam(ramProducer.operator()<Byte>());
  ASSERT(!get<1>(byteCpuWithStatus).has_value());
  ASSERT(get<1>(byteCpuWithStatus).error() == InvalidMemoryRead);


  CONSTEXPR auto wordCpuWithStatus = cpuWithRam(ramProducer.operator()<Word>());
  ASSERT(!get<1>(wordCpuWithStatus).has_value());
  ASSERT(get<1>(wordCpuWithStatus).error() == InvalidMemoryRead);


  CONSTEXPR auto dwordCpuWithStatus = cpuWithRam(ramProducer.operator()<DWord>());
  ASSERT(!get<1>(dwordCpuWithStatus).has_value());
  ASSERT(get<1>(dwordCpuWithStatus).error() == InvalidMemoryRead);
}

TEST_CASE("If SP points outside of RAM boundaries on stack operation, the CPU exits with failure") {
  using enum ccpu::StackInstruction::OpCode;
  using enum ccpu::FaultyInstruction;

  constexpr auto ramProducer = []<ccpu::StackInstruction::OpCode opCode> {
    return RamFactory().produce(
      {
        MemoryEntry{64, 4098},
      },
      {
        IB().withType(MemoryTransfer).withSourceAndBase(13, 0).withOffset(64).withTransferSize(DWord).get(),
        IB().withType(StackOperation).withOpcode(opCode).withRegisters({1}).get(),
      }
    );
  };

  CONSTEXPR auto popCpuWithStatus = cpuWithRam(ramProducer.operator()<POP>());
  ASSERT(!get<1>(popCpuWithStatus).has_value());
  ASSERT(get<1>(popCpuWithStatus).error() == InvalidMemoryRead);

  CONSTEXPR auto pushCpuWithStatus = cpuWithRam(ramProducer.operator()<PUSH>());
  ASSERT(!get<1>(pushCpuWithStatus).has_value());
  ASSERT(get<1>(pushCpuWithStatus).error() == InvalidMemoryWrite);
}

TEST_CASE("CPU properly computes CPSR registers") {
  using enum ccpu::FaultyInstruction;

  constexpr auto ramProducer = []<uint32_t lhs, uint32_t rhs> {
    return RamFactory().produce(
      {
        MemoryEntry{64, lhs},
        MemoryEntry{68, rhs},
      },
      {
        IB().withType(MemoryTransfer).withSourceAndBase(1, 0).withOffset(64).withTransferSize(DWord).get(),
        IB().withType(MemoryTransfer).withSourceAndBase(2, 0).withOffset(68).withTransferSize(DWord).get(),
        IB().withType(AluOperation).withOpcode(ADD).withSourceAndDestination(1, 3).withOffset(LogicalLeft, 0, 2).get(),
      }
    );
  };

  CONSTEXPR auto zeroSumCpuWithStatus = cpuWithRam(ramProducer.operator()<5, -5u>());
  ASSERT(get<1>(zeroSumCpuWithStatus).has_value());
  ASSERT(get<0>(zeroSumCpuWithStatus).readCPSR().nFlag() == false);
  ASSERT(get<0>(zeroSumCpuWithStatus).readCPSR().zFlag() == true);
  ASSERT(get<0>(zeroSumCpuWithStatus).readCPSR().cFlag() == true);
  ASSERT(get<0>(zeroSumCpuWithStatus).readCPSR().vFlag() == false);

  CONSTEXPR auto positiveNonOverflowingSumCpuWithStatus = cpuWithRam(ramProducer.operator()<5, 3>());
  ASSERT(get<1>(positiveNonOverflowingSumCpuWithStatus).has_value());
  ASSERT(get<0>(positiveNonOverflowingSumCpuWithStatus).readCPSR().nFlag() == false);
  ASSERT(get<0>(positiveNonOverflowingSumCpuWithStatus).readCPSR().zFlag() == false);
  ASSERT(get<0>(positiveNonOverflowingSumCpuWithStatus).readCPSR().cFlag() == false);
  ASSERT(get<0>(positiveNonOverflowingSumCpuWithStatus).readCPSR().vFlag() == false);

  CONSTEXPR auto positiveOverflowingSumCpuWithStatus = cpuWithRam(ramProducer.operator()<0x7FFFFFFF, 1>());
  ASSERT(get<1>(positiveOverflowingSumCpuWithStatus).has_value());
  ASSERT(get<0>(positiveOverflowingSumCpuWithStatus).readCPSR().nFlag() == true);
  ASSERT(get<0>(positiveOverflowingSumCpuWithStatus).readCPSR().zFlag() == false);
  ASSERT(get<0>(positiveOverflowingSumCpuWithStatus).readCPSR().cFlag() == false);
  ASSERT(get<0>(positiveOverflowingSumCpuWithStatus).readCPSR().vFlag() == true);

  CONSTEXPR auto negativeNonOverflowingSumCpuWithStatus = cpuWithRam(ramProducer.operator()<5, static_cast<uint32_t>(1 << 31)>());
  ASSERT(get<1>(negativeNonOverflowingSumCpuWithStatus).has_value());
  ASSERT(get<0>(negativeNonOverflowingSumCpuWithStatus).readCPSR().nFlag() == true);
  ASSERT(get<0>(negativeNonOverflowingSumCpuWithStatus).readCPSR().zFlag() == false);
  ASSERT(get<0>(negativeNonOverflowingSumCpuWithStatus).readCPSR().cFlag() == false);
  ASSERT(get<0>(negativeNonOverflowingSumCpuWithStatus).readCPSR().vFlag() == false);

  CONSTEXPR auto negativeOverflowingSumCpuWithStatus = cpuWithRam(ramProducer.operator()<static_cast<uint32_t>(1 << 31), -1u>());
  ASSERT(get<1>(negativeOverflowingSumCpuWithStatus).has_value());
  ASSERT(get<0>(negativeOverflowingSumCpuWithStatus).readCPSR().nFlag() == false);
  ASSERT(get<0>(negativeOverflowingSumCpuWithStatus).readCPSR().zFlag() == false);
  ASSERT(get<0>(negativeOverflowingSumCpuWithStatus).readCPSR().cFlag() == true);
  ASSERT(get<0>(negativeOverflowingSumCpuWithStatus).readCPSR().vFlag() == true);
}

TEST_CASE("CPU should properly execute conditional instructions") {
  using enum ccpu::FaultyInstruction;
  using enum ccpu::Condition;

  constexpr auto ramProducer = []<uint32_t lhs, uint32_t rhs, ccpu::Condition condition> {
    return RamFactory().produce(
      {
        MemoryEntry {64, lhs},
        MemoryEntry {68, rhs},
        MemoryEntry {72, 128},
      },
      {
        IB().withType(MemoryTransfer).withSourceAndBase(1, 0).withOffset(64).withTransferSize(DWord).get(),
        IB().withType(MemoryTransfer).withSourceAndBase(2, 0).withOffset(68).withTransferSize(DWord).get(),
        IB().withType(AluOperation).withOpcode(ADD).withSourceAndDestination(1, 3).withOffset(LogicalLeft, 0, 2).get(),
        IB().withType(MemoryTransfer).withCondition(condition).withSourceAndBase(4, 0).withOffset(72).get(),
      });
  };

  CONSTEXPR auto eqTrueWithStatus = cpuWithRam(ramProducer.operator()<5, -5u, EQ>());
  ASSERT(get<1>(eqTrueWithStatus).has_value());
  ASSERT(get<0>(eqTrueWithStatus).readRegister(4) == 128);

  CONSTEXPR auto eqFalseWithStatus = cpuWithRam(ramProducer.operator()<5, -4u, EQ>());
  ASSERT(get<1>(eqFalseWithStatus).has_value());
  ASSERT(get<0>(eqFalseWithStatus).readRegister(4) == 0);


  CONSTEXPR auto neTrueWithStatus = cpuWithRam(ramProducer.operator()<5, -4u, NE>());
  ASSERT(get<1>(neTrueWithStatus).has_value());
  ASSERT(get<0>(neTrueWithStatus).readRegister(4) == 128);

  CONSTEXPR auto neFalseWithStatus = cpuWithRam(ramProducer.operator()<5, -5u, NE>());
  ASSERT(get<1>(neFalseWithStatus).has_value());
  ASSERT(get<0>(neFalseWithStatus).readRegister(4) == 0);


  CONSTEXPR auto ngTrueWithStatus = cpuWithRam(ramProducer.operator()<2, -4u, NG>());
  ASSERT(get<1>(ngTrueWithStatus).has_value());
  ASSERT(get<0>(ngTrueWithStatus).readRegister(4) == 128);

  CONSTEXPR auto ngFalseWithStatus = cpuWithRam(ramProducer.operator()<23, -5u, NG>());
  ASSERT(get<1>(ngFalseWithStatus).has_value());
  ASSERT(get<0>(ngFalseWithStatus).readRegister(4) == 0);


  CONSTEXPR auto poTrueWithStatus = cpuWithRam(ramProducer.operator()<13, -4u, PO>());
  ASSERT(get<1>(poTrueWithStatus).has_value());
  ASSERT(get<0>(poTrueWithStatus).readRegister(4) == 128);

  CONSTEXPR auto poFalseWithStatus = cpuWithRam(ramProducer.operator()<-2u, -5u, PO>());
  ASSERT(get<1>(poFalseWithStatus).has_value());
  ASSERT(get<0>(poFalseWithStatus).readRegister(4) == 0);


  CONSTEXPR auto vsTrueWithStatus = cpuWithRam(ramProducer.operator()<0x7FFF'FFFF, 1, VS>());
  ASSERT(get<1>(vsTrueWithStatus).has_value());
  ASSERT(get<0>(vsTrueWithStatus).readRegister(4) == 128);

  CONSTEXPR auto vsFalseWithStatus = cpuWithRam(ramProducer.operator()<0x7FFF'FFFE, 1, VS>());
  ASSERT(get<1>(vsFalseWithStatus).has_value());
  ASSERT(get<0>(vsFalseWithStatus).readRegister(4) == 0);


  CONSTEXPR auto vcTrueWithStatus = cpuWithRam(ramProducer.operator()<0x7FFF'FFFE, 1, VC>());
  ASSERT(get<1>(vcTrueWithStatus).has_value());
  ASSERT(get<0>(vcTrueWithStatus).readRegister(4) == 128);

  CONSTEXPR auto vcFalseWithStatus = cpuWithRam(ramProducer.operator()<0x7FFF'FFFF, 1, VC>());
  ASSERT(get<1>(vcFalseWithStatus).has_value());
  ASSERT(get<0>(vcFalseWithStatus).readRegister(4) == 0);


  CONSTEXPR auto alTrueWithStatus = cpuWithRam(ramProducer.operator()<0x7FFF'FFFE, 1, AL>());
  ASSERT(get<1>(alTrueWithStatus).has_value());
  ASSERT(get<0>(alTrueWithStatus).readRegister(4) == 128);

  CONSTEXPR auto nvFalseWithStatus = cpuWithRam(ramProducer.operator()<0x7FFF'FFFE, 1, NV>());
  ASSERT(get<1>(nvFalseWithStatus).has_value());
  ASSERT(get<0>(nvFalseWithStatus).readRegister(4) == 0);
}