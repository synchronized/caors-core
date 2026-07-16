// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_QUERY_H_
#define CAORS_META_QUERY_H_

#include "caors/meta/detail/type_descriptor_model.h"
#include "caors/meta/proxy.h"
#include "caors/meta/registry.h"

#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::meta {

/** @brief RTTR 风格的类/普通值类型注册构建器。 */
template <typename T> class class_builder {
public:
  class_builder() = default;

  class_builder& type(std::string_view name) {
    status_ = get_registry().register_type_for<T>(make_type_registration_data<T>(name));
    refresh();
    return *this;
  }

  class_builder& guid(type_guid guid_value) {
    if (!type_.registered()) {
      status_ = {error_domain::meta, errc::invalid_type};
      return *this;
    }

    status_ = get_registry().set_type_guid(type_.id(), guid_value);
    refresh();
    return *this;
  }

  class_builder& guid(caors::guid guid_value) { return guid(type_guid{guid_value}); }

  class_builder& parent(type_id parent_id) {
    if (!type_.registered()) {
      status_ = {error_domain::meta, errc::invalid_type};
      return *this;
    }

    status_ = get_registry().set_type_parent(type_.id(), parent_id);
    refresh();
    return *this;
  }

  /** @brief 设置 C++ 父类型，并保存安全 upcast 信息供普通类多态使用。 */
  template <typename Parent> class_builder& parent() {
    if (!type_.registered()) {
      status_ = {error_domain::meta, errc::invalid_type};
      return *this;
    }

    status_ = get_registry().template set_type_parent_for<T, Parent>();
    refresh();
    return *this;
  }

  /**
   * @brief 为类型注册资源感知默认工厂。
   *
   * 该工厂使用 `memory::make<T>()` / `memory::destroy<T>()`，适合默认可构造类型。非默认可构造
   * 类型应使用带函数指针的 `factory()` 重载。
   */
  class_builder& factory() {
    return factory(detail::resource_default_construct_fn_of<T>(),
                   detail::resource_destroy_fn_of<T>());
  }

  /**
   * @brief 为类型注册自定义资源感知工厂。
   *
   * `construct` 可使用传入的 `memory_resource` 分配并构造对象；`destroy` 必须用匹配策略销毁对象。
   */
  class_builder& factory(factory_ops::construct_fn construct, factory_ops::destroy_fn destroy) {
    if (!type_.registered()) {
      status_ = {error_domain::meta, errc::invalid_type};
      return *this;
    }

    const factory_ops ops{
        construct,
        destroy,
    };
    if (!ops.valid()) {
      status_ = {error_domain::meta, errc::invalid_type};
      return *this;
    }

    const factory_ops* existing = type_.get_raw_ops<factory_ops>();
    if (existing != nullptr) {
      status_ = existing->construct == ops.construct && existing->destroy == ops.destroy
                    ? error{error_domain::meta, errc::duplicate_id}
                    : error{error_domain::meta, errc::invalid_type};
      return *this;
    }

    status_ = detail::register_owned_capability_for_type<factory_ops, T>(ops);
    refresh();
    return *this;
  }

  /**
   * @brief 注册一组运行期构造参数。
   *
   * 具体模板实现位于 `caors/meta/constructor.h`。该入口使用 `any_ref`
   * 参数列表做精确类型匹配，适合 runtime facade、编辑器和工具层按运行期参数创建对象。
   */
  template <typename... Arguments> class_builder& constructor();

  /**
   * @brief 为当前 C++ 类型挂接一份运行期 capability 指针。
   *
   * `capability` 必须在运行期索引可访问期间保持有效；普通查询优先使用
   * `meta::type::get_capability<Capability>()`。
   */
  template <typename Capability> class_builder& capability(const Capability* capability) {
    using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
    status_ = detail::register_capability_for_type<capability_type, T>(capability);
    refresh();
    return *this;
  }

  /**
   * @brief 为当前 C++ 类型挂接一份由运行期索引拥有的 capability 对象。
   *
   * 该入口适合用户自定义 poly / concept table。能力对象会复制或移动到运行期索引内部，
   * 查询得到的指针在进程内 append-only 注册表生命周期内保持稳定。
   */
  template <typename Capability>
    requires(!std::is_pointer_v<std::remove_reference_t<Capability>>)
  class_builder& capability(Capability&& capability) {
    using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
    status_ = detail::register_owned_capability_for_type<capability_type, T>(
        capability_type{std::forward<Capability>(capability)});
    refresh();
    return *this;
  }

  /**
   * @brief 把 `capability_model<Capability, T>` 或静态能力挂接到运行期索引。
   *
   * 如果存在 `capability_model<Capability, T>`，优先注册 model 生成的 `Capability::ops_type`；
   * 否则尝试使用 `Capability::model<T>` 自动 model，最后回退到 `static_capability<Capability, T>`。
   * 该入口适合只剩 `type_id` 的运行期查询路径；携带 descriptor 的路径无需显式挂接。
   */
  template <typename Capability> class_builder& capability() {
    using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
    if constexpr (detail::has_capability_model_v<capability_type, T>) {
      status_ = detail::register_capability_model_for<capability_type, T>();
    } else if constexpr (detail::has_capability_auto_model_v<capability_type, T>) {
      status_ = detail::register_capability_auto_model_for<capability_type, T>();
    } else {
      status_ = detail::register_static_capability_for<capability_type, T>();
    }
    refresh();
    return *this;
  }

  /** @brief 给当前类型写入 metadata。 */
  template <typename Value> class_builder& metadata(std::string_view key, Value&& value) {
    return metadata(string_name{key}, std::forward<Value>(value));
  }

  /** @brief 给当前类型写入 metadata。 */
  template <typename Value> class_builder& metadata(string_name key, Value&& value) {
    if (!type_.registered() || !key) {
      status_ = {error_domain::meta, errc::invalid_type};
      return *this;
    }

    type_metadata* data = get_registry().find_component_mutable_ptr<type_metadata>(type_.id());
    if (data == nullptr) {
      status_ = get_registry().emplace_component<type_metadata>(type_.id());
      if (status_ && status_.value() != errc::duplicate_component) {
        return *this;
      }

      data = get_registry().find_component_mutable_ptr<type_metadata>(type_.id());
    }

    if (data == nullptr) {
      status_ = {error_domain::meta, errc::invalid_type};
      return *this;
    }

    set_metadata(data->values, key, make_metadata_value(std::forward<Value>(value)));
    refresh();
    return *this;
  }

  [[nodiscard]] ::caors::meta::type get() const noexcept { return type_; }
  [[nodiscard]] error result() const noexcept { return status_; }
  [[nodiscard]] bool succeeded() const noexcept {
    return status_.succeeded() || status_.value() == errc::duplicate_id;
  }

private:
  void refresh() {
    ::caors::meta::type refreshed = get_registry().find_type<T>();
    if (refreshed.registered()) {
      type_ = refreshed;
    }
  }

  ::caors::meta::type type_{};
  error status_{};
};

/** @brief RTTR 风格的枚举类型注册构建器。 */
template <typename T> class enum_builder {
  static_assert(std::is_enum_v<std::remove_cv_t<std::remove_reference_t<T>>>,
                "T must be an enum type");

public:
  enum_builder() = default;

  enum_builder& type(std::string_view name) {
    status_ = get_registry().register_type_for<T>(make_type_registration_data<T>(name));
    refresh();
    return *this;
  }

  enum_builder& guid(type_guid guid_value) {
    if (!type_.registered()) {
      status_ = {error_domain::meta, errc::invalid_type};
      return *this;
    }

    status_ = get_registry().set_type_guid(type_.id(), guid_value);
    refresh();
    return *this;
  }

  enum_builder& guid(caors::guid guid_value) { return guid(type_guid{guid_value}); }

  template <typename Capability> enum_builder& capability(const Capability* capability) {
    using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
    status_ = detail::register_capability_for_type<capability_type, T>(capability);
    refresh();
    return *this;
  }

  template <typename Capability>
    requires(!std::is_pointer_v<std::remove_reference_t<Capability>>)
  enum_builder& capability(Capability&& capability) {
    using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
    status_ = detail::register_owned_capability_for_type<capability_type, T>(
        capability_type{std::forward<Capability>(capability)});
    refresh();
    return *this;
  }

  template <typename Capability> enum_builder& capability() {
    using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
    if constexpr (detail::has_capability_model_v<capability_type, T>) {
      status_ = detail::register_capability_model_for<capability_type, T>();
    } else if constexpr (detail::has_capability_auto_model_v<capability_type, T>) {
      status_ = detail::register_capability_auto_model_for<capability_type, T>();
    } else {
      status_ = detail::register_static_capability_for<capability_type, T>();
    }
    refresh();
    return *this;
  }

  /** @brief 给当前枚举类型写入 metadata。 */
  template <typename Value> enum_builder& metadata(std::string_view key, Value&& value) {
    return metadata(string_name{key}, std::forward<Value>(value));
  }

  /** @brief 给当前枚举类型写入 metadata。 */
  template <typename Value> enum_builder& metadata(string_name key, Value&& value) {
    if (!type_.registered() || !key) {
      status_ = {error_domain::meta, errc::invalid_type};
      return *this;
    }

    type_metadata* data = get_registry().find_component_mutable_ptr<type_metadata>(type_.id());
    if (data == nullptr) {
      status_ = get_registry().emplace_component<type_metadata>(type_.id());
      if (status_ && status_.value() != errc::duplicate_component) {
        return *this;
      }

      data = get_registry().find_component_mutable_ptr<type_metadata>(type_.id());
    }

    if (data == nullptr) {
      status_ = {error_domain::meta, errc::invalid_type};
      return *this;
    }

    set_metadata(data->values, key, make_metadata_value(std::forward<Value>(value)));
    refresh();
    return *this;
  }

  [[nodiscard]] ::caors::meta::type get() const noexcept { return type_; }
  [[nodiscard]] error result() const noexcept { return status_; }
  [[nodiscard]] bool succeeded() const noexcept {
    return status_.succeeded() || status_.value() == errc::duplicate_id;
  }

private:
  void refresh() {
    ::caors::meta::type refreshed = get_registry().find_type<T>();
    if (refreshed.registered()) {
      type_ = refreshed;
    }
  }

  ::caors::meta::type type_{};
  error status_{};
};

/** @brief 获取指定 C++ 类/普通值类型的元类型注册构建器；默认不注册类型。 */
template <typename T> [[nodiscard]] class_builder<T> class_() { return class_builder<T>{}; }

/** @brief 显式注册指定 C++ 类/普通值类型并设置类型名。 */
template <typename T> [[nodiscard]] class_builder<T> class_(std::string_view name) {
  return class_builder<T>{}.type(name);
}

/** @brief 获取指定 C++ 枚举类型的元类型注册构建器；默认不注册类型。 */
template <typename T> [[nodiscard]] enum_builder<T> enum_() { return enum_builder<T>{}; }

/** @brief 显式注册指定 C++ 枚举类型并设置类型名。 */
template <typename T> [[nodiscard]] enum_builder<T> enum_(std::string_view name) {
  return enum_builder<T>{}.type(name);
}

/**
 * @brief 解析 C++ 类型 descriptor 视图；类型不要求已经显式注册。
 *
 * `resolve<T>()` 优先返回已注册 `meta::type`；类型尚未注册时退回 descriptor-only 视图。
 * 需要强制判断是否存在稳定名称、GUID、父类型或模块注册组件时，应使用 `find<T>()` 查询已注册类型。
 */
template <typename T> [[nodiscard]] type resolve() {
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  return detail::make_type_view(detail::descriptor_type_id<value_type>(),
                                detail::descriptor_ptr_of<value_type>());
}

/** @brief 按 C++ 类型查找已注册基础类型；类型未显式注册时返回空视图。 */
template <typename T> [[nodiscard]] type find() {
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  const type stored = get_registry().find_type(detail::descriptor_type_id<value_type>());
  return stored.registered() ? stored : type{};
}

/** @brief 按类型 id 查找基础类型。 */
[[nodiscard]] CAORS_API type find(type_id id);

/** @brief 按稳定 GUID 查找基础类型。 */
[[nodiscard]] CAORS_API type find(type_guid guid);

/** @brief 按类型名查找基础类型。 */
[[nodiscard]] CAORS_API type find(std::string_view name);

/** @brief 按类型名查找基础类型。 */
[[nodiscard]] CAORS_API type find(string_name name);

/** @brief 获取 C++ 类型的运行期类型 id；类型不要求已经显式注册。 */
template <typename T> [[nodiscard]] type_id id_of() {
  const type stored = resolve<T>();
  return stored ? stored.id() : type_id::invalid();
}

/** @brief 根据已注册类型名查询运行期类型 id。 */
[[nodiscard]] CAORS_API type_id id(string_name name) noexcept;

/** @brief 根据已注册类型名查询运行期类型 id。 */
[[nodiscard]] CAORS_API type_id id(std::string_view name);

/** @brief 在全局元数据注册表中判断类型继承关系。 */
[[nodiscard]] CAORS_API bool is_a(type_id id, type_id base_id);

template <typename T>
[[nodiscard]] error registry::register_type_for(const type_registration_data& data) {
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  const type_id expected_id = detail::descriptor_type_id<value_type>();
  if ((data.id() && data.id() != expected_id) || !expected_id) {
    return {error_domain::meta, errc::invalid_type};
  }

  type_registration_data stored_type = data;
  if (!stored_type.id()) {
    stored_type.set_id(expected_id);
  }

  const error status = register_type(stored_type);
  if (status && status.value() != errc::duplicate_id) {
    return status;
  }

  const error type_info_status = register_cpp_type_info(typeid(value_type), expected_id);
  if (type_info_status) {
    return type_info_status;
  }

  const error descriptor_status = detail::ensure_type_descriptor_for<value_type>();
  if (descriptor_status) {
    return descriptor_status;
  }

  return status;
}

template <typename T> [[nodiscard]] type registry::find_type() const {
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  const type_id id = detail::descriptor_type_id<value_type>();
  if (!id) {
    return {};
  }

  (void)detail::ensure_type_descriptor_for<value_type>();
  return find_type(id);
}

template <typename Child, typename Parent> [[nodiscard]] error registry::set_type_parent_for() {
  using child_type = std::remove_cv_t<std::remove_reference_t<Child>>;
  using parent_type = std::remove_cv_t<std::remove_reference_t<Parent>>;
  static_assert(std::is_base_of_v<parent_type, child_type>, "Parent must be a base class of Child");

  return set_type_parent(
      detail::descriptor_type_id<child_type>(), detail::descriptor_type_id<parent_type>(),
      detail::parent_cast_ops{
          detail::descriptor_type_id<parent_type>(),
          [](const void* value) noexcept -> const void* {
            return value != nullptr
                       ? static_cast<const parent_type*>(static_cast<const child_type*>(value))
                       : nullptr;
          },
          [](void* value) noexcept -> void* {
            return value != nullptr ? static_cast<parent_type*>(static_cast<child_type*>(value))
                                    : nullptr;
          },
      });
}

template <typename... Components>
template <typename Component>
[[nodiscard]] std::optional<Component> view<Components...>::find(type_id id) const {
  if (registry_ == nullptr) {
    return std::nullopt;
  }

  return registry_->template find_component<Component>(id);
}

template <typename... Components>
[[nodiscard]] view<Components...> registry::component_view() const {
  static_assert(sizeof...(Components) > 0, "Meta view requires at least one component type");

  detail::meta_component_key keys[] = {
      detail::meta_component_key_of<std::decay_t<Components>>()...};
  return ::caors::meta::view<Components...>{this, view_component_ids(keys, sizeof...(Components))};
}

} // namespace caors::meta

#endif
