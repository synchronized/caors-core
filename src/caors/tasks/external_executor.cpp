// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/tasks/external_executor.h"

#include "executor_control_block.h"

#include <mutex>
#include <utility>

namespace caors::tasks {
namespace {

struct external_control final : detail::executor_control_block {
  std::mutex mutex;
  std::shared_ptr<external_executor_context> context;
};

error post_external(detail::executor_control_block* base, task next) {
  auto* control = static_cast<external_control*>(base);
  std::shared_ptr<external_executor_context> context;
  {
    std::lock_guard lock{control->mutex};
    context = control->context;
  }
  // 不在内部锁下调用宿主；局部引用使并发关闭时上下文仍有效。
#if CAORS_HAS_EXCEPTIONS
  try {
#endif
    return context ? context->post(std::move(next)) : error{error_domain::core, errc::frozen};
#if CAORS_HAS_EXCEPTIONS
  } catch (...) {
    return {error_domain::core, errc::backend_failure};
  }
#endif
}

bool external_valid(const detail::executor_control_block* base) noexcept {
  auto* control = const_cast<external_control*>(static_cast<const external_control*>(base));
  std::lock_guard lock{control->mutex};
  return control->context != nullptr;
}

void destroy_external(detail::executor_control_block* base) noexcept {
  auto* control = static_cast<external_control*>(base);
  caors::destroy(control, control->resource);
}

} // namespace

external_executor::external_executor(std::shared_ptr<external_executor_context> context,
                                     memory_resource* resource) {
  if (!context) {
    return;
  }
  auto* memory = resource != nullptr ? resource : get_default_memory_resource();
  auto* control = caors::make<external_control>(memory);
  control->resource = memory;
  control->post = &post_external;
  control->valid = &external_valid;
  control->destroy = &destroy_external;
  control->context = std::move(context);
  control_ = control;
}

external_executor::~external_executor() noexcept {
  close();
  detail::release_executor_control(control_);
}

executor external_executor::get_executor() const noexcept { return executor{control_, true}; }

void external_executor::close() noexcept {
  if (control_ == nullptr) {
    return;
  }
  auto* control = static_cast<external_control*>(control_);
  std::shared_ptr<external_executor_context> context;
  {
    std::lock_guard lock{control->mutex};
    context = std::move(control->context);
  }
  if (context) {
    context->close();
  }
}

} // namespace caors::tasks
