// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_H_
#define CAORS_OBJECT_H_

#include "caors/error.h"
#include "caors/export.h"
#include "caors/memory.h"
#include "caors/meta.h"
#include "caors/object/ref.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors {

class object;

namespace objects {

class registry;

using try_create_fn = object* (*)(memory_resource * resource) noexcept;
using create_fn = object* (*)(memory_resource * resource);
using destroy_fn = void (*)(object* instance, memory_resource* resource) noexcept;

/** @brief 对象类型底层数据。 */
struct type_data {
  try_create_fn try_create = nullptr;
  create_fn create = nullptr;
  destroy_fn destroy = nullptr;

  [[nodiscard]] constexpr bool constructible() const noexcept {
    return try_create != nullptr && create != nullptr;
  }
};

/** @brief 对象类型只读视图。 */
class CAORS_API type final {
public:
  type() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] meta::type_view info() const noexcept;
  [[nodiscard]] object_type_id id() const noexcept;
  [[nodiscard]] string_name name() const noexcept;
  [[nodiscard]] type_guid guid() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] std::size_t alignment() const noexcept;
  [[nodiscard]] object_type_id parent_id() const noexcept;
  [[nodiscard]] bool constructible() const noexcept;

private:
  friend class registry;

  type(meta::type_view info, const type_data* data) noexcept;
  [[nodiscard]] const type_data* data() const noexcept;

  meta::type_view type_{};
  const type_data* data_ = nullptr;
};

} // namespace objects

/**
 * @brief CAORS 对象基类。
 *
 * 该类型只提供对象身份和运行期类型入口，不负责引用计数、GC、脚本或属性反射。
 */
class CAORS_API object {
public:
  object() noexcept;
  virtual ~object();

  object(const object&) = delete;
  object& operator=(const object&) = delete;
  object(object&&) = delete;
  object& operator=(object&&) = delete;

  [[nodiscard]] object_id id() const noexcept;
  [[nodiscard]] virtual object_type_id type_id() const noexcept;
  [[nodiscard]] virtual std::string_view type_name() const noexcept;

  [[nodiscard]] static object_type_id static_type_id() noexcept;
  [[nodiscard]] static std::string_view static_type_name() noexcept;

private:
  object_id id_;
};

namespace objects::detail {

template <typename T> struct object_ref_traits {
  static constexpr bool is_object_ref = false;
};

template <typename T> struct object_ref_traits<ref<T>> {
  static constexpr bool is_object_ref = true;
  using object_type = T;
};

template <typename T> struct object_ref_traits<weak_handle<T>> {
  static constexpr bool is_object_ref = false;
  using object_type = T;
};

template <typename T, typename = void> struct object_type_guid_traits {
  [[nodiscard]] static constexpr type_guid get() noexcept { return type_guid::nil(); }
};

template <typename T>
struct object_type_guid_traits<T, std::void_t<decltype(T::object_type_guid)>> {
  static_assert(
      std::is_same_v<std::remove_cv_t<std::remove_reference_t<decltype(T::object_type_guid)>>,
                     type_guid>,
      "object_type_guid must be caors::type_guid");

  [[nodiscard]] static constexpr type_guid get() noexcept { return T::object_type_guid; }
};

template <typename T> [[nodiscard]] constexpr type_guid object_type_guid_of() noexcept {
  return object_type_guid_traits<T>::get();
}

template <typename T, typename = void> struct object_base_type_traits {
  using type = object;
};

template <typename T> struct object_base_type_traits<T, std::void_t<typename T::base_type>> {
  using type = typename T::base_type;
};

} // namespace objects::detail

/**
 * @brief 为派生对象提供默认运行期类型名实现。
 *
 * `Derived` 需要提供 `static constexpr std::string_view object_type_name`。可序列化、可反射或跨插件
 * 暴露的类型建议额外提供 `static constexpr caors::type_guid object_type_guid`。
 */
template <typename Derived, typename Base = object> class object_base : public Base {
  static_assert(std::is_base_of_v<object, Base>, "Base must derive from caors::object");

public:
  using base_type = Base;

  [[nodiscard]] static object_type_id static_type_id() noexcept {
#if CAORS_HAS_EXCEPTIONS
    try {
      return caors::meta::get<Derived>().id();
    } catch (...) {
      return object_type_id::invalid();
    }
#else
    return caors::meta::get<Derived>().id();
#endif
  }

  [[nodiscard]] static std::string_view static_type_name() noexcept {
    return Derived::object_type_name;
  }

  [[nodiscard]] static constexpr type_guid static_type_guid() noexcept {
    return objects::detail::object_type_guid_of<Derived>();
  }

  [[nodiscard]] object_type_id type_id() const noexcept override { return static_type_id(); }

  [[nodiscard]] std::string_view type_name() const noexcept override { return static_type_name(); }
};

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

  return make<T>(resource);
}

template <typename T>
void destroy_object_instance(object* instance, memory_resource* resource) noexcept {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");

  destroy(static_cast<T*>(instance), resource);
}

namespace objects {

/** @brief 创建对象类型基础元信息。 */
template <typename T>
[[nodiscard]] meta::type_data
make_meta_type_data(std::string_view name,
                    object_type_id parent_id = object::static_type_id()) noexcept {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");
  return caors::meta::make_type_data<T>(name, detail::object_type_guid_of<T>(), parent_id);
}

/** @brief 创建对象类型底层数据。 */
template <typename T> [[nodiscard]] type_data make_type_data() noexcept {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");

  type_data data{nullptr, nullptr, &destroy_object_instance<T>};
  if constexpr (std::is_default_constructible_v<T>) {
    data.try_create = &try_create_object_instance<T>;
    data.create = &create_object_instance<T>;
  }

  return data;
}

/** @brief 获取进程内默认对象类型注册表。 */
[[nodiscard]] CAORS_API registry& get_registry() noexcept;

/**
 * @brief 全局对象类型注册表 facade 和默认构造工厂。
 *
 * `objects::registry` 不表示彼此隔离的注册表实例。它复用全局 `meta::registry`
 * 管理基础类型信息，并把 `objects::type_data` 作为组件挂载到同一个 `type_id`
 * 上。普通代码应通过 `objects::class_<T>()`、`objects::get_registry()` 和查询接口访问它。
 */
class CAORS_API registry final {
public:
  registry(const registry&) = delete;
  registry& operator=(const registry&) = delete;

  [[nodiscard]] error register_type(const meta::type_data& type, const type_data& object);
  template <typename T>
  [[nodiscard]] error register_type_for(const meta::type_data& type, const type_data& object) {
    return register_type(caors::detail::cpp_type_key_of<T>(), type, object);
  }

  [[nodiscard]] bool contains(object_type_id id) const;
  [[nodiscard]] std::optional<type> find_type(object_type_id id) const;
  [[nodiscard]] std::optional<type> find_type(std::string_view name) const;
  template <typename T> [[nodiscard]] std::optional<type> find_type() const {
    const std::optional<meta::type_view> type =
        find_registered_type(caors::detail::cpp_type_key_of<T>());
    if (!type.has_value()) {
      return std::nullopt;
    }

    return find_type(type->id());
  }

  [[nodiscard]] bool is_a(object_type_id type_id, object_type_id base_type_id) const;
  [[nodiscard]] std::size_t size() const noexcept;

  [[nodiscard]] object* try_create(object_type_id id,
                                   memory_resource* resource = nullptr) const noexcept;
  [[nodiscard]] object* try_create(std::string_view name,
                                   memory_resource* resource = nullptr) const noexcept;
  template <typename T>
  [[nodiscard]] T* try_create(memory_resource* resource = nullptr) const noexcept {
    const std::optional<type> type = find_type<T>();
    if (!type.has_value()) {
      return nullptr;
    }

    return static_cast<T*>(try_create(type->id(), resource));
  }

  [[nodiscard]] object* create(object_type_id id, memory_resource* resource = nullptr) const;
  [[nodiscard]] object* create(std::string_view name, memory_resource* resource = nullptr) const;
  template <typename T> [[nodiscard]] T* create(memory_resource* resource = nullptr) const {
    const std::optional<type> type = find_type<T>();
    if (!type.has_value()) {
      return nullptr;
    }

    return static_cast<T*>(create(type->id(), resource));
  }

  bool destroy(object* instance, memory_resource* resource = nullptr) const noexcept;

private:
  friend registry& get_registry() noexcept;

  registry();
  ~registry();

  [[nodiscard]] error register_type(caors::detail::cpp_type_key key, const meta::type_data& type,
                                    const type_data& object);
  [[nodiscard]] std::optional<meta::type_view>
  find_registered_type(caors::detail::cpp_type_key key) const;
  [[nodiscard]] const type_data* find_type_data(object_type_id id) const;
};

} // namespace objects

namespace objects::detail {

/** @brief 向全局对象注册表注册对象类型。内部入口，公开代码优先使用 `objects::class_<T>()`。 */
template <typename T>
[[nodiscard]] error register_object_type(std::string_view name,
                                         object_type_id parent_id = object_type_id::invalid()) {
  objects::registry& registry = objects::get_registry();
  if (!parent_id) {
    using base_type = typename object_base_type_traits<T>::type;
    const std::optional<objects::type> base = registry.find_type<base_type>();
    if (base.has_value()) {
      parent_id = base->id();
    }
  }

  return registry.register_type_for<T>(objects::make_meta_type_data<T>(name, parent_id),
                                       objects::make_type_data<T>());
}

} // namespace objects::detail

namespace objects {

/** @brief 根据已注册对象类型名查询对象类型标识。 */
[[nodiscard]] inline object_type_id id(std::string_view name) { return meta::id(name); }

/** @brief 在全局对象注册表中按对象类型 id 查找类型。 */
[[nodiscard]] inline std::optional<type> find(object_type_id id) {
  return get_registry().find_type(id);
}

/** @brief 在全局对象注册表中按对象类型名查找类型。 */
[[nodiscard]] inline std::optional<type> find(std::string_view name) {
  return get_registry().find_type(name);
}

/** @brief 在全局对象注册表中按 C++ 类型查找对象类型。 */
template <typename T> [[nodiscard]] std::optional<type> find() {
  return get_registry().find_type<T>();
}

/** @brief 判断对象类型是否已注册。 */
[[nodiscard]] inline bool contains(object_type_id id) { return get_registry().contains(id); }

/** @brief 判断对象类型继承关系。 */
[[nodiscard]] inline bool is_a(object_type_id id, object_type_id base_id) {
  return get_registry().is_a(id, base_id);
}

/** @brief 获取当前对象类型数量。 */
[[nodiscard]] inline std::size_t size() noexcept { return get_registry().size(); }

/** @brief 对象类型注册构建器。 */
template <typename T> class class_builder {
public:
  explicit class_builder(std::string_view name) : name_(name) {}

  class_builder& parent(object_type_id parent_id) noexcept {
    parent_id_ = parent_id;
    return *this;
  }

  [[nodiscard]] error commit() {
    status_ = detail::register_object_type<T>(name_, parent_id_);
    refresh();
    return status_;
  }

  [[nodiscard]] type get() const noexcept { return type_; }
  [[nodiscard]] error result() const noexcept { return status_; }
  [[nodiscard]] bool succeeded() const noexcept {
    return status_.succeeded() || status_.value() == errc::duplicate_id;
  }

  [[nodiscard]] operator type() const noexcept { return type_; }

private:
  void refresh() {
    const std::optional<type> refreshed = get_registry().find_type<T>();
    if (refreshed.has_value()) {
      type_ = *refreshed;
    }
  }

  std::string_view name_{};
  object_type_id parent_id_{};
  type type_{};
  error status_{};
};

/** @brief 创建对象类型注册构建器。 */
template <typename T> [[nodiscard]] class_builder<T> class_(std::string_view name) {
  return class_builder<T>{name};
}

/** @brief 直接创建指定 C++ 类型的对象。 */
template <typename T, typename... Args>
[[nodiscard]] T* create(memory_resource* resource, Args&&... args) {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");
  return make<T>(resource, std::forward<Args>(args)...);
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

} // namespace objects

}

#endif
