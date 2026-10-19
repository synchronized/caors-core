// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TASKS_DETAIL_ABANDON_GUARD_H_
#define CAORS_TASKS_DETAIL_ABANDON_GUARD_H_

#include "caors/tasks/task.h"

#include <memory>
#include <mutex>

namespace caors::tasks::detail {

// 投递结果和任务销毁可能先后任意发生；拒绝路径由调用者报告具体错误。
class abandonment final {
public:
  explicit abandonment(task callback) : callback_(std::move(callback)) {}
  void arm() noexcept { update(true, false, false); }
  void dropped() noexcept { update(false, true, false); }
  void release() noexcept { update(false, false, true); }

private:
  void update(bool arm, bool drop, bool release) noexcept {
    task notify;
    {
      std::lock_guard lock{mutex_};
      armed_ = armed_ || arm;
      dropped_ = dropped_ || drop;
      released_ = released_ || release;
      if (released_) {
        callback_.reset();
      } else if (armed_ && dropped_) {
        notify = std::move(callback_);
      }
    }
    if (notify) {
      (void)notify.run();
    }
  }
  std::mutex mutex_;
  task callback_;
  bool armed_ = false;
  bool dropped_ = false;
  bool released_ = false;
};

class abandon_guard final {
public:
  explicit abandon_guard(std::shared_ptr<abandonment> state) noexcept : state_(std::move(state)) {}
  abandon_guard(abandon_guard&&) noexcept = default;
  abandon_guard(const abandon_guard&) = delete;
  ~abandon_guard() noexcept {
    if (state_) {
      state_->dropped();
    }
  }
  void release() noexcept { state_->release(); }

private:
  std::shared_ptr<abandonment> state_;
};

} // namespace caors::tasks::detail

#endif
