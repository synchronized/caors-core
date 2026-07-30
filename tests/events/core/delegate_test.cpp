// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/events.h"

#include <catch2/catch_all.hpp>

namespace {

[[nodiscard]] int double_value(int value) { return value * 2; }

struct receiver {
  void add(int value) { total += value; }
  [[nodiscard]] int plus(int value) const { return base + value; }

  int total = 0;
  int base = 10;
};

template <typename Target>
concept accepts_temporary_callable = requires(Target& target) { target.connect([] {}); };

static_assert(!accepts_temporary_callable<caors::events::delegate<void()>>);
static_assert(!accepts_temporary_callable<caors::events::signal<void()>>);

} // namespace

TEST_CASE("events - delegate invokes free functions", "[events]") {
  caors::events::delegate<int(int)> callback;
  callback.connect<&double_value>();

  REQUIRE(callback);
  CHECK(callback(7) == 14);
}

TEST_CASE("events - delegate invokes member functions", "[events]") {
  receiver target{};

  caors::events::delegate<void(int)> add;
  add.connect<&receiver::add>(&target);
  add(3);
  add(4);

  CHECK(target.total == 7);

  caors::events::delegate<int(int)> plus;
  plus.connect<&receiver::plus>(&target);

  CHECK(plus(5) == 15);
}

TEST_CASE("events - delegate invokes function object lvalues", "[events]") {
  int total = 0;
  auto accumulate = [&](int value) { total += value; };

  caors::events::delegate<void(int)> callback;
  callback.connect(accumulate);

  callback(2);
  callback(5);

  CHECK(total == 7);
}
