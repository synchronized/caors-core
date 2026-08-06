// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/tasks/executor.h"

#include "caors/tasks/inline_executor.h"

#include "executor_control_block.h"

#include <utility>

namespace caors::tasks {

executor::executor(detail::executor_control_block* control, bool add_reference) noexcept
    : control_(control) {
  if (add_reference) {
    detail::retain_executor_control(control_);
  }
}

executor::executor(const executor& other) noexcept : executor(other.control_, true) {}

executor& executor::operator=(const executor& other) noexcept {
  executor copy{other};
  swap(copy);
  return *this;
}

executor::executor(executor&& other) noexcept : control_(std::exchange(other.control_, nullptr)) {}

executor& executor::operator=(executor&& other) noexcept {
  executor moved{std::move(other)};
  swap(moved);
  return *this;
}

executor::~executor() noexcept { detail::release_executor_control(control_); }

error executor::post(task next) const {
  if (!next) {
    return {error_domain::core, errc::invalid_argument};
  }
  if (control_ == nullptr || control_->post == nullptr) {
    return {error_domain::core, errc::frozen};
  }
  return control_->post(control_, std::move(next));
}

bool executor::valid() const noexcept {
  return control_ != nullptr && control_->valid != nullptr && control_->valid(control_);
}

memory_resource* executor::resource() const noexcept {
  return control_ != nullptr && control_->resource != nullptr ? control_->resource
                                                              : get_default_memory_resource();
}

void executor::swap(executor& other) noexcept { std::swap(control_, other.control_); }

namespace {

error post_inline(detail::executor_control_block*, task next) { return next.run(); }

bool inline_is_valid(const detail::executor_control_block*) noexcept { return true; }

detail::executor_control_block inline_control{
    .references = 1,
    .resource = nullptr,
    .post = &post_inline,
    .valid = &inline_is_valid,
    .destroy = nullptr,
    .immortal = true,
};

} // namespace

inline_executor::inline_executor() noexcept : executor_(&inline_control, false) {}

} // namespace caors::tasks
