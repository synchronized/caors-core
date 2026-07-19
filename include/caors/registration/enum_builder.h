// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REGISTRATION_ENUM_BUILDER_H_
#define CAORS_REGISTRATION_ENUM_BUILDER_H_

#include "caors/meta/capability_bind.h"
#include "caors/meta/detail/enum_value_data.h"
#include "caors/meta/detail/extension_data.h"
#include "caors/meta/detail/type_data.h"
#include "caors/meta/detail/type_descriptor_model.h"
#include "caors/meta/metadata.h"
#include "caors/meta/proxy.h"
#include "caors/meta/query.h"
#include "caors/meta/registration.h"
#include "caors/meta/registry.h"
#include "caors/meta/static_capability.h"

#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::registration {

template <typename T> class enum_builder;
template <typename T> class enum_value_bind;

/** @brief 枚举项注册输入。 */
template <typename T> struct enum_value_entry {
  using enum_type = std::remove_cv_t<std::remove_reference_t<T>>;
  static_assert(std::is_enum_v<enum_type>, "registration::enum_value_entry requires an enum type");

  string_name name{};
  enum_type value{};
};

/** @brief 创建枚举项注册输入，用于 `registration::enum_<T>()(...)`。 */
template <typename Enum>
[[nodiscard]] enum_value_entry<Enum> value(string_name name, Enum enum_value) {
  using enum_type = std::remove_cv_t<std::remove_reference_t<Enum>>;
  static_assert(std::is_enum_v<enum_type>, "registration::value requires an enum type");
  return enum_value_entry<enum_type>{name, static_cast<enum_type>(enum_value)};
}

/** @brief 创建枚举项注册输入，用于 `registration::enum_<T>()(...)`。 */
template <typename Enum>
[[nodiscard]] enum_value_entry<Enum> value(std::string_view name, Enum enum_value) {
  return value(string_name{name}, enum_value);
}

/** @brief 枚举类型注册构建器，提交前在 pending type_data 中收集枚举项和 metadata。 */
template <typename T> class enum_builder {
  static_assert(std::is_enum_v<std::remove_cv_t<std::remove_reference_t<T>>>,
                "T must be an enum type");

public:
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;

  enum_builder(meta::context& ctx, std::string_view name)
      : registry_(&ctx.get_registry()),
        pending_(meta::detail::descriptor_type_id<value_type>(), core::string_name{name}) {
    if (name.empty()) {
      status_ = {error_domain::meta, errc::invalid_argument};
    }
  }

  /** @brief 设置枚举类型稳定 GUID。 */
  enum_builder& guid(type_guid guid_value) {
    pending_.set_guid(guid_value);
    return *this;
  }

  /** @brief 设置枚举类型稳定 GUID。 */
  enum_builder& guid(caors::guid guid_value) { return guid(type_guid{guid_value}); }

  /** @brief 挂载外部生命周期管理的运行期 capability。 */
  template <typename Capability> enum_builder& capability(const Capability* capability) {
    using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
    status_ = pending_.add_capability(meta::capability_key_of<capability_type>(), capability);
    return *this;
  }

  /** @brief 挂载由 CAORS 持有的运行期 capability。 */
  template <typename Capability>
    requires(!std::is_pointer_v<std::remove_reference_t<Capability>>)
  enum_builder& capability(Capability&& capability) {
    using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
    status_ =
        pending_.add_owned_capability(meta::capability_key_of<capability_type>(),
                                      meta::detail::make_owned_capability<capability_type>(
                                          capability_type{std::forward<Capability>(capability)}));
    return *this;
  }

  /** @brief 从 descriptor 静态 capability provider 挂载 capability。 */
  template <typename Capability> enum_builder& capability() {
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

  /** @brief 注册枚举项。重复名称返回 `errc::duplicate_name`，重复数值允许作为别名。 */
  enum_value_bind<T> value(std::string_view name, T enum_value);

  /** @brief 注册枚举项。重复名称返回 `errc::duplicate_name`，重复数值允许作为别名。 */
  enum_value_bind<T> value(string_name name, T enum_value);

  /** @brief 写入枚举类型 metadata。 */
  template <typename Value> enum_builder& metadata(std::string_view key, Value&& value) {
    return metadata(core::string_name{key}, std::forward<Value>(value));
  }

  /** @brief 写入枚举类型 metadata。 */
  template <typename Value> enum_builder& metadata(core::string_name key, Value&& value) {
    if (status_) {
      return *this;
    }

    if (!key) {
      status_ = {error_domain::meta, errc::invalid_type};
      return *this;
    }

    meta::type_metadata* data = pending_type_metadata();
    if (data == nullptr) {
      status_ = {error_domain::meta, errc::invalid_type};
      return *this;
    }

    meta::set_metadata(data->values, key, meta::make_metadata_value(std::forward<Value>(value)));
    return *this;
  }

  /** @brief 用批量注册输入追加枚举项。 */
  template <typename... Entries> enum_builder& operator()(Entries&&... entries) {
    static_assert(
        (std::is_same_v<std::remove_cvref_t<Entries>, enum_value_entry<value_type>> && ...),
        "registration::enum_builder::operator() expects registration::value(...) entries");
    (apply_enum_value_entry(std::forward<Entries>(entries)), ...);
    return *this;
  }

  /** @brief 提交 pending 枚举类型数据；已注册枚举不会被继续修改。 */
  [[nodiscard]] error commit() {
    if (status_) {
      return status_;
    }

    const meta::detail::type_descriptor* descriptor = meta::detail::descriptor_ptr_of<value_type>();
    if (descriptor == nullptr || !descriptor->valid()) {
      status_ = {error_domain::meta, errc::invalid_type};
      return status_;
    }

    pending_.set_descriptor(descriptor);
    status_ = registry_->commit_type(std::move(pending_), typeid(value_type));
    if (!status_) {
      committed_type_ = registry_->template find_type<value_type>();
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
  friend class enum_value_bind<T>;

  enum_builder& apply_enum_value_entry(const enum_value_entry<value_type>& entry) {
    return add_enum_value(entry.name, entry.value);
  }

  enum_builder& apply_enum_value_entry(enum_value_entry<value_type>&& entry) {
    return add_enum_value(entry.name, entry.value);
  }

  enum_builder& apply_enum_value_metadata(string_name value_name,
                                          const meta::registration::metadata_entry& entry) {
    return set_enum_value_metadata(value_name, entry.key, entry.value);
  }

  enum_builder& apply_enum_value_metadata(string_name value_name,
                                          meta::registration::metadata_entry&& entry) {
    return set_enum_value_metadata(value_name, entry.key, std::move(entry.value));
  }

  enum_builder& add_enum_value(string_name name, T enum_value) {
    if (status_) {
      return *this;
    }

    if (!name || pending_.find_enum_value_ptr(name) != nullptr) {
      status_ = {error_domain::meta, !name ? errc::invalid_type : errc::duplicate_name};
      return *this;
    }

    using underlying_type = std::underlying_type_t<value_type>;
    const underlying_type raw = static_cast<underlying_type>(enum_value);
    pending_.add_enum_value(meta::detail::enum_value_data{name, static_cast<std::int64_t>(raw),
                                                          static_cast<std::uint64_t>(raw)});
    return *this;
  }

  enum_builder& set_enum_value_metadata(string_name value_name, meta::metadata_key key,
                                        meta::metadata_value value) {
    if (status_) {
      return *this;
    }

    if (!key || !value_name) {
      status_ = {error_domain::meta, errc::invalid_type};
      return *this;
    }

    meta::detail::enum_value_data* enum_value = pending_.mutable_find_enum_value_ptr(value_name);
    if (enum_value == nullptr) {
      status_ = {error_domain::meta, errc::invalid_type};
      return *this;
    }

    enum_value->set_metadata(key, std::move(value));
    return *this;
  }

  [[nodiscard]] meta::type_metadata* pending_type_metadata() {
    meta::detail::extension_data* extension =
        pending_.mutable_find_extension(meta::detail::extension_key_of<meta::type_metadata>());
    if (extension == nullptr) {
      meta::type_metadata data{};
      data.values = meta::make_metadata_map(&memory::library_resource());
      status_ = pending_.add_extension(meta::detail::make_extension_data<meta::type_metadata>(
          &memory::library_resource(), data));
      if (status_) {
        return nullptr;
      }

      extension =
          pending_.mutable_find_extension(meta::detail::extension_key_of<meta::type_metadata>());
    }

    return extension != nullptr
               ? meta::detail::mutable_extension_value<meta::type_metadata>(*extension)
               : nullptr;
  }

  meta::registry* registry_ = nullptr;
  meta::detail::type_data pending_{};
  meta::type committed_type_{};
  error status_{};
};

/** @brief 刚注册枚举项的绑定对象，用于追加枚举项 metadata。 */
template <typename T> class enum_value_bind final {
public:
  enum_value_bind(enum_builder<T>& builder, string_name value_name) noexcept
      : builder_(&builder), value_name_(value_name) {}

  template <typename... Entries> enum_builder<T>& operator()(Entries&&... entries) {
    static_assert(
        (std::is_same_v<std::remove_cvref_t<Entries>, meta::registration::metadata_entry> && ...),
        "registration::enum_value_bind::operator() expects meta::registration::metadata(...) "
        "entries");
    (builder_->apply_enum_value_metadata(value_name_, std::forward<Entries>(entries)), ...);
    return *builder_;
  }

  enum_value_bind<T> value(std::string_view name, T enum_value) {
    return builder_->value(name, enum_value);
  }

  enum_value_bind<T> value(string_name name, T enum_value) {
    return builder_->value(name, enum_value);
  }

  enum_builder<T>& guid(type_guid guid_value) { return builder_->guid(guid_value); }
  enum_builder<T>& guid(caors::guid guid_value) { return builder_->guid(guid_value); }

  template <typename Capability> enum_builder<T>& capability(const Capability* capability) {
    return builder_->capability(capability);
  }

  template <typename Capability>
    requires(!std::is_pointer_v<std::remove_reference_t<Capability>>)
  enum_builder<T>& capability(Capability&& capability) {
    return builder_->capability(std::forward<Capability>(capability));
  }

  template <typename Capability> enum_builder<T>& capability() {
    return builder_->template capability<Capability>();
  }

  template <typename Value> enum_builder<T>& metadata(std::string_view key, Value&& value) {
    return builder_->metadata(key, std::forward<Value>(value));
  }

  template <typename Value> enum_builder<T>& metadata(string_name key, Value&& value) {
    return builder_->metadata(key, std::forward<Value>(value));
  }

  [[nodiscard]] error commit() { return builder_->commit(); }
  [[nodiscard]] meta::type get() const noexcept { return builder_->get(); }
  [[nodiscard]] error result() const noexcept { return builder_->result(); }
  [[nodiscard]] bool succeeded() const noexcept { return builder_->succeeded(); }
  [[nodiscard]] operator enum_builder<T>&() noexcept { return *builder_; }
  [[nodiscard]] operator const enum_builder<T>&() const noexcept { return *builder_; }

private:
  enum_builder<T>* builder_ = nullptr;
  string_name value_name_{};
};

template <typename T>
enum_value_bind<T> enum_builder<T>::value(std::string_view name, T enum_value) {
  return value(string_name{name}, enum_value);
}

template <typename T> enum_value_bind<T> enum_builder<T>::value(string_name name, T enum_value) {
  add_enum_value(name, enum_value);
  return enum_value_bind<T>{*this, status_.succeeded() ? name : string_name{}};
}

/** @brief 在指定上下文中创建枚举类型注册构建器。 */
template <typename T>
[[nodiscard]] enum_builder<T> enum_(meta::context& ctx, std::string_view name) {
  return enum_builder<T>{ctx, name};
}

/** @brief 使用默认上下文创建枚举类型注册构建器。 */
template <typename T> [[nodiscard]] enum_builder<T> enum_(std::string_view name) {
  return enum_<T>(meta::default_context(), name);
}

} // namespace caors::registration

#endif
