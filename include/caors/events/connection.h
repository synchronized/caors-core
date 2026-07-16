// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_EVENTS_CONNECTION_H_
#define CAORS_EVENTS_CONNECTION_H_

#include <cstdint>
#include <utility>

namespace caors::events {

/**
 * @brief 信号订阅连接句柄。
 *
 * `connection` 是非拥有句柄，只能在创建它的 `signal` 存活期间使用。调用 `disconnect()` 会从对应
 * signal 中断开订阅，并让当前句柄失效。
 */
class connection final {
public:
  using id_type = std::uint64_t;

  struct connection_ops {
    using disconnect_fn = bool (*)(void* owner, id_type id) noexcept;

    disconnect_fn disconnect = nullptr;

    [[nodiscard]] constexpr bool valid() const noexcept { return disconnect != nullptr; }
  };

  constexpr connection() noexcept = default;
  connection(void* owner, id_type id, connection_ops ops) noexcept
      : owner_(owner), id_(id), ops_(ops) {}

  connection(const connection&) = delete;
  connection& operator=(const connection&) = delete;

  connection(connection&& other) noexcept { move_from(other); }

  connection& operator=(connection&& other) noexcept {
    if (this != &other) {
      reset();
      move_from(other);
    }
    return *this;
  }

  /** @brief 返回订阅 id；空连接返回 0。 */
  [[nodiscard]] constexpr id_type id() const noexcept { return id_; }

  /** @brief 判断当前句柄是否还引用一个 signal。 */
  [[nodiscard]] explicit constexpr operator bool() const noexcept { return owner_ != nullptr; }

  /** @brief 断开订阅。重复断开会返回 false。 */
  bool disconnect() noexcept {
    if (owner_ == nullptr || !ops_.valid()) {
      return false;
    }

    bool disconnected = ops_.disconnect(owner_, id_);
    reset();
    return disconnected;
  }

  /** @brief 释放句柄但不主动断开订阅。 */
  void release() noexcept { reset(); }

private:
  void reset() noexcept {
    owner_ = nullptr;
    id_ = 0;
    ops_ = {};
  }

  void move_from(connection& other) noexcept {
    owner_ = std::exchange(other.owner_, nullptr);
    id_ = std::exchange(other.id_, 0);
    ops_ = std::exchange(other.ops_, {});
  }

  void* owner_ = nullptr;
  id_type id_ = 0;
  connection_ops ops_{};
};

/**
 * @brief RAII 信号连接。
 *
 * `scoped_connection` 析构时自动断开订阅。它不延长 signal 生命周期，因此应保证 scoped connection
 * 早于对应 signal 销毁。
 */
class scoped_connection final {
public:
  scoped_connection() noexcept = default;
  explicit scoped_connection(connection&& current) noexcept : current_(std::move(current)) {}

  scoped_connection(const scoped_connection&) = delete;
  scoped_connection& operator=(const scoped_connection&) = delete;

  scoped_connection(scoped_connection&& other) noexcept : current_(std::move(other.current_)) {}

  scoped_connection& operator=(scoped_connection&& other) noexcept {
    if (this != &other) {
      disconnect();
      current_ = std::move(other.current_);
    }
    return *this;
  }

  ~scoped_connection() noexcept { disconnect(); }

  /** @brief 判断当前是否持有连接句柄。 */
  [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(current_); }

  /** @brief 手动断开当前连接。 */
  bool disconnect() noexcept { return current_.disconnect(); }

  /** @brief 替换当前连接；旧连接会先断开。 */
  void reset(connection&& next = {}) noexcept {
    disconnect();
    current_ = std::move(next);
  }

  /** @brief 释放连接句柄并交给调用方，析构时不再自动断开。 */
  [[nodiscard]] connection release() noexcept { return std::move(current_); }

private:
  connection current_{};
};

} // namespace caors::events

namespace caors {

using event_connection = events::connection;
using scoped_event_connection = events::scoped_connection;

} // namespace caors

#endif
