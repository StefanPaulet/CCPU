//
// Created by stefan on 7/9/24.
//

#include <catch2/catch_test_macros.hpp>
#include <expected>
auto dummyTest() -> bool { return true; }

TEST_CASE("Dummy test passes", "true") {
  REQUIRE(dummyTest() == true);
}
