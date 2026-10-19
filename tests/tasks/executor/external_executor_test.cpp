// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/tasks/external_executor.h"
#include "caors/tasks/request.h"

#include <catch2/catch_all.hpp>

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

namespace {
class host_queue final : public caors::tasks::external_executor_context {
public:
  caors::error post(caors::tasks::task next) override {
    std::lock_guard lock{mutex};
    if (closed) {
      return {caors::error_domain::core, caors::errc::frozen};
    }
    queue.push_back(std::move(next));
    return {};
  }
  void close() noexcept override {
    std::vector<caors::tasks::task> discarded;
    {
      std::lock_guard lock{mutex};
      closed = true;
      discarded.swap(queue);
    }
  }
  void run() {
    std::vector<caors::tasks::task> ready;
    {
      std::lock_guard lock{mutex};
      ready.swap(queue);
    }
    for (auto& next : ready) {
      CHECK_FALSE(next.run());
    }
  }

private:
  std::mutex mutex;
  bool closed = false;
  std::vector<caors::tasks::task> queue;
};
}

TEST_CASE("tasks - external executor closes requests and late handles", "[tasks]") {
  auto context = std::make_shared<host_queue>();
  caors::tasks::executor late;
  caors::tasks::request dropped;
  int calls = 0;
  {
    caors::tasks::external_executor owner{context};
    late = owner.get_executor();
    auto completed = caors::tasks::submit_request(late, caors::tasks::task{[&] { ++calls; }});
    context->run();
    CHECK(completed.completed());
    dropped = caors::tasks::submit_request(late, caors::tasks::task{[&] { ++calls; }});
    CHECK_FALSE(dropped.done());
    owner.close();
    CHECK(dropped.done());
    auto refused = caors::tasks::submit_request(late, caors::tasks::task{[] {}});
    CHECK(refused.error() == caors::error{caors::error_domain::core, caors::errc::frozen});
  }
  CHECK(calls == 1);
  CHECK_FALSE(late.valid());
  CHECK(late.post(caors::tasks::task{[] {}}) ==
        caors::error{caors::error_domain::core, caors::errc::frozen});
}

TEST_CASE("tasks - external submission races shutdown without pending requests", "[tasks]") {
  for (int round = 0; round != 64; ++round) {
    auto context = std::make_shared<host_queue>();
    caors::tasks::external_executor owner{context};
    auto executor = owner.get_executor();
    std::atomic<bool> start{false};
    caors::tasks::request request;
    std::thread producer{[&] {
      while (!start.load(std::memory_order_acquire)) {
        std::this_thread::yield();
      }
      request = caors::tasks::submit_request(executor, caors::tasks::task{[] {}});
    }};
    start.store(true, std::memory_order_release);
    owner.close();
    producer.join();
    CHECK(request.done());
  }
}

TEST_CASE("tasks - external inline executor permits nested submission", "[tasks]") {
  class inline_context final : public caors::tasks::external_executor_context {
  public:
    caors::error post(caors::tasks::task next) override {
      std::lock_guard lock{mutex};
      return closed ? caors::error{caors::error_domain::core, caors::errc::frozen} : next.run();
    }
    void close() noexcept override {
      std::lock_guard lock{mutex};
      closed = true;
    }

  private:
    std::recursive_mutex mutex;
    bool closed = false;
  };
  caors::tasks::external_executor owner{std::make_shared<inline_context>()};
  auto executor = owner.get_executor();
  int calls = 0;
  auto request = caors::tasks::submit_request(
      executor, caors::tasks::task{[&] {
        ++calls;
        CHECK_FALSE(executor.post(caors::tasks::task{[&] { ++calls; }}));
      }});
  CHECK(request.completed());
  CHECK(calls == 2);
}
