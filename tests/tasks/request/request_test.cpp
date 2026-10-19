// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/tasks.h"

#include "caors/memory/tracking.h"

#include <catch2/catch_all.hpp>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <stdexcept>

namespace {

class holding_executor final {
public:
  holding_executor() : executor(context.get_executor()) {}

  operator caors::tasks::executor() const noexcept { return executor; }

  [[nodiscard]] caors::error run_once() {
    if (context.poll_one() == 0) {
      return {caors::error_domain::core, caors::errc::invalid_argument};
    }
    return {};
  }

  caors::tasks::execution_context context;
  caors::tasks::executor executor;
};

} // namespace

TEST_CASE("tasks - request reports completed inline task", "[tasks]") {
  caors::tasks::executor executor = caors::tasks::inline_executor();
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
  caors::tasks::executor executor = caors::tasks::inline_executor();

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
  caors::tasks::executor inline_target = caors::tasks::inline_executor();

  caors::tasks::request invalid = caors::tasks::submit_request(inline_target, caors::tasks::task{});
  CHECK(invalid.failed());
  CHECK(invalid.error() == caors::error{caors::error_domain::core, caors::errc::invalid_argument});

  caors::tasks::thread_pool pool{1};
  pool.shutdown();

  caors::tasks::request rejected =
      caors::tasks::submit_request(pool.get_executor(), caors::tasks::task{[] {}});
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
    caors::tasks::thread_pool pool{1};
    request = caors::tasks::submit_request(pool.get_executor(),
                                           caors::tasks::task{[&] { value.store(11); }});
    REQUIRE(request);
    pool.shutdown();
  }

  CHECK(request.completed());
  CHECK_FALSE(request.result());
  CHECK(value.load() == 11);
}

TEST_CASE("tasks - request copies observe the same state transitions", "[tasks]") {
  holding_executor executor;
  caors::tasks::request first = caors::tasks::submit_request(executor, caors::tasks::task{[] {}});
  caors::tasks::request second = first;

  CHECK(first.status() == caors::tasks::request_status::queued);
  CHECK(second.status() == caors::tasks::request_status::queued);

  REQUIRE_FALSE(executor.run_once());
  CHECK(first.completed());
  CHECK(second.completed());
  CHECK_FALSE(first.result());
  CHECK_FALSE(second.result());
}

TEST_CASE("tasks - cancelling a running request does not interrupt its callable", "[tasks]") {
  caors::tasks::thread_pool pool{1};
  std::mutex mutex;
  std::condition_variable condition;
  bool started = false;
  bool release = false;
  std::atomic<int> observed = 0;

  caors::tasks::request request =
      caors::tasks::submit_request(pool.get_executor(), caors::tasks::task{[&] {
                                     std::unique_lock lock{mutex};
                                     started = true;
                                     condition.notify_all();
                                     condition.wait(lock, [&] { return release; });
                                     observed.fetch_add(1);
                                   }});

  {
    std::unique_lock lock{mutex};
    condition.wait(lock, [&] { return started; });
  }
  CHECK(request.status() == caors::tasks::request_status::running);
  REQUIRE(request.cancel());
  CHECK(request.cancelled());

  {
    std::lock_guard lock{mutex};
    release = true;
  }
  condition.notify_all();
  pool.shutdown();

  CHECK(observed.load() == 1);
  CHECK(request.cancelled());
  CHECK(request.result() == caors::error{caors::error_domain::core, caors::errc::cancelled});
}

#if CAORS_HAS_EXCEPTIONS
TEST_CASE("tasks - request converts callable exceptions to backend failure", "[tasks]") {
  caors::tasks::executor executor = caors::tasks::inline_executor();
  caors::tasks::request request = caors::tasks::submit_request(
      executor, caors::tasks::task{[] { throw std::runtime_error{"任务失败"}; }});

  CHECK(request.failed());
  CHECK(request.error() == caors::error{caors::error_domain::core, caors::errc::backend_failure});
  CHECK(request.result() == caors::error{caors::error_domain::core, caors::errc::backend_failure});
}
#endif

TEST_CASE("tasks - request factories expose terminal and empty states", "[tasks]") {
  caors::tasks::request empty;
  CHECK_FALSE(empty.valid());
  CHECK(empty.cancelled());
  CHECK(empty.done());
  CHECK(empty.error() == caors::error{caors::error_domain::core, caors::errc::invalid_argument});

  caors::tasks::request completed = caors::tasks::request::make_completed();
  CHECK(completed.completed());
  CHECK_FALSE(completed.cancel());

  caors::tasks::request failed = caors::tasks::request::make_failed({});
  CHECK(failed.failed());
  CHECK(failed.error() == caors::error{caors::error_domain::core, caors::errc::backend_failure});
  CHECK_FALSE(failed.cancel());
}

TEST_CASE("tasks - request uses configured memory resource", "[tasks]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};
  caors::tasks::executor executor = caors::tasks::inline_executor();

  {
    caors::tasks::request request =
        caors::tasks::submit_request(executor, caors::tasks::task{&tracking, [] {}}, &tracking);
    CHECK(request.completed());
    CHECK(tracking.statistics().bytes_in_use > 0);
  }

  CHECK(tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("tasks - request helpers are available from module namespace", "[tasks]") {
  caors::tasks::executor executor = caors::tasks::inline_executor();
  caors::tasks::request request = caors::tasks::submit_request(executor, caors::tasks::task{[] {}});

  CHECK(request.status() == caors::tasks::request_status::completed);
}
