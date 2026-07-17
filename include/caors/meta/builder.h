// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_BUILDER_H_
#define CAORS_META_BUILDER_H_

#include "caors/meta/detail/value_ops_model.h"
#include "caors/meta/metadata.h"
#include "caors/meta/proxy.h"
#include "caors/meta/registry.h"

#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::meta {

template <typename T> class class_builder;
template <typename T> class field_bind;
template <typename T> class method_bind;

/** @brief 字段/方法 bind 共享的类注册链式入口。 */
template <typename T, typename Derived> class class_builder_bind_chain {
public:
  class_builder<T>& type(std::string_view name);
  class_builder<T>& guid(type_guid guid_value);
  class_builder<T>& guid(caors::guid guid_value);
  class_builder<T>& parent(type_id parent_id);
  template <typename Parent> class_builder<T>& parent();
  class_builder<T>& factory();
  class_builder<T>& factory(factory_ops::construct_fn construct, factory_ops::destroy_fn destroy);
  template <typename... Arguments> class_builder<T>& constructor();
  template <typename Member> field_bind<T> field(std::string_view name, Member member);
  template <typename Member>
  field_bind<T> field(std::string_view name, Member member, std::string_view type_name);
  template <typename Member> method_bind<T> method(std::string_view name, Member member);
  template <typename Value> class_builder<T>& field_metadata(std::string_view key, Value&& value);
  template <typename Value> class_builder<T>& field_metadata(string_name key, Value&& value);
  template <typename Capability> class_builder<T>& capability(const Capability* capability);
  template <typename Capability>
    requires(!std::is_pointer_v<std::remove_reference_t<Capability>>)
  class_builder<T>& capability(Capability&& capability);
  template <typename Capability> class_builder<T>& capability();
  template <typename Value> class_builder<T>& metadata(std::string_view key, Value&& value);
  template <typename Value> class_builder<T>& metadata(string_name key, Value&& value);
  [[nodiscard]] ::caors::meta::type get() const noexcept;
  [[nodiscard]] error result() const noexcept;
  [[nodiscard]] error commit() const noexcept;
  [[nodiscard]] bool succeeded() const noexcept;

private:
  [[nodiscard]] class_builder<T>& builder() noexcept {
    return static_cast<Derived&>(*this).builder();
  }

  [[nodiscard]] const class_builder<T>& builder() const noexcept {
    return static_cast<const Derived&>(*this).builder();
  }
};

/** @brief RTTR 风格的类/普通值类型注册构建器。 */
template <typename T> class class_builder {
public:
  class_builder() { refresh(); }

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

  /** @brief 注册成员字段反射信息。 */
  template <typename Member> field_bind<T> field(std::string_view name, Member member);

  /** @brief 注册成员字段反射信息，并校验字段类型名。 */
  template <typename Member>
  field_bind<T> field(std::string_view name, Member member, std::string_view type_name);

  /** @brief 注册成员方法反射信息。 */
  template <typename Member> method_bind<T> method(std::string_view name, Member member);

  /** @brief 给最近添加的字段写入 metadata。 */
  template <typename Value> class_builder& field_metadata(std::string_view key, Value&& value);

  /** @brief 给最近添加的字段写入 metadata。 */
  template <typename Value> class_builder& field_metadata(string_name key, Value&& value);

  /** @brief 用 RTTR 风格语法给当前类型写入 metadata。 */
  template <typename... Entries> class_builder& operator()(Entries&&... entries) {
    static_assert((std::is_same_v<std::remove_cvref_t<Entries>, metadata_entry> && ...),
                  "meta::class_builder::operator() expects meta::metadata(...) entries");
    (apply_type_metadata(std::forward<Entries>(entries)), ...);
    return *this;
  }

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
  [[nodiscard]] error commit() const noexcept { return status_; }
  [[nodiscard]] bool succeeded() const noexcept {
    return status_.succeeded() || status_.value() == errc::duplicate_id;
  }

private:
  friend class field_bind<T>;
  friend class method_bind<T>;

  class_builder& apply_type_metadata(const metadata_entry& entry) {
    return metadata(entry.key, entry.value);
  }

  class_builder& apply_type_metadata(metadata_entry&& entry) {
    return metadata(entry.key, std::move(entry.value));
  }

  class_builder& apply_field_metadata(string_name field_name, const metadata_entry& entry) {
    return set_field_metadata(field_name, entry.key, entry.value);
  }

  class_builder& apply_field_metadata(string_name field_name, metadata_entry&& entry) {
    return set_field_metadata(field_name, entry.key, std::move(entry.value));
  }

  class_builder& apply_method_metadata(string_name method_name, const metadata_entry& entry) {
    return set_method_metadata(method_name, entry.key, entry.value);
  }

  class_builder& apply_method_metadata(string_name method_name, metadata_entry&& entry) {
    return set_method_metadata(method_name, entry.key, std::move(entry.value));
  }

  class_builder& set_field_metadata(string_name field_name, metadata_key key, metadata_value value);
  class_builder& set_method_metadata(string_name method_name, metadata_key key,
                                     metadata_value value);

  class_builder& apply_field_metadata(const metadata_entry& entry) {
    return field_metadata(entry.key, entry.value);
  }

  class_builder& apply_field_metadata(metadata_entry&& entry) {
    return field_metadata(entry.key, std::move(entry.value));
  }

  void refresh() {
    ::caors::meta::type refreshed = get_registry().find_type<T>();
    if (refreshed.registered()) {
      type_ = refreshed;
    }
  }

  ::caors::meta::type type_{};
  error status_{};
  string_name last_field_name_{};
};

/** @brief 刚注册字段的 RTTR 风格 bind 对象。 */
template <typename T> class field_bind final : public class_builder_bind_chain<T, field_bind<T>> {
public:
  field_bind(class_builder<T>& builder, string_name field_name) noexcept
      : builder_(&builder), field_name_(field_name) {}

  template <typename... Entries> class_builder<T>& operator()(Entries&&... entries) {
    static_assert((std::is_same_v<std::remove_cvref_t<Entries>, metadata_entry> && ...),
                  "meta::field_bind::operator() expects meta::metadata(...) entries");
    (builder_->apply_field_metadata(field_name_, std::forward<Entries>(entries)), ...);
    return *builder_;
  }

  [[nodiscard]] operator class_builder<T>&() noexcept { return *builder_; }
  [[nodiscard]] operator const class_builder<T>&() const noexcept { return *builder_; }

private:
  friend class class_builder_bind_chain<T, field_bind<T>>;

  [[nodiscard]] class_builder<T>& builder() noexcept { return *builder_; }
  [[nodiscard]] const class_builder<T>& builder() const noexcept { return *builder_; }

  class_builder<T>* builder_ = nullptr;
  string_name field_name_{};
};

/** @brief 刚注册方法的 RTTR 风格 bind 对象。 */
template <typename T> class method_bind final : public class_builder_bind_chain<T, method_bind<T>> {
public:
  method_bind(class_builder<T>& builder, string_name method_name) noexcept
      : builder_(&builder), method_name_(method_name) {}

  template <typename... Entries> class_builder<T>& operator()(Entries&&... entries) {
    static_assert((std::is_same_v<std::remove_cvref_t<Entries>, metadata_entry> && ...),
                  "meta::method_bind::operator() expects meta::metadata(...) entries");
    (builder_->apply_method_metadata(method_name_, std::forward<Entries>(entries)), ...);
    return *builder_;
  }

  [[nodiscard]] operator class_builder<T>&() noexcept { return *builder_; }
  [[nodiscard]] operator const class_builder<T>&() const noexcept { return *builder_; }

private:
  friend class class_builder_bind_chain<T, method_bind<T>>;

  [[nodiscard]] class_builder<T>& builder() noexcept { return *builder_; }
  [[nodiscard]] const class_builder<T>& builder() const noexcept { return *builder_; }

  class_builder<T>* builder_ = nullptr;
  string_name method_name_{};
};

template <typename T, typename Derived>
class_builder<T>& class_builder_bind_chain<T, Derived>::type(std::string_view name) {
  return builder().type(name);
}

template <typename T, typename Derived>
class_builder<T>& class_builder_bind_chain<T, Derived>::guid(type_guid guid_value) {
  return builder().guid(guid_value);
}

template <typename T, typename Derived>
class_builder<T>& class_builder_bind_chain<T, Derived>::guid(caors::guid guid_value) {
  return builder().guid(guid_value);
}

template <typename T, typename Derived>
class_builder<T>& class_builder_bind_chain<T, Derived>::parent(type_id parent_id) {
  return builder().parent(parent_id);
}

template <typename T, typename Derived>
template <typename Parent>
class_builder<T>& class_builder_bind_chain<T, Derived>::parent() {
  return builder().template parent<Parent>();
}

template <typename T, typename Derived>
class_builder<T>& class_builder_bind_chain<T, Derived>::factory() {
  return builder().factory();
}

template <typename T, typename Derived>
class_builder<T>& class_builder_bind_chain<T, Derived>::factory(factory_ops::construct_fn construct,
                                                                factory_ops::destroy_fn destroy) {
  return builder().factory(construct, destroy);
}

template <typename T, typename Derived>
template <typename... Arguments>
class_builder<T>& class_builder_bind_chain<T, Derived>::constructor() {
  return builder().template constructor<Arguments...>();
}

template <typename T, typename Derived>
template <typename Member>
field_bind<T> class_builder_bind_chain<T, Derived>::field(std::string_view name, Member member) {
  return builder().field(name, member);
}

template <typename T, typename Derived>
template <typename Member>
field_bind<T> class_builder_bind_chain<T, Derived>::field(std::string_view name, Member member,
                                                          std::string_view type_name) {
  return builder().field(name, member, type_name);
}

template <typename T, typename Derived>
template <typename Member>
method_bind<T> class_builder_bind_chain<T, Derived>::method(std::string_view name, Member member) {
  return builder().method(name, member);
}

template <typename T, typename Derived>
template <typename Value>
class_builder<T>& class_builder_bind_chain<T, Derived>::field_metadata(std::string_view key,
                                                                       Value&& value) {
  return builder().field_metadata(key, std::forward<Value>(value));
}

template <typename T, typename Derived>
template <typename Value>
class_builder<T>& class_builder_bind_chain<T, Derived>::field_metadata(string_name key,
                                                                       Value&& value) {
  return builder().field_metadata(key, std::forward<Value>(value));
}

template <typename T, typename Derived>
template <typename Capability>
class_builder<T>& class_builder_bind_chain<T, Derived>::capability(const Capability* capability) {
  return builder().capability(capability);
}

template <typename T, typename Derived>
template <typename Capability>
  requires(!std::is_pointer_v<std::remove_reference_t<Capability>>)
class_builder<T>& class_builder_bind_chain<T, Derived>::capability(Capability&& capability) {
  return builder().capability(std::forward<Capability>(capability));
}

template <typename T, typename Derived>
template <typename Capability>
class_builder<T>& class_builder_bind_chain<T, Derived>::capability() {
  return builder().template capability<Capability>();
}

template <typename T, typename Derived>
template <typename Value>
class_builder<T>& class_builder_bind_chain<T, Derived>::metadata(std::string_view key,
                                                                 Value&& value) {
  return builder().metadata(key, std::forward<Value>(value));
}

template <typename T, typename Derived>
template <typename Value>
class_builder<T>& class_builder_bind_chain<T, Derived>::metadata(string_name key, Value&& value) {
  return builder().metadata(key, std::forward<Value>(value));
}

template <typename T, typename Derived>
::caors::meta::type class_builder_bind_chain<T, Derived>::get() const noexcept {
  return builder().get();
}

template <typename T, typename Derived>
error class_builder_bind_chain<T, Derived>::result() const noexcept {
  return builder().result();
}

template <typename T, typename Derived>
error class_builder_bind_chain<T, Derived>::commit() const noexcept {
  return builder().commit();
}

template <typename T, typename Derived>
bool class_builder_bind_chain<T, Derived>::succeeded() const noexcept {
  return builder().succeeded();
}

/** @brief RTTR 风格的枚举类型注册构建器。 */
template <typename T> class enum_builder {
  static_assert(std::is_enum_v<std::remove_cv_t<std::remove_reference_t<T>>>,
                "T must be an enum type");

public:
  enum_builder() { refresh(); }

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
  [[nodiscard]] error commit() const noexcept { return status_; }
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

} // namespace caors::meta

#endif
