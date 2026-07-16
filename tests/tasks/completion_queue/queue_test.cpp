// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/tasks.h"

#include "caors/core/containers.h"
#include "caors/memory/tracking.h"

#include <catch2/catch_all.hpp>

TEST_CASE("tasks - completion queue drains tasks in queued order", "[tasks]") {
  caors::tasks::completion_queue queue;
  caors::vector<int> observed;

  CHECK(queue.empty());
  CHECK_FALSE(queue.push(caors::tasks::task{[&] { observed.push_back(1); }}));
  CHECK_FALSE(queue.push(caors::tasks::task{[&] { observed.push_back(2); }}));

  CHECK(queue.size() == 2);
  CHECK_FALSE(queue.empty());

  CHECK(queue.drain() == 2);

  REQUIRE(observed.size() == 2);
  CHECK(observed[0] == 1);
  CHECK(observed[1] == 2);
  CHECK(queue.empty());
}

TEST_CASE("tasks - completion queue rejects empty task", "[tasks]") {
  caors::tasks::completion_queue queue;

  CHECK(queue.push(caors::tasks::task{}) ==
        caors::error{caors::error_domain::core, caors::errc::invalid_argument});
  CHECK(queue.empty());
}

TEST_CASE("tasks - completion queue keeps reentrant push for next drain", "[tasks]") {
  caors::tasks::completion_queue queue;
  int value = 0;

  CHECK_FALSE(queue.push(caors::tasks::task{[&] {
    value += 1;
    CHECK_FALSE(queue.push(caors::tasks::task{[&] { value += 10; }}));
  }}));

  CHECK(queue.drain() == 1);
  CHECK(value == 1);
  CHECK(queue.size() == 1);

  CHECK(queue.drain() == 1);
  CHECK(value == 11);
  CHECK(queue.empty());
}

TEST_CASE("tasks - completion queue uses configured memory resource", "[tasks]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};

  {
    caors::tasks::completion_queue queue{&tracking};
    CHECK(queue.resource() == &tracking);

    CHECK_FALSE(queue.push(caors::tasks::task{&tracking, [] {}}));
    CHECK(queue.drain() == 1);
  }

  CHECK(tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("tasks - completion queue is available from module namespace", "[tasks]") {
  caors::tasks::completion_queue queue;
  bool called = false;

  CHECK_FALSE(queue.push(caors::tasks::task{[&] { called = true; }}));
  CHECK(queue.drain() == 1);
  CHECK(called);
}
