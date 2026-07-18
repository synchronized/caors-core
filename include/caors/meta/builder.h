// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_BUILDER_H_
#define CAORS_META_BUILDER_H_

#include "caors/meta/context.h"
#include "caors/meta/metadata.h"
#include "caors/meta/proxy.h"
#include "caors/meta/registration.h"
#include "caors/meta/registry.h"

#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::meta {

template <typename T> class enum_builder;
template <typename T> class enum_value_bind;

/** @brief RTTR 风格的枚举类型注册构建器。 */
template <typename T> class enum_builder {
  static_assert(std::is_enum_v<std::remove_cv_t<std::remove_reference_t<T>>>,
                "T must be an enum type");

public:
  explicit enum_builder(context& ctx = default_context()) : registry_(&ctx.get_registry()) {
    refresh();
  }

  enum_builder& type(std::string_view name) {
    status_ = registry_ref().template register_type_for<T>(string_name{name});
    refresh();
    return *this;
  }

  enum_builder& guid(type_guid guid_value) {
    if (!type_.registered()) {
      status_ = {error_domain::meta, errc::invalid_type};
      return *this;
    }

    status_ = registry_ref().set_type_guid(type_.id(), guid_value);
    refresh();
    return *this;
  }

  enum_builder& guid(caors::guid guid_value) { return guid(type_guid{guid_value}); }

  template <typename Capability> enum_builder& capability(const Capability* capability) {
    using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
    status_ = detail::register_capability_for_type<capability_type, T>(registry_ref(), capability);
    refresh();
    return *this;
  }

  template <typename Capability>
    requires(!std::is_pointer_v<std::remove_reference_t<Capability>>)
  enum_builder& capability(Capability&& capability) {
    using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
    status_ = detail::register_owned_capability_for_type<capability_type, T>(
        registry_ref(), capability_type{std::forward<Capability>(capability)});
    refresh();
    return *this;
  }

  template <typename Capability> enum_builder& capability() {
    using capability_type = std::remove_cv_t<std::remove_reference_t<Capability>>;
    if constexpr (detail::has_capability_model_v<capability_type, T>) {
      status_ = detail::register_capability_model_for<capability_type, T>(registry_ref());
    } else if constexpr (detail::has_capability_auto_model_v<capability_type, T>) {
      status_ = detail::register_capability_auto_model_for<capability_type, T>(registry_ref());
    } else {
      status_ = detail::register_static_capability_for<capability_type, T>(registry_ref());
    }
    refresh();
    return *this;
  }

  /** @brief 注册枚举项。重复名称会返回 `errc::duplicate_name`；重复数值允许作为别名。 */
  enum_value_bind<T> value(std::string_view name, T value);

  /** @brief 注册枚举项。重复名称会返回 `errc::duplicate_name`；重复数值允许作为别名。 */
  enum_value_bind<T> value(string_name name, T value);

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

    type_metadata* data =
        registry_ref().template find_extension_mutable_ptr<type_metadata>(type_.id());
    if (data == nullptr) {
      status_ = registry_ref().template emplace_extension<type_metadata>(type_.id());
      if (status_ && status_.value() != errc::duplicate_component) {
        return *this;
      }

      data = registry_ref().template find_extension_mutable_ptr<type_metadata>(type_.id());
    }

    if (data == nullptr) {
      status_ = {error_domain::meta, errc::invalid_type};
      return *this;
    }

    set_metadata(data->values, key, make_metadata_value(std::forward<Value>(value)));
    refresh();
    return *this;
  }

  /** @brief 用 RTTR 风格语法批量注册枚举项。 */
  template <typename... Entries> enum_builder& operator()(Entries&&... entries) {
    static_assert(
        (std::is_same_v<std::remove_cvref_t<Entries>, registration::enum_value_entry<T>> && ...),
        "meta::enum_builder::operator() expects meta::registration::value(...) entries");
    (apply_enum_value_entry(std::forward<Entries>(entries)), ...);
    return *this;
  }

  [[nodiscard]] ::caors::meta::type get() const noexcept { return type_; }
  [[nodiscard]] error result() const noexcept { return status_; }
  [[nodiscard]] error commit() const noexcept { return status_; }
  [[nodiscard]] bool succeeded() const noexcept {
    return status_.succeeded() || status_.value() == errc::duplicate_id;
  }

private:
  friend class enum_value_bind<T>;

  [[nodiscard]] registry& registry_ref() noexcept { return *registry_; }
  [[nodiscard]] const registry& registry_ref() const noexcept { return *registry_; }

  enum_builder& apply_enum_value_entry(const registration::enum_value_entry<T>& entry) {
    return add_enum_value(entry.name, entry.value);
  }

  enum_builder& apply_enum_value_entry(registration::enum_value_entry<T>&& entry) {
    return add_enum_value(entry.name, entry.value);
  }

  enum_builder& apply_enum_value_metadata(string_name value_name,
                                          const registration::metadata_entry& entry) {
    return set_enum_value_metadata(value_name, entry.key, entry.value);
  }

  enum_builder& apply_enum_value_metadata(string_name value_name,
                                          registration::metadata_entry&& entry) {
    return set_enum_value_metadata(value_name, entry.key, std::move(entry.value));
  }

  enum_builder& add_enum_value(string_name name, T value);
  enum_builder& set_enum_value_metadata(string_name value_name, metadata_key key,
                                        metadata_value value);

  void refresh() {
    ::caors::meta::type refreshed = registry_ref().template find_type<T>();
    if (refreshed.registered()) {
      type_ = refreshed;
    }
  }

  registry* registry_ = nullptr;
  ::caors::meta::type type_{};
  error status_{};
};

/** @brief 刚注册枚举项的 RTTR 风格 bind 对象。 */
template <typename T> class enum_value_bind final {
public:
  enum_value_bind(enum_builder<T>& builder, string_name value_name) noexcept
      : builder_(&builder), value_name_(value_name) {}

  template <typename... Entries> enum_builder<T>& operator()(Entries&&... entries) {
    static_assert(
        (std::is_same_v<std::remove_cvref_t<Entries>, registration::metadata_entry> && ...),
        "meta::enum_value_bind::operator() expects meta::registration::metadata(...) entries");
    (builder_->apply_enum_value_metadata(value_name_, std::forward<Entries>(entries)), ...);
    return *builder_;
  }

  enum_builder<T>& type(std::string_view name) { return builder_->type(name); }
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
  enum_value_bind<T> value(std::string_view name, T value) { return builder_->value(name, value); }
  enum_value_bind<T> value(string_name name, T value) { return builder_->value(name, value); }
  template <typename Value> enum_builder<T>& metadata(std::string_view key, Value&& value) {
    return builder_->metadata(key, std::forward<Value>(value));
  }
  template <typename Value> enum_builder<T>& metadata(string_name key, Value&& value) {
    return builder_->metadata(key, std::forward<Value>(value));
  }

  [[nodiscard]] ::caors::meta::type get() const noexcept { return builder_->get(); }
  [[nodiscard]] error result() const noexcept { return builder_->result(); }
  [[nodiscard]] error commit() const noexcept { return builder_->commit(); }
  [[nodiscard]] bool succeeded() const noexcept { return builder_->succeeded(); }
  [[nodiscard]] operator enum_builder<T>&() noexcept { return *builder_; }
  [[nodiscard]] operator const enum_builder<T>&() const noexcept { return *builder_; }

private:
  enum_builder<T>* builder_ = nullptr;
  string_name value_name_{};
};

template <typename T> enum_value_bind<T> enum_builder<T>::value(std::string_view name, T value) {
  return this->value(string_name{name}, value);
}

template <typename T> enum_value_bind<T> enum_builder<T>::value(string_name name, T value) {
  add_enum_value(name, value);
  return enum_value_bind<T>{*this, status_.succeeded() ? name : string_name{}};
}

/** @brief 获取指定 C++ 枚举类型的元类型注册构建器；默认不注册类型。 */
template <typename T> [[nodiscard]] enum_builder<T> enum_(context& ctx) {
  return enum_builder<T>{ctx};
}

/** @brief 获取指定 C++ 枚举类型的元类型注册构建器；默认不注册类型。 */
template <typename T> [[nodiscard]] enum_builder<T> enum_() { return enum_<T>(default_context()); }

/** @brief 在指定上下文中显式注册指定 C++ 枚举类型并设置类型名。 */
template <typename T> [[nodiscard]] enum_builder<T> enum_(context& ctx, std::string_view name) {
  return enum_builder<T>{ctx}.type(name);
}

/** @brief 显式注册指定 C++ 枚举类型并设置类型名。 */
template <typename T> [[nodiscard]] enum_builder<T> enum_(std::string_view name) {
  return enum_<T>(default_context(), name);
}

} // namespace caors::meta

#include "caors/meta/enum_registration.h"

#endif
