// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory/tracking.h"
#include "caors/tasks/request.h"

#if __has_include("caors/tasks/execution_context.h")
#include "caors/tasks/execution_context.h"
#include "caors/tasks/thread_pool.h"
#else
#include "caors/tasks/completion_queue.h"
#include "caors/tasks/inline_executor.h"
#include "caors/tasks/thread_pool_executor.h"
#endif

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <semaphore>
#include <string_view>
#include <vector>

namespace {

void require(bool success) {
  if (!success) {
    std::cerr << "调度器基准校验失败\n";
    std::exit(EXIT_FAILURE);
  }
}

template <typename Operation> void measure(std::string_view name, Operation operation) {
  constexpr std::size_t iterations = 10'000;
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};
  operation(&tracking);
  const auto statistics = tracking.statistics();
  require(statistics.bytes_in_use == 0);
  std::vector<std::int64_t> samples;
  samples.reserve(iterations);
  const auto total_begin = std::chrono::steady_clock::now();
  for (std::size_t index = 0; index < iterations; ++index) {
    const auto begin = std::chrono::steady_clock::now();
    operation(&caors::get_system_memory_resource());
    samples.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(
                          std::chrono::steady_clock::now() - begin)
                          .count());
  }
  const auto elapsed =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - total_begin);
  std::ranges::sort(samples);
  std::cout << name << " allocations=" << statistics.allocation_count
            << " bytes=" << statistics.bytes_allocated << " live=" << statistics.bytes_in_use
            << " p50_ns=" << samples[iterations / 2] << " p95_ns=" << samples[iterations * 95 / 100]
            << " operations_per_second=" << static_cast<double>(iterations) / elapsed.count()
            << '\n';
}

} // namespace

int main() {
#if __has_include("caors/tasks/execution_context.h")
  auto inline_target = caors::tasks::inline_executor();
  auto post = [](auto& target, caors::tasks::task next) { return target.post(std::move(next)); };
  using context_type = caors::tasks::execution_context;
  using pool_type = caors::tasks::thread_pool;
  std::cout << "backend=asio\n";
#else
  caors::tasks::inline_executor inline_target;
  auto post = [](auto& target, caors::tasks::task next) { return target.submit(std::move(next)); };
  using context_type = caors::tasks::completion_queue;
  using pool_type = caors::tasks::thread_pool_executor;
  std::cout << "backend=legacy\n";
#endif
  std::cout << "sizeof_task=" << sizeof(caors::tasks::task)
            << " sizeof_request=" << sizeof(caors::tasks::request)
            << " sizeof_context=" << sizeof(context_type) << '\n';
  measure("inline", [&](caors::memory_resource* resource) {
    bool ran = false;
    require(!post(inline_target, caors::tasks::task{resource, [&] { ran = true; }}));
    require(ran);
  });
  measure("request.inline", [&](caors::memory_resource* resource) {
    auto request =
        caors::tasks::submit_request(inline_target, caors::tasks::task{resource, [] {}}, resource);
    require(request.done() && !request.result());
  });
  // 单次样本包含构造、投递、执行和销毁，确保长期资源全部释放；不代表常驻队列的纯提交成本。
  measure("context.lifecycle", [&](caors::memory_resource* resource) {
    context_type context{resource};
    bool ran = false;
#if __has_include("caors/tasks/execution_context.h")
    auto target = context.get_executor();
    require(!post(target, caors::tasks::task{resource, [&] { ran = true; }}));
#else
    require(!context.push(caors::tasks::task{resource, [&] { ran = true; }}));
#endif
    require(context.drain() == 1 && ran);
  });
  context_type persistent_context;
  measure("context.post_drain", [&](caors::memory_resource* resource) {
    bool ran = false;
#if __has_include("caors/tasks/execution_context.h")
    auto context_target = persistent_context.get_executor();
    require(!post(context_target, caors::tasks::task{resource, [&] { ran = true; }}));
#else
    require(!persistent_context.push(caors::tasks::task{resource, [&] { ran = true; }}));
#endif
    require(persistent_context.drain() == 1 && ran);
  });
  // 常驻单 worker，逐个等待完成，避免队列积压改变延迟含义；同步信号量包含在延迟中。
  pool_type pool{1};
#if __has_include("caors/tasks/execution_context.h")
  auto target = pool.get_executor();
#else
  auto& target = pool;
#endif
  measure("pool.roundtrip", [&](caors::memory_resource* resource) {
    std::binary_semaphore completed{0};
    require(!post(target, caors::tasks::task{resource, [] {}}));
    // 单 worker 的下一任务开始时，前一任务的 callable 已释放；屏障自身使用系统资源。
    require(!post(target, caors::tasks::task{&caors::get_system_memory_resource(),
                                             [&] { completed.release(); }}));
    completed.acquire();
  });
}
