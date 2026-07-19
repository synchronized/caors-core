// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/type.h"
#include "caors/meta/detail/type_data.h"
#include "caors/meta/detail/type_descriptor.h"
#include "caors/meta/enum_value.h"
#include "caors/meta/metadata.h"
#include "caors/meta/method.h"
#include "caors/meta/property.h"
#include "caors/meta/registry.h"

namespace caors::meta {
namespace {

[[nodiscard]] const metadata_map& empty_metadata_map() noexcept {
  static const metadata_map empty{
      typename metadata_map::allocator_type{&memory::library_resource()}};
  return empty;
}

} // namespace

bool type::valid() const noexcept { return descriptor_ != nullptr && descriptor_->valid(); }

bool type::registered() const noexcept {
  const detail::type_data* data = registered_data();
  return data != nullptr && data->registered();
}

type_id type::id() const noexcept {
  const detail::type_descriptor* desc = descriptor();
  if (desc != nullptr) {
    return desc->type;
  }

  const detail::type_data* data = registered_data();
  return data != nullptr ? data->id() : type_id::invalid();
}

core::string_name type::name() const noexcept {
  const detail::type_data* data = registered_data();
  if (data != nullptr && data->name()) {
    return data->name();
  }

  const detail::type_descriptor* desc = descriptor();
  return desc != nullptr ? desc->name : core::string_name{};
}

core::type_guid type::guid() const noexcept {
  const detail::type_data* data = registered_data();
  return data != nullptr ? data->guid() : core::type_guid::nil();
}

type_id type::parent_id() const noexcept {
  const detail::type_data* data = registered_data();
  return data != nullptr ? data->parent_id() : type_id::invalid();
}

type type::parent() const noexcept {
  if (registry_ == nullptr || !parent_id()) {
    return {};
  }

  return registry_->find_type(parent_id());
}

type_kind type::kind() const noexcept {
  const detail::type_descriptor* desc = descriptor();
  return desc != nullptr ? detail::descriptor_kind(*desc) : type_kind::value;
}

wrapper_semantics type::wrapper() const noexcept {
  const detail::type_descriptor* desc = descriptor();
  return desc != nullptr ? detail::descriptor_wrapper_semantics(*desc) : wrapper_semantics::none;
}

bool type::indirect() const noexcept {
  const detail::type_descriptor* desc = descriptor();
  return desc != nullptr && detail::descriptor_indirect(*desc);
}

bool type::nullable() const noexcept {
  const detail::type_descriptor* desc = descriptor();
  return desc != nullptr && detail::descriptor_nullable(*desc);
}

const metadata_map& type::metadata() const noexcept {
  if (registry_ == nullptr || !id()) {
    return empty_metadata_map();
  }

  const type_metadata* data = registry_->find_extension_ptr<type_metadata>(id());
  return data != nullptr ? data->values : empty_metadata_map();
}

const metadata_value* type::find_metadata(metadata_key key) const noexcept {
  return meta::find_metadata(metadata(), key);
}

type type::get_value_type() const noexcept {
  const detail::type_descriptor* desc = descriptor();
  if (desc == nullptr) {
    return {};
  }

  switch (detail::descriptor_kind(*desc)) {
  case type_kind::value:
  case type_kind::enumeration:
    return *this;
  case type_kind::raw_pointer:
  case type_kind::wrapper: {
    if (const auto* indirect_ops = get_raw_ops<meta::indirect_ops>()) {
      return indirect_ops->get_value_type != nullptr ? indirect_ops->get_value_type()
                                                     : meta::type{};
    }
    break;
  }
  case type_kind::sequence: {
    if (const auto* sequence_ops = get_raw_ops<meta::sequence_ops>()) {
      return sequence_ops->get_value_type != nullptr ? sequence_ops->get_value_type()
                                                     : meta::type{};
    }
    break;
  }
  case type_kind::set: {
    if (const auto* set_ops = get_raw_ops<meta::set_ops>()) {
      return set_ops->get_value_type != nullptr ? set_ops->get_value_type() : meta::type{};
    }
    break;
  }
  case type_kind::associative: {
    if (const auto* associative_ops = get_raw_ops<meta::associative_ops>()) {
      return associative_ops->get_value_type != nullptr ? associative_ops->get_value_type()
                                                        : meta::type{};
    }
    break;
  }
  case type_kind::pair: {
    if (const auto* pair_ops = get_raw_ops<meta::pair_ops>()) {
      return pair_ops->get_value_type != nullptr ? pair_ops->get_value_type() : meta::type{};
    }
    break;
  }
  case type_kind::tuple:
  case type_kind::variant:
    break;
  }

  return {};
}

type type::get_key_type() const noexcept {
  const detail::type_descriptor* desc = descriptor();
  if (desc == nullptr) {
    return {};
  }

  switch (detail::descriptor_kind(*desc)) {
  case type_kind::associative: {
    if (const auto* associative_ops = get_raw_ops<meta::associative_ops>()) {
      return associative_ops->get_key_type != nullptr ? associative_ops->get_key_type()
                                                      : meta::type{};
    }
    break;
  }
  case type_kind::pair: {
    if (const auto* pair_ops = get_raw_ops<meta::pair_ops>()) {
      return pair_ops->get_key_type != nullptr ? pair_ops->get_key_type() : meta::type{};
    }
    break;
  }
  case type_kind::value:
  case type_kind::enumeration:
  case type_kind::raw_pointer:
  case type_kind::wrapper:
  case type_kind::sequence:
  case type_kind::set:
  case type_kind::tuple:
  case type_kind::variant:
    break;
  }

  return {};
}

std::size_t type::property_count() const noexcept {
  const detail::type_data* data = registered_data();
  return data != nullptr ? data->property_count() : 0;
}

property type::property_at(std::size_t index) const {
  const detail::type_data* data = registered_data();
  return data != nullptr ? property{data->property_at(index)} : property{};
}

property type::find_property(core::string_name property_name, member_search search) const {
  if (!property_name) {
    return {};
  }

  const detail::type_data* data = registered_data();
  if (data != nullptr) {
    property found{data->find_property_ptr(property_name)};
    if (found || search == member_search::local_only) {
      return found;
    }
  }

  type base = parent();
  while (base) {
    const detail::type_data* base_data = base.registered_data();
    if (base_data != nullptr) {
      property found{base_data->find_property_ptr(property_name)};
      if (found) {
        return found;
      }
    }

    base = base.parent();
  }

  return {};
}

property type::find_property(std::string_view property_name, member_search search) const {
  const std::optional<core::string_name> interned_name = core::string_name::find(property_name);
  if (!interned_name.has_value()) {
    return {};
  }

  return find_property(*interned_name, search);
}

std::size_t type::method_count() const noexcept {
  const detail::type_data* data = registered_data();
  return data != nullptr ? data->method_count() : 0;
}

method_set type::method_at(std::size_t index) const {
  const detail::type_data* data = registered_data();
  return data != nullptr ? method_set{data->method_at(index)} : method_set{};
}

method_set type::find_method(core::string_name method_name, member_search search) const {
  if (!method_name) {
    return {};
  }

  const detail::type_data* data = registered_data();
  if (data != nullptr) {
    method_set found{data->find_method_ptr(method_name)};
    if (found || search == member_search::local_only) {
      return found;
    }
  }

  type base = parent();
  while (base) {
    const detail::type_data* base_data = base.registered_data();
    if (base_data != nullptr) {
      method_set found{base_data->find_method_ptr(method_name)};
      if (found) {
        return found;
      }
    }

    base = base.parent();
  }

  return {};
}

method_set type::find_method(std::string_view method_name, member_search search) const {
  const std::optional<core::string_name> interned_name = core::string_name::find(method_name);
  if (!interned_name.has_value()) {
    return {};
  }

  return find_method(*interned_name, search);
}

std::size_t type::nested_count() const noexcept {
  const detail::type_data* data = registered_data();
  return data != nullptr ? data->nested_type_count() : 0;
}

core::string_name type::nested_name_at(std::size_t index) const noexcept {
  const detail::type_data* data = registered_data();
  const detail::nested_type_data* nested_type =
      data != nullptr ? data->nested_type_at(index) : nullptr;
  return nested_type != nullptr ? nested_type->name : core::string_name{};
}

type type::nested_at(std::size_t index) const {
  if (registry_ == nullptr) {
    return {};
  }

  const detail::type_data* data = registered_data();
  const detail::nested_type_data* nested_type =
      data != nullptr ? data->nested_type_at(index) : nullptr;
  return nested_type != nullptr ? registry_->find_type(nested_type->nested_type) : type{};
}

type type::find_nested(core::string_name nested_name) const {
  if (registry_ == nullptr || !nested_name) {
    return {};
  }

  const detail::type_data* data = registered_data();
  const detail::nested_type_data* nested_type =
      data != nullptr ? data->find_nested_type_ptr(nested_name) : nullptr;
  return nested_type != nullptr ? registry_->find_type(nested_type->nested_type) : type{};
}

type type::find_nested(std::string_view nested_name) const {
  const std::optional<core::string_name> interned_name = core::string_name::find(nested_name);
  if (!interned_name.has_value()) {
    return {};
  }

  return find_nested(*interned_name);
}

const detail::type_data* type::registered_data() const noexcept {
  return data_ != nullptr && data_->registered() ? data_ : nullptr;
}

const detail::type_descriptor* type::descriptor() const noexcept {
  if (data_ != nullptr && data_->descriptor() != nullptr) {
    return data_->descriptor();
  }

  return descriptor_;
}

namespace detail {

type make_type_view(type_id id, const type_descriptor* descriptor) noexcept {
  return make_type_view(get_registry(), id, descriptor);
}

type make_type_view(registry& registry, type_id id, const type_descriptor* descriptor) noexcept {
  if (id) {
    meta::type registered = registry.find_type(id);
    if (registered) {
      return registered;
    }
  }

  if (descriptor == nullptr || !descriptor->valid()) {
    return {};
  }

  return meta::type{&registry, nullptr, descriptor};
}

std::size_t enum_value_count(const type& source) noexcept {
  const type_data* data = source.registered_data();
  return data != nullptr ? data->enum_value_count() : 0;
}

enum_value_view enum_value_at(const type& source, std::size_t index) {
  const type_data* data = source.registered_data();
  return data != nullptr ? make_enum_value_view(data->enum_value_at(index)) : enum_value_view{};
}

enum_value_view find_enum_value(const type& source, core::string_name enum_value_name) {
  if (!enum_value_name) {
    return {};
  }

  const type_data* data = source.registered_data();
  return data != nullptr ? make_enum_value_view(data->find_enum_value_ptr(enum_value_name))
                         : enum_value_view{};
}

enum_value_view find_enum_value(const type& source, std::string_view enum_value_name) {
  const std::optional<core::string_name> interned_name = core::string_name::find(enum_value_name);
  if (!interned_name.has_value()) {
    return {};
  }

  return find_enum_value(source, *interned_name);
}

enum_value_view find_enum_value_by_i64(const type& source, std::int64_t value) noexcept {
  const type_data* data = source.registered_data();
  return data != nullptr ? make_enum_value_view(data->find_enum_value_ptr(value))
                         : enum_value_view{};
}

enum_value_view find_enum_value_by_u64(const type& source, std::uint64_t value) noexcept {
  const type_data* data = source.registered_data();
  return data != nullptr ? make_enum_value_view(data->find_enum_value_ptr(value))
                         : enum_value_view{};
}

} // namespace detail

} // namespace caors::meta
