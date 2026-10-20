// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_HANDLE_H_
#define CAORS_OBJECT_HANDLE_H_

#include "caors/meta/capability.h"
#include "caors/meta/object.h"
#include "caors/object/id.h"

#include <cstddef>
#include <type_traits>

namespace caors::objects {

/**
 * @brief 不拥有对象的运行期身份句柄。
 *
 * `handle` 只保存对象 id 和声明类型 id，不保证对象仍然存活，也不提供解引用能力。它适合在
 * 序列化、资源表或生命周期管理器之间传递对象身份；真正解析为指针需要上层对象数据库或 resolver。
 */
class handle {
public:
  constexpr handle() noexcept = default;
  constexpr handle(std::nullptr_t) noexcept {}
  constexpr handle(object_id id, type_id type) noexcept : id_(id), type_(type) {}

  [[nodiscard]] static constexpr handle invalid() noexcept { return {}; }
  [[nodiscard]] constexpr object_id id() const noexcept { return id_; }
  [[nodiscard]] constexpr type_id type() const noexcept { return type_; }
  [[nodiscard]] explicit constexpr operator bool() const noexcept { return id_ && type_; }

  constexpr void reset() noexcept {
    id_ = {};
    type_ = {};
  }

  constexpr void reset(handle value) noexcept {
    id_ = value.id();
    type_ = value.type();
  }

private:
  object_id id_{};
  type_id type_{};
};

[[nodiscard]] constexpr bool operator==(handle lhs, handle rhs) noexcept {
  return lhs.id() == rhs.id() && lhs.type() == rhs.type();
}

/**
 * @brief 类型化对象身份句柄。
 *
 * 该类型只保存对象身份，不拥有对象、不延长生命周期，也不隐式解析指针。它适合表达 manager / world
 * 托管对象的弱身份，需要显式交给 `objects::manager` 或上层对象数据库解析。
 */
template <typename T> class typed_handle {
  static_assert(std::is_base_of_v<meta::object, T>, "T must derive from caors::meta::object");

public:
  constexpr typed_handle() noexcept = default;
  constexpr typed_handle(std::nullptr_t) noexcept {}
  constexpr typed_handle(object_id id, type_id type) noexcept : value_(id, type) {}
  explicit constexpr typed_handle(objects::handle value) noexcept : value_(value) {}
  explicit typed_handle(const T& instance) noexcept : value_(instance.id(), instance.type_id()) {}

  [[nodiscard]] static constexpr typed_handle invalid() noexcept { return {}; }
  [[nodiscard]] constexpr object_id id() const noexcept { return value_.id(); }
  [[nodiscard]] constexpr type_id type() const noexcept { return value_.type(); }
  [[nodiscard]] explicit constexpr operator bool() const noexcept {
    return static_cast<bool>(value_);
  }

  [[nodiscard]] constexpr objects::handle handle() const noexcept { return value_; }

  constexpr void reset() noexcept { value_.reset(); }
  constexpr void reset(objects::handle value) noexcept { value_.reset(value); }

private:
  objects::handle value_{};
};

template <typename T>
[[nodiscard]] constexpr bool operator==(typed_handle<T> lhs, handle rhs) noexcept {
  return lhs.handle() == rhs;
}

template <typename T>
[[nodiscard]] constexpr bool operator==(handle lhs, typed_handle<T> rhs) noexcept {
  return lhs == rhs.handle();
}

template <typename Lhs, typename Rhs>
[[nodiscard]] constexpr bool operator==(typed_handle<Lhs> lhs, typed_handle<Rhs> rhs) noexcept {
  return lhs.handle() == rhs.handle();
}

namespace detail {

/** @brief 对象身份句柄类型和统一身份句柄之间的访问函数表。 */
struct handle_ops {
  using get_fn = handle (*)(const void*) noexcept;
  using set_fn = bool (*)(void*, handle) noexcept;

  get_fn get = nullptr;
  set_fn set = nullptr;

  [[nodiscard]] bool valid() const noexcept { return get != nullptr && set != nullptr; }
};

template <typename T> struct handle_traits {
  static constexpr bool is_handle = false;
};

template <> struct handle_traits<handle> {
  static constexpr bool is_handle = true;
};

template <typename T> struct handle_traits<typed_handle<T>> {
  static constexpr bool is_handle = true;
};

[[nodiscard]] constexpr handle to_untyped_handle(handle source) noexcept { return source; }

template <typename T>
[[nodiscard]] constexpr handle to_untyped_handle(typed_handle<T> source) noexcept {
  return source.handle();
}

template <typename T> struct handle_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  [[nodiscard]] static handle get(const void* storage) noexcept {
    if (storage == nullptr) {
      return {};
    }

    return to_untyped_handle(*static_cast<const type*>(storage));
  }

  static bool set(void* storage, handle source) noexcept {
    if (storage == nullptr) {
      return false;
    }

    static_cast<type*>(storage)->reset(source);
    return true;
  }

  inline static const handle_ops value{&get, &set};
};

template <typename T> [[nodiscard]] const handle_ops* handle_ops_ptr_of() noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (handle_traits<type>::is_handle) {
    return handle_ops_model<type>::value.valid() ? &handle_ops_model<type>::value : nullptr;
  } else {
    return nullptr;
  }
}

} // namespace detail

} // namespace caors::objects

namespace caors::meta::capabilities {

template <typename T> struct static_capability<objects::detail::handle_ops, T> {
  [[nodiscard]] static const objects::detail::handle_ops* get() {
    return objects::detail::handle_ops_ptr_of<T>();
  }
};

template <> struct module_static_capability_list<object_static_capability_module, objects::handle> {
  using type = type_list<objects::detail::handle_ops>;
};

template <typename T>
struct module_static_capability_list<object_static_capability_module, objects::typed_handle<T>> {
  using type = type_list<objects::detail::handle_ops>;
};

} // namespace caors::meta::capabilities

namespace caors {

using object_handle = objects::handle;

template <typename T> using typed_object_handle = objects::typed_handle<T>;

} // namespace caors

#endif
