// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TASKS_EXECUTOR_H_
#define CAORS_TASKS_EXECUTOR_H_

#include "caors/error.h"
#include "caors/export.h"
#include "caors/tasks/task.h"

#include <utility>

namespace caors::tasks {

namespace detail {
struct executor_control_block;
}

class execution_context;
class executor;
[[nodiscard]] CAORS_API executor inline_executor() noexcept;
class thread_pool;

/**
 * @brief 可复制的轻量任务提交句柄。
 *
 * executor 不拥有具体线程或队列。关联的 context 销毁后句柄仍可安全访问，但 `post()` 会返回
 * `errc::frozen`。复制和销毁句柄是线程安全的。
 */
class CAORS_API executor final {
public:
  constexpr executor() noexcept = default;
  executor(const executor& other) noexcept;
  executor& operator=(const executor& other) noexcept;
  executor(executor&& other) noexcept;
  executor& operator=(executor&& other) noexcept;
  ~executor() noexcept;

  /** @brief 投递任务。空任务返回 `errc::invalid_argument`。 */
  [[nodiscard]] error post(task next) const;

  /** @brief 判断句柄当前是否关联仍接受任务的执行环境。 */
  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  /** @brief 查询执行环境的长期内存资源；空句柄返回当前默认资源。 */
  [[nodiscard]] memory_resource* resource() const noexcept;

  /** @brief 交换两个句柄。 */
  void swap(executor& other) noexcept;

private:
  explicit executor(detail::executor_control_block* control, bool add_reference) noexcept;

  detail::executor_control_block* control_ = nullptr;

  friend class execution_context;
  friend class thread_pool;
  friend executor inline_executor() noexcept;
};

inline void swap(executor& lhs, executor& rhs) noexcept { lhs.swap(rhs); }

} // namespace caors::tasks

#endif
