// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REGISTRATION_TYPE_BUILDER_H_
#define CAORS_REGISTRATION_TYPE_BUILDER_H_

#include "caors/meta/constructor.h"
#include "caors/meta/detail/extension_data.h"
#include "caors/meta/detail/method_model.h"
#include "caors/meta/detail/parent_cast_ops.h"
#include "caors/meta/detail/pending_type_record.h"
#include "caors/meta/detail/property_model.h"
#include "caors/meta/detail/static_type_record_model.h"
#include "caors/meta/metadata.h"
#include "caors/meta/method_registration.h"
#include "caors/meta/object.h"
#include "caors/meta/property.h"
#include "caors/meta/proxy.h"
#include "caors/meta/query.h"
#include "caors/meta/registration.h"
#include "caors/meta/registry.h"
#include "caors/object/factory.h"
#include "caors/object/registry.h"
#include "caors/serialization/schema_query.h"
#include "caors/serialization/schema_registration.h"
#include "caors/traits/type_identity.h"

#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::registration {

template <typename T> class type_builder;
template <typename T> class meta_section;
template <typename T> class serialization_section;

namespace detail {

template <typename Member> struct member_object_pointer_traits;

template <typename Class, typename Property>
struct member_object_pointer_traits<Property Class::*> {
  using class_type = Class;
  using property_type = Property;
};

template <typename Member> struct member_function_pointer_traits;

template <typename Class, typename Return, typename... Arguments>
struct member_function_pointer_traits<Return (Class::*)(Arguments...)> {
  using class_type = Class;
  using return_type = Return;
  static constexpr bool is_const = false;
};

template <typename Class, typename Return, typename... Arguments>
struct member_function_pointer_traits<Return (Class::*)(Arguments...) const> {
  using class_type = Class;
  using return_type = Return;
  static constexpr bool is_const = true;
};

template <typename Class, typename Return, typename... Arguments>
struct member_function_pointer_traits<Return (Class::*)(Arguments...) noexcept> {
  using class_type = Class;
  using return_type = Return;
  static constexpr bool is_const = false;
};

template <typename Class, typename Return, typename... Arguments>
struct member_function_pointer_traits<Return (Class::*)(Arguments...) const noexcept> {
  using class_type = Class;
  using return_type = Return;
  static constexpr bool is_const = true;
};

template <typename Entry>
inline constexpr bool is_property_entry_v =
    std::is_same_v<std::remove_cvref_t<Entry>, meta::registration::metadata_entry> ||
    std::is_same_v<std::remove_cvref_t<Entry>, meta::registration::property_schema_entry>;

template <typename Entry>
inline constexpr bool is_method_entry_v =
    std::is_same_v<std::remove_cvref_t<Entry>, meta::registration::metadata_entry> ||
    std::is_same_v<std::remove_cvref_t<Entry>, meta::registration::method_default_argument_entry> ||
    std::is_same_v<std::remove_cvref_t<Entry>, meta::registration::method_parameter_name_entry>;

template <typename Child, typename Parent>
[[nodiscard]] constexpr meta::detail::parent_cast_ops parent_cast_ops_for() noexcept {
  using child_type = std::remove_cv_t<std::remove_reference_t<Child>>;
  using parent_type = std::remove_cv_t<std::remove_reference_t<Parent>>;
  static_assert(std::is_base_of_v<parent_type, child_type>, "Parent must be a base class of Child");

  return meta::detail::parent_cast_ops{
      meta::detail::static_type_id<parent_type>(),
      [](const void* value) noexcept -> const void* {
        return value != nullptr
                   ? static_cast<const parent_type*>(static_cast<const child_type*>(value))
                   : nullptr;
      },
      [](void* value) noexcept -> void* {
        return value != nullptr ? static_cast<parent_type*>(static_cast<child_type*>(value))
                                : nullptr;
      },
  };
}

} // namespace detail

/**
 * @brief 跨模块类型注册构建器。
 *
 * builder 在注册阶段持有独立的 `pending_type_record`，meta 属性/方法、serialization schema 和
 * object capability 都写入 pending 记录；`commit()` 校验成功后一次性发布为 registry 内的
 * `type_record`。
 */
template <typename T> class type_builder {
public:
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;

  type_builder(meta::context& ctx, std::string_view name)
      : registry_(&ctx.get_registry()),
        pending_(meta::detail::static_type_id<value_type>(), core::string_name{name}) {
    if (name.empty()) {
      status_ = {error_domain::meta, errc::invalid_argument};
    }
  }

  /** @brief 设置类型稳定 GUID。 */
  type_builder& guid(type_guid guid_value) {
    pending_.set_guid(guid_value);
    return *this;
  }

  /** @brief 设置类型稳定 GUID。 */
  type_builder& guid(caors::guid guid_value) { return guid(type_guid{guid_value}); }

  /** @brief 设置已注册父类型 id。 */
  type_builder& parent(type_id parent_id) {
    pending_.set_parent_id(parent_id);
    return *this;
  }

  /** @brief 设置 C++ 父类型并保存安全 upcast 信息。 */
  template <typename Parent> type_builder& parent() {
    pending_.set_parent_id(meta::detail::static_type_id<Parent>());
    pending_.set_parent_cast(detail::parent_cast_ops_for<value_type, Parent>());
    return *this;
  }

  /** @brief 进入 meta 属性、方法和基础能力注册段。 */
  [[nodiscard]] meta_section<T> meta() noexcept { return meta_section<T>{*this}; }

  /** @brief 进入序列化 schema 注册段。 */
  [[nodiscard]] serialization_section<T> serialization() noexcept {
    serialization_enabled_ = true;
    return serialization_section<T>{*this};
  }

  /** @brief 提交 pending 类型数据；已注册类型不会被继续修改。 */
  [[nodiscard]] error commit() {
    if (status_) {
      return status_;
    }

    enable_object_if_needed();
    if (status_) {
      return status_;
    }

    const meta::detail::type_record* fallback =
        meta::detail::static_type_record_ptr_of<value_type>();
    if (fallback == nullptr || !fallback->valid() || !fallback->value_ops().valid()) {
      status_ = {error_domain::meta, errc::invalid_type};
      return status_;
    }

    status_ = pending_.set_static_data(fallback);
    if (status_) {
      return status_;
    }
    status_ = registry_->commit_type(std::move(pending_), typeid(value_type));
    if (!status_) {
      committed_type_ = registry_->template find_type<value_type>();
      if (serialization_enabled_) {
        status_ = serialization::prepare_type_schema(committed_type_, serialization_schema_);
        if (status_) {
          return status_;
        }

        status_ = meta::detail::register_owned_capability(
            *registry_, committed_type_.id(),
            meta::capability_key_of<serialization::schema_capability>(),
            meta::detail::make_owned_capability<serialization::type_schema>(
                serialization_schema_, &memory::library_resource()));
      }
    }
    return status_;
  }

  [[nodiscard]] meta::type get() const noexcept {
    return committed_type_ ? committed_type_
                           : (registry_ != nullptr ? registry_->template find_type<value_type>()
                                                   : meta::type{});
  }

  [[nodiscard]] error result() const noexcept { return status_; }
  [[nodiscard]] bool succeeded() const noexcept { return !status_; }

private:
  friend class meta_section<T>;
  friend class serialization_section<T>;

  /** @brief 注册默认 factory。 */
  type_builder& register_factory() {
    return register_factory(meta::detail::resource_default_construct_fn_of<value_type>(),
                            meta::detail::resource_destroy_fn_of<value_type>());
  }

  /** @brief 注册自定义 factory。 */
  type_builder& register_factory(meta::factory_ops::construct_fn construct,
                                 meta::factory_ops::destroy_fn destroy) {
    const meta::factory_ops ops{construct, destroy};
    if (!ops.valid()) {
      status_ = {error_domain::meta, errc::invalid_type};
      return *this;
    }

    status_ =
        pending_.add_owned_capability(meta::capability_key_of<meta::factory_ops>(),
                                      meta::detail::make_owned_capability<meta::factory_ops>(ops));
    return *this;
  }

  /** @brief 注册运行期构造参数。 */
  template <typename... Arguments> type_builder& register_constructor() {
    const meta::constructor_ops ops =
        meta::detail::make_constructor_ops<value_type, Arguments...>();
    if (!ops.valid()) {
      status_ = {error_domain::meta, errc::invalid_type};
      return *this;
    }

    status_ = pending_.add_owned_capability(
        meta::capability_key_of<meta::constructor_ops>(),
        meta::detail::make_owned_capability<meta::constructor_ops>(ops));
    return *this;
  }

  /** @brief 写入类型 metadata。 */
  template <typename Value> type_builder& register_metadata(std::string_view key, Value&& value) {
    return register_metadata(core::string_name{key}, std::forward<Value>(value));
  }

  /** @brief 写入类型 metadata。 */
  template <typename Value> type_builder& register_metadata(core::string_name key, Value&& value) {
    if (!key) {
      status_ = {error_domain::meta, errc::invalid_type};
      return *this;
    }

    status_ =
        pending_.set_type_metadata(key, meta::make_metadata_value(std::forward<Value>(value)));
    return *this;
  }

  /** @brief 挂载外部生命周期管理的运行期 capability。 */
  template <typename Capability> type_builder& register_capability(const Capability* capability) {
    using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
    status_ = pending_.add_capability(meta::capability_key_of<capability_type>(), capability);
    return *this;
  }

  /** @brief 挂载由 CAORS 持有的运行期 capability。 */
  template <typename Capability>
    requires(!std::is_pointer_v<std::remove_reference_t<Capability>>)
  type_builder& register_capability(Capability&& capability) {
    using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
    status_ =
        pending_.add_owned_capability(meta::capability_key_of<capability_type>(),
                                      meta::detail::make_owned_capability<capability_type>(
                                          capability_type{std::forward<Capability>(capability)}));
    return *this;
  }

  /** @brief 从 C++ 类型静态 capability provider 挂载 capability。 */
  template <typename Capability> type_builder& register_capability() {
    using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
    if constexpr (meta::detail::has_capability_model_v<capability_type, value_type>) {
      using model_type = meta::capability_model<capability_type, value_type>;
      using ops_type = typename model_type::ops_type;
      const ops_type* ops = model_type::get();
      status_ = ops != nullptr ? pending_.add_capability(meta::capability_key_of<ops_type>(), ops)
                               : error{error_domain::meta, errc::unsupported};
    } else if constexpr (meta::detail::has_capability_auto_model_v<capability_type, value_type>) {
      using model_type = typename capability_type::template model<value_type>;
      using ops_type = typename model_type::ops_type;
      const ops_type* ops = model_type::get();
      status_ = ops != nullptr ? pending_.add_capability(meta::capability_key_of<ops_type>(), ops)
                               : error{error_domain::meta, errc::unsupported};
    } else {
      const capability_type* capability = meta::capabilities::get<capability_type, value_type>();
      status_ =
          capability != nullptr
              ? pending_.add_capability(meta::capability_key_of<capability_type>(), capability)
              : error{error_domain::meta, errc::unsupported};
    }
    return *this;
  }

  void enable_object_if_needed() {
    if constexpr (std::is_base_of_v<meta::object, value_type>) {
      if (object_enabled_) {
        return;
      }

      object_enabled_ = true;
      using base_type = typename meta::detail::object_base_type_traits<value_type>::type;
      if constexpr (!std::is_same_v<value_type, meta::object>) {
        if (registry_ != nullptr) {
          status_ = objects::detail::ensure_object_base_registered(*registry_);
        }
        if (!status_ && !pending_.parent_id() && registry_ != nullptr &&
            registry_->template find_type<base_type>()) {
          parent<base_type>();
        }
      }

      const objects::detail::object_factory_ops* factory =
          objects::detail::object_factory_ops_ptr_of<value_type>();
      if (factory != nullptr &&
          !pending_.contains_capability(
              meta::capability_key_of<objects::detail::object_factory_ops>())) {
        status_ = pending_.add_capability(
            meta::capability_key_of<objects::detail::object_factory_ops>(), factory);
      }
    }
  }

  template <typename Member>
  [[nodiscard]] core::string_name add_property(std::string_view name, Member member,
                                               std::string_view type_name = {}) {
    if (status_) {
      return {};
    }

    static_assert(std::is_member_object_pointer_v<Member>,
                  "registration::type_builder::property requires a data member pointer");
    using traits_type = detail::member_object_pointer_traits<Member>;
    using owner_type = typename traits_type::class_type;
    using property_type = std::remove_cv_t<typename traits_type::property_type>;
    static_assert(std::is_same_v<owner_type, value_type>,
                  "Property member pointer owner must match builder type");

    if (type_name.empty() && !caors::meta::resolve<property_type>()) {
      status_ = {error_domain::meta, errc::invalid_field};
      return {};
    }

    meta::detail::property_record property =
        meta::detail::make_property_record<value_type, typename traits_type::property_type>(
            name, type_name, meta::detail::member_offset(member));
    if (!property.valid() || pending_.find_property_ptr(property.name()) != nullptr) {
      status_ = {error_domain::meta,
                 property.valid() ? errc::duplicate_field : errc::invalid_field};
      return {};
    }

    const core::string_name property_name = property.name();
    pending_.add_property(std::move(property));
    status_ = {};
    return property_name;
  }

  type_builder& apply_property_entry(core::string_name property_name,
                                     const meta::registration::metadata_entry& entry) {
    return set_property_entry(property_name, entry.key, entry.value);
  }

  type_builder& apply_property_entry(core::string_name property_name,
                                     meta::registration::metadata_entry&& entry) {
    return set_property_entry(property_name, entry.key, std::move(entry.value));
  }

  type_builder& apply_property_entry(core::string_name property_name,
                                     const meta::registration::property_schema_entry& entry) {
    return set_property_schema_entry(property_name, entry);
  }

  type_builder& apply_property_entry(core::string_name property_name,
                                     meta::registration::property_schema_entry&& entry) {
    return set_property_schema_entry(property_name, std::move(entry));
  }

  type_builder& set_property_entry(core::string_name property_name, meta::metadata_key key,
                                   meta::metadata_value value) {
    if (!key || !property_name) {
      status_ = {error_domain::meta, errc::invalid_field};
      return *this;
    }

    status_ = pending_.set_property_metadata(property_name, key, std::move(value));
    return *this;
  }

  type_builder& set_property_schema_entry(core::string_name property_name,
                                          meta::registration::property_schema_entry entry) {
    if (!property_name) {
      status_ = {error_domain::meta, errc::invalid_field};
      return *this;
    }

    status_ = pending_.apply_property_schema_entry(property_name, std::move(entry));
    return *this;
  }

  template <typename Getter, typename Setter>
  [[nodiscard]] core::string_name add_property(std::string_view name, Getter getter,
                                               Setter setter) {
    if (status_) {
      return {};
    }

    static_assert(std::is_member_function_pointer_v<Getter>,
                  "registration::type_builder::property getter must be a member function pointer");
    static_assert(std::is_member_function_pointer_v<Setter>,
                  "registration::type_builder::property setter must be a member function pointer");

    meta::detail::property_record property =
        meta::detail::make_accessor_property_record<value_type>(name, getter, setter);
    if (!property.valid() || pending_.find_property_ptr(property.name()) != nullptr) {
      status_ = {error_domain::meta,
                 property.valid() ? errc::duplicate_field : errc::invalid_field};
      return {};
    }

    const core::string_name property_name = property.name();
    pending_.add_property(std::move(property));
    status_ = {};
    return property_name;
  }

  template <typename Getter>
  [[nodiscard]] core::string_name add_readonly_property(std::string_view name, Getter getter) {
    if (status_) {
      return {};
    }

    static_assert(
        std::is_member_function_pointer_v<Getter>,
        "registration::type_builder::property_readonly requires a member function pointer");

    meta::detail::property_record property =
        meta::detail::make_readonly_property_record<value_type>(name, getter);
    if (!property.valid() || pending_.find_property_ptr(property.name()) != nullptr) {
      status_ = {error_domain::meta,
                 property.valid() ? errc::duplicate_field : errc::invalid_field};
      return {};
    }

    const core::string_name property_name = property.name();
    pending_.add_property(std::move(property));
    status_ = {};
    return property_name;
  }

  template <typename Member>
  [[nodiscard]] std::pair<core::string_name, std::size_t> add_method(std::string_view name,
                                                                     Member member) {
    if (status_) {
      return {};
    }

    static_assert(std::is_member_function_pointer_v<Member>,
                  "registration::type_builder::method requires a member function pointer");
    using traits_type = detail::member_function_pointer_traits<Member>;
    using owner_type = typename traits_type::class_type;
    static_assert(std::is_same_v<owner_type, value_type>,
                  "Method member pointer owner must match builder type");

    meta::detail::method_record method = meta::detail::make_method_record<value_type>(name, member);
    if (!method.valid()) {
      status_ = {error_domain::meta, errc::invalid_field};
      return {};
    }

    const core::string_name method_name = method.name();
    const std::optional<std::size_t> overload_index = pending_.add_method(std::move(method));
    if (!overload_index.has_value()) {
      status_ = {error_domain::meta, errc::duplicate_method};
      return {};
    }

    status_ = {};
    return {method_name, *overload_index};
  }

  type_builder& add_nested(std::string_view name, meta::type nested_type) {
    if (status_) {
      return *this;
    }

    if (name.empty() || !nested_type.registered() || nested_type.id() == pending_.id()) {
      status_ = {error_domain::meta, errc::invalid_type};
      return *this;
    }

    status_ = pending_.add_nested_type(
        meta::detail::nested_type_data{core::string_name{name}, nested_type.id()});
    return *this;
  }

  template <typename Nested> type_builder& add_nested(std::string_view name) {
    using nested_type = std::remove_cv_t<std::remove_reference_t<Nested>>;
    meta::type nested =
        registry_ != nullptr ? registry_->template find_type<nested_type>() : meta::type{};
    if (!nested.registered()) {
      status_ = {error_domain::meta, errc::missing_type};
      return *this;
    }

    return add_nested(name, nested);
  }

  type_builder& apply_method_entry(core::string_name method_name, std::size_t overload_index,
                                   const meta::registration::metadata_entry& entry) {
    return set_method_metadata(method_name, overload_index, entry.key, entry.value);
  }

  type_builder& apply_method_entry(core::string_name method_name, std::size_t overload_index,
                                   meta::registration::metadata_entry&& entry) {
    return set_method_metadata(method_name, overload_index, entry.key, std::move(entry.value));
  }

  type_builder& apply_method_entry(core::string_name method_name, std::size_t overload_index,
                                   const meta::registration::method_default_argument_entry& entry) {
    return add_method_default_argument(method_name, overload_index, entry.value);
  }

  type_builder& apply_method_entry(core::string_name method_name, std::size_t overload_index,
                                   meta::registration::method_default_argument_entry&& entry) {
    return add_method_default_argument(method_name, overload_index, std::move(entry.value));
  }

  type_builder& apply_method_entry(core::string_name method_name, std::size_t overload_index,
                                   meta::registration::method_parameter_name_entry entry) {
    return add_method_parameter_name(method_name, overload_index, entry.name);
  }

  type_builder& set_method_metadata(core::string_name method_name, std::size_t overload_index,
                                    meta::metadata_key key, meta::metadata_value value) {
    if (!key || !method_name || overload_index == static_cast<std::size_t>(-1)) {
      status_ = {error_domain::meta, errc::invalid_field};
      return *this;
    }

    status_ = pending_.set_method_metadata(method_name, overload_index, key, std::move(value));
    return *this;
  }

  type_builder& add_method_default_argument(core::string_name method_name,
                                            std::size_t overload_index, meta::any value) {
    if (!value || !method_name || overload_index == static_cast<std::size_t>(-1)) {
      status_ = {error_domain::meta, errc::invalid_field};
      return *this;
    }

    status_ = pending_.add_method_default_argument(method_name, overload_index, std::move(value));
    return *this;
  }

  type_builder& add_method_parameter_name(core::string_name method_name, std::size_t overload_index,
                                          core::string_name name) {
    if (!name || !method_name || overload_index == static_cast<std::size_t>(-1)) {
      status_ = {error_domain::meta, errc::invalid_field};
      return *this;
    }

    status_ = pending_.add_method_parameter_name(method_name, overload_index, name);
    return *this;
  }

  meta::registry* registry_ = nullptr;
  meta::detail::pending_type_record pending_{};
  meta::type committed_type_{};
  serialization::type_schema serialization_schema_{};
  bool serialization_enabled_ = false;
  bool object_enabled_ = false;
  error status_{};
};

/** @brief `registration::type_builder` 的 meta 注册段。 */
template <typename T> class meta_section {
public:
  explicit meta_section(type_builder<T>& owner) noexcept : owner_(&owner) {}

  meta_section& guid(type_guid guid_value) {
    owner_->guid(guid_value);
    return *this;
  }

  meta_section& guid(caors::guid guid_value) {
    owner_->guid(guid_value);
    return *this;
  }

  meta_section& parent(type_id parent_id) {
    owner_->parent(parent_id);
    return *this;
  }

  template <typename Parent> meta_section& parent() {
    owner_->template parent<Parent>();
    return *this;
  }

  meta_section& factory() {
    owner_->register_factory();
    return *this;
  }

  meta_section& factory(meta::factory_ops::construct_fn construct,
                        meta::factory_ops::destroy_fn destroy) {
    owner_->register_factory(construct, destroy);
    return *this;
  }

  template <typename... Arguments> meta_section& constructor() {
    owner_->template register_constructor<Arguments...>();
    return *this;
  }

  template <typename Member> meta_section& property(std::string_view name, Member member) {
    (void)owner_->add_property(name, member);
    return *this;
  }

  template <typename Member, typename... Entries>
    requires(sizeof...(Entries) > 0 && (detail::is_property_entry_v<Entries> && ...))
  meta_section& property(std::string_view name, Member member, Entries&&... entries) {
    const core::string_name property_name = owner_->add_property(name, member);
    if (property_name) {
      (owner_->apply_property_entry(property_name, std::forward<Entries>(entries)), ...);
    }
    return *this;
  }

  template <typename Member>
  meta_section& property(std::string_view name, Member member, std::string_view type_name) {
    (void)owner_->add_property(name, member, type_name);
    return *this;
  }

  template <typename Getter, typename Setter>
  meta_section& property(std::string_view name, Getter getter, Setter setter)
    requires(!std::is_convertible_v<Setter, std::string_view> &&
             !detail::is_property_entry_v<Setter>)
  {
    (void)owner_->add_property(name, getter, setter);
    return *this;
  }

  template <typename Getter> meta_section& property_readonly(std::string_view name, Getter getter) {
    (void)owner_->add_readonly_property(name, getter);
    return *this;
  }

  template <typename Member> meta_section& method(std::string_view name, Member member) {
    (void)owner_->add_method(name, member);
    return *this;
  }

  template <typename Member, typename... Entries>
    requires(sizeof...(Entries) > 0 && (detail::is_method_entry_v<Entries> && ...))
  meta_section& method(std::string_view name, Member member, Entries&&... entries) {
    const auto [method_name, overload_index] = owner_->add_method(name, member);
    if (method_name) {
      (owner_->apply_method_entry(method_name, overload_index, std::forward<Entries>(entries)),
       ...);
    }
    return *this;
  }

  template <typename Nested> meta_section& nested(std::string_view name) {
    owner_->template add_nested<Nested>(name);
    return *this;
  }

  meta_section& nested(std::string_view name, meta::type nested_type) {
    owner_->add_nested(name, nested_type);
    return *this;
  }

  template <typename Capability> meta_section& capability(const Capability* capability) {
    owner_->register_capability(capability);
    return *this;
  }

  template <typename Capability>
    requires(!std::is_pointer_v<std::remove_reference_t<Capability>>)
  meta_section& capability(Capability&& capability) {
    owner_->register_capability(std::forward<Capability>(capability));
    return *this;
  }

  template <typename Capability> meta_section& capability() {
    owner_->template register_capability<Capability>();
    return *this;
  }

  template <typename Value> meta_section& metadata(std::string_view key, Value&& value) {
    owner_->register_metadata(key, std::forward<Value>(value));
    return *this;
  }

  [[nodiscard]] serialization_section<T> serialization() noexcept {
    return owner_->serialization();
  }

  [[nodiscard]] error commit() { return owner_->commit(); }

private:
  type_builder<T>* owner_ = nullptr;
};

/** @brief `registration::type_builder` 的序列化 schema 注册段。 */
template <typename T> class serialization_section {
public:
  explicit serialization_section(type_builder<T>& owner) noexcept : owner_(&owner) {}

  serialization_section& field(std::string_view field_name, std::string_view serialized_name = {},
                               bool skipped = false, std::uint32_t field_id = 0) {
    owner_->serialization_schema_.fields.emplace_back(
        serialization::make_field(field_name, serialized_name, skipped, field_id));
    return *this;
  }

  serialization_section& field(serialization::field_options options) {
    owner_->serialization_schema_.fields.emplace_back(std::move(options));
    return *this;
  }

  serialization_section& fields(std::initializer_list<serialization::field_options> fields) {
    for (const serialization::field_options& field : fields) {
      owner_->serialization_schema_.fields.emplace_back(field);
    }
    return *this;
  }

  [[nodiscard]] meta_section<T> meta() noexcept { return owner_->meta(); }

  [[nodiscard]] error commit() { return owner_->commit(); }

private:
  type_builder<T>* owner_ = nullptr;
};

/** @brief 创建跨模块类型注册构建器。 */
template <typename T>
[[nodiscard]] type_builder<T> type_(meta::context& ctx, std::string_view name) {
  return type_builder<T>{ctx, name};
}

/** @brief 使用默认上下文创建跨模块类型注册构建器。 */
template <typename T> [[nodiscard]] type_builder<T> type_(std::string_view name) {
  return type_<T>(meta::default_context(), name);
}

/** @brief 使用对象静态身份声明的类型名创建跨模块注册构建器。 */
template <typename T>
  requires(std::is_base_of_v<meta::object, T>)
[[nodiscard]] type_builder<T> type_() {
  return type_<T>(traits::type_identity<T>::name());
}

} // namespace caors::registration

#endif
