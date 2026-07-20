// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_PTR_H_
#define CAORS_OBJECT_PTR_H_

#include "caors/meta/capability.h"
#include "caors/meta/object.h"
#include "caors/object/id.h"
#include "caors/traits/object_ptr.h"

#include <type_traits>

namespace caors::objects {

/**
 * @brief 非拥有对象指针。
 *
 * `objects::ptr<T>` 只保存对象指针，不延长对象生命周期。序列化时它通过对象引用 resolver
 * 转换为 `object_id` 或 `object_guid`，反序列化时再解析回对象指针。
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

/** @brief 对象指针类型和对象指针之间的访问函数表。 */
struct ptr_ops {
  using get_fn = object* (*)(const void*) noexcept;
  using set_fn = bool (*)(void*, object*) noexcept;

  get_fn get = nullptr;
  set_fn set = nullptr;

  [[nodiscard]] bool valid() const noexcept { return get != nullptr && set != nullptr; }
};

namespace detail {

using objects::ptr_ops;

template <typename T> struct ptr_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  [[nodiscard]] static object* get(const void* storage) noexcept {
    if (storage == nullptr) {
      return nullptr;
    }

    return caors::traits::object_ptr_access<type>::get(*static_cast<const type*>(storage));
  }

  static bool set(void* storage, object* instance) noexcept {
    if (storage == nullptr) {
      return false;
    }

    return caors::traits::object_ptr_access<type>::set(*static_cast<type*>(storage), instance);
  }

  inline static const ptr_ops value{&get, &set};
};

template <typename T> [[nodiscard]] const ptr_ops* ptr_ops_ptr_of() noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (caors::traits::object_ptr_traits<type>::is_object_ptr) {
    return ptr_ops_model<type>::value.valid() ? &ptr_ops_model<type>::value : nullptr;
  } else {
    return nullptr;
  }
}

} // namespace detail

} // namespace caors::objects

namespace caors::traits {

template <typename T> struct object_ptr_traits<objects::ptr<T>> {
  static constexpr bool is_object_ptr = true;
  using object_type = T;
};

template <typename T> struct object_ptr_access<objects::ptr<T>> {
  using object_type = T;

  [[nodiscard]] static objects::object* get(const objects::ptr<T>& source) noexcept {
    return source.get();
  }

  static bool set(objects::ptr<T>& source, objects::object* instance) noexcept {
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

} // namespace caors::traits

namespace caors::meta::capabilities {

template <typename T> struct static_capability<objects::ptr_ops, T> {
  [[nodiscard]] static const objects::ptr_ops* get() {
    return objects::detail::ptr_ops_ptr_of<T>();
  }
};

template <typename T>
struct module_static_capability_list<object_static_capability_module, objects::ptr<T>> {
  using type = type_list<objects::ptr_ops>;
};

} // namespace caors::meta::capabilities

namespace caors {

template <typename T> using object_ptr = objects::ptr<T>;

} // namespace caors

#endif
