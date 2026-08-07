// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "support/benchmark_support.h"

#include "caors/tasks/asio.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <coroutine>
#include <cstdlib>
#include <iostream>
#include <utility>
#include <vector>

namespace caors::benchmarks {
namespace {

constexpr std::size_t waiter_count = 4;
constexpr std::size_t latency_batch_width = 16;

struct latency_summary {
  std::uint64_t p50_nanoseconds = 0;
  std::uint64_t p95_nanoseconds = 0;
  std::uint64_t maximum_nanoseconds = 0;
};

class coroutine_probe final {
public:
  struct promise_type {
    [[nodiscard]] coroutine_probe get_return_object() noexcept {
      return coroutine_probe{std::coroutine_handle<promise_type>::from_promise(*this)};
    }
    [[nodiscard]] std::suspend_never initial_suspend() const noexcept { return {}; }
    [[nodiscard]] std::suspend_always final_suspend() const noexcept { return {}; }
    void return_void() const noexcept {}
    void unhandled_exception() noexcept { failed = true; }

    bool failed = false;
  };

  explicit coroutine_probe(std::coroutine_handle<promise_type> handle) noexcept : handle_(handle) {}
  coroutine_probe(const coroutine_probe&) = delete;
  coroutine_probe& operator=(const coroutine_probe&) = delete;
  coroutine_probe(coroutine_probe&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
  ~coroutine_probe() {
    if (handle_) {
      handle_.destroy();
    }
  }

  [[nodiscard]] bool succeeded() const noexcept {
    return handle_ && handle_.done() && !handle_.promise().failed;
  }

private:
  std::coroutine_handle<promise_type> handle_{};
};

coroutine_probe wait_for_request(tasks::request request, error* result) {
  *result = co_await tasks::async_wait(std::move(request));
}

void require_success(const coroutine_probe& probe, error result) {
  if (!probe.succeeded() || result) {
    std::cerr << "任务协程 benchmark 失败\n";
    std::exit(EXIT_FAILURE);
  }
}

void run_ready_waits(std::size_t iterations, memory_resource* resource) {
  const tasks::request request = tasks::request::make_completed(resource);
  for (std::size_t index = 0; index < iterations; ++index) {
    error result;
    coroutine_probe probe = wait_for_request(request, &result);
    require_success(probe, result);
  }
}

void run_single_waits(std::size_t iterations, memory_resource* resource) {
  for (std::size_t index = 0; index < iterations; ++index) {
    tasks::request request = tasks::request::make(resource);
    error result;
    coroutine_probe probe = wait_for_request(request, &result);
    (void)tasks::detail::request_access::complete(tasks::detail::request_access::state(request));
    require_success(probe, result);
  }
}

void run_multiple_waits(std::size_t iterations, memory_resource* resource) {
  for (std::size_t index = 0; index < iterations; ++index) {
    tasks::request request = tasks::request::make(resource);
    std::array<error, waiter_count> results{};
    coroutine_probe first = wait_for_request(request, &results[0]);
    coroutine_probe second = wait_for_request(request, &results[1]);
    coroutine_probe third = wait_for_request(request, &results[2]);
    coroutine_probe fourth = wait_for_request(request, &results[3]);
    (void)tasks::detail::request_access::complete(tasks::detail::request_access::state(request));
    require_success(first, results[0]);
    require_success(second, results[1]);
    require_success(third, results[2]);
    require_success(fourth, results[3]);
  }
}

template <typename Operation>
[[nodiscard]] latency_summary sample_latency(std::size_t iterations, Operation&& operation) {
  const std::size_t batch_count = (iterations + latency_batch_width - 1) / latency_batch_width;
  std::vector<std::uint64_t> samples;
  samples.reserve(batch_count);
  std::size_t completed = 0;
  while (completed < iterations) {
    const std::size_t width = std::min(latency_batch_width, iterations - completed);
    const auto begin = std::chrono::steady_clock::now();
    operation(width);
    const auto end = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();
    samples.push_back(static_cast<std::uint64_t>(elapsed) / width);
    completed += width;
  }

  std::ranges::sort(samples);
  const std::size_t last = samples.size() - 1;
  return {samples[last * 50 / 100], samples[last * 95 / 100], samples[last]};
}

template <typename Operation>
void benchmark_wait_case(std::string_view name, std::size_t iterations, Operation&& operation) {
  tracking_resource tracking{&get_system_memory_resource()};
  operation(iterations, &tracking);
  print_memory_statistics(name, tracking.statistics(), iterations);

  const latency_summary latency = sample_latency(
      iterations, [&](std::size_t count) { operation(count, &get_system_memory_resource()); });
  std::cout << name << ": samples=" << (iterations + latency_batch_width - 1) / latency_batch_width
            << " batch_width=" << latency_batch_width << " p50_ns=" << latency.p50_nanoseconds
            << " p95_ns=" << latency.p95_nanoseconds << " max_ns=" << latency.maximum_nanoseconds
            << '\n';
}

} // namespace

void benchmark_task_coroutines(std::size_t iterations) {
  benchmark_wait_case("tasks.coroutine.ready", iterations, run_ready_waits);
  benchmark_wait_case("tasks.coroutine.single_waiter", iterations, run_single_waits);
  benchmark_wait_case("tasks.coroutine.four_waiters", iterations, run_multiple_waits);
}

} // namespace caors::benchmarks
