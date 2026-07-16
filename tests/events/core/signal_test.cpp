// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/events.h"

#include <catch2/catch_all.hpp>

namespace {

void add_one(int& value) { ++value; }

struct counter {
  void add(int value) { total += value; }

  int total = 0;
};

} // namespace

TEST_CASE("events - signal emits listeners in connection order", "[events]") {
  caors::events::signal<void(int&)> value_changed;
  int value = 0;

  auto multiply = [](int& current) { current *= 2; };

  caors::events::connection first = value_changed.connect<&add_one>();
  caors::events::connection second = value_changed.connect(multiply);

  value_changed.emit(value);

  CHECK(value == 2);
  CHECK(value_changed.size() == 2);

  REQUIRE(first.disconnect());
  value_changed.emit(value);

  CHECK(value == 4);
  CHECK(value_changed.size() == 1);

  REQUIRE(second.disconnect());
  CHECK(value_changed.empty());
}

TEST_CASE("events - signal can connect member functions", "[events]") {
  caors::events::signal<void(int)> value_changed;
  counter target{};

  caors::events::connection current = value_changed.connect<&counter::add>(&target);

  value_changed(3);
  value_changed(4);

  CHECK(target.total == 7);
  CHECK(current.disconnect());
}

TEST_CASE("events - scoped connection disconnects on scope exit", "[events]") {
  caors::events::signal<void(int)> value_changed;
  int total = 0;
  auto add = [&](int value) { total += value; };

  {
    caors::events::scoped_connection scoped{value_changed.connect(add)};
    value_changed.emit(5);
  }

  value_changed.emit(5);

  CHECK(total == 5);
  CHECK(value_changed.empty());
}

TEST_CASE("events - scoped connection release keeps listener connected", "[events]") {
  caors::events::signal<void(int)> value_changed;
  int total = 0;
  auto add = [&](int value) { total += value; };
  caors::events::connection released;

  {
    caors::events::scoped_connection scoped{value_changed.connect(add)};
    released = scoped.release();
  }

  value_changed.emit(5);

  CHECK(total == 5);
  CHECK(value_changed.size() == 1);
  REQUIRE(released.disconnect());
  CHECK(value_changed.empty());
}

TEST_CASE("events - signal defers disconnect cleanup while emitting", "[events]") {
  caors::events::signal<void()> changed;
  int first_count = 0;
  int second_count = 0;
  caors::events::connection first;

  auto disconnect_self = [&]() {
    ++first_count;
    CHECK(first.disconnect());
  };
  auto second = [&]() { ++second_count; };

  first = changed.connect(disconnect_self);
  caors::events::connection second_connection = changed.connect(second);

  changed.emit();
  changed.emit();

  CHECK(first_count == 1);
  CHECK(second_count == 2);
  CHECK(changed.size() == 1);

  CHECK(second_connection.disconnect());
}
