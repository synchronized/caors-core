// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/events.h"

#include "caors/memory/tracking.h"

#include <catch2/catch_all.hpp>

namespace {

struct value_event {
  int value = 0;
};

struct other_event {
  int value = 0;
};

} // namespace

TEST_CASE("events - sink connects without exposing emit", "[events]") {
  caors::events::signal<void(int)> changed;
  caors::events::sink<void(int)> listeners{changed};
  int total = 0;

  auto add = [&](int value) { total += value; };
  caors::events::connection current = listeners.connect(add);

  changed.emit(3);

  CHECK(total == 3);
  CHECK(listeners.size() == 1);

  REQUIRE(listeners.disconnect(current));
  changed.emit(3);

  CHECK(total == 3);
  CHECK(listeners.empty());
}

TEST_CASE("events - dispatcher enqueues events until update", "[events]") {
  caors::events::dispatcher dispatcher;
  int total = 0;

  auto add = [&](const value_event& event) { total += event.value; };
  caors::events::connection current = dispatcher.sink<value_event>().connect(add);

  dispatcher.enqueue(value_event{3});

  CHECK(total == 0);
  CHECK(dispatcher.size<value_event>() == 1);
  CHECK(dispatcher.size() == 1);

  dispatcher.update<value_event>();

  CHECK(total == 3);
  CHECK(dispatcher.empty<value_event>());
  CHECK(dispatcher.empty());

  CHECK(current.disconnect());
}

TEST_CASE("events - dispatcher update by type only dispatches that type", "[events]") {
  caors::events::dispatcher dispatcher;
  int first_total = 0;
  int second_total = 0;

  auto add_first = [&](const value_event& event) { first_total += event.value; };
  auto add_second = [&](const other_event& event) { second_total += event.value; };

  caors::events::connection first = dispatcher.sink<value_event>().connect(add_first);
  caors::events::connection second = dispatcher.sink<other_event>().connect(add_second);

  dispatcher.enqueue(value_event{2});
  dispatcher.enqueue(other_event{5});
  dispatcher.update<value_event>();

  CHECK(first_total == 2);
  CHECK(second_total == 0);
  CHECK(dispatcher.empty<value_event>());
  CHECK(dispatcher.size<other_event>() == 1);

  dispatcher.update();

  CHECK(second_total == 5);
  CHECK(dispatcher.empty());

  CHECK(first.disconnect());
  CHECK(second.disconnect());
}

TEST_CASE("events - dispatcher keeps queued event order for one type", "[events]") {
  caors::events::dispatcher dispatcher;
  caors::vector<int> observed;

  auto record = [&](const value_event& event) { observed.push_back(event.value); };
  caors::events::connection current = dispatcher.sink<value_event>().connect(record);

  dispatcher.enqueue(value_event{1});
  dispatcher.enqueue(value_event{2});
  dispatcher.emplace<value_event>(3);
  dispatcher.update();

  REQUIRE(observed.size() == 3);
  CHECK(observed[0] == 1);
  CHECK(observed[1] == 2);
  CHECK(observed[2] == 3);

  CHECK(current.disconnect());
}

TEST_CASE("events - dispatcher clears queued events without dispatching", "[events]") {
  caors::events::dispatcher dispatcher;
  int total = 0;

  auto add = [&](const value_event& event) { total += event.value; };
  caors::events::connection current = dispatcher.sink<value_event>().connect(add);

  dispatcher.enqueue(value_event{3});
  dispatcher.clear<value_event>();
  dispatcher.update<value_event>();

  CHECK(total == 0);
  CHECK(dispatcher.empty<value_event>());

  dispatcher.enqueue(value_event{4});
  dispatcher.clear();
  dispatcher.update();

  CHECK(total == 0);
  CHECK(dispatcher.empty());

  CHECK(current.disconnect());
}

TEST_CASE("events - dispatcher trigger emits immediately", "[events]") {
  caors::events::dispatcher dispatcher;
  int total = 0;

  auto add = [&](const value_event& event) { total += event.value; };
  caors::events::connection current = dispatcher.sink<value_event>().connect(add);

  dispatcher.trigger(value_event{7});

  CHECK(total == 7);
  CHECK(dispatcher.empty<value_event>());

  CHECK(current.disconnect());
}

TEST_CASE("events - dispatcher uses configured memory resource", "[events]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};

  {
    caors::events::dispatcher dispatcher{&tracking};
    CHECK(dispatcher.resource() == &tracking);

    auto ignore = [](const value_event&) {};
    caors::events::connection current = dispatcher.sink<value_event>().connect(ignore);
    dispatcher.enqueue(value_event{1});
    dispatcher.update();
    CHECK(current.disconnect());
  }

  CHECK(tracking.statistics().bytes_in_use == 0);
}
