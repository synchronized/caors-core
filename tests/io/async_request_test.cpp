// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/config.h"
#include "caors/io/async_request.h"
#include "caors/tasks/execution_context.h"
#include "caors/tasks/external_executor.h"
#include "io_test_support.h"
#include <barrier>
#include <thread>

namespace {
using namespace caors;
using namespace caors::io;
}
TEST_CASE("io - native completion handshake preserves start refusal", "[io][async]") {
  int called = 0;
  auto refused = start_io<int>(
      tasks::inline_executor(), [&](const auto&) { ++called; },
      [](io_completion<int> completion) {
        CHECK(completion.complete({{}, 42}));
        CHECK_FALSE(completion.complete({{}, 43}));
        return io_error{io_errc::unsupported, 19, 21};
      });
  CHECK_FALSE(refused);
  CHECK(refused.error().backend_id == 19);
  CHECK(refused.error().native_code == 21);
  CHECK(called == 0);
  auto accepted = start_io<int>(
      tasks::inline_executor(),
      [&](const auto& value) {
        ++called;
        CHECK(value.value == 42);
      },
      [](io_completion<int> completion) {
        (void)completion.complete({{}, 42});
        return io_error{};
      });
  REQUIRE(accepted);
  REQUIRE(accepted->result());
  CHECK(accepted->result()->value == 42);
  CHECK(called == 1);
  accepted->cancel();
  CHECK(accepted->result()->value == 42);
}
TEST_CASE("io - dropped notification is queryable without thread fallback", "[io][async]") {
  tasks::execution_context notifications;
  int called = 0;
  auto request = start_io<int>(
      notifications.get_executor(), [&](const auto&) { ++called; },
      [](io_completion<int> completion) {
        (void)completion.complete({{}, 42});
        return io_error{};
      });
  REQUIRE(request);
  CHECK_FALSE(request->result());
  notifications.clear();
  REQUIRE(request->result());
  CHECK(request->result()->error().value == io_errc::completion_rejected);
  CHECK(called == 0);
}
TEST_CASE("io - cancellation wins before delivery and abandoned receipt closes request",
          "[io][async]") {
  tasks::execution_context notifications;
  io_errc observed = io_errc::success;
  auto request = start_io<int>(
      notifications.get_executor(), [&](const auto& value) { observed = value.error().value; },
      [](io_completion<int> completion) {
        (void)completion.complete({{}, 42});
        return io_error{};
      });
  REQUIRE(request);
  request->cancel();
  CHECK(notifications.drain() == 1);
  REQUIRE(request->result());
  CHECK(observed == io_errc::cancelled);
  auto abandoned =
      start_io<int>(tasks::inline_executor(), {}, [](io_completion<int>) { return io_error{}; });
  REQUIRE(abandoned);
  REQUIRE(abandoned->result());
  CHECK(abandoned->result()->error().value == io_errc::cancelled);
}

namespace {
class notification_resource final : public memory_resource {
public:
  bool fail = false;

private:
  void* do_try_allocate(std::size_t size, std::size_t alignment) noexcept override {
    return fail ? nullptr : get_default_memory_resource()->try_allocate(size, alignment);
  }
  void do_deallocate(void* ptr, std::size_t size, std::size_t alignment) noexcept override {
    get_default_memory_resource()->deallocate(ptr, size, alignment);
  }
  bool do_is_equal(const memory_resource& other) const noexcept override { return this == &other; }
};
}
TEST_CASE("io - notification allocation failure reports out of memory", "[io][async]") {
  notification_resource resource;
  tasks::execution_context notifications{&resource};
  io_completion<int> pending;
  int called = 0;
  auto request = start_io<int>(
      notifications.get_executor(), [&](const auto&) { ++called; },
      [&](io_completion<int> receipt) {
        pending = std::move(receipt);
        return io_error{};
      });
  REQUIRE(request);
  resource.fail = true;
  CHECK(pending.complete({{}, 42}));
  resource.fail = false;
  REQUIRE(request->result());
  CHECK(request->result()->error().value == io_errc::out_of_memory);
  CHECK(called == 0);
  CHECK(notifications.pending() == 0);
}
#if CAORS_HAS_BACKGROUND_THREADS
TEST_CASE("io - concurrent cancellation and delivery make one terminal decision", "[io][async]") {
  for (int round = 0; round < 100; ++round) {
    tasks::execution_context notifications;
    std::atomic<int> called{0};
    std::atomic<bool> valid{true};
    auto request = start_io<int>(
        notifications.get_executor(),
        [&](const auto& result) {
          ++called;
          if (result.error().value != io_errc::cancelled && (!result || result.value != 42))
            valid = false;
        },
        [](io_completion<int> receipt) {
          (void)receipt.complete({{}, 42});
          return io_error{};
        });
    REQUIRE(request);
    std::barrier barrier{3};
    std::jthread cancel{[&] {
      barrier.arrive_and_wait();
      request->cancel();
    }};
    std::jthread deliver{[&] {
      barrier.arrive_and_wait();
      (void)notifications.drain();
    }};
    barrier.arrive_and_wait();
    cancel.join();
    deliver.join();
    REQUIRE(request->result());
    CHECK(called == 1);
    CHECK(valid.load());
    CHECK(
        (request->result()->error().value == io_errc::cancelled || request->result()->value == 42));
  }
}

#endif

namespace {
class dropping_context final : public tasks::external_executor_context {
public:
  bool throw_allocation = false;
  error post(tasks::task next) override {
    next.reset();
    if (throw_allocation)
      throw std::bad_alloc{};
    return {};
  }
  void close() noexcept override {}
};
}
TEST_CASE("io - notification dropped before post returns follows acceptance handshake",
          "[io][async]") {
  for (bool throw_allocation : {false, true}) {
    auto context = std::make_shared<dropping_context>();
    context->throw_allocation = throw_allocation;
    tasks::external_executor executor{context};
    int called = 0;
    auto request = start_io<int>(
        executor.get_executor(), [&](const auto&) { ++called; },
        [](io_completion<int> receipt) {
          (void)receipt.complete({{}, 42});
          return io_error{};
        });
    REQUIRE(request);
    REQUIRE(request->result());
    CHECK(request->result()->error().value ==
          (throw_allocation ? io_errc::out_of_memory : io_errc::completion_rejected));
    CHECK(called == 0);
  }
}

TEST_CASE("io - terminal delivery releases callback captures while request survives",
          "[io][async]") {
  for (bool drop : {false, true}) {
    CAPTURE(drop);
    tasks::execution_context executor;
    auto owner = std::make_shared<int>(7);
    std::weak_ptr<int> weak = owner;
    auto request = start_io<int>(
        executor.get_executor(), [owner](const auto&) { CHECK(*owner == 7); },
        [](io_completion<int> receipt) {
          CHECK(receipt.complete({{}, 42}));
          return io_error{};
        });
    REQUIRE(request);
    owner.reset();
    REQUIRE_FALSE(weak.expired());
    if (drop)
      executor.clear();
    else
      REQUIRE(executor.drain() == 1);
    REQUIRE(request->result());
    CHECK(weak.expired());
  }
}
