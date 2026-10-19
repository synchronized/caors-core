// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/tasks.h"

#include "caors/memory/tracking.h"

#include <atomic>
#include <catch2/catch_all.hpp>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

TEST_CASE("tasks - thread pool executor drains submitted tasks on shutdown", "[tasks]") {
  std::atomic<int> total = 0;

  {
    caors::tasks::thread_pool pool{2};
    caors::tasks::executor executor = pool.get_executor();
    CHECK(pool.thread_count() == 2);

    for (int index = 0; index < 32; ++index) {
      CHECK_FALSE(executor.post(caors::tasks::task{[&] { total.fetch_add(1); }}));
    }

    pool.shutdown();
  }

  CHECK(total.load() == 32);
}

TEST_CASE("tasks - thread pool executor reports invalid and frozen submissions", "[tasks]") {
  caors::tasks::thread_pool pool{1};
  caors::tasks::executor executor = pool.get_executor();

  CHECK(executor.post(caors::tasks::task{}) ==
        caors::error{caors::error_domain::core, caors::errc::invalid_argument});

  pool.shutdown();

  CHECK(pool.stopping());
  CHECK(executor.post(caors::tasks::task{[] {}}) ==
        caors::error{caors::error_domain::core, caors::errc::frozen});
}

TEST_CASE("tasks - thread pool executor can post back to execution context", "[tasks]") {
  caors::tasks::execution_context completions;
  caors::tasks::executor completion_executor = completions.get_executor();
  std::atomic<int> background_total = 0;
  int main_total = 0;

  {
    caors::tasks::thread_pool pool{2};
    caors::tasks::executor executor = pool.get_executor();
    for (int index = 0; index < 8; ++index) {
      CHECK_FALSE(executor.post(caors::tasks::task{[&, index] {
        background_total.fetch_add(index);
        CHECK_FALSE(
            completion_executor.post(caors::tasks::task{[&, index] { main_total += index; }}));
      }}));
    }

    pool.shutdown();
  }

  CHECK(background_total.load() == 28);
  CHECK(completions.drain() == 8);
  CHECK(main_total == 28);
}

TEST_CASE("tasks - thread pool executor uses configured memory resource", "[tasks]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};

  {
    caors::tasks::thread_pool pool{1, &tracking};
    caors::tasks::executor executor = pool.get_executor();
    CHECK(pool.resource() == &tracking);

    CHECK_FALSE(executor.post(caors::tasks::task{&tracking, [] {}}));
    pool.shutdown();
  }

  CHECK(tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("tasks - thread pool executor is available from module namespace", "[tasks]") {
  std::atomic<int> value = 0;

  {
    caors::tasks::thread_pool pool{1};
    caors::tasks::executor base = pool.get_executor();

    CHECK_FALSE(base.post(caors::tasks::task{[&] { value.store(3); }}));
    pool.shutdown();
  }

  CHECK(value.load() == 3);
}

TEST_CASE("tasks - thread pool cancel discards queued tasks", "[tasks]") {
  caors::tasks::thread_pool pool{1};
  caors::tasks::executor executor = pool.get_executor();
  std::mutex mutex;
  std::condition_variable condition;
  bool started = false;
  bool release = false;
  std::atomic<int> executed = 0;

  REQUIRE_FALSE(executor.post(caors::tasks::task{[&] {
    std::unique_lock lock{mutex};
    started = true;
    condition.notify_all();
    condition.wait(lock, [&] { return release; });
    executed.fetch_add(1);
  }}));

  {
    std::unique_lock lock{mutex};
    condition.wait(lock, [&] { return started; });
  }
  for (int index = 0; index < 8; ++index) {
    REQUIRE_FALSE(executor.post(caors::tasks::task{[&] { executed.fetch_add(1); }}));
  }
  CHECK(pool.pending() == 8);

  std::jthread shutdown_thread{[&] { pool.shutdown(caors::tasks::shutdown_mode::cancel); }};
  while (!pool.stopping()) {
    std::this_thread::yield();
  }
  {
    std::lock_guard lock{mutex};
    release = true;
  }
  condition.notify_all();
  shutdown_thread.join();

  CHECK(executed.load() == 1);
  CHECK(pool.pending() == 0);
}

TEST_CASE("tasks - thread pool executor remains safely invalid after pool destruction", "[tasks]") {
  caors::tasks::executor executor;
  {
    caors::tasks::thread_pool pool{1};
    executor = pool.get_executor();
    CHECK(executor.valid());
  }

  CHECK_FALSE(executor.valid());
  CHECK(executor.post(caors::tasks::task{[] {}}) ==
        caors::error{caors::error_domain::core, caors::errc::frozen});
}

TEST_CASE("tasks - standard pool accepts concurrent producers and concurrent close", "[tasks]") {
  caors::tasks::thread_pool pool{3};
  auto executor = pool.get_executor();
  std::atomic<int> executed{0};
  std::atomic<int> accepted{0};
  std::atomic<bool> start{false};
  std::vector<std::jthread> producers;
  for (int index = 0; index < 4; ++index) {
    producers.emplace_back([&] {
      while (!start.load()) {
        std::this_thread::yield();
      }
      for (int task_index = 0; task_index < 200; ++task_index) {
        if (!executor.post(caors::tasks::task{[&] { ++executed; }})) {
          ++accepted;
        }
      }
    });
  }
  start.store(true);
  std::jthread closer{[&] { pool.shutdown(); }};
  pool.shutdown();
  producers.clear();
  closer.join();
  CHECK(executed.load() == accepted.load());
  CHECK(pool.pending() == 0);
  CHECK_FALSE(executor.valid());
}

#if CAORS_HAS_EXCEPTIONS
TEST_CASE("tasks - standard pool survives throwing task", "[tasks]") {
  caors::tasks::thread_pool pool{1};
  std::atomic<int> executed{0};
  auto executor = pool.get_executor();
  REQUIRE_FALSE(executor.post(caors::tasks::task{[] { throw 42; }}));
  REQUIRE_FALSE(executor.post(caors::tasks::task{[&] { ++executed; }}));
  pool.shutdown();
  CHECK(executed.load() == 1);
}
#endif
