// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory.h"
#include "caors/tasks.h"

#include <catch2/catch_all.hpp>

TEST_CASE("tasks - execution context polls a snapshot batch", "[tasks][asio]") {
  caors::tasks::execution_context context;
  caors::tasks::executor executor = context.get_executor();
  int observed = 0;

  CHECK_FALSE(executor.post(caors::tasks::task{[&] {
    ++observed;
    CHECK_FALSE(executor.post(caors::tasks::task{[&] { observed += 10; }}));
  }}));

  CHECK(context.poll() == 1);
  CHECK(observed == 1);
  CHECK(context.poll() == 1);
  CHECK(observed == 11);
}

TEST_CASE("tasks - executor remains safely invalid after context destruction", "[tasks][asio]") {
  caors::tasks::executor executor;
  {
    caors::tasks::execution_context context;
    executor = context.get_executor();
    CHECK(executor.valid());
  }

  CHECK_FALSE(executor.valid());
  CHECK(executor.post(caors::tasks::task{[] {}}) ==
        caors::error{caors::error_domain::core, caors::errc::frozen});
}

TEST_CASE("tasks - empty executor rejects submissions", "[tasks][asio]") {
  caors::tasks::executor executor;

  CHECK_FALSE(executor.valid());
  CHECK(executor.post(caors::tasks::task{[] {}}) ==
        caors::error{caors::error_domain::core, caors::errc::frozen});
}

TEST_CASE("tasks - execution context supports stop and restart", "[tasks][asio]") {
  caors::tasks::execution_context context;
  caors::tasks::executor executor = context.get_executor();

  context.stop();
  CHECK(context.stopped());
  CHECK_FALSE(executor.post(caors::tasks::task{[] {}}));
  CHECK(context.poll() == 0);

  context.restart();
  CHECK_FALSE(context.stopped());
  CHECK(context.poll() == 1);
}

TEST_CASE("tasks - execution context reports and clears pending tasks", "[tasks][asio]") {
  caors::tasks::execution_context context;
  caors::tasks::executor executor = context.get_executor();
  int observed = 0;

  CHECK_FALSE(executor.post(caors::tasks::task{[&] { ++observed; }}));
  CHECK_FALSE(executor.post(caors::tasks::task{[&] { ++observed; }}));
  CHECK(context.pending() == 2);

  context.clear();
  CHECK(context.pending() == 0);
  CHECK(context.poll() == 0);
  CHECK(observed == 0);

  CHECK_FALSE(executor.post(caors::tasks::task{[&] { ++observed; }}));
  CHECK(context.poll() == 1);
  CHECK(observed == 1);
}

TEST_CASE("tasks - execution context drain accepts batches after becoming empty", "[tasks][asio]") {
  caors::tasks::execution_context context;
  caors::tasks::executor executor = context.get_executor();
  int observed = 0;

  CHECK(context.drain() == 0);
  CHECK_FALSE(executor.post(caors::tasks::task{[&] { ++observed; }}));
  CHECK(context.drain() == 1);
  CHECK(observed == 1);

  CHECK(context.drain() == 0);
  CHECK_FALSE(executor.post(caors::tasks::task{[&] { ++observed; }}));
  CHECK(context.drain() == 1);
  CHECK(observed == 2);
}

TEST_CASE("tasks - executor is a one pointer handle", "[tasks][asio]") {
  STATIC_CHECK(sizeof(caors::tasks::executor) == sizeof(void*));
}

TEST_CASE("tasks - execution context uses configured long lived resource", "[tasks][asio]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};
  caors::tasks::executor executor;

  {
    caors::tasks::execution_context context{&tracking};
    CHECK(context.resource() == &tracking);
    executor = context.get_executor();
  }

  CHECK(tracking.statistics().bytes_in_use > 0);
  executor = {};
  CHECK(tracking.statistics().bytes_in_use == 0);
}
