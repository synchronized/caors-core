// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_PENDING_TYPE_RECORD_H_
#define CAORS_META_DETAIL_PENDING_TYPE_RECORD_H_

#include "caors/meta/detail/enum_value_data.h"
#include "caors/meta/detail/method_data.h"
#include "caors/meta/detail/property_data.h"
#include "caors/meta/detail/type_data.h"

namespace caors::meta::detail {

/**
 * @brief 尚未发布的完整类型注册记录。
 *
 * builder 仅修改本类型；`validate()` 成功后由 registry 一次性取得内部 `type_data` 并发布。
 */
class pending_type_record final {
public:
  pending_type_record() = default;
  pending_type_record(type_id id, string_name name) : data_(id, name) {}

  pending_type_record(const pending_type_record&) = delete;
  pending_type_record& operator=(const pending_type_record&) = delete;
  pending_type_record(pending_type_record&&) noexcept = default;
  pending_type_record& operator=(pending_type_record&&) noexcept = default;

  [[nodiscard]] constexpr type_id id() const noexcept { return data_.id(); }
  [[nodiscard]] type_id parent_id() const noexcept { return data_.parent_id(); }

  constexpr void set_guid(type_guid guid) noexcept { data_.set_guid(guid); }
  void set_parent_id(type_id parent_id) { data_.set_parent_id(parent_id); }
  [[nodiscard]] error set_static_data(const type_data* fallback) noexcept {
    if (fallback == nullptr || !fallback->valid() || fallback->id() != data_.id()) {
      return {error_domain::meta, errc::invalid_type};
    }

    if (data_.name() && fallback->name() && data_.name() != fallback->name()) {
      return {error_domain::meta, errc::inconsistent_type};
    }
    if (!data_.name()) {
      data_.set_name(fallback->name());
    }

    if (data_.guid() && fallback->guid() && data_.guid() != fallback->guid()) {
      return {error_domain::meta, errc::inconsistent_type};
    }
    if (!data_.guid()) {
      data_.set_guid(fallback->guid());
    }

    data_.set_static_data(*fallback);
    return {};
  }
  void set_parent_cast(parent_cast_ops cast) { data_.set_parent_cast(cast); }

  [[nodiscard]] std::size_t property_count() const noexcept { return data_.property_count(); }
  [[nodiscard]] const property_data* property_at(std::size_t index) const noexcept {
    return data_.property_at(index);
  }
  [[nodiscard]] const property_data* find_property_ptr(string_name name) const noexcept {
    return data_.find_property_ptr(name);
  }
  [[nodiscard]] error set_property_metadata(string_name name, metadata_key key,
                                            metadata_value value) {
    property_data* property = data_.mutable_find_property_ptr(name);
    if (property == nullptr || !key) {
      return {error_domain::meta, errc::invalid_field};
    }
    property->set_metadata(key, std::move(value));
    return {};
  }
  [[nodiscard]] error apply_property_schema_entry(
      string_name name, registration::property_schema_entry entry) {
    property_data* property = data_.mutable_find_property_ptr(name);
    if (property == nullptr) {
      return {error_domain::meta, errc::invalid_field};
    }
    property->apply_schema_entry(std::move(entry));
    return {};
  }
  void add_property(property_data property) { data_.add_property(std::move(property)); }

  [[nodiscard]] error set_method_metadata(string_name name, std::size_t overload_index,
                                          metadata_key key, metadata_value value) {
    method_set_data* method_set = data_.mutable_find_method_ptr(name);
    method_data* method =
        method_set != nullptr ? method_set->mutable_overload_at(overload_index) : nullptr;
    if (method == nullptr || !key) {
      return {error_domain::meta, errc::invalid_field};
    }
    method->set_metadata(key, std::move(value));
    return {};
  }
  [[nodiscard]] error add_method_default_argument(string_name name, std::size_t overload_index,
                                                  any value) {
    method_set_data* method_set = data_.mutable_find_method_ptr(name);
    method_data* method =
        method_set != nullptr ? method_set->mutable_overload_at(overload_index) : nullptr;
    if (method == nullptr || !method->add_default_argument(std::move(value))) {
      return {error_domain::meta, errc::invalid_field};
    }
    return {};
  }
  [[nodiscard]] error add_method_parameter_name(string_name method_name,
                                                std::size_t overload_index, string_name name) {
    method_set_data* method_set = data_.mutable_find_method_ptr(method_name);
    method_data* method =
        method_set != nullptr ? method_set->mutable_overload_at(overload_index) : nullptr;
    if (method == nullptr || !method->add_parameter_name(name)) {
      return {error_domain::meta, errc::invalid_field};
    }
    return {};
  }
  [[nodiscard]] std::optional<std::size_t> add_method(method_data method) {
    return data_.add_method(std::move(method));
  }

  [[nodiscard]] error add_nested_type(nested_type_data nested_type) {
    return data_.add_nested_type(std::move(nested_type));
  }

  [[nodiscard]] const enum_value_data* find_enum_value_ptr(string_name name) const noexcept {
    return data_.find_enum_value_ptr(name);
  }
  [[nodiscard]] error set_enum_value_metadata(string_name name, metadata_key key,
                                              metadata_value value) {
    enum_value_data* enum_value = data_.mutable_find_enum_value_ptr(name);
    if (enum_value == nullptr || !key) {
      return {error_domain::meta, errc::invalid_type};
    }
    enum_value->set_metadata(key, std::move(value));
    return {};
  }
  void add_enum_value(enum_value_data value) { data_.add_enum_value(std::move(value)); }

  [[nodiscard]] bool contains_capability(capability_key key) const noexcept {
    return data_.contains_capability(key);
  }
  [[nodiscard]] error add_capability(capability_key key, const void* capability) {
    return data_.add_capability(key, capability);
  }
  [[nodiscard]] error add_owned_capability(capability_key key,
                                           memory::unique_ptr<owned_capability_base> capability) {
    return data_.add_owned_capability(key, std::move(capability));
  }

  [[nodiscard]] extension_data* mutable_find_extension(extension_key key) noexcept {
    return data_.mutable_find_extension(key);
  }
  [[nodiscard]] error add_extension(extension_data extension) {
    return data_.add_extension(std::move(extension));
  }

  /** @brief 校验不依赖 registry 状态的记录结构。 */
  [[nodiscard]] error validate() const noexcept;

  [[nodiscard]] const type_data& data() const noexcept { return data_; }
  [[nodiscard]] type_data release() && noexcept { return std::move(data_); }

private:
  type_data data_{};
};

} // namespace caors::meta::detail

#endif
