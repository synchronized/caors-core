// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/tasks.h"

#include "caors/memory/tracking.h"

#include <catch2/catch_all.hpp>

#include <memory>

TEST_CASE("tasks - task runs move only callable", "[tasks]") {
  auto owned = std::make_unique<int>(0);
  int* observed = owned.get();

  caors::tasks::task current{[owned = std::move(owned)]() mutable { *owned = 42; }};

  CHECK(static_cast<bool>(current));
  CHECK_FALSE(current.run());
  CHECK(*observed == 42);
}

TEST_CASE("tasks - task returns callable error", "[tasks]") {
  caors::tasks::task current{
      [] { return caors::error{caors::error_domain::core, caors::errc::unsupported}; }};

  CHECK(current.run() == caors::error{caors::error_domain::core, caors::errc::unsupported});
}

TEST_CASE("tasks - empty task reports invalid argument", "[tasks]") {
  caors::tasks::task current;

  CHECK(current.run() == caors::error{caors::error_domain::core, caors::errc::invalid_argument});
}

TEST_CASE("tasks - task uses configured memory resource", "[tasks]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};

  {
    caors::tasks::task current{&tracking, [] {}};
    CHECK(current.resource() == &tracking);
    CHECK(tracking.statistics().bytes_in_use > 0);
  }

  CHECK(tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("tasks - task is available from module namespace", "[tasks]") {
  bool called = false;
  caors::tasks::task current{[&] { called = true; }};

  CHECK_FALSE(current());
  CHECK(called);
}
