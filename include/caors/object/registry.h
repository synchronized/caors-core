// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_REGISTRY_H_
#define CAORS_OBJECT_REGISTRY_H_

#include "caors/meta.h"
#include "caors/object/factory.h"
#include "caors/object/ptr.h"
#include "caors/object/type.h"

#include <optional>
#include <string_view>
#include <type_traits>

namespace caors::objects {

class registry;
template <typename T> class class_builder;

/** @brief 获取进程内默认对象类型注册表。 */
[[nodiscard]] CAORS_API registry& get_registry() noexcept;

} // namespace caors::objects

namespace caors::objects::detail {

template <typename T, typename = void> struct object_type_name_traits {
  static constexpr bool exists = false;
  static constexpr bool valid = false;

  [[nodiscard]] static constexpr std::string_view get() noexcept { return {}; }
};

template <typename T>
struct object_type_name_traits<T, std::void_t<decltype(T::object_type_name)>> {
  static constexpr bool exists = true;
  static constexpr bool valid =
      std::is_convertible_v<decltype(T::object_type_name), std::string_view>;

  [[nodiscard]] static constexpr std::string_view get() noexcept { return T::object_type_name; }
};

} // namespace caors::objects::detail

namespace caors::objects {

/**
 * @brief 全局对象类型注册表 facade 和默认构造工厂。
 *
 * `objects::registry` 不表示彼此隔离的注册表实例。它复用全局 `meta::registry`
 * 管理基础类型信息，并把 `objects::object_factory_ops` 作为 capability 挂载到同一个 `type_id`
 * 上。普通代码应通过 `objects::class_<T>()`、`objects::get_registry()` 和查询接口访问它。
 */
class CAORS_API registry final {
public:
  registry(const registry&) = delete;
  registry& operator=(const registry&) = delete;

  [[nodiscard]] bool contains(object_type_id id) const;
  [[nodiscard]] type find_type(object_type_id id) const;
  [[nodiscard]] type find_type(std::string_view name) const;
  template <typename T> [[nodiscard]] type find_type() const {
    const meta::type type = meta::get_registry().find_type<T>();
    if (!type) {
      return {};
    }

    return find_type(type.id());
  }

  [[nodiscard]] bool is_a(object_type_id type_id, object_type_id base_type_id) const;
  [[nodiscard]] std::size_t size() const noexcept;

  [[nodiscard]] object* try_create(object_type_id id,
                                   memory_resource* resource = nullptr) const noexcept;
  [[nodiscard]] object* try_create(std::string_view name,
                                   memory_resource* resource = nullptr) const noexcept;
  template <typename T>
  [[nodiscard]] T* try_create(memory_resource* resource = nullptr) const noexcept {
    const type object_type = find_type<T>();
    if (!object_type) {
      return nullptr;
    }

    return static_cast<T*>(try_create(object_type.id(), resource));
  }

  [[nodiscard]] object* create(object_type_id id, memory_resource* resource = nullptr) const;
  [[nodiscard]] object* create(std::string_view name, memory_resource* resource = nullptr) const;
  template <typename T> [[nodiscard]] T* create(memory_resource* resource = nullptr) const {
    const type object_type = find_type<T>();
    if (!object_type) {
      return nullptr;
    }

    return static_cast<T*>(create(object_type.id(), resource));
  }

  bool destroy(object_type_id id, object* instance,
               memory_resource* resource = nullptr) const noexcept;
  bool destroy(object* instance, memory_resource* resource = nullptr) const noexcept;

private:
  friend registry& get_registry() noexcept;
  template <typename T> friend class class_builder;

  registry();
  ~registry();

  template <typename T>
  [[nodiscard]] error attach_object_type(std::string_view name, type_guid guid,
                                         object_type_id parent_id,
                                         const object_factory_ops* factory) {
    using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
    const meta::type expected = meta::resolve<value_type>();
    const type_id expected_id = expected.id();
    if (!expected_id) {
      return {error_domain::object, errc::invalid_type};
    }

    if (parent_id && !meta::find(parent_id)) {
      return {error_domain::object, errc::missing_parent};
    }

    if (guid) {
      const meta::type existing_guid = meta::find(guid);
      if (existing_guid && existing_guid.id() != expected_id) {
        return {error_domain::object, errc::duplicate_guid};
      }
    }

    meta::class_builder<value_type> builder = meta::class_<value_type>(name);
    if (builder.result() && builder.result().value() != errc::duplicate_id) {
      return rethrow_as(builder.result(), error_domain::object);
    }

    if (guid) {
      builder.guid(guid);
      if (builder.result() && builder.result().value() != errc::duplicate_id) {
        return rethrow_as(builder.result(), error_domain::object);
      }
    }

    if (parent_id) {
      builder.parent(parent_id);
      if (builder.result()) {
        return rethrow_as(builder.result(), error_domain::object);
      }
    }

    const meta::type stored_meta = meta::get_registry().find_type(expected_id);
    if (!stored_meta) {
      return {error_domain::object, errc::invalid_type};
    }

    if (factory == nullptr) {
      factory = meta::capabilities::get<object_factory_ops, value_type>();
    }
    if (factory == nullptr) {
      return {error_domain::object, errc::invalid_type};
    }

    builder.template capability<object_factory_ops>(factory);
    return rethrow_as(builder.result(), error_domain::object, errc::duplicate_component,
                      errc::duplicate_id);
  }

  [[nodiscard]] const object_factory_ops* find_factory_ops(object_type_id id) const;
};

} // namespace caors::objects

namespace caors::objects {

/** @brief 根据已注册对象类型名查询对象类型标识。 */
[[nodiscard]] inline object_type_id id(std::string_view name) { return meta::id(name); }

/** @brief 在全局对象注册表中按对象类型 id 查找类型。 */
[[nodiscard]] inline type find(object_type_id id) { return get_registry().find_type(id); }

/** @brief 在全局对象注册表中按对象类型名查找类型。 */
[[nodiscard]] inline type find(std::string_view name) { return get_registry().find_type(name); }

/** @brief 在全局对象注册表中按 C++ 类型查找对象类型。 */
template <typename T> [[nodiscard]] type find() { return get_registry().find_type<T>(); }

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
    object_type_id parent_id = parent_id_;
    if (!parent_id) {
      using base_type = typename detail::object_base_type_traits<T>::type;
      const objects::type base = get_registry().find_type<base_type>();
      if (base) {
        parent_id = base.id();
      }
    }

    status_ = get_registry().template attach_object_type<T>(
        name_, detail::object_type_guid_of<T>(), parent_id,
        meta::capabilities::get<objects::object_factory_ops, T>());
    refresh();
    return status_;
  }

  [[nodiscard]] type get() const noexcept { return type_; }
  [[nodiscard]] error result() const noexcept { return status_; }
  [[nodiscard]] bool succeeded() const noexcept {
    return status_.succeeded() || status_.value() == errc::duplicate_id;
  }

private:
  void refresh() {
    const type refreshed = get_registry().find_type<T>();
    if (refreshed) {
      type_ = refreshed;
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

/**
 * @brief 创建对象类型注册构建器，并使用对象类型自身声明的类型名。
 *
 * `T` 通常应继承 `object_base<T>` 并提供 `object_type_name`。该重载避免注册时重复书写
 * `T::object_type_name`，也避免注册名和对象自身类型名不一致。
 */
template <typename T> [[nodiscard]] class_builder<T> class_() {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");
  static_assert(detail::object_type_name_traits<T>::exists, "T must declare object_type_name");
  static_assert(detail::object_type_name_traits<T>::valid,
                "object_type_name must be convertible to std::string_view");
  return class_builder<T>{detail::object_type_name_traits<T>::get()};
}

} // namespace caors::objects

#endif
