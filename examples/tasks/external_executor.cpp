// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/tasks.h"

#include <memory>
#include <mutex>
#include <vector>

namespace {
// 示例只在所属线程 drain；close 必须在停止 drain 后由宿主调用。
class host_queue final : public caors::tasks::external_executor_context {
public:
  caors::error post(caors::tasks::task next) override {
    std::lock_guard lock{mutex_};
    if (closed_) {
      return {caors::error_domain::core, caors::errc::frozen};
    }
    tasks_.push_back(std::move(next));
    return {};
  }
  void close() noexcept override {
    std::vector<caors::tasks::task> discarded;
    {
      std::lock_guard lock{mutex_};
      closed_ = true;
      discarded.swap(tasks_);
    }
  }
  void drain() {
    std::vector<caors::tasks::task> ready;
    {
      std::lock_guard lock{mutex_};
      ready.swap(tasks_);
    }
    for (auto& next : ready) {
      (void)next.run();
    }
  }

private:
  std::mutex mutex_;
  bool closed_ = false;
  std::vector<caors::tasks::task> tasks_;
};
}

int main() {
  auto queue = std::make_shared<host_queue>();
  caors::tasks::external_executor adapter{queue};
  int value = 0;
  auto request =
      caors::tasks::submit_request(adapter.get_executor(), caors::tasks::task{[&] { value = 42; }});
  queue->drain();
  return request.completed() && value == 42 ? 0 : 1;
}
