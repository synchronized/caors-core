// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_CORE_DETAIL_SHARED_SNAPSHOT_H_
#define CAORS_CORE_DETAIL_SHARED_SNAPSHOT_H_

#include <atomic>
#include <memory>
#include <mutex>
#include <utility>

namespace caors::detail {

#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
inline constexpr bool has_atomic_shared_ptr = true;
#else
inline constexpr bool has_atomic_shared_ptr = false;
#endif

/** @brief 保持共享所有权和原子发布语义的内部快照槽。 */
template <class T, bool Native = has_atomic_shared_ptr> class shared_snapshot;

#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
template <class T> class shared_snapshot<T, true> final {
public:
  shared_snapshot() noexcept = default;
  explicit shared_snapshot(std::shared_ptr<T> value) noexcept : value_(std::move(value)) {}
  [[nodiscard]] std::shared_ptr<T>
  load(std::memory_order order = std::memory_order_seq_cst) const noexcept {
    return value_.load(order);
  }
  void store(std::shared_ptr<T> value,
             std::memory_order order = std::memory_order_seq_cst) noexcept {
    value_.store(std::move(value), order);
  }
  bool compare_exchange_strong(std::shared_ptr<T>& expected, std::shared_ptr<T> desired) noexcept {
    return value_.compare_exchange_strong(expected, std::move(desired));
  }

private:
  std::atomic<std::shared_ptr<T>> value_{};
};
#endif

/** @brief 缺少标准原子特化时采用短临界区；析构和 expected 更新均在锁外执行。 */
template <class T> class shared_snapshot<T, false> final {
public:
  shared_snapshot() noexcept = default;
  explicit shared_snapshot(std::shared_ptr<T> value) noexcept : value_(std::move(value)) {}
  shared_snapshot(const shared_snapshot&) = delete;
  shared_snapshot& operator=(const shared_snapshot&) = delete;
  [[nodiscard]] std::shared_ptr<T>
  load([[maybe_unused]] std::memory_order order = std::memory_order_seq_cst) const noexcept {
    std::lock_guard lock{mutex_};
    return value_;
  }
  void store(std::shared_ptr<T> value,
             [[maybe_unused]] std::memory_order order = std::memory_order_seq_cst) noexcept {
    {
      std::lock_guard lock{mutex_};
      value_.swap(value);
    }
  }
  bool compare_exchange_strong(std::shared_ptr<T>& expected, std::shared_ptr<T> desired) noexcept {
    std::shared_ptr<T> observed;
    bool matched;
    {
      std::lock_guard lock{mutex_};
      // 相同地址不等于相同拥有者；别名 shared_ptr 必须与标准 CAS 保持一致。
      matched = value_.get() == expected.get() && !value_.owner_before(expected) &&
                !expected.owner_before(value_);
      if (matched) {
        value_.swap(desired);
      } else {
        observed = value_;
      }
    }
    if (!matched)
      expected = std::move(observed);
    return matched;
  }

private:
  mutable std::mutex mutex_;
  std::shared_ptr<T> value_;
};

} // namespace caors::detail
#endif
