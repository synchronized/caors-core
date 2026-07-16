// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_EVENTS_SIGNAL_H_
#define CAORS_EVENTS_SIGNAL_H_

#include "caors/core/containers.h"
#include "caors/events/connection.h"
#include "caors/events/delegate.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace caors::events {

template <typename Signature> class signal;

/**
 * @brief 同步广播信号。
 *
 * 第一版只支持 `void(Args...)` 签名。signal
 * 按连接顺序同步调用监听者，不做线程同步，不拥有监听对象。 emit 期间断开连接会延迟清理；emit
 * 期间新连接的监听者从下一次 emit 开始生效。
 */
template <typename... Args> class signal<void(Args...)> final {
public:
  using signature_type = void(Args...);
  using delegate_type = delegate<signature_type>;
  using connection_type = connection;
  using id_type = connection::id_type;

  explicit signal(memory_resource* resource = nullptr)
      : slots_(typename slot_list::allocator_type{resource}) {}

  signal(const signal&) = delete;
  signal& operator=(const signal&) = delete;
  signal(signal&&) = delete;
  signal& operator=(signal&&) = delete;

  /** @brief 返回当前仍处于连接状态的监听者数量。 */
  [[nodiscard]] std::size_t size() const noexcept {
    std::size_t count = 0;
    for (const slot& current : slots_) {
      if (current.connected) {
        ++count;
      }
    }
    return count;
  }

  [[nodiscard]] bool empty() const noexcept { return size() == 0; }

  /** @brief 连接编译期已知的自由函数、静态函数或无捕获 callable。 */
  template <auto Candidate> [[nodiscard]] connection connect() {
    delegate_type callback;
    callback.template connect<Candidate>();
    return add(std::move(callback));
  }

  /** @brief 连接成员函数。对象由调用方持有，signal 不负责生命周期。 */
  template <auto Member, typename Type> [[nodiscard]] connection connect(Type* instance) {
    delegate_type callback;
    callback.template connect<Member>(instance);
    return add(std::move(callback));
  }

  /** @brief 连接 const 成员函数。对象由调用方持有，signal 不负责生命周期。 */
  template <auto Member, typename Type> [[nodiscard]] connection connect(const Type* instance) {
    delegate_type callback;
    callback.template connect<Member>(instance);
    return add(std::move(callback));
  }

  /** @brief 连接函数对象左值。函数对象由调用方持有，signal 不复制也不延长生命周期。 */
  template <typename Callable> [[nodiscard]] connection connect(Callable& callable) {
    delegate_type callback;
    callback.connect(callable);
    return add(std::move(callback));
  }

  /** @brief 连接 const 函数对象左值。函数对象由调用方持有，signal 不复制也不延长生命周期。 */
  template <typename Callable> [[nodiscard]] connection connect(const Callable& callable) {
    delegate_type callback;
    callback.connect(callable);
    return add(std::move(callback));
  }

  /** @brief 断开指定连接。 */
  bool disconnect(connection& current) noexcept { return current.disconnect(); }

  /** @brief 断开指定 id 的连接。 */
  bool disconnect(id_type id) noexcept { return disconnect_id(id); }

  /** @brief 断开全部监听者。已有 connection 句柄不会被主动回写失效。 */
  void clear() noexcept {
    if (emitting_ > 0) {
      for (slot& current : slots_) {
        current.connected = false;
      }
      dirty_ = true;
      return;
    }

    slots_.clear();
  }

  /** @brief 按连接顺序同步广播。 */
  void emit(Args... args) {
    ++emitting_;
    const std::size_t initial_size = slots_.size();
    for (std::size_t index = 0; index < initial_size; ++index) {
      if (slots_[index].connected) {
        delegate_type callback = slots_[index].callback;
        callback(args...);
      }
    }
    --emitting_;

    if (emitting_ == 0 && dirty_) {
      compact();
    }
  }

  /** @brief `emit()` 的调用运算符别名。 */
  void operator()(Args... args) { emit(args...); }

private:
  struct slot {
    id_type id = 0;
    delegate_type callback{};
    bool connected = true;
  };

  using slot_list = core::vector<slot>;

  [[nodiscard]] connection add(delegate_type callback) {
    const id_type id = next_id_++;
    slots_.push_back(slot{id, std::move(callback), true});
    return connection{this, id, connection::connection_ops{&signal::disconnect_from_connection}};
  }

  static bool disconnect_from_connection(void* owner, id_type id) noexcept {
    auto* target = static_cast<signal*>(owner);
    return target != nullptr && target->disconnect_id(id);
  }

  bool disconnect_id(id_type id) noexcept {
    for (slot& current : slots_) {
      if (current.id != id || !current.connected) {
        continue;
      }

      current.connected = false;
      if (emitting_ > 0) {
        dirty_ = true;
      } else {
        compact();
      }
      return true;
    }

    return false;
  }

  void compact() noexcept {
    slots_.erase(std::remove_if(slots_.begin(), slots_.end(),
                                [](const slot& current) { return !current.connected; }),
                 slots_.end());
    dirty_ = false;
  }

  slot_list slots_;
  id_type next_id_ = 1;
  std::uint32_t emitting_ = 0;
  bool dirty_ = false;
};

template <typename Result, typename... Args> class signal<Result(Args...)> {
  static_assert(std::is_void_v<Result>, "events::signal only supports void return signatures");
};

} // namespace caors::events

#endif
