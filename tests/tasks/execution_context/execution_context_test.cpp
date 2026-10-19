// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory.h"
#include "caors/tasks.h"

#include <catch2/catch_all.hpp>
#include <memory>

TEST_CASE("tasks - execution context polls a snapshot batch", "[tasks]") {
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

TEST_CASE("tasks - executor remains safely invalid after context destruction", "[tasks]") {
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

TEST_CASE("tasks - empty executor rejects submissions", "[tasks]") {
  caors::tasks::executor executor;

  CHECK_FALSE(executor.valid());
  CHECK(executor.post(caors::tasks::task{[] {}}) ==
        caors::error{caors::error_domain::core, caors::errc::frozen});
}

TEST_CASE("tasks - execution context supports stop and restart", "[tasks]") {
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

TEST_CASE("tasks - execution context reports and clears pending tasks", "[tasks]") {
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

TEST_CASE("tasks - execution context drain accepts batches after becoming empty", "[tasks]") {
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

TEST_CASE("tasks - executor is a one pointer handle", "[tasks]") {
  STATIC_CHECK(sizeof(caors::tasks::executor) == sizeof(void*));
}

TEST_CASE("tasks - execution context uses configured long lived resource", "[tasks]") {
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

TEST_CASE("tasks - clear destroys captures outside queue lock", "[tasks]") {
  caors::tasks::execution_context context;
  auto executor = context.get_executor();
  int completed = 0;
  struct repost final {
    caors::tasks::executor target;
    int* completed;
    ~repost() {
      (void)target.post(caors::tasks::task{[value = completed] { ++*value; }});
    }
  };
  auto captured = std::make_unique<repost>();
  captured->target = executor;
  captured->completed = &completed;
  REQUIRE_FALSE(executor.post(caors::tasks::task{[owner = std::move(captured)] {}}));
  context.clear();
  CHECK(context.pending() == 1);
  CHECK(context.drain() == 1);
  CHECK(completed == 1);
}

TEST_CASE("tasks - run consumes nested work until exhaustion", "[tasks]") {
  caors::tasks::execution_context context;
  auto executor = context.get_executor();
  int completed = 0;
  REQUIRE_FALSE(executor.post(caors::tasks::task{[&] {
    ++completed;
    (void)executor.post(caors::tasks::task{[&] { ++completed; }});
  }}));
  CHECK(context.run() == 2);
  CHECK(context.stopped());
  REQUIRE_FALSE(executor.post(caors::tasks::task{[&] { ++completed; }}));
  CHECK(context.run_one() == 0);
  context.restart();
  CHECK(context.run_one() == 1);
  CHECK(completed == 3);
}
