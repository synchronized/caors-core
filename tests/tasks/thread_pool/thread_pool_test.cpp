// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/tasks.h"

#include "caors/memory/tracking.h"

#include <atomic>
#include <catch2/catch_all.hpp>

TEST_CASE("tasks - thread pool executor drains submitted tasks on shutdown", "[tasks]") {
  std::atomic<int> total = 0;

  {
    caors::tasks::thread_pool_executor executor{2};
    CHECK(executor.thread_count() == 2);

    for (int index = 0; index < 32; ++index) {
      CHECK_FALSE(executor.submit(caors::tasks::task{[&] { total.fetch_add(1); }}));
    }

    executor.shutdown();
  }

  CHECK(total.load() == 32);
}

TEST_CASE("tasks - thread pool executor reports invalid and frozen submissions", "[tasks]") {
  caors::tasks::thread_pool_executor executor{1};

  CHECK(executor.submit(caors::tasks::task{}) ==
        caors::error{caors::error_domain::core, caors::errc::invalid_argument});

  executor.shutdown();

  CHECK(executor.stopping());
  CHECK(executor.submit(caors::tasks::task{[] {}}) ==
        caors::error{caors::error_domain::core, caors::errc::frozen});
}

TEST_CASE("tasks - thread pool executor can post completions back to completion queue", "[tasks]") {
  caors::tasks::completion_queue completions;
  std::atomic<int> background_total = 0;
  int main_total = 0;

  {
    caors::tasks::thread_pool_executor executor{2};
    for (int index = 0; index < 8; ++index) {
      CHECK_FALSE(executor.submit(caors::tasks::task{[&, index] {
        background_total.fetch_add(index);
        CHECK_FALSE(completions.push(caors::tasks::task{[&, index] { main_total += index; }}));
      }}));
    }

    executor.shutdown();
  }

  CHECK(background_total.load() == 28);
  CHECK(completions.drain() == 8);
  CHECK(main_total == 28);
}

TEST_CASE("tasks - thread pool executor uses configured memory resource", "[tasks]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};

  {
    caors::tasks::thread_pool_executor executor{1, &tracking};
    CHECK(executor.resource() == &tracking);

    CHECK_FALSE(executor.submit(caors::tasks::task{&tracking, [] {}}));
    executor.shutdown();
  }

  CHECK(tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("tasks - thread pool executor is available from module namespace", "[tasks]") {
  std::atomic<int> value = 0;

  {
    caors::tasks::thread_pool_executor executor{1};
    caors::tasks::executor base = executor.get_executor();

    CHECK_FALSE(base.post(caors::tasks::task{[&] { value.store(3); }}));
    executor.shutdown();
  }

  CHECK(value.load() == 3);
}
