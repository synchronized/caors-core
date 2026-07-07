// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/tasks.h"

#include <catch2/catch_all.hpp>

TEST_CASE("tasks - inline executor runs submitted task immediately", "[tasks]") {
  caors::tasks::inline_executor executor;
  int value = 0;

  CHECK_FALSE(executor.submit(caors::tasks::task{[&] { value = 7; }}));

  CHECK(value == 7);
}

TEST_CASE("tasks - inline executor reports empty task", "[tasks]") {
  caors::tasks::inline_executor executor;

  CHECK(executor.submit(caors::tasks::task{}) ==
        caors::error{caors::error_domain::core, caors::errc::invalid_argument});
}

TEST_CASE("tasks - root executor aliases are available", "[tasks]") {
  caors::inline_task_executor executor;
  caors::task_executor* base = &executor;
  bool called = false;

  CHECK_FALSE(base->submit(caors::task{[&] { called = true; }}));

  CHECK(called);
}
