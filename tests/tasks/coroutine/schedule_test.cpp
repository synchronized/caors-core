// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory/tracking.h"
#include "caors/tasks/asio.h"
#include "caors/tasks/execution_context.h"
#include "caors/tasks/thread_pool.h"

#include <catch2/catch_all.hpp>

#include <coroutine>
#include <thread>
#include <utility>
#include <vector>

namespace {

class probe_coroutine final {
public:
  struct promise_type {
    [[nodiscard]] probe_coroutine get_return_object() noexcept {
      return probe_coroutine{std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    [[nodiscard]] std::suspend_never initial_suspend() const noexcept { return {}; }
    [[nodiscard]] std::suspend_always final_suspend() const noexcept { return {}; }
    void return_value(caors::error value) noexcept { status = value; }
    void unhandled_exception() noexcept {
      status = {caors::error_domain::core, caors::errc::backend_failure};
    }

    caors::error status{};
  };

  explicit probe_coroutine(std::coroutine_handle<promise_type> handle) noexcept : handle_(handle) {}
  probe_coroutine(const probe_coroutine&) = delete;
  probe_coroutine& operator=(const probe_coroutine&) = delete;
  probe_coroutine(probe_coroutine&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
  ~probe_coroutine() {
    if (handle_) {
      handle_.destroy();
    }
  }

  [[nodiscard]] bool done() const noexcept { return handle_.done(); }
  [[nodiscard]] caors::error result() const noexcept { return handle_.promise().status; }

private:
  std::coroutine_handle<promise_type> handle_{};
};

probe_coroutine switch_to(caors::tasks::executor executor, std::thread::id* observed = nullptr) {
  const caors::error status = co_await caors::tasks::schedule(std::move(executor));
  if (observed != nullptr) {
    *observed = std::this_thread::get_id();
  }
  co_return status;
}

} // namespace

TEST_CASE("tasks - schedule resumes on execution context", "[tasks][asio][coroutine]") {
  caors::tasks::execution_context context;
  probe_coroutine operation = switch_to(context.get_executor());

  CHECK_FALSE(operation.done());
  CHECK(context.poll() == 1);
  CHECK(operation.done());
  CHECK_FALSE(operation.result());
}

TEST_CASE("tasks - schedule resumes on thread pool worker", "[tasks][asio][coroutine]") {
  caors::tasks::thread_pool pool{1};
  const std::thread::id caller = std::this_thread::get_id();
  std::thread::id observed;
  probe_coroutine operation = switch_to(pool.get_executor(), &observed);

  pool.shutdown();
  CHECK(operation.done());
  CHECK_FALSE(operation.result());
  CHECK(observed != caller);
}

TEST_CASE("tasks - schedule reports invalid executor without suspending",
          "[tasks][asio][coroutine]") {
  probe_coroutine operation = switch_to({});

  CHECK(operation.done());
  CHECK(operation.result() == caors::error{caors::error_domain::core, caors::errc::frozen});
}

TEST_CASE("tasks - schedule resumes frozen when context discards pending work",
          "[tasks][asio][coroutine]") {
  probe_coroutine operation = [&] {
    caors::tasks::execution_context context;
    return switch_to(context.get_executor());
  }();

  CHECK(operation.done());
  CHECK(operation.result() == caors::error{caors::error_domain::core, caors::errc::frozen});
}

TEST_CASE("tasks - schedule tolerates coroutine frame destruction while pending",
          "[tasks][asio][coroutine]") {
  caors::tasks::execution_context context;
  {
    probe_coroutine operation = switch_to(context.get_executor());
    CHECK_FALSE(operation.done());
  }

  CHECK(context.poll() == 1);
}

TEST_CASE("tasks - schedule coordinates fast concurrent completion", "[tasks][asio][coroutine]") {
  caors::tasks::thread_pool pool{2};
  std::vector<probe_coroutine> operations;
  operations.reserve(256);

  for (int index = 0; index < 256; ++index) {
    operations.emplace_back(switch_to(pool.get_executor()));
  }
  pool.shutdown();

  for (const probe_coroutine& operation : operations) {
    CHECK(operation.done());
    CHECK_FALSE(operation.result());
  }
}

TEST_CASE("tasks - schedule state uses executor long lived resource", "[tasks][asio][coroutine]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};
  caors::tasks::execution_context context{&tracking};
  const std::size_t baseline = tracking.statistics().bytes_in_use;

  {
    probe_coroutine operation = switch_to(context.get_executor());
    CHECK(tracking.statistics().bytes_in_use > baseline);
    CHECK(context.poll() == 1);
    CHECK(operation.done());
  }

  CHECK(tracking.statistics().bytes_in_use == baseline);
}
