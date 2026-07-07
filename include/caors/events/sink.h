// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_EVENTS_SINK_H_
#define CAORS_EVENTS_SINK_H_

#include "caors/events/connection.h"
#include "caors/events/signal.h"

#include <cassert>
#include <cstddef>

namespace caors::events {

template <typename Signature> class sink;

/**
 * @brief signal 的连接视图。
 *
 * `sink<void(Args...)>` 只暴露连接、断开和查询能力，不暴露 `emit()`。它适合把订阅入口交给外部代码，
 * 同时把事件触发权留在 signal 或 dispatcher 的拥有者手中。
 */
template <typename... Args> class sink<void(Args...)> final {
public:
  using signal_type = signal<void(Args...)>;
  using connection_type = connection;
  using id_type = connection::id_type;

  constexpr sink() noexcept = default;
  explicit constexpr sink(signal_type& target) noexcept : target_(&target) {}

  /** @brief 判断当前视图是否绑定到一个 signal。 */
  [[nodiscard]] explicit constexpr operator bool() const noexcept { return target_ != nullptr; }

  /** @brief 返回当前仍处于连接状态的监听者数量。空视图返回 0。 */
  [[nodiscard]] std::size_t size() const noexcept {
    return target_ != nullptr ? target_->size() : 0;
  }

  /** @brief 判断当前是否没有连接的监听者。 */
  [[nodiscard]] bool empty() const noexcept { return size() == 0; }

  /** @brief 连接编译期已知的自由函数、静态函数或无捕获 callable。 */
  template <auto Candidate> [[nodiscard]] connection connect() {
    assert(target_ != nullptr);
    return target_->template connect<Candidate>();
  }

  /** @brief 连接成员函数。对象由调用方持有，sink 不负责生命周期。 */
  template <auto Member, typename Type> [[nodiscard]] connection connect(Type* instance) {
    assert(target_ != nullptr);
    return target_->template connect<Member>(instance);
  }

  /** @brief 连接 const 成员函数。对象由调用方持有，sink 不负责生命周期。 */
  template <auto Member, typename Type> [[nodiscard]] connection connect(const Type* instance) {
    assert(target_ != nullptr);
    return target_->template connect<Member>(instance);
  }

  /** @brief 连接函数对象左值。函数对象由调用方持有，sink 不复制也不延长生命周期。 */
  template <typename Callable> [[nodiscard]] connection connect(Callable& callable) {
    assert(target_ != nullptr);
    return target_->connect(callable);
  }

  /** @brief 连接 const 函数对象左值。函数对象由调用方持有，sink 不复制也不延长生命周期。 */
  template <typename Callable> [[nodiscard]] connection connect(const Callable& callable) {
    assert(target_ != nullptr);
    return target_->connect(callable);
  }

  /** @brief 断开指定连接。 */
  bool disconnect(connection& current) noexcept {
    return target_ != nullptr && target_->disconnect(current);
  }

  /** @brief 断开指定 id 的连接。 */
  bool disconnect(id_type id) noexcept { return target_ != nullptr && target_->disconnect(id); }

  /** @brief 断开全部监听者。已有 connection 句柄不会被主动回写失效。 */
  void clear() noexcept {
    if (target_ != nullptr) {
      target_->clear();
    }
  }

private:
  signal_type* target_ = nullptr;
};

} // namespace caors::events

namespace caors {

template <typename Signature> using event_sink = events::sink<Signature>;

} // namespace caors

#endif
