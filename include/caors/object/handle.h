// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_HANDLE_H_
#define CAORS_OBJECT_HANDLE_H_

#include "caors/object/id.h"

#include <type_traits>

namespace caors::objects {

class object;

/**
 * @brief 不拥有对象的运行期身份句柄。
 *
 * `object_handle` 只保存对象 id 和声明类型 id，不保证对象仍然存活，也不提供解引用能力。它适合在
 * 序列化、资源表或生命周期管理器之间传递对象身份；真正解析为指针需要上层对象数据库或 resolver。
 */
class object_handle {
public:
  constexpr object_handle() noexcept = default;
  constexpr object_handle(object_id id, object_type_id type) noexcept : id_(id), type_(type) {}

  [[nodiscard]] static constexpr object_handle invalid() noexcept { return {}; }
  [[nodiscard]] constexpr object_id id() const noexcept { return id_; }
  [[nodiscard]] constexpr object_type_id type() const noexcept { return type_; }
  [[nodiscard]] explicit constexpr operator bool() const noexcept { return id_ && type_; }

  friend constexpr bool operator==(object_handle lhs, object_handle rhs) noexcept {
    return lhs.id_ == rhs.id_ && lhs.type_ == rhs.type_;
  }

  friend constexpr bool operator!=(object_handle lhs, object_handle rhs) noexcept {
    return !(lhs == rhs);
  }

private:
  object_id id_{};
  object_type_id type_{};
};

/**
 * @brief 类型化对象身份句柄。
 *
 * 该类型只保存对象身份，不拥有对象、不延长生命周期，也不隐式解析指针。它适合表达 manager / world
 * 托管对象的弱身份，需要显式交给 `objects::manager` 或上层对象数据库解析。
 */
template <typename T> class typed_object_handle {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");

public:
  typed_object_handle() noexcept = default;
  typed_object_handle(std::nullptr_t) noexcept {}
  explicit typed_object_handle(object_handle handle) noexcept : handle_(handle) {}
  explicit typed_object_handle(const T& instance) noexcept
      : handle_(instance.id(), instance.type_id()) {}

  [[nodiscard]] object_handle handle() const noexcept { return handle_; }
  [[nodiscard]] object_id id() const noexcept { return handle_.id(); }
  [[nodiscard]] object_type_id type() const noexcept { return handle_.type(); }
  [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(handle_); }

  void reset(object_handle handle = {}) noexcept { handle_ = handle; }

private:
  object_handle handle_{};
};

} // namespace caors::objects

#endif
