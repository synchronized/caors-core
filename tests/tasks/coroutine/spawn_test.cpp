// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/config.h"
#include "caors/tasks/coroutine.h"
#include "caors/tasks/execution_context.h"
#include "caors/tasks/inline_executor.h"
#include "caors/tasks/thread_pool.h"

#include <catch2/catch_all.hpp>

#include <memory>
#include <stdexcept>
#include <thread>

namespace {

caors::tasks::async_operation succeed(int* observed) {
  *observed = 7;
  co_return caors::error{};
}

caors::tasks::async_operation fail() {
  co_return caors::error{caors::error_domain::core, caors::errc::unsupported};
}

caors::tasks::async_operation switch_and_succeed(caors::tasks::executor target,
                                                 std::thread::id* observed) {
  const caors::error status = co_await caors::tasks::schedule(std::move(target));
  if (status) {
    co_return status;
  }
  *observed = std::this_thread::get_id();
  co_return caors::error{};
}

#if CAORS_HAS_EXCEPTIONS
caors::tasks::async_operation throw_failure() {
  throw std::runtime_error{"coroutine 失败"};
  co_return caors::error{};
}
#endif

caors::tasks::async_operation retain_marker(std::shared_ptr<int> marker, int* observed) {
  *observed = *marker;
  co_return caors::error{};
}

} // namespace

TEST_CASE("tasks - spawn completes inline operation", "[tasks][coroutine]") {
  caors::tasks::executor executor = caors::tasks::inline_executor();
  int observed = 0;

  caors::tasks::request request = caors::tasks::spawn(executor, succeed(&observed));

  CHECK(request.completed());
  CHECK_FALSE(request.result());
  CHECK(observed == 7);
}

TEST_CASE("tasks - spawn preserves coroutine error", "[tasks][coroutine]") {
  caors::tasks::executor executor = caors::tasks::inline_executor();
  caors::tasks::request request = caors::tasks::spawn(executor, fail());

  CHECK(request.failed());
  CHECK(request.result() == caors::error{caors::error_domain::core, caors::errc::unsupported});
}

TEST_CASE("tasks - spawn owns frame across executor switch", "[tasks][coroutine]") {
  caors::tasks::executor start = caors::tasks::inline_executor();
  caors::tasks::thread_pool pool{1};
  const std::thread::id caller = std::this_thread::get_id();
  std::thread::id observed;

  caors::tasks::request request =
      caors::tasks::spawn(start, switch_and_succeed(pool.get_executor(), &observed));
  CHECK((request.status() == caors::tasks::request_status::running || request.completed()));

  pool.shutdown();
  CHECK(request.completed());
  CHECK(observed != caller);
}

TEST_CASE("tasks - spawn rejects invalid operation and executor", "[tasks][coroutine]") {
  caors::tasks::executor inline_target = caors::tasks::inline_executor();
  caors::tasks::request invalid =
      caors::tasks::spawn(inline_target, caors::tasks::async_operation{});
  CHECK(invalid.failed());
  CHECK(invalid.result() == caors::error{caors::error_domain::core, caors::errc::invalid_argument});

  caors::tasks::request rejected = caors::tasks::spawn({}, succeed(nullptr));
  CHECK(rejected.failed());
  CHECK(rejected.result() == caors::error{caors::error_domain::core, caors::errc::frozen});
}

TEST_CASE("tasks - spawn cancellation before start destroys frame", "[tasks][coroutine]") {
  caors::tasks::execution_context context;
  auto marker = std::make_shared<int>(9);
  std::weak_ptr<int> weak = marker;
  int observed = 0;
  caors::tasks::request request =
      caors::tasks::spawn(context.get_executor(), retain_marker(std::move(marker), &observed));

  REQUIRE(request.cancel());
  CHECK(context.poll() == 1);
  CHECK(request.cancelled());
  CHECK(observed == 0);
  CHECK(weak.expired());
}

TEST_CASE("tasks - discarded spawn fails request and destroys frame", "[tasks][coroutine]") {
  auto marker = std::make_shared<int>(9);
  std::weak_ptr<int> weak = marker;
  int observed = 0;
  caors::tasks::request request;
  {
    caors::tasks::execution_context context;
    request =
        caors::tasks::spawn(context.get_executor(), retain_marker(std::move(marker), &observed));
  }

  CHECK(request.failed());
  CHECK(request.result() == caors::error{caors::error_domain::core, caors::errc::frozen});
  CHECK(observed == 0);
  CHECK(weak.expired());
}

#if CAORS_HAS_EXCEPTIONS
TEST_CASE("tasks - spawn converts unhandled exception to backend failure",
          "[tasks][coroutine]") {
  caors::tasks::executor executor = caors::tasks::inline_executor();
  caors::tasks::request request = caors::tasks::spawn(executor, throw_failure());

  CHECK(request.failed());
  CHECK(request.result() == caors::error{caors::error_domain::core, caors::errc::backend_failure});
}
#endif
