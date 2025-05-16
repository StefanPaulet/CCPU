//
// Created by stefan on 2/21/25.
//

#include <catch2/catch_test_macros.hpp>
#include <instruction/Instruction.hpp>
#include "Utils.hpp"

namespace {
using ccpu::AluInstruction;
using ccpu::BranchInstruction;
using ccpu::Instruction;
using ccpu::MemoryTransferInstruction;
using ccpu::OffsetBasedInstruction;
using ccpu::StackInstruction;

using ccpu::Condition;
using ccpu::InstructionType;

struct MyInteger {
  constexpr auto shiftVal(uint8_t shift, uint32_t val) -> MyInteger& {
    _val |= val << shift;
    return *this;
  }

  constexpr auto neg() -> MyInteger& {
    _val = ~_val;
    return *this;
  }

  uint32_t _val;
};

[[nodiscard]] inline constexpr auto myInt() { return MyInteger{}; }
[[nodiscard]] inline constexpr auto myInt(uint32_t val) { return MyInteger{val}; }

template <typename InstrT>
constexpr auto getInstruction(MyInteger value) { return InstrT{Instruction{value._val}}; }
} // namespace

TEST_CASE("Instruction should have expected conditions") {
  using enum ccpu::Condition;
  ASSERT(getInstruction<Instruction>(myInt().shiftVal(28, 0)).getCondition() == EQ);
  ASSERT(getInstruction<Instruction>(myInt().shiftVal(28, 1)).getCondition() == NE);
  ASSERT(getInstruction<Instruction>(myInt().shiftVal(28, 2)).getCondition() == NG);
  ASSERT(getInstruction<Instruction>(myInt().shiftVal(28, 3)).getCondition() == PO);
  ASSERT(getInstruction<Instruction>(myInt().shiftVal(28, 4)).getCondition() == VS);
  ASSERT(getInstruction<Instruction>(myInt().shiftVal(28, 5)).getCondition() == VC);
  ASSERT(getInstruction<Instruction>(myInt().shiftVal(28, 14)).getCondition() == AL);
  ASSERT(getInstruction<Instruction>(myInt().shiftVal(28, 15)).getCondition() == NV);

  ASSERT(getInstruction<Instruction>(myInt().shiftVal(28, 14)).isUnconditional());
  ASSERT(!getInstruction<Instruction>(myInt().shiftVal(28, 0).neg()).isUnconditional());
  ASSERT(!getInstruction<Instruction>(myInt().shiftVal(28, 1)).isUnconditional());
  ASSERT(!getInstruction<Instruction>(myInt().shiftVal(28, 2)).isUnconditional());
  ASSERT(!getInstruction<Instruction>(myInt().shiftVal(28, 3)).isUnconditional());
  ASSERT(!getInstruction<Instruction>(myInt().shiftVal(28, 4)).isUnconditional());
  ASSERT(!getInstruction<Instruction>(myInt().shiftVal(28, 5)).isUnconditional());

  ASSERT(getInstruction<Instruction>(myInt(0x50000000)).getCondition() == VC);
  ASSERT(getInstruction<Instruction>(myInt(0x2FAFAFAF)).getCondition() == NG);
  ASSERT(getInstruction<Instruction>(myInt(0x12345678)).getCondition() == NE);
}

TEST_CASE("Instruction should test expected bits") {
  ASSERT(getInstruction<Instruction>(myInt(1)).testBit(0));
  ASSERT(!getInstruction<Instruction>(myInt(1)).testBit(1));

  ASSERT(getInstruction<Instruction>(myInt(1 << 23)).testBit(23));
  ASSERT(!getInstruction<Instruction>(myInt(1 << 24)).testBit(23));

  ASSERT(!getInstruction<Instruction>(myInt(0x7 << 5)).testBit(4));
  ASSERT(getInstruction<Instruction>(myInt(0x7 << 5)).testBit(5));
  ASSERT(getInstruction<Instruction>(myInt(0x7 << 5)).testBit(6));
  ASSERT(getInstruction<Instruction>(myInt(0x7 << 5)).testBit(7));
  ASSERT(!getInstruction<Instruction>(myInt(0x7 << 5)).testBit(8));

  ASSERT(!getInstruction<Instruction>(myInt(0xA << 19)).testBit(19));
  ASSERT(getInstruction<Instruction>(myInt(0xA << 19)).testBit(20));
  ASSERT(!getInstruction<Instruction>(myInt(0xA << 19)).testBit(21));
  ASSERT(getInstruction<Instruction>(myInt(0xA << 19)).testBit(22));

  ASSERT(getInstruction<Instruction>(myInt(0xBAF10000)).testBit(31));
  ASSERT(!getInstruction<Instruction>(myInt(0xBAF10000)).testBit(30));
  ASSERT(getInstruction<Instruction>(myInt(0xBAF10000)).testBit(27));
  ASSERT(!getInstruction<Instruction>(myInt(0xBAF10000)).testBit(24));
}

TEST_CASE("Instruction should test expected types") {
  using enum ccpu::InstructionType;
  ASSERT(getInstruction<Instruction>(myInt().shiftVal(25, 0)).type() == MemoryTransfer);
  ASSERT(getInstruction<Instruction>(myInt().shiftVal(25, 1)).type() == AluOperation);
  ASSERT(getInstruction<Instruction>(myInt().shiftVal(25, 2)).type() == Branch);
  ASSERT(getInstruction<Instruction>(myInt().shiftVal(25, 3)).type() == StackOperation);

  ASSERT(getInstruction<Instruction>(myInt(0xF3000000)).type() == AluOperation);
  ASSERT(getInstruction<Instruction>(myInt(0xF50024A0)).type() == Branch);
}

TEST_CASE("OffsetBasedInstruction should have an expected offset") {
  ASSERT(getInstruction<OffsetBasedInstruction>(myInt(1)).offset() == 1);
  ASSERT(getInstruction<OffsetBasedInstruction>(myInt(15)).offset() == 15);
  ASSERT(getInstruction<OffsetBasedInstruction>(myInt(123)).offset() == 123);

  ASSERT(getInstruction<OffsetBasedInstruction>(myInt().shiftVal(27, 32).shiftVal(0, 123)).offset() == 123);
  ASSERT(getInstruction<OffsetBasedInstruction>(myInt().shiftVal(12, 0xFFFFF).shiftVal(0, 256)).offset() == 256);
}

TEST_CASE("OffsetBasedInstruction should have the expected register") {
  ASSERT(getInstruction<OffsetBasedInstruction>(myInt(0)).offsetRegister() == 0);
  ASSERT(getInstruction<OffsetBasedInstruction>(myInt(1)).offsetRegister() == 1);
  ASSERT(getInstruction<OffsetBasedInstruction>(myInt(4)).offsetRegister() == 4);
  ASSERT(getInstruction<OffsetBasedInstruction>(myInt(15)).offsetRegister() == 15);

  ASSERT(getInstruction<OffsetBasedInstruction>(myInt(16)).offsetRegister() == 0);
  ASSERT(getInstruction<OffsetBasedInstruction>(myInt().shiftVal(4, 0xFF).shiftVal(0, 5)).offsetRegister() == 5);
  ASSERT(getInstruction<OffsetBasedInstruction>(myInt().shiftVal(4, 0xFF).shiftVal(28, 0xF).shiftVal(0, 12)).offsetRegister() == 12);

  ASSERT(getInstruction<OffsetBasedInstruction>(myInt(0xFABCFFFF)).offsetRegister() == 15);
  ASSERT(getInstruction<OffsetBasedInstruction>(myInt(0x123123F4)).offsetRegister() == 4);
}


TEST_CASE("OffsetBasedInstruction should have the expected shift type") {
  using enum OffsetBasedInstruction::ShiftType;

  ASSERT(getInstruction<OffsetBasedInstruction>(myInt().shiftVal(10, 0)).shiftType() == LogicalLeft);
  ASSERT(getInstruction<OffsetBasedInstruction>(myInt().shiftVal(10, 1)).shiftType() == LogicalRight);
  ASSERT(getInstruction<OffsetBasedInstruction>(myInt().shiftVal(10, 2)).shiftType() == ArithmeticLeft);
  ASSERT(getInstruction<OffsetBasedInstruction>(myInt().shiftVal(10, 3)).shiftType() == ArithmeticRight);

  ASSERT(getInstruction<OffsetBasedInstruction>(myInt(0xFFF)).shiftType() == ArithmeticRight);
  ASSERT(getInstruction<OffsetBasedInstruction>(myInt(0x312)).shiftType() == LogicalLeft);
  ASSERT(getInstruction<OffsetBasedInstruction>(myInt(0x823)).shiftType() == ArithmeticLeft);
}


TEST_CASE("OffsetBasedInstruction should have the expected shift amount") {
  ASSERT(getInstruction<OffsetBasedInstruction>(myInt().shiftVal(4, 1)).shiftAmount() == 1);
  ASSERT(getInstruction<OffsetBasedInstruction>(myInt().shiftVal(4, 5)).shiftAmount() == 5);
  ASSERT(getInstruction<OffsetBasedInstruction>(myInt().shiftVal(4, 0x3F)).shiftAmount() == 0x3F);
  ASSERT(getInstruction<OffsetBasedInstruction>(myInt().shiftVal(4, 0x7F)).shiftAmount() == 0x3F);

  ASSERT(getInstruction<OffsetBasedInstruction>(myInt(0x23F)).shiftAmount() == 0x23);
  ASSERT(getInstruction<OffsetBasedInstruction>(myInt(0x4411A)).shiftAmount() == 0x11);
  ASSERT(getInstruction<OffsetBasedInstruction>(myInt(0xFFAA401A)).shiftAmount() == 0x1);
}

TEST_CASE("MemoryTransferInstruction should have the expected immediate flag") {
  ASSERT(!getInstruction<MemoryTransferInstruction>(myInt().shiftVal(24, 1).neg()).immediate());
  ASSERT(getInstruction<MemoryTransferInstruction>(myInt().shiftVal(24, 1)).immediate());

  ASSERT(getInstruction<MemoryTransferInstruction>(myInt(0x1000000)).immediate());
  ASSERT(!getInstruction<MemoryTransferInstruction>(myInt(0x2FFEEFF)).immediate());
}

TEST_CASE("MemoryTransferInstruction should have the expected transfer size") {
  using enum MemoryTransferInstruction::TransferSize;

  ASSERT(getInstruction<MemoryTransferInstruction>(myInt().shiftVal(22, 0)).transferSize() == Byte);
  ASSERT(getInstruction<MemoryTransferInstruction>(myInt().shiftVal(22, 1)).transferSize() == Word);
  ASSERT(getInstruction<MemoryTransferInstruction>(myInt().shiftVal(22, 2)).transferSize() == DWord);

  ASSERT(getInstruction<MemoryTransferInstruction>(myInt(0x300000)).transferSize() == Byte);
  ASSERT(getInstruction<MemoryTransferInstruction>(myInt(0x400000)).transferSize() == Word);
  ASSERT(getInstruction<MemoryTransferInstruction>(myInt(0x800000)).transferSize() == DWord);
}

TEST_CASE("MemoryTransferInstruction should have the expected store bit") {
  ASSERT(!getInstruction<MemoryTransferInstruction>(myInt().shiftVal(21, 1).neg()).store());
  ASSERT(getInstruction<MemoryTransferInstruction>(myInt().shiftVal(21, 1)).store());

  ASSERT(!getInstruction<MemoryTransferInstruction>(myInt(0x14AAFF1)).store());
  ASSERT(getInstruction<MemoryTransferInstruction>(myInt(0x012F0000)).store());
}

TEST_CASE("MemoryTransferInstruction should have the expected subtract bit") {
  ASSERT(!getInstruction<MemoryTransferInstruction>(myInt().shiftVal(20, 1).neg()).subtract());
  ASSERT(getInstruction<MemoryTransferInstruction>(myInt().shiftVal(20, 1)).subtract());

  ASSERT(!getInstruction<MemoryTransferInstruction>(myInt(0x2AAFFB)).subtract());
  ASSERT(getInstruction<MemoryTransferInstruction>(myInt(0xF100000)).subtract());
}

TEST_CASE("MemoryTransferInstruction should have the expected source register") {
  ASSERT(getInstruction<MemoryTransferInstruction>(myInt().shiftVal(16, 1)).source() == 1);
  ASSERT(getInstruction<MemoryTransferInstruction>(myInt().shiftVal(16, 4)).source() == 4);
  ASSERT(getInstruction<MemoryTransferInstruction>(myInt().shiftVal(16, 11)).source() == 11);

  ASSERT(getInstruction<MemoryTransferInstruction>(myInt(0x450000)).source() == 5);
  ASSERT(getInstruction<MemoryTransferInstruction>(myInt(0xFFB1122)).source() == 11);
  ASSERT(getInstruction<MemoryTransferInstruction>(myInt(0x110AAAA)).source() == 0);
}

TEST_CASE("MemoryTransferInstruction should have the expected base register") {
  ASSERT(getInstruction<MemoryTransferInstruction>(myInt().shiftVal(12, 3)).base() == 3);
  ASSERT(getInstruction<MemoryTransferInstruction>(myInt().shiftVal(12, 5)).base() == 5);
  ASSERT(getInstruction<MemoryTransferInstruction>(myInt().shiftVal(12, 12)).base() == 12);

  ASSERT(getInstruction<MemoryTransferInstruction>(myInt(0x42FFF)).base() == 2);
  ASSERT(getInstruction<MemoryTransferInstruction>(myInt(0xABCD9000)).base() == 9);
  ASSERT(getInstruction<MemoryTransferInstruction>(myInt(0xFF120000)).base() == 0);
}

TEST_CASE("AluInstruction should have the expected opcode") {
  using enum AluInstruction::OpCode;

  ASSERT(getInstruction<AluInstruction>(myInt().shiftVal(20, 0)).opcode() == AND);
  ASSERT(getInstruction<AluInstruction>(myInt().shiftVal(20, 1)).opcode() == XOR);
  ASSERT(getInstruction<AluInstruction>(myInt().shiftVal(20, 2)).opcode() == OR);
  ASSERT(getInstruction<AluInstruction>(myInt().shiftVal(20, 3)).opcode() == ADD);
  ASSERT(getInstruction<AluInstruction>(myInt().shiftVal(20, 4)).opcode() == SUB);
  ASSERT(getInstruction<AluInstruction>(myInt().shiftVal(20, 5)).opcode() == RSUB);
  ASSERT(getInstruction<AluInstruction>(myInt().shiftVal(20, 6)).opcode() == ADDC);
  ASSERT(getInstruction<AluInstruction>(myInt().shiftVal(20, 7)).opcode() == SUBC);
  ASSERT(getInstruction<AluInstruction>(myInt().shiftVal(20, 8)).opcode() == RSUBC);
  ASSERT(getInstruction<AluInstruction>(myInt().shiftVal(20, 9)).opcode() == TST);
  ASSERT(getInstruction<AluInstruction>(myInt().shiftVal(20, 10)).opcode() == TEQ);
  ASSERT(getInstruction<AluInstruction>(myInt().shiftVal(20, 11)).opcode() == CMP);
  ASSERT(getInstruction<AluInstruction>(myInt().shiftVal(20, 12)).opcode() == CMN);
  ASSERT(getInstruction<AluInstruction>(myInt().shiftVal(20, 13)).opcode() == MVN);
}

TEST_CASE("AluInstruction should have the expected source register") {
  ASSERT(getInstruction<AluInstruction>(myInt().shiftVal(16, 4)).source() == 4);
  ASSERT(getInstruction<AluInstruction>(myInt().shiftVal(16, 5)).source() == 5);
  ASSERT(getInstruction<AluInstruction>(myInt().shiftVal(16, 9)).source() == 9);

  ASSERT(getInstruction<AluInstruction>(myInt(0x450000)).source() == 5);
  ASSERT(getInstruction<AluInstruction>(myInt(0xFFB1122)).source() == 11);
  ASSERT(getInstruction<AluInstruction>(myInt(0x110AAAA)).source() == 0);
}

TEST_CASE("AluInstruction should have the expected destination register") {
  ASSERT(getInstruction<AluInstruction>(myInt().shiftVal(12, 3)).destination() == 3);
  ASSERT(getInstruction<AluInstruction>(myInt().shiftVal(12, 5)).destination() == 5);
  ASSERT(getInstruction<AluInstruction>(myInt().shiftVal(12, 12)).destination() == 12);

  ASSERT(getInstruction<AluInstruction>(myInt(0x42FFF)).destination() == 2);
  ASSERT(getInstruction<AluInstruction>(myInt(0xABCD9000)).destination() == 9);
  ASSERT(getInstruction<AluInstruction>(myInt(0xFF120000)).destination() == 0);
}

TEST_CASE("BranchInstruction should have the link flag") {
  ASSERT(!getInstruction<BranchInstruction>(myInt().shiftVal(24, 1).neg()).link());
  ASSERT(getInstruction<BranchInstruction>(myInt().shiftVal(24, 1)).link());
}

TEST_CASE("BranchInstruction should have the expected offset") {
  ASSERT(getInstruction<BranchInstruction>(myInt(234)).offset() == 234);
  ASSERT(getInstruction<BranchInstruction>(myInt().shiftVal(12, 12)).offset() == 12 * 4096);
}

TEST_CASE("StackInstruction should have the expected opcode") {
  using enum StackInstruction::OpCode;

  ASSERT(getInstruction<StackInstruction>(myInt().shiftVal(24, 0)).opcode() == POP);
  ASSERT(getInstruction<StackInstruction>(myInt().shiftVal(24, 1)).opcode() == PUSH);
}

TEST_CASE("StackInstruction should have the expected register list") {
  CONSTEXPR auto registerList = getInstruction<StackInstruction>(myInt().shiftVal(8, 0b110011)).registerList();
  ASSERT(registerList & (1 << 0));
  ASSERT(registerList & (1 << 1));
  ASSERT(!(registerList & (1 << 2)));
  ASSERT(!(registerList & (1 << 3)));
  ASSERT(registerList & (1 << 4));
  ASSERT(registerList & (1 << 5));
}
