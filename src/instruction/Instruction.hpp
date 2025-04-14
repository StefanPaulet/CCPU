//
// Created by stefan on 2/18/25.
//

#pragma once

#include <cstdint>

namespace ccpu {
/**
 * 32 bit instruction
 * 0000 0000 0000 0000 0000 0000 0000 0000
 * bits 31-28 = condition
 * bits 27-25 = instruction type:
 *  -000 = memory transfer
 *  -001 = alu operation
 *  -010 = branch
 *  -011 = stack operation
 *  -100...111 = invalid
 */

enum struct Condition : uint8_t {
  EQ = 0b0000,  // Zero set (equal)
  NE = 0b0001,  // Zero clear (not equal)
  NG = 0b0010,  // Sign set (negative)
  PO = 0b0011,  // Sign clear (positive or zero)
  VS = 0b0100,  // Overflow set (overflow)
  VC = 0b0101,  // Overflow clear (no overflow)

  AL = 0b1110,  // Always
  NV = 0b1111   // Reserved
};

enum struct InstructionType : uint8_t {
  MemoryTransfer = 0b00,
  AluOperation   = 0b01,
  Branch         = 0b10,
  StackOperation = 0b11
};

struct Instruction {
  [[nodiscard]] constexpr auto getCondition() const noexcept -> Condition { return static_cast<Condition>((val >> 28) & 0xF); }
  [[nodiscard]] constexpr auto isUnconditional() const noexcept -> bool { return getCondition() == Condition::AL; }
  [[nodiscard]] constexpr auto type() const noexcept -> InstructionType { return static_cast<InstructionType>((val >> 25) & 0x7); }
  [[nodiscard]] constexpr auto testBit(uint8_t bit) const noexcept -> bool { return val & (uint32_t{1} << bit); }
  uint32_t val;
};

/**
 *  For all operations involving offsets (MemoryTransfer and Alu) offset is specified as follows:
 *  -For register with shift:
 *   -bits 11-4:
 *    -bits 11-10 = shift type:
 *     -00 = logical left
 *     -01 = logical right
 *     -10 = arithmetic left
 *     -11 = arithmetic right
 *    -bits 9-4 = shift amount
 *   -bits 3-0 = register to shift
 *  -For immediate value:
 *   -bits 11-0 = immediate value
 */

struct OffsetBasedInstruction : public Instruction {
  enum struct ShiftType : uint8_t {
    LogicalLeft     = 0b00,
    LogicalRight    = 0b01,
    ArithmeticLeft  = 0b10,
    ArithmeticRight = 0b11
  };

  constexpr explicit OffsetBasedInstruction(Instruction instruction) noexcept : Instruction{instruction} {}

  [[nodiscard]] constexpr auto offset() const noexcept -> uint16_t { return val & 0X0FFF; }
  [[nodiscard]] constexpr auto offsetRegister() const noexcept -> uint8_t { return val & 0xF; }

  [[nodiscard]] constexpr auto offsetShift() const noexcept -> uint8_t { return (val >> 4) & 0xFF; }
  [[nodiscard]] constexpr auto shiftType() const noexcept -> ShiftType { return static_cast<ShiftType>((offsetShift() >> 6) & 0b11); }
  [[nodiscard]] constexpr auto shiftAmount() const noexcept -> uint8_t { return offsetShift() & 0x3F; }
};

/**
 * 4 bit condition | 3 bit instruction type | 25 bit of information
 *      0000       |           000          | 0 0000 0000 0000 0000 0000 0000
 * bit 24 = use register/immediate:
 *  -0 = bits 11-0 represent register + shift
 *  -1 = bits 11-0 represent an immediate value
 * bits 23-22 = size of transfer:
 *  -00 = byte
 *  -01 = word
 *  -10 = dword
 *  -11 = reserved
 * bit 21 = load/store:
 *  -0 = load from memory => bits 19-16 are destination register
 *  -1 = store in memory => base +/- offset is destination location
 * bit 20 = subtract/add:
 *  -0 = add offset to base register
 *  -1 = subtract offset from base register
 * bits 19-16 = source register
 * bits 15-12 = base register
 * bits 11-0 = offset (see above)
 */

struct MemoryTransferInstruction : public OffsetBasedInstruction {
  enum struct TransferSize : uint8_t {
    Byte = 0b00,
    Word = 0b01,
    DWord = 0b10
  };

  using OffsetBasedInstruction::OffsetBasedInstruction;

  [[nodiscard]] constexpr auto immediate() const noexcept { return testBit(24); }
  [[nodiscard]] constexpr auto transferSize() const noexcept -> TransferSize { return static_cast<TransferSize>((val >> 22) & 0b11); }
  [[nodiscard]] constexpr auto store() const noexcept { return testBit(21); }
  [[nodiscard]] constexpr auto subtract() const noexcept { return testBit(20); }
  [[nodiscard]] constexpr auto source() const noexcept -> uint8_t { return (val >> 16) & 0xF; }
  [[nodiscard]] constexpr auto base() const noexcept -> uint8_t { return (val >> 12) & 0xF; }
};


/**
 * 4 bit condition | 3 bit instruction type | 25 bit of information
 *      0000       |           001          | 0 0000 0000 0000 0000 0000 0000
 * bit 24 = use register/immediate:
 *  -0 = bits 11-0 represent register + shift
 *  -1 = bits 11-0 represent an immediate value
 * bits 23-20 = opcode of operation:
 *  -0000 = AND
 *  -0001 = XOR
 *  -0010 = OR
 *  -0011 = ADD
 *  -0100 = SUB (Reg - Op)
 *  -0101 = RSUB (Op - Reg)
 *  -0110 = ADD with carry (Reg + Op + Carry)
 *  -0111 = SUB with carry (Reg - Op + Carry - 1)
 *  -1000 = RSUB with carry (Op - Reg + Carry - 1)
 *  -1001 = TST (set flags for Reg AND Op)
 *  -1010 = TEQ (set flags for Reg XOR Op)
 *  -1011 = CMP (set flags for Reg - Op)
 *  -1100 = CMN (set flags for Reg + Op)
 *  -1101 = MVN (Result = NOT Op)
 *  -1110,1111 = unused
 * bits 19-16 = source register (Reg)
 * bits 15-12 = destination register (Result)
 * bits 11-0 = offset (see MemoryInstruction specification above)
 */

struct AluInstruction : public OffsetBasedInstruction {
  enum struct OpCode : uint8_t {
    AND   = 0b0000,
    XOR   = 0b0001,
    OR    = 0b0010,
    ADD   = 0b0011,
    SUB   = 0b0100,
    RSUB  = 0b0101,
    ADDC  = 0b0110,
    SUBC  = 0b0111,
    RSUBC = 0b1000,
    TST   = 0b1001,
    TEQ   = 0b1010,
    CMP   = 0b1011,
    CMN   = 0b1100,
    MVN   = 0b1101,
    MOV   = 0b1110
  };

  using OffsetBasedInstruction::OffsetBasedInstruction;

  [[nodiscard]] constexpr auto immediate() const noexcept { return testBit(24); }
  [[nodiscard]] constexpr auto opcode() const noexcept -> OpCode { return static_cast<OpCode>((val >> 20) & 0xF); }
  [[nodiscard]] constexpr auto source() const noexcept -> uint8_t { return (val >> 16) & 0xF; }
  [[nodiscard]] constexpr auto destination() const noexcept -> uint8_t { return (val >> 12) & 0xF; }
};

/**
 * 4 bit condition | 3 bit instruction type | 25 bit of information
 *      0000       |           010          | 0 0000 0000 0000 0000 0000 0000
 * bit 24 = link/not link:
 *  -0 = branch and do not link
 *  -1 = branch and link (put program counter in a specific register)
 * bits 23-0 = immediate value
 */
struct BranchInstruction : public Instruction {
  constexpr explicit BranchInstruction(Instruction instruction) noexcept : Instruction{instruction} {}
  [[nodiscard]] constexpr auto link() const noexcept -> bool { return testBit(24); }
  [[nodiscard]] constexpr auto offset() const noexcept -> uint32_t { return val & 0x7FFFFF; }
};

/**
 * 4 bit condition | 3 bit instruction type | 25 bit of information
 *      0000       |           011          | 0 0000 0000 0000 0000 0000 0000
 * bit 24 = push/pop:
 *  -0 = pop into registers
 *  -1 = push from registers
 * bits 23-8 = list of registers:
 *  -for each register, if bit from list is set than register is part of operation
 * bits 7-0 = unused
 */
struct StackInstruction : public Instruction {
  enum struct OpCode : uint8_t {
    POP  = 0b0,
    PUSH = 0b1
  };

  constexpr explicit StackInstruction(Instruction instruction) noexcept : Instruction{instruction} {}

  [[nodiscard]] constexpr auto opcode() const noexcept -> OpCode { return static_cast<OpCode>((val >> 24) & 1); }
  [[nodiscard]] constexpr auto registerList() const noexcept -> uint16_t { return (val >> 8) & 0xFFFF; }
};
} // namespace ccpu
