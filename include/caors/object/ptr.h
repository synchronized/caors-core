// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_PTR_H_
#define CAORS_OBJECT_PTR_H_

#include "caors/meta/capability.h"
#include "caors/object/id.h"

#include <type_traits>

namespace caors::objects {

class object;

/**
 * @brief 非拥有对象指针。
 *
 * `objects::ptr<T>` 只保存对象指针，不延长对象生命周期。序列化时它通过
 * `serialization::reference_resolver` 转换为 `object_id`，反序列化时再从 `object_id`
 * 解析回对象指针。
 */
template <typename T> class ptr {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");

public:
  ptr() noexcept = default;
  ptr(std::nullptr_t) noexcept {}
  explicit ptr(T* instance) noexcept : instance_(instance) {}

  [[nodiscard]] T* get() const noexcept { return instance_; }
  [[nodiscard]] object_id id() const noexcept {
    return instance_ != nullptr ? instance_->id() : object_id::invalid();
  }
  [[nodiscard]] explicit operator bool() const noexcept { return instance_ != nullptr; }

  void reset(T* instance = nullptr) noexcept { instance_ = instance; }

private:
  T* instance_ = nullptr;
};

namespace detail {

using ptr_get_fn = object* (*)(const void*) noexcept;
using ptr_set_fn = bool (*)(void*, object*) noexcept;

/** @brief 对象指针类型和对象指针之间的访问函数表。 */
struct ptr_ops {
  ptr_get_fn get = nullptr;
  ptr_set_fn set = nullptr;

  [[nodiscard]] bool valid() const noexcept { return get != nullptr && set != nullptr; }
};

template <typename T> struct ptr_traits {
  static constexpr bool is_ptr = false;
  using object_type = void;
};

template <typename T> struct ptr_traits<ptr<T>> {
  static constexpr bool is_ptr = true;
  using object_type = T;
};

template <typename T> struct ptr_access {
  [[nodiscard]] static object* get(const T&) noexcept { return nullptr; }
  static bool set(T&, object*) noexcept { return false; }
};

template <typename T> struct ptr_access<ptr<T>> {
  using object_type = T;

  [[nodiscard]] static object* get(const ptr<T>& source) noexcept { return source.get(); }

  static bool set(ptr<T>& source, object* instance) noexcept {
    if (instance == nullptr) {
      source.reset();
      return true;
    }

    auto* typed = dynamic_cast<object_type*>(instance);
    if (typed == nullptr) {
      return false;
    }

    source.reset(typed);
    return true;
  }
};

template <typename T> [[nodiscard]] const ptr_ops* ptr_ops_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (ptr_traits<type>::is_ptr) {
    static const ptr_ops data{
        [](const void* storage) noexcept -> object* {
          if (storage == nullptr) {
            return nullptr;
          }

          return ptr_access<type>::get(*static_cast<const type*>(storage));
        },
        [](void* storage, object* instance) noexcept -> bool {
          if (storage == nullptr) {
            return false;
          }

          return ptr_access<type>::set(*static_cast<type*>(storage), instance);
        },
    };

    return data.valid() ? &data : nullptr;
  } else {
    return nullptr;
  }
}

} // namespace detail

} // namespace caors::objects

namespace caors::meta::capabilities {

template <typename T> struct static_capability<objects::detail::ptr_ops, T> {
  [[nodiscard]] static const objects::detail::ptr_ops* get() {
    return objects::detail::ptr_ops_ptr_of<T>();
  }
};

template <typename T>
struct module_static_capability_list<object_static_capability_module, objects::ptr<T>> {
  using type = type_list<objects::detail::ptr_ops>;
};

} // namespace caors::meta::capabilities

namespace caors {

template <typename T> using object_ptr = objects::ptr<T>;

} // namespace caors

#endif
