// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/tasks.h"

#include <catch2/catch_all.hpp>

TEST_CASE("tasks - inline executor runs submitted task immediately", "[tasks]") {
  caors::tasks::inline_executor executor;
  caors::tasks::executor handle = executor.get_executor();
  int value = 0;

  CHECK_FALSE(handle.post(caors::tasks::task{[&] { value = 7; }}));

  CHECK(value == 7);
}

TEST_CASE("tasks - inline executor reports empty task", "[tasks]") {
  caors::tasks::inline_executor executor;
  caors::tasks::executor handle = executor.get_executor();

  CHECK(handle.post(caors::tasks::task{}) ==
        caors::error{caors::error_domain::core, caors::errc::invalid_argument});
}

TEST_CASE("tasks - executor interfaces are available from module namespace", "[tasks]") {
  caors::tasks::inline_executor executor;
  caors::tasks::executor base = executor.get_executor();
  bool called = false;

  CHECK_FALSE(base.post(caors::tasks::task{[&] { called = true; }}));

  CHECK(called);
}
