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

namespace detail {

struct untyped_handle_tag {};

template <typename T>
struct valid_handle_type : std::bool_constant<std::is_base_of_v<object, T>> {};

template <> struct valid_handle_type<untyped_handle_tag> : std::true_type {};

template <typename T>
inline constexpr bool typed_handle_type_v = !std::is_same_v<T, untyped_handle_tag>;

/**
 * @brief 对象身份句柄的共享实现。
 *
 * 非类型化句柄和类型化句柄都只保存对象 id 和声明类型 id。类型化版本额外在编译期约束
 * `T` 必须继承 `meta::object`，但不会拥有对象或隐式解析对象。
 */
template <typename T> class basic_handle {
  static_assert(valid_handle_type<T>::value, "T must derive from caors::meta::object");

public:
  constexpr basic_handle() noexcept = default;
  constexpr basic_handle(std::nullptr_t) noexcept {}
  constexpr basic_handle(object_id id, object_type_id type) noexcept : id_(id), type_(type) {}

  template <typename U = T>
    requires typed_handle_type_v<U>
  explicit constexpr basic_handle(const basic_handle<untyped_handle_tag>& value) noexcept
      : id_(value.id()), type_(value.type()) {}

  template <typename U = T>
    requires typed_handle_type_v<U>
  explicit basic_handle(const U& instance) noexcept
      : id_(instance.id()), type_(instance.type_id()) {}

  [[nodiscard]] static constexpr basic_handle invalid() noexcept { return {}; }
  [[nodiscard]] constexpr object_id id() const noexcept { return id_; }
  [[nodiscard]] constexpr object_type_id type() const noexcept { return type_; }
  [[nodiscard]] explicit constexpr operator bool() const noexcept { return id_ && type_; }

  [[nodiscard]] constexpr basic_handle<untyped_handle_tag> handle() const noexcept
    requires typed_handle_type_v<T>
  {
    return {id_, type_};
  }

  constexpr void reset() noexcept {
    id_ = {};
    type_ = {};
  }

  constexpr void reset(basic_handle<untyped_handle_tag> value) noexcept {
    id_ = value.id();
    type_ = value.type();
  }

private:
  object_id id_{};
  object_type_id type_{};
};

template <typename Lhs, typename Rhs>
[[nodiscard]] constexpr bool operator==(basic_handle<Lhs> lhs, basic_handle<Rhs> rhs) noexcept {
  return lhs.id() == rhs.id() && lhs.type() == rhs.type();
}

template <typename Lhs, typename Rhs>
[[nodiscard]] constexpr bool operator!=(basic_handle<Lhs> lhs, basic_handle<Rhs> rhs) noexcept {
  return !(lhs == rhs);
}

/** @brief 对象身份句柄类型和统一身份句柄之间的访问函数表。 */
struct handle_ops {
  using get_fn = basic_handle<untyped_handle_tag> (*)(const void*) noexcept;
  using set_fn = bool (*)(void*, basic_handle<untyped_handle_tag>) noexcept;

  get_fn get = nullptr;
  set_fn set = nullptr;

  [[nodiscard]] bool valid() const noexcept { return get != nullptr && set != nullptr; }
};

template <typename T> struct handle_traits {
  static constexpr bool is_handle = false;
};

template <typename T> struct handle_traits<basic_handle<T>> {
  static constexpr bool is_handle = true;
};

template <typename T>
[[nodiscard]] constexpr basic_handle<untyped_handle_tag>
to_untyped_handle(basic_handle<T> source) noexcept {
  if constexpr (typed_handle_type_v<T>) {
    return source.handle();
  } else {
    return source;
  }
}

template <typename T> struct handle_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  [[nodiscard]] static basic_handle<untyped_handle_tag> get(const void* storage) noexcept {
    if (storage == nullptr) {
      return {};
    }

    return to_untyped_handle(*static_cast<const type*>(storage));
  }

  static bool set(void* storage, basic_handle<untyped_handle_tag> value) noexcept {
    if (storage == nullptr) {
      return false;
    }

    static_cast<type*>(storage)->reset(value);
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

using handle_ops = detail::handle_ops;

/**
 * @brief 不拥有对象的运行期身份句柄。
 *
 * `handle` 只保存对象 id 和声明类型 id，不保证对象仍然存活，也不提供解引用能力。它适合在
 * 序列化、资源表或生命周期管理器之间传递对象身份；真正解析为指针需要上层对象数据库或 resolver。
 */
using handle = detail::basic_handle<detail::untyped_handle_tag>;

/**
 * @brief 类型化对象身份句柄。
 *
 * 该类型只保存对象身份，不拥有对象、不延长生命周期，也不隐式解析指针。它适合表达 manager / world
 * 托管对象的弱身份，需要显式交给 `objects::manager` 或上层对象数据库解析。
 */
template <typename T> using typed_handle = detail::basic_handle<T>;

} // namespace caors::objects

namespace caors::meta::capabilities {

template <typename T> struct static_capability<objects::handle_ops, T> {
  [[nodiscard]] static const objects::handle_ops* get() {
    return objects::detail::handle_ops_ptr_of<T>();
  }
};

template <typename T>
struct module_static_capability_list<object_static_capability_module,
                                     objects::detail::basic_handle<T>> {
  using type = type_list<objects::handle_ops>;
};

} // namespace caors::meta::capabilities

namespace caors {

using object_handle = objects::handle;

template <typename T> using typed_object_handle = objects::typed_handle<T>;

} // namespace caors

#endif
