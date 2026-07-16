// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TASKS_TASK_H_
#define CAORS_TASKS_TASK_H_

#include "caors/error.h"
#include "caors/memory/resource.h"

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

namespace caors::tasks {

namespace detail {

template <typename Callable>
concept task_callable =
    std::invocable<Callable&> && (std::same_as<std::invoke_result_t<Callable&>, void> ||
                                  std::same_as<std::invoke_result_t<Callable&>, error>);

struct task_ops {
  using run_fn = error (*)(void*);
  using destroy_fn = void (*)(memory_resource*, void*) noexcept;

  run_fn run = nullptr;
  destroy_fn destroy = nullptr;

  [[nodiscard]] constexpr bool valid() const noexcept { return run != nullptr; }
};

template <typename Callable> [[nodiscard]] error run_task_callable(void* object) {
  auto& callable = *static_cast<Callable*>(object);
  if constexpr (std::same_as<std::invoke_result_t<Callable&>, void>) {
    std::invoke(callable);
    return {};
  } else {
    return std::invoke(callable);
  }
}

template <typename Callable>
void destroy_task_callable(memory_resource* resource, void* object) noexcept {
  caors::destroy(static_cast<Callable*>(object), resource);
}

} // namespace detail

/**
 * @brief move-only 任务包装器。
 *
 * `task` 保存一个可调用对象，调用对象可以返回 `void` 或 `caors::error`。任务内部使用传入的
 * `memory_resource` 分配 callable 存储，因此可以保存 move-only lambda。第一版不捕获 callable
 * 抛出的异常；需要异常隔离时应在 callable 或具体 executor 策略中处理。
 */
class task final {
public:
  constexpr task() noexcept = default;

  template <detail::task_callable Callable>
    requires(!std::same_as<std::remove_cvref_t<Callable>, task>)
  explicit task(Callable&& callable) {
    reset(nullptr, std::forward<Callable>(callable));
  }

  template <detail::task_callable Callable>
    requires(!std::same_as<std::remove_cvref_t<Callable>, task>)
  task(memory_resource* resource, Callable&& callable) {
    reset(resource, std::forward<Callable>(callable));
  }

  task(const task&) = delete;
  task& operator=(const task&) = delete;

  task(task&& other) noexcept { move_from(other); }

  task& operator=(task&& other) noexcept {
    if (this != &other) {
      reset();
      move_from(other);
    }
    return *this;
  }

  ~task() noexcept { reset(); }

  /** @brief 查询任务是否持有 callable。 */
  [[nodiscard]] explicit constexpr operator bool() const noexcept { return object_ != nullptr; }

  /** @brief 查询 callable 存储使用的内存资源。空任务返回 nullptr。 */
  [[nodiscard]] constexpr memory_resource* resource() const noexcept { return resource_; }

  /**
   * @brief 执行任务。
   *
   * 空任务返回 `errc::invalid_argument`。非空任务会调用内部 callable；如果 callable 返回
   * `caors::error`，该错误会原样返回。
   */
  [[nodiscard]] error run() {
    if (object_ == nullptr || !ops_.valid()) {
      return {error_domain::core, errc::invalid_argument};
    }

    return ops_.run(object_);
  }

  /** @brief 等价于 `run()`。 */
  [[nodiscard]] error operator()() { return run(); }

  /** @brief 释放当前 callable，让任务变为空。 */
  void reset() noexcept {
    if (object_ != nullptr && ops_.destroy != nullptr) {
      ops_.destroy(resource_, object_);
    }

    object_ = nullptr;
    resource_ = nullptr;
    ops_ = {};
  }

  /** @brief 替换当前 callable。 */
  template <detail::task_callable Callable>
    requires(!std::same_as<std::remove_cvref_t<Callable>, task>)
  void reset(memory_resource* resource, Callable&& callable) {
    using stored_type = std::remove_cvref_t<Callable>;

    reset();
    memory_resource* resolved = resource != nullptr ? resource : get_default_memory_resource();
    object_ = caors::make<stored_type>(resolved, std::forward<Callable>(callable));
    resource_ = resolved;
    ops_ = detail::task_ops{
        &detail::run_task_callable<stored_type>,
        &detail::destroy_task_callable<stored_type>,
    };
  }

  /** @brief 交换两个任务。 */
  void swap(task& other) noexcept {
    using std::swap;
    swap(object_, other.object_);
    swap(resource_, other.resource_);
    swap(ops_, other.ops_);
  }

private:
  void move_from(task& other) noexcept {
    object_ = std::exchange(other.object_, nullptr);
    resource_ = std::exchange(other.resource_, nullptr);
    ops_ = std::exchange(other.ops_, {});
  }

  void* object_ = nullptr;
  memory_resource* resource_ = nullptr;
  detail::task_ops ops_{};
};

inline void swap(task& lhs, task& rhs) noexcept { lhs.swap(rhs); }

} // namespace caors::tasks

#endif
