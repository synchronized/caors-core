// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_FACTORY_H_
#define CAORS_OBJECT_FACTORY_H_

#include "caors/memory.h"
#include "caors/object/base.h"
#include "caors/object/detail/factory_ops.h"

#include <type_traits>
#include <utility>

namespace caors::objects::detail {

template <typename T>
[[nodiscard]] object* try_create_object_instance(memory_resource* resource) noexcept {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");
  static_assert(std::is_default_constructible_v<T>, "T must be default constructible");

  memory_resource* resolved = resource != nullptr ? resource : get_default_memory_resource();
  void* storage = resolved->try_allocate(sizeof(T), alignof(T));
  if (storage == nullptr) {
    return nullptr;
  }

#if CAORS_HAS_EXCEPTIONS
  try {
    return ::new (storage) T();
  } catch (...) {
    resolved->deallocate(storage, sizeof(T), alignof(T));
    return nullptr;
  }
#else
  return ::new (storage) T();
#endif
}

template <typename T> [[nodiscard]] object* create_object_instance(memory_resource* resource) {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");
  static_assert(std::is_default_constructible_v<T>, "T must be default constructible");

  return caors::make<T>(resource);
}

template <typename T>
void destroy_object_instance(object* instance, memory_resource* resource) noexcept {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");

  caors::destroy(static_cast<T*>(instance), resource);
}

template <typename T> struct object_factory_ops_model {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");

  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  [[nodiscard]] static object_factory_ops make() noexcept {
    object_factory_ops data{};
    if constexpr (!std::is_same_v<type, object>) {
      data.destroy = &destroy_object_instance<type>;
      if constexpr (std::is_default_constructible_v<type>) {
        data.try_create = &try_create_object_instance<type>;
        data.create = &create_object_instance<type>;
      }
    }

    return data;
  }

  inline static const object_factory_ops value = make();
};

/** @brief 返回指定对象类型的稳定工厂操作表。 */
template <typename T> [[nodiscard]] const object_factory_ops* object_factory_ops_ptr_of() noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  return &object_factory_ops_model<type>::value;
}

} // namespace caors::objects::detail

namespace caors::meta::capabilities {

template <typename T> struct static_capability<objects::object_factory_ops, T> {
  [[nodiscard]] static const objects::object_factory_ops* get() {
    using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
    if constexpr (std::is_base_of_v<objects::object, value_type>) {
      return objects::detail::object_factory_ops_ptr_of<value_type>();
    } else {
      return nullptr;
    }
  }
};

template <typename T>
  requires std::is_base_of_v<objects::object, std::remove_cv_t<std::remove_reference_t<T>>>
struct module_static_capability_list<object_static_capability_module, T> {
  using type = type_list<objects::object_factory_ops>;
};

} // namespace caors::meta::capabilities

namespace caors::objects {

/** @brief 直接创建指定 C++ 类型的对象。 */
template <typename T, typename... Args>
[[nodiscard]] T* create(memory_resource* resource, Args&&... args) {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");
  return caors::make<T>(resource, std::forward<Args>(args)...);
}

template <typename T> [[nodiscard]] T* create() {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");
  return create<T>(static_cast<memory_resource*>(nullptr));
}

template <typename T, typename First, typename... Rest>
  requires(!std::is_convertible_v<std::remove_cvref_t<First>, memory_resource*>)
[[nodiscard]] T* create(First&& first, Rest&&... rest) {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");
  return create<T>(static_cast<memory_resource*>(nullptr), std::forward<First>(first),
                   std::forward<Rest>(rest)...);
}

/** @brief 销毁通过 CAORS 内存资源创建的对象。 */
template <typename T> void destroy(T* instance, memory_resource* resource = nullptr) noexcept {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");
  caors::destroy(instance, resource);
}

} // namespace caors::objects

#endif
