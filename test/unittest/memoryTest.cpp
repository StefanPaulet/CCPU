//
// Created by stefan on 2/16/25.
//

#include <catch2/catch_test_macros.hpp>
#include <memory/RAM.hpp>
#include "Utils.hpp"

namespace {
using ccpu::RandomAccessMemory;
using ccpu::IsAddressableSize;
using std::array;
using std::tuple;
using MemoryAccessViolation = ccpu::MemoryAccessViolation;

struct Load {
  uint32_t address;
  uint8_t size;
  uint32_t value;
};

template <size_t size>
using LoadArray = array<Load, size>;

constexpr auto ramSize = 4096;
using RAM = RandomAccessMemory<ramSize>;

template <LoadArray loads>
constexpr auto getMemory() -> RAM {
  RAM ram {};
  for (auto [address, size, value] : loads) {
    switch (size) {
      case 1: {
        ram.writeByte(address, value);
        break;
      }
      case 2: {
        ram.writeWord(address, value);
        break;
      }
      case 4: {
        ram.writeDWord(address, value);
        break;
      }
      default: {
        assert(false && "Should never pass a value of size different than 1, 2 or 4");
      }
    }
  }
  return ram;
}
} // namespace

TEST_CASE("Memory should allow reads and writes") {
  CONSTEXPR auto ram = getMemory<{
    Load{0, 1, 1},
    Load{1, 2, 256},
    Load{3, 4, 0xABCDEF01}
  }>();
  ASSERT(ram.readByte(0) == 1);
  ASSERT(ram.readWord(1) == 256);
  ASSERT(ram.readDWord(3) == 0xABCDEF01);
  ASSERT(ram.readByte(3) == 0x01);
  ASSERT(ram.readWord(3) == 0xEF01);
}

TEST_CASE("Memory is persistent") {
  CONSTEXPR auto ram = getMemory<{
    Load{0, 1, 1},
    Load{0, 1, 4},

    Load{2, 2, 0xBCDE},
    Load{2, 2, 0xEDCB},

    Load{8, 4, 0xAFAFAFAF},
    Load{8, 4, 0x00AA00AA}
  }>();
  ASSERT(ram.readByte(0) == 4);
  ASSERT(ram.readWord(2) == 0xEDCB);
  ASSERT(ram.readDWord(8) == 0x00AA00AA);
}

TEST_CASE("Memory reads and writes can overlap") {
  CONSTEXPR auto ram = getMemory<{
    Load{0, 4, 0x000000AF},
    Load{1, 2, 0xBCBC},
    Load{3, 1, 0x12}
  }>();

  ASSERT(ram.readDWord(0) == 0x12BCBCAF);
  ASSERT(ram.readWord(1) == 0xBCBC);
  ASSERT(ram.readByte(3) == 0x12);

  CONSTEXPR auto ram2 = getMemory<{
    Load{0, 1, 0x11},
    Load{1, 4, 0x00456700},
    Load{0, 2, 0xBCBC}
  }>();

  ASSERT(ram2.readByte(0) == 0xBC);
  ASSERT(ram2.readWord(1) == 0x67BC);
}

TEST_CASE("Invalid memory reads should signal failure") {
  using enum MemoryAccessViolation;

  CONSTEXPR auto ram = getMemory<LoadArray<0>{}>();
  ASSERT(ram.readByte(ramSize).error() == InvalidRead);
  ASSERT(ram.readWord(ramSize - 1).error() == InvalidRead);
  ASSERT(ram.readDWord(ramSize - 3).error() == InvalidRead);
}

TEST_CASE("Invalid memory write should signal failure") {
  using enum MemoryAccessViolation;

  CONSTEXPR auto checkInvalidRamWrite = []<auto callable, uint16_t address>() {
    auto ram = getMemory<LoadArray<0>{}>();
    ASSERT((ram.*callable)(address, 0).error() == InvalidWrite);
  };
  checkInvalidRamWrite.operator()<&RAM::writeByte, ramSize>();
  checkInvalidRamWrite.operator()<&RAM::writeWord, ramSize - 1>();
  checkInvalidRamWrite.operator()<&RAM::writeDWord, ramSize - 3>();
}