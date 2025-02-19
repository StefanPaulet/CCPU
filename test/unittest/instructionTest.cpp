//
// Created by stefan on 2/21/25.
//

#include <catch2/catch_test_macros.hpp>
#include <instruction/Instruction.hpp>

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
  STATIC_CHECK(getInstruction<Instruction>(myInt().shiftVal(28, 0)).getCondition() == EQ);
  STATIC_CHECK(getInstruction<Instruction>(myInt().shiftVal(28, 1)).getCondition() == NE);
  STATIC_CHECK(getInstruction<Instruction>(myInt().shiftVal(28, 2)).getCondition() == NG);
  STATIC_CHECK(getInstruction<Instruction>(myInt().shiftVal(28, 3)).getCondition() == PO);
  STATIC_CHECK(getInstruction<Instruction>(myInt().shiftVal(28, 4)).getCondition() == VS);
  STATIC_CHECK(getInstruction<Instruction>(myInt().shiftVal(28, 5)).getCondition() == VC);
  STATIC_CHECK(getInstruction<Instruction>(myInt().shiftVal(28, 14)).getCondition() == AL);
  STATIC_CHECK(getInstruction<Instruction>(myInt().shiftVal(28, 15)).getCondition() == NV);

  STATIC_CHECK(getInstruction<Instruction>(myInt().shiftVal(28, 14)).isUnconditional());
  STATIC_CHECK(!getInstruction<Instruction>(myInt().shiftVal(28, 0).neg()).isUnconditional());
  STATIC_CHECK(!getInstruction<Instruction>(myInt().shiftVal(28, 1)).isUnconditional());
  STATIC_CHECK(!getInstruction<Instruction>(myInt().shiftVal(28, 2)).isUnconditional());
  STATIC_CHECK(!getInstruction<Instruction>(myInt().shiftVal(28, 3)).isUnconditional());
  STATIC_CHECK(!getInstruction<Instruction>(myInt().shiftVal(28, 4)).isUnconditional());
  STATIC_CHECK(!getInstruction<Instruction>(myInt().shiftVal(28, 5)).isUnconditional());

  STATIC_CHECK(getInstruction<Instruction>(myInt(0x50000000)).getCondition() == VC);
  STATIC_CHECK(getInstruction<Instruction>(myInt(0x2FAFAFAF)).getCondition() == NG);
  STATIC_CHECK(getInstruction<Instruction>(myInt(0x12345678)).getCondition() == NE);
}

TEST_CASE("Instruction should test expected bits") {
  STATIC_CHECK(getInstruction<Instruction>(myInt(1)).testBit(0));
  STATIC_CHECK(!getInstruction<Instruction>(myInt(1)).testBit(1));

  STATIC_CHECK(getInstruction<Instruction>(myInt(1 << 23)).testBit(23));
  STATIC_CHECK(!getInstruction<Instruction>(myInt(1 << 24)).testBit(23));

  STATIC_CHECK(!getInstruction<Instruction>(myInt(0x7 << 5)).testBit(4));
  STATIC_CHECK(getInstruction<Instruction>(myInt(0x7 << 5)).testBit(5));
  STATIC_CHECK(getInstruction<Instruction>(myInt(0x7 << 5)).testBit(6));
  STATIC_CHECK(getInstruction<Instruction>(myInt(0x7 << 5)).testBit(7));
  STATIC_CHECK(!getInstruction<Instruction>(myInt(0x7 << 5)).testBit(8));

  STATIC_CHECK(!getInstruction<Instruction>(myInt(0xA << 19)).testBit(19));
  STATIC_CHECK(getInstruction<Instruction>(myInt(0xA << 19)).testBit(20));
  STATIC_CHECK(!getInstruction<Instruction>(myInt(0xA << 19)).testBit(21));
  STATIC_CHECK(getInstruction<Instruction>(myInt(0xA << 19)).testBit(22));

  STATIC_CHECK(getInstruction<Instruction>(myInt(0xBAF10000)).testBit(31));
  STATIC_CHECK(!getInstruction<Instruction>(myInt(0xBAF10000)).testBit(30));
  STATIC_CHECK(getInstruction<Instruction>(myInt(0xBAF10000)).testBit(27));
  STATIC_CHECK(!getInstruction<Instruction>(myInt(0xBAF10000)).testBit(24));
}

TEST_CASE("Instruction should test expected types") {
  using enum ccpu::InstructionType;
  STATIC_CHECK(getInstruction<Instruction>(myInt().shiftVal(25, 0)).type() == MemoryTransfer);
  STATIC_CHECK(getInstruction<Instruction>(myInt().shiftVal(25, 1)).type() == AluOperation);
  STATIC_CHECK(getInstruction<Instruction>(myInt().shiftVal(25, 2)).type() == Branch);
  STATIC_CHECK(getInstruction<Instruction>(myInt().shiftVal(25, 3)).type() == StackOperation);

  STATIC_CHECK(getInstruction<Instruction>(myInt(0xF3000000)).type() == AluOperation);
  STATIC_CHECK(getInstruction<Instruction>(myInt(0xF50024A0)).type() == Branch);
}

TEST_CASE("OffsetBasedInstruction should have an expected offset") {
  STATIC_CHECK(getInstruction<OffsetBasedInstruction>(myInt(1)).offset() == 1);
  STATIC_CHECK(getInstruction<OffsetBasedInstruction>(myInt(15)).offset() == 15);
  STATIC_CHECK(getInstruction<OffsetBasedInstruction>(myInt(123)).offset() == 123);

  STATIC_CHECK(getInstruction<OffsetBasedInstruction>(myInt().shiftVal(27, 32).shiftVal(0, 123)).offset() == 123);
  STATIC_CHECK(getInstruction<OffsetBasedInstruction>(myInt().shiftVal(12, 0xFFFFF).shiftVal(0, 256)).offset() == 256);
}

TEST_CASE("OffsetBasedInstruction should have the expected register") {
  STATIC_CHECK(getInstruction<OffsetBasedInstruction>(myInt(0)).offsetRegister() == 0);
  STATIC_CHECK(getInstruction<OffsetBasedInstruction>(myInt(1)).offsetRegister() == 1);
  STATIC_CHECK(getInstruction<OffsetBasedInstruction>(myInt(4)).offsetRegister() == 4);
  STATIC_CHECK(getInstruction<OffsetBasedInstruction>(myInt(15)).offsetRegister() == 15);

  STATIC_CHECK(getInstruction<OffsetBasedInstruction>(myInt(16)).offsetRegister() == 0);
  STATIC_CHECK(getInstruction<OffsetBasedInstruction>(myInt().shiftVal(4, 0xFF).shiftVal(0, 5)).offsetRegister() == 5);
  STATIC_CHECK(getInstruction<OffsetBasedInstruction>(myInt().shiftVal(4, 0xFF).shiftVal(28, 0xF).shiftVal(0, 12)).offsetRegister() == 12);

  STATIC_CHECK(getInstruction<OffsetBasedInstruction>(myInt(0xFABCFFFF)).offsetRegister() == 15);
  STATIC_CHECK(getInstruction<OffsetBasedInstruction>(myInt(0x123123F4)).offsetRegister() == 4);
}


TEST_CASE("OffsetBasedInstruction should have the expected shift type") {
  using enum OffsetBasedInstruction::ShiftType;

  STATIC_CHECK(getInstruction<OffsetBasedInstruction>(myInt().shiftVal(10, 0)).shiftType() == LogicalLeft);
  STATIC_CHECK(getInstruction<OffsetBasedInstruction>(myInt().shiftVal(10, 1)).shiftType() == LogicalRight);
  STATIC_CHECK(getInstruction<OffsetBasedInstruction>(myInt().shiftVal(10, 2)).shiftType() == ArithmeticLeft);
  STATIC_CHECK(getInstruction<OffsetBasedInstruction>(myInt().shiftVal(10, 3)).shiftType() == ArithmeticRight);

  STATIC_CHECK(getInstruction<OffsetBasedInstruction>(myInt(0xFFF)).shiftType() == ArithmeticRight);
  STATIC_CHECK(getInstruction<OffsetBasedInstruction>(myInt(0x312)).shiftType() == LogicalLeft);
  STATIC_CHECK(getInstruction<OffsetBasedInstruction>(myInt(0x823)).shiftType() == ArithmeticLeft);
}


TEST_CASE("OffsetBasedInstruction should have the expected shift amount") {
  STATIC_CHECK(getInstruction<OffsetBasedInstruction>(myInt().shiftVal(4, 1)).shiftAmount() == 1);
  STATIC_CHECK(getInstruction<OffsetBasedInstruction>(myInt().shiftVal(4, 5)).shiftAmount() == 5);
  STATIC_CHECK(getInstruction<OffsetBasedInstruction>(myInt().shiftVal(4, 0x3F)).shiftAmount() == 0x3F);
  STATIC_CHECK(getInstruction<OffsetBasedInstruction>(myInt().shiftVal(4, 0x7F)).shiftAmount() == 0x3F);

  STATIC_CHECK(getInstruction<OffsetBasedInstruction>(myInt(0x23F)).shiftAmount() == 0x23);
  STATIC_CHECK(getInstruction<OffsetBasedInstruction>(myInt(0x4411A)).shiftAmount() == 0x11);
  STATIC_CHECK(getInstruction<OffsetBasedInstruction>(myInt(0xFFAA401A)).shiftAmount() == 0x1);
}

TEST_CASE("MemoryTransferInstruction should have the expected immediate flag") {
  STATIC_CHECK(!getInstruction<MemoryTransferInstruction>(myInt().shiftVal(24, 1).neg()).immediate());
  STATIC_CHECK(getInstruction<MemoryTransferInstruction>(myInt().shiftVal(24, 1)).immediate());

  STATIC_CHECK(getInstruction<MemoryTransferInstruction>(myInt(0x1000000)).immediate());
  STATIC_CHECK(!getInstruction<MemoryTransferInstruction>(myInt(0x2FFEEFF)).immediate());
}

TEST_CASE("MemoryTransferInstruction should have the expected transfer size") {
  using enum MemoryTransferInstruction::TransferSize;

  STATIC_CHECK(getInstruction<MemoryTransferInstruction>(myInt().shiftVal(22, 0)).transferSize() == Byte);
  STATIC_CHECK(getInstruction<MemoryTransferInstruction>(myInt().shiftVal(22, 1)).transferSize() == Word);
  STATIC_CHECK(getInstruction<MemoryTransferInstruction>(myInt().shiftVal(22, 2)).transferSize() == DWord);

  STATIC_CHECK(getInstruction<MemoryTransferInstruction>(myInt(0x300000)).transferSize() == Byte);
  STATIC_CHECK(getInstruction<MemoryTransferInstruction>(myInt(0x400000)).transferSize() == Word);
  STATIC_CHECK(getInstruction<MemoryTransferInstruction>(myInt(0x800000)).transferSize() == DWord);
}

TEST_CASE("MemoryTransferInstruction should have the expected store bit") {
  STATIC_CHECK(!getInstruction<MemoryTransferInstruction>(myInt().shiftVal(21, 1).neg()).store());
  STATIC_CHECK(getInstruction<MemoryTransferInstruction>(myInt().shiftVal(21, 1)).store());

  STATIC_CHECK(!getInstruction<MemoryTransferInstruction>(myInt(0x14AAFF1)).store());
  STATIC_CHECK(getInstruction<MemoryTransferInstruction>(myInt(0x012F0000)).store());
}

TEST_CASE("MemoryTransferInstruction should have the expected subtract bit") {
  STATIC_CHECK(!getInstruction<MemoryTransferInstruction>(myInt().shiftVal(20, 1).neg()).subtract());
  STATIC_CHECK(getInstruction<MemoryTransferInstruction>(myInt().shiftVal(20, 1)).subtract());

  STATIC_CHECK(!getInstruction<MemoryTransferInstruction>(myInt(0x2AAFFB)).subtract());
  STATIC_CHECK(getInstruction<MemoryTransferInstruction>(myInt(0xF100000)).subtract());
}

TEST_CASE("MemoryTransferInstruction should have the expected source register") {
  STATIC_CHECK(getInstruction<MemoryTransferInstruction>(myInt().shiftVal(16, 1)).source() == 1);
  STATIC_CHECK(getInstruction<MemoryTransferInstruction>(myInt().shiftVal(16, 4)).source() == 4);
  STATIC_CHECK(getInstruction<MemoryTransferInstruction>(myInt().shiftVal(16, 11)).source() == 11);

  STATIC_CHECK(getInstruction<MemoryTransferInstruction>(myInt(0x450000)).source() == 5);
  STATIC_CHECK(getInstruction<MemoryTransferInstruction>(myInt(0xFFB1122)).source() == 11);
  STATIC_CHECK(getInstruction<MemoryTransferInstruction>(myInt(0x110AAAA)).source() == 0);
}

TEST_CASE("MemoryTransferInstruction should have the expected base register") {
  STATIC_CHECK(getInstruction<MemoryTransferInstruction>(myInt().shiftVal(12, 3)).base() == 3);
  STATIC_CHECK(getInstruction<MemoryTransferInstruction>(myInt().shiftVal(12, 5)).base() == 5);
  STATIC_CHECK(getInstruction<MemoryTransferInstruction>(myInt().shiftVal(12, 12)).base() == 12);

  STATIC_CHECK(getInstruction<MemoryTransferInstruction>(myInt(0x42FFF)).base() == 2);
  STATIC_CHECK(getInstruction<MemoryTransferInstruction>(myInt(0xABCD9000)).base() == 9);
  STATIC_CHECK(getInstruction<MemoryTransferInstruction>(myInt(0xFF120000)).base() == 0);
}

TEST_CASE("AluInstruction should have the expected opcode") {
  using enum AluInstruction::OpCode;

  STATIC_CHECK(getInstruction<AluInstruction>(myInt().shiftVal(20, 0)).opcode() == AND);
  STATIC_CHECK(getInstruction<AluInstruction>(myInt().shiftVal(20, 1)).opcode() == XOR);
  STATIC_CHECK(getInstruction<AluInstruction>(myInt().shiftVal(20, 2)).opcode() == OR);
  STATIC_CHECK(getInstruction<AluInstruction>(myInt().shiftVal(20, 3)).opcode() == ADD);
  STATIC_CHECK(getInstruction<AluInstruction>(myInt().shiftVal(20, 4)).opcode() == SUB);
  STATIC_CHECK(getInstruction<AluInstruction>(myInt().shiftVal(20, 5)).opcode() == RSUB);
  STATIC_CHECK(getInstruction<AluInstruction>(myInt().shiftVal(20, 6)).opcode() == ADDC);
  STATIC_CHECK(getInstruction<AluInstruction>(myInt().shiftVal(20, 7)).opcode() == SUBC);
  STATIC_CHECK(getInstruction<AluInstruction>(myInt().shiftVal(20, 8)).opcode() == RSUBC);
  STATIC_CHECK(getInstruction<AluInstruction>(myInt().shiftVal(20, 9)).opcode() == TST);
  STATIC_CHECK(getInstruction<AluInstruction>(myInt().shiftVal(20, 10)).opcode() == TEQ);
  STATIC_CHECK(getInstruction<AluInstruction>(myInt().shiftVal(20, 11)).opcode() == CMP);
  STATIC_CHECK(getInstruction<AluInstruction>(myInt().shiftVal(20, 12)).opcode() == CMN);
  STATIC_CHECK(getInstruction<AluInstruction>(myInt().shiftVal(20, 13)).opcode() == MVN);
}

TEST_CASE("AluInstruction should have the expected source register") {
  STATIC_CHECK(getInstruction<AluInstruction>(myInt().shiftVal(16, 4)).source() == 4);
  STATIC_CHECK(getInstruction<AluInstruction>(myInt().shiftVal(16, 5)).source() == 5);
  STATIC_CHECK(getInstruction<AluInstruction>(myInt().shiftVal(16, 9)).source() == 9);

  STATIC_CHECK(getInstruction<AluInstruction>(myInt(0x450000)).source() == 5);
  STATIC_CHECK(getInstruction<AluInstruction>(myInt(0xFFB1122)).source() == 11);
  STATIC_CHECK(getInstruction<AluInstruction>(myInt(0x110AAAA)).source() == 0);
}

TEST_CASE("AluInstruction should have the expected destination register") {
  STATIC_CHECK(getInstruction<AluInstruction>(myInt().shiftVal(12, 3)).destination() == 3);
  STATIC_CHECK(getInstruction<AluInstruction>(myInt().shiftVal(12, 5)).destination() == 5);
  STATIC_CHECK(getInstruction<AluInstruction>(myInt().shiftVal(12, 12)).destination() == 12);

  STATIC_CHECK(getInstruction<AluInstruction>(myInt(0x42FFF)).destination() == 2);
  STATIC_CHECK(getInstruction<AluInstruction>(myInt(0xABCD9000)).destination() == 9);
  STATIC_CHECK(getInstruction<AluInstruction>(myInt(0xFF120000)).destination() == 0);
}

TEST_CASE("BranchInstruction should have the link flag") {
  STATIC_CHECK(!getInstruction<BranchInstruction>(myInt().shiftVal(24, 1).neg()).link());
  STATIC_CHECK(getInstruction<BranchInstruction>(myInt().shiftVal(24, 1)).link());
}

TEST_CASE("BranchInstruction should have the expected offset") {
  STATIC_CHECK(getInstruction<BranchInstruction>(myInt(234)).offset() == 234);
  STATIC_CHECK(getInstruction<BranchInstruction>(myInt().shiftVal(12, 12)).offset() == 12 * 4096);
}

TEST_CASE("StackInstruction should have the expected opcode") {
  using enum StackInstruction::OpCode;

  STATIC_CHECK(getInstruction<StackInstruction>(myInt().shiftVal(24, 0)).opcode() == POP);
  STATIC_CHECK(getInstruction<StackInstruction>(myInt().shiftVal(24, 1)).opcode() == PUSH);
}

TEST_CASE("StackInstruction should have the expected register list") {
  constexpr auto registerList = getInstruction<StackInstruction>(myInt().shiftVal(8, 0b110011)).registerList();
  STATIC_CHECK(registerList & (1 << 0));
  STATIC_CHECK(registerList & (1 << 1));
  STATIC_CHECK(!(registerList & (1 << 2)));
  STATIC_CHECK(!(registerList & (1 << 3)));
  STATIC_CHECK(registerList & (1 << 4));
  STATIC_CHECK(registerList & (1 << 5));
}
