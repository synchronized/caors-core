// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TASKS_EXECUTOR_CONTROL_BLOCK_H_
#define CAORS_TASKS_EXECUTOR_CONTROL_BLOCK_H_

#include "caors/error.h"
#include "caors/memory/resource.h"
#include "caors/tasks/task.h"

#include <atomic>
#include <cstddef>

namespace caors::tasks::detail {

struct executor_control_block {
  using post_fn = error (*)(executor_control_block*, task);
  using valid_fn = bool (*)(const executor_control_block*) noexcept;
  using destroy_fn = void (*)(executor_control_block*) noexcept;

  std::atomic_size_t references{1};
  memory_resource* resource = nullptr;
  post_fn post = nullptr;
  valid_fn valid = nullptr;
  destroy_fn destroy = nullptr;
  bool immortal = false;
};

inline void retain_executor_control(executor_control_block* control) noexcept {
  if (control != nullptr && !control->immortal) {
    control->references.fetch_add(1, std::memory_order_relaxed);
  }
}

inline void release_executor_control(executor_control_block* control) noexcept {
  if (control != nullptr && !control->immortal &&
      control->references.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    control->destroy(control);
  }
}

} // namespace caors::tasks::detail

#endif
