// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_REF_H_
#define CAORS_OBJECT_REF_H_

#include "caors/object/id.h"

#include <cstddef>
#include <type_traits>

namespace caors {

class object;

namespace objects {

/**
 * @brief 不拥有对象的运行期身份句柄。
 *
 * `objects::handle` 只保存对象 id 和声明类型 id，不保证对象仍然存活，也不提供解引用能力。它适合在
 * 序列化、资源表或生命周期管理器之间传递对象身份；真正解析为指针需要上层对象数据库或 resolver。
 */
class handle {
public:
  constexpr handle() noexcept = default;
  constexpr handle(object_id id, object_type_id type) noexcept : id_(id), type_(type) {}

  [[nodiscard]] static constexpr handle invalid() noexcept { return {}; }
  [[nodiscard]] constexpr object_id id() const noexcept { return id_; }
  [[nodiscard]] constexpr object_type_id type() const noexcept { return type_; }
  [[nodiscard]] explicit constexpr operator bool() const noexcept { return id_ && type_; }

  friend constexpr bool operator==(handle lhs, handle rhs) noexcept {
    return lhs.id_ == rhs.id_ && lhs.type_ == rhs.type_;
  }

  friend constexpr bool operator!=(handle lhs, handle rhs) noexcept { return !(lhs == rhs); }

private:
  object_id id_{};
  object_type_id type_{};
};

/**
 * @brief 非拥有对象引用。
 *
 * `objects::ref<T>` 只保存对象指针，不延长对象生命周期。序列化时它通过
 * `serialization::reference_resolver` 转换为 `object_id`，反序列化时再从 `object_id`
 * 解析回对象指针。
 */
template <typename T> class ref {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");

public:
  ref() noexcept = default;
  ref(std::nullptr_t) noexcept {}
  explicit ref(T* instance) noexcept : instance_(instance) {}

  [[nodiscard]] T* get() const noexcept { return instance_; }
  [[nodiscard]] object_id id() const noexcept {
    return instance_ != nullptr ? instance_->id() : object_id::invalid();
  }
  [[nodiscard]] explicit operator bool() const noexcept { return instance_ != nullptr; }

  void reset(T* instance = nullptr) noexcept { instance_ = instance; }

private:
  T* instance_ = nullptr;
};

/**
 * @brief 类型化弱对象句柄。
 *
 * 该类型只保存对象身份，不拥有对象、不延长生命周期，也不隐式解析指针。它为后续对象生命周期管理器、
 * 弱引用 resolver 和资源引用序列化预留稳定边界。
 */
template <typename T> class weak_handle {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");

public:
  weak_handle() noexcept = default;
  weak_handle(std::nullptr_t) noexcept {}
  explicit weak_handle(handle object_handle) noexcept : handle_(object_handle) {}
  explicit weak_handle(const T& instance) noexcept : handle_(instance.id(), instance.type_id()) {}

  [[nodiscard]] handle handle() const noexcept { return handle_; }
  [[nodiscard]] object_id id() const noexcept { return handle_.id(); }
  [[nodiscard]] object_type_id type() const noexcept { return handle_.type(); }
  [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(handle_); }

  void reset(::caors::objects::handle object_handle = {}) noexcept { handle_ = object_handle; }

private:
  ::caors::objects::handle handle_{};
};

} // namespace objects

template <typename T> using object_ref = objects::ref<T>;

} // namespace caors

#endif
