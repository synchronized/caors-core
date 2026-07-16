// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/tasks.h"

#include "caors/memory/tracking.h"

#include <catch2/catch_all.hpp>

#include <atomic>

namespace {

class holding_executor final : public caors::tasks::executor {
public:
  [[nodiscard]] caors::error submit(caors::tasks::task next) override {
    if (!next) {
      return {caors::error_domain::core, caors::errc::invalid_argument};
    }

    queued = std::move(next);
    return {};
  }

  [[nodiscard]] caors::error run_once() {
    if (!queued) {
      return {caors::error_domain::core, caors::errc::invalid_argument};
    }

    return queued.run();
  }

  caors::tasks::task queued{};
};

} // namespace

TEST_CASE("tasks - request reports completed inline task", "[tasks]") {
  caors::tasks::inline_executor executor;
  int value = 0;

  caors::tasks::request request =
      caors::tasks::submit_request(executor, caors::tasks::task{[&] { value = 7; }});

  REQUIRE(request);
  CHECK(request.completed());
  CHECK(request.done());
  CHECK_FALSE(request.error());
  CHECK_FALSE(request.result());
  CHECK(value == 7);
}

TEST_CASE("tasks - request reports callable errors", "[tasks]") {
  caors::tasks::inline_executor executor;

  caors::tasks::request request = caors::tasks::submit_request(
      executor, caors::tasks::task{[] {
        return caors::error{caors::error_domain::core, caors::errc::unsupported};
      }});

  REQUIRE(request);
  CHECK(request.failed());
  CHECK(request.done());
  CHECK(request.error() == caors::error{caors::error_domain::core, caors::errc::unsupported});
  CHECK(request.result() == caors::error{caors::error_domain::core, caors::errc::unsupported});
}

TEST_CASE("tasks - request reports invalid and rejected submissions", "[tasks]") {
  caors::tasks::inline_executor inline_executor;

  caors::tasks::request invalid =
      caors::tasks::submit_request(inline_executor, caors::tasks::task{});
  CHECK(invalid.failed());
  CHECK(invalid.error() == caors::error{caors::error_domain::core, caors::errc::invalid_argument});

  caors::tasks::thread_pool_executor pool{1};
  pool.shutdown();

  caors::tasks::request rejected = caors::tasks::submit_request(pool, caors::tasks::task{[] {}});
  CHECK(rejected.failed());
  CHECK(rejected.error() == caors::error{caors::error_domain::core, caors::errc::frozen});
}

TEST_CASE("tasks - request can be cancelled before held task runs", "[tasks]") {
  holding_executor executor;
  int value = 0;

  caors::tasks::request request =
      caors::tasks::submit_request(executor, caors::tasks::task{[&] { value = 3; }});

  REQUIRE(request);
  CHECK(request.status() == caors::tasks::request_status::queued);
  CHECK(request.result() == caors::error{caors::error_domain::core, caors::errc::pending});

  REQUIRE(request.cancel());
  CHECK(request.cancelled());
  CHECK(request.done());
  CHECK(request.result() == caors::error{caors::error_domain::core, caors::errc::cancelled});

  CHECK_FALSE(executor.run_once());
  CHECK(value == 0);
  CHECK_FALSE(request.cancel());
}

TEST_CASE("tasks - request observes thread pool completion", "[tasks]") {
  std::atomic<int> value = 0;
  caors::tasks::request request;

  {
    caors::tasks::thread_pool_executor executor{1};
    request = caors::tasks::submit_request(executor, caors::tasks::task{[&] { value.store(11); }});
    REQUIRE(request);
    executor.shutdown();
  }

  CHECK(request.completed());
  CHECK_FALSE(request.result());
  CHECK(value.load() == 11);
}

TEST_CASE("tasks - request uses configured memory resource", "[tasks]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};
  caors::tasks::inline_executor executor;

  {
    caors::tasks::request request =
        caors::tasks::submit_request(executor, caors::tasks::task{&tracking, [] {}}, &tracking);
    CHECK(request.completed());
    CHECK(tracking.statistics().bytes_in_use > 0);
  }

  CHECK(tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("tasks - request helpers are available from module namespace", "[tasks]") {
  caors::tasks::inline_executor executor;
  caors::tasks::request request = caors::tasks::submit_request(executor, caors::tasks::task{[] {}});

  CHECK(request.status() == caors::tasks::request_status::completed);
}
