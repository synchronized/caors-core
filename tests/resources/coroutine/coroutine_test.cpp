// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory/tracking.h"
#include "caors/resources/coroutine.h"

#include <catch2/catch_all.hpp>

#include <coroutine>
#include <utility>

namespace {

class coroutine_resource final : public caors::resource {
public:
  using caors::resource::resource;
};

class probe_coroutine final {
public:
  struct promise_type {
    [[nodiscard]] probe_coroutine get_return_object() noexcept {
      return probe_coroutine{std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    [[nodiscard]] std::suspend_never initial_suspend() const noexcept { return {}; }
    [[nodiscard]] std::suspend_always final_suspend() const noexcept { return {}; }
    void return_void() const noexcept {}
    void unhandled_exception() noexcept { failed = true; }

    bool failed = false;
  };

  explicit probe_coroutine(std::coroutine_handle<promise_type> handle) noexcept : handle_(handle) {}
  probe_coroutine(const probe_coroutine&) = delete;
  probe_coroutine& operator=(const probe_coroutine&) = delete;
  probe_coroutine(probe_coroutine&& other) noexcept
      : handle_(std::exchange(other.handle_, {})) {}
  ~probe_coroutine() {
    if (handle_) {
      handle_.destroy();
    }
  }

  [[nodiscard]] bool done() const noexcept { return handle_.done(); }
  [[nodiscard]] bool failed() const noexcept { return handle_.promise().failed; }

private:
  std::coroutine_handle<promise_type> handle_{};
};

probe_coroutine wait_load(caors::resources::load_request<coroutine_resource> request,
                          caors::resources::handle_result<coroutine_resource>* result) {
  *result = co_await caors::resources::async_wait(std::move(request));
}

probe_coroutine wait_preload(caors::resources::preload_request request,
                             caors::resources::preload_result* result) {
  *result = co_await caors::resources::async_wait(std::move(request));
}

} // namespace

TEST_CASE("resources coroutine - load request resumes all waiters",
          "[resources][coroutine]") {
  auto request = caors::resources::detail::load_request_access<coroutine_resource>::make();
  caors::resources::handle_result<coroutine_resource> first;
  caors::resources::handle_result<coroutine_resource> second;
  probe_coroutine first_wait = wait_load(request, &first);
  probe_coroutine second_wait = wait_load(request, &second);

  REQUIRE_FALSE(first_wait.done());
  REQUIRE_FALSE(second_wait.done());
  auto owned = caors::memory::make_unique<coroutine_resource>(nullptr, caors::resource_guid{});
  auto handle = caors::resources::adopt_handle(std::move(owned), nullptr);
  caors::resources::detail::load_request_access<coroutine_resource>::complete(request,
                                                                               std::move(handle));

  CHECK(first_wait.done());
  CHECK(second_wait.done());
  CHECK_FALSE(first_wait.failed());
  CHECK_FALSE(second_wait.failed());
  CHECK(first.value);
  CHECK(second.value);
}

TEST_CASE("resources coroutine - load failure and cancellation become results",
          "[resources][coroutine]") {
  auto failed = caors::resources::load_request<coroutine_resource>::failed(
      {caors::error_domain::resources, caors::errc::not_found});
  caors::resources::handle_result<coroutine_resource> failed_result;
  probe_coroutine failed_wait = wait_load(failed, &failed_result);
  CHECK(failed_wait.done());
  CHECK(failed_result.status ==
        caors::error{caors::error_domain::resources, caors::errc::not_found});

  auto cancelled = caors::resources::detail::load_request_access<coroutine_resource>::make();
  caors::resources::handle_result<coroutine_resource> cancelled_result;
  probe_coroutine cancelled_wait = wait_load(cancelled, &cancelled_result);
  REQUIRE_FALSE(cancelled_wait.done());
  REQUIRE(cancelled.cancel());
  CHECK(cancelled_wait.done());
  CHECK(cancelled_result.status ==
        caors::error{caors::error_domain::resources, caors::errc::cancelled});
}

TEST_CASE("resources coroutine - preload resumes with aggregate result",
          "[resources][coroutine]") {
  caors::resources::preload_plan plan;
  const caors::resources::preload_item item{};
  plan.load_order.push_back(item);
  auto request = caors::resources::preload_request::from_plan(std::move(plan));
  caors::resources::preload_result result;
  probe_coroutine wait = wait_preload(request, &result);

  REQUIRE_FALSE(wait.done());
  caors::resources::detail::preload_request_access::complete_item(
      caors::resources::detail::preload_request_access::state(request), item);

  CHECK(wait.done());
  CHECK(result.succeeded());
  REQUIRE(result.loaded.size() == 1);
  CHECK(result.loaded.front() == item);
}

TEST_CASE("resources coroutine - destroyed frame abandons only its wait",
          "[resources][coroutine]") {
  auto request = caors::resources::detail::load_request_access<coroutine_resource>::make();
  caors::resources::handle_result<coroutine_resource> ignored;
  {
    probe_coroutine wait = wait_load(request, &ignored);
    REQUIRE_FALSE(wait.done());
  }

  CHECK(request.cancel());
  CHECK(request.cancelled());
}

TEST_CASE("resources coroutine - helper state uses request memory resource",
          "[resources][coroutine]") {
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};
  auto request =
      caors::resources::detail::load_request_access<coroutine_resource>::make(&tracking);
  const std::size_t baseline = tracking.statistics().bytes_in_use;
  caors::resources::handle_result<coroutine_resource> result;

  {
    probe_coroutine wait = wait_load(request, &result);
    CHECK(tracking.statistics().bytes_in_use > baseline);
    REQUIRE(request.cancel());
    CHECK(wait.done());
  }

  CHECK(tracking.statistics().bytes_in_use == baseline);
}
