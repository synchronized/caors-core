// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_REGISTRY_H_
#define CAORS_OBJECT_REGISTRY_H_

#include "caors/meta.h"
#include "caors/object/factory.h"
#include "caors/object/ref.h"
#include "caors/object/type.h"

#include <optional>
#include <string_view>
#include <type_traits>

namespace caors::objects {

class registry;

/** @brief 获取进程内默认对象类型注册表。 */
[[nodiscard]] CAORS_API registry& get_registry() noexcept;

} // namespace caors::objects

namespace caors::objects::detail {

template <typename T>
[[nodiscard]] error register_object_type(std::string_view name,
                                         object_type_id parent_id = object_type_id::invalid());

/** @brief 创建对象类型基础元信息。 */
template <typename T>
[[nodiscard]] meta::detail::type_data
make_meta_type_data(std::string_view name,
                    object_type_id parent_id = object::static_type_id()) noexcept {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");
  return caors::meta::detail::make_type_data<T>(name, detail::object_type_guid_of<T>(), parent_id);
}

} // namespace caors::objects::detail

namespace caors::objects {

/**
 * @brief 全局对象类型注册表 facade 和默认构造工厂。
 *
 * `objects::registry` 不表示彼此隔离的注册表实例。它复用全局 `meta::registry`
 * 管理基础类型信息，并把 `objects::detail::type_data` 作为组件挂载到同一个 `type_id`
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

  bool destroy(object* instance, memory_resource* resource = nullptr) const noexcept;

private:
  friend registry& get_registry() noexcept;
  template <typename T>
  friend error detail::register_object_type(std::string_view name, object_type_id parent_id);

  registry();
  ~registry();

  [[nodiscard]] error register_type(const meta::detail::type_data& type,
                                    const detail::type_data& object);
  template <typename T>
  [[nodiscard]] error register_type_for(const meta::detail::type_data& type,
                                        const detail::type_data& object) {
    using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
    const type_id expected_id = meta::detail::type_id_of_cpp_type<value_type>();
    if (type.id() && type.id() != expected_id) {
      return {error_domain::object, errc::invalid_type};
    }

    meta::detail::type_data stored_type = type;
    if (!stored_type.id()) {
      stored_type.set_id(expected_id);
    }

    const error type_status =
        meta::get_registry().template register_type_for<value_type>(stored_type);
    if (type_status && type_status.value() != errc::duplicate_id) {
      return rethrow_as(type_status, error_domain::object);
    }

    const meta::type stored_meta = meta::get_registry().find_type(expected_id);
    if (!stored_meta) {
      return {error_domain::object, errc::invalid_type};
    }

    return rethrow_as(meta::get_registry().emplace<detail::type_data>(stored_meta.id(), object),
                      error_domain::object, errc::duplicate_component, errc::duplicate_id);
  }

  [[nodiscard]] const detail::type_data* find_type_data(object_type_id id) const;
};

} // namespace caors::objects

namespace caors::objects::detail {

/** @brief 向全局对象注册表注册对象类型。内部入口，公开代码优先使用 `objects::class_<T>()`。 */
template <typename T>
[[nodiscard]] error register_object_type(std::string_view name, object_type_id parent_id) {
  objects::registry& registry = objects::get_registry();
  if (!parent_id) {
    using base_type = typename object_base_type_traits<T>::type;
    const objects::type base = registry.find_type<base_type>();
    if (base) {
      parent_id = base.id();
    }
  }

  return registry.register_type_for<T>(objects::detail::make_meta_type_data<T>(name, parent_id),
                                       objects::detail::make_type_data<T>());
}

} // namespace caors::objects::detail

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
    status_ = detail::register_object_type<T>(name_, parent_id_);
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

} // namespace caors::objects

#endif
