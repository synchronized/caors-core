// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/detail/type_data.h"

#include "caors/core/containers.h"
#include "caors/memory/resource.h"
#include "caors/meta/detail/enum_value_data.h"
#include "caors/meta/detail/method_data.h"
#include "caors/meta/detail/property_data.h"

#include <utility>

namespace caors::meta::detail {

struct type_details {
  type_id parent_id{};
  parent_cast_ops parent_cast{};
  vector<property_data> properties{};
  vector<method_set_data> methods{};
  vector<nested_type_data> nested_types{};
  vector<enum_value_data> enum_values{};
  vector<capability_data> capabilities{};
  vector<extension_data> extensions{};
};

type_data::type_data(type_id id, string_name name, type_guid guid, type_id parent_id)
    : id_(id), name_(name), guid_(guid), registered_(true),
      details_(memory::make<type_details>(&memory::library_resource())) {
  details_->parent_id = parent_id;
}

type_data::type_data(const type_descriptor& descriptor, bool registered) noexcept
    : id_(descriptor.type), name_(descriptor.name), value_(descriptor.value),
      semantics_(descriptor.semantics), builtin_(descriptor.builtin),
      static_capabilities_(descriptor.static_capabilities),
      static_capability_count_(descriptor.static_capability_count), registered_(registered) {}

type_data::type_data(type_data&& other) noexcept
    : id_(other.id_), name_(other.name_), guid_(other.guid_), value_(other.value_),
      semantics_(other.semantics_), builtin_(other.builtin_),
      static_capabilities_(other.static_capabilities_),
      static_capability_count_(other.static_capability_count_), registered_(other.registered_),
      details_(std::exchange(other.details_, nullptr)) {}

type_data& type_data::operator=(type_data&& other) noexcept {
  if (this != &other) {
    if (details_ != nullptr) {
      memory::destroy(details_, &memory::library_resource());
    }
    id_ = other.id_;
    name_ = other.name_;
    guid_ = other.guid_;
    value_ = other.value_;
    semantics_ = other.semantics_;
    builtin_ = other.builtin_;
    static_capabilities_ = other.static_capabilities_;
    static_capability_count_ = other.static_capability_count_;
    registered_ = other.registered_;
    details_ = std::exchange(other.details_, nullptr);
  }
  return *this;
}

type_data::~type_data() {
  if (details_ != nullptr) {
    memory::destroy(details_, &memory::library_resource());
  }
}

void type_data::set_static_data(const type_descriptor& descriptor) noexcept {
  value_ = descriptor.value;
  semantics_ = descriptor.semantics;
  builtin_ = descriptor.builtin;
  static_capabilities_ = descriptor.static_capabilities;
  static_capability_count_ = descriptor.static_capability_count;
}

void type_data::set_static_data(const type_data& source) noexcept {
  value_ = source.value_;
  semantics_ = source.semantics_;
  builtin_ = source.builtin_;
  static_capabilities_ = source.static_capabilities_;
  static_capability_count_ = source.static_capability_count_;
}

type_details& type_data::ensure_details() {
  if (details_ == nullptr) {
    details_ = memory::make<type_details>(&memory::library_resource());
  }
  return *details_;
}

type_id type_data::parent_id() const noexcept {
  const type_details* value = details();
  return value != nullptr ? value->parent_id : type_id::invalid();
}

const parent_cast_ops* type_data::parent_cast() const noexcept {
  const type_details* value = details();
  return value != nullptr && value->parent_cast.valid() ? &value->parent_cast : nullptr;
}

void type_data::set_parent_id(type_id parent_id) { ensure_details().parent_id = parent_id; }

void type_data::set_parent_cast(parent_cast_ops cast) { ensure_details().parent_cast = cast; }

bool type_data::members_valid() const noexcept {
  const type_details* value = details();
  if (value == nullptr) {
    return true;
  }

  for (const property_data& property : value->properties) {
    if (!property.valid()) {
      return false;
    }
  }

  for (const method_set_data& method_set : value->methods) {
    if (!method_set.valid()) {
      return false;
    }
  }

  for (const nested_type_data& nested_type : value->nested_types) {
    if (!nested_type.valid()) {
      return false;
    }
  }

  for (const enum_value_data& enum_value : value->enum_values) {
    if (!enum_value.valid()) {
      return false;
    }
  }

  for (const capability_data& capability : value->capabilities) {
    if (!capability.valid()) {
      return false;
    }
  }

  for (const extension_data& extension : value->extensions) {
    if (!extension.valid()) {
      return false;
    }
  }

  return true;
}

std::size_t type_data::property_count() const noexcept {
  const type_details* value = details();
  return value != nullptr ? value->properties.size() : 0;
}

const property_data* type_data::property_at(std::size_t index) const noexcept {
  const type_details* value = details();
  return value != nullptr && index < value->properties.size() ? &value->properties[index] : nullptr;
}

const property_data* type_data::find_property_ptr(core::string_name name) const noexcept {
  if (!name) {
    return nullptr;
  }

  const type_details* value = details();
  if (value == nullptr) {
    return nullptr;
  }
  for (const property_data& property : value->properties) {
    if (property.name() == name) {
      return &property;
    }
  }

  return nullptr;
}

property_data* type_data::mutable_find_property_ptr(core::string_name name) noexcept {
  if (!name) {
    return nullptr;
  }

  for (property_data& property : ensure_details().properties) {
    if (property.name() == name) {
      return &property;
    }
  }

  return nullptr;
}

void type_data::add_property(property_data property) {
  ensure_details().properties.push_back(std::move(property));
}

std::size_t type_data::method_count() const noexcept {
  const type_details* value = details();
  return value != nullptr ? value->methods.size() : 0;
}

const method_set_data* type_data::method_at(std::size_t index) const noexcept {
  const type_details* value = details();
  return value != nullptr && index < value->methods.size() ? &value->methods[index] : nullptr;
}

const method_set_data* type_data::find_method_ptr(core::string_name name) const noexcept {
  if (!name) {
    return nullptr;
  }

  const type_details* value = details();
  if (value == nullptr) {
    return nullptr;
  }
  for (const method_set_data& method_set : value->methods) {
    if (method_set.name() == name) {
      return &method_set;
    }
  }

  return nullptr;
}

method_set_data* type_data::mutable_find_method_ptr(core::string_name name) noexcept {
  if (!name) {
    return nullptr;
  }

  for (method_set_data& method_set : ensure_details().methods) {
    if (method_set.name() == name) {
      return &method_set;
    }
  }

  return nullptr;
}

std::optional<std::size_t> type_data::add_method(method_data method) {
  if (!method.valid() || !method.name()) {
    return std::nullopt;
  }

  method_set_data* existing = mutable_find_method_ptr(method.name());
  if (existing != nullptr) {
    return existing->add_overload(std::move(method));
  }

  method_set_data method_set{method.name()};
  std::optional<std::size_t> overload_index = method_set.add_overload(std::move(method));
  if (!overload_index.has_value()) {
    return std::nullopt;
  }

  ensure_details().methods.push_back(std::move(method_set));
  return overload_index;
}

std::size_t type_data::nested_type_count() const noexcept {
  const type_details* value = details();
  return value != nullptr ? value->nested_types.size() : 0;
}

const nested_type_data* type_data::nested_type_at(std::size_t index) const noexcept {
  const type_details* value = details();
  return value != nullptr && index < value->nested_types.size() ? &value->nested_types[index]
                                                                : nullptr;
}

const nested_type_data* type_data::find_nested_type_ptr(core::string_name name) const noexcept {
  if (!name) {
    return nullptr;
  }

  const type_details* value = details();
  if (value == nullptr) {
    return nullptr;
  }
  for (const nested_type_data& nested_type : value->nested_types) {
    if (nested_type.name == name) {
      return &nested_type;
    }
  }

  return nullptr;
}

bool type_data::contains_nested_type(type_id id) const noexcept {
  if (!id) {
    return false;
  }

  const type_details* value = details();
  if (value == nullptr) {
    return false;
  }
  for (const nested_type_data& nested_type : value->nested_types) {
    if (nested_type.nested_type == id) {
      return true;
    }
  }

  return false;
}

error type_data::add_nested_type(nested_type_data nested_type) {
  if (!nested_type.valid()) {
    return {error_domain::meta, errc::invalid_type};
  }

  if (find_nested_type_ptr(nested_type.name) != nullptr) {
    return {error_domain::meta, errc::duplicate_name};
  }

  if (contains_nested_type(nested_type.nested_type)) {
    return {error_domain::meta, errc::duplicate_id};
  }

  ensure_details().nested_types.push_back(std::move(nested_type));
  return {};
}

std::size_t type_data::enum_value_count() const noexcept {
  const type_details* value = details();
  return value != nullptr ? value->enum_values.size() : 0;
}

const enum_value_data* type_data::enum_value_at(std::size_t index) const noexcept {
  const type_details* value = details();
  return value != nullptr && index < value->enum_values.size() ? &value->enum_values[index]
                                                               : nullptr;
}

const enum_value_data* type_data::find_enum_value_ptr(core::string_name name) const noexcept {
  if (!name) {
    return nullptr;
  }

  const type_details* details_value = details();
  if (details_value == nullptr) {
    return nullptr;
  }
  for (const enum_value_data& value : details_value->enum_values) {
    if (value.name() == name) {
      return &value;
    }
  }

  return nullptr;
}

enum_value_data* type_data::mutable_find_enum_value_ptr(core::string_name name) noexcept {
  if (!name) {
    return nullptr;
  }

  for (enum_value_data& value : ensure_details().enum_values) {
    if (value.name() == name) {
      return &value;
    }
  }

  return nullptr;
}

const enum_value_data* type_data::find_enum_value_ptr(std::int64_t value) const noexcept {
  const type_details* details_value = details();
  if (details_value == nullptr) {
    return nullptr;
  }
  for (const enum_value_data& enum_value : details_value->enum_values) {
    if (enum_value.to_i64() == value) {
      return &enum_value;
    }
  }

  return nullptr;
}

const enum_value_data* type_data::find_enum_value_ptr(std::uint64_t value) const noexcept {
  const type_details* details_value = details();
  if (details_value == nullptr) {
    return nullptr;
  }
  for (const enum_value_data& enum_value : details_value->enum_values) {
    if (enum_value.to_u64() == value) {
      return &enum_value;
    }
  }

  return nullptr;
}

void type_data::add_enum_value(enum_value_data value) {
  ensure_details().enum_values.push_back(std::move(value));
}

const void* type_data::find_capability(capability_key key) const noexcept {
  if (key == nullptr) {
    return nullptr;
  }

  const type_details* value = details();
  if (value == nullptr) {
    return nullptr;
  }
  for (const capability_data& capability : value->capabilities) {
    if (capability.key == key) {
      return capability.data;
    }
  }

  return nullptr;
}

const void* type_data::find_static_capability(capability_key key) const noexcept {
  if (key == nullptr) {
    return nullptr;
  }

  if (key == capability_key_of<value_storage_ops>()) {
    return value_.valid() ? &value_ : nullptr;
  }

#define CAORS_FIND_BUILTIN_OPS(Ops)                                                                \
  if (key == capability_key_of<Ops>()) {                                                           \
    const auto* value = std::get_if<const Ops*>(&builtin_);                                        \
    return value != nullptr ? *value : nullptr;                                                    \
  }

  CAORS_FIND_BUILTIN_OPS(scalar_ops)
  CAORS_FIND_BUILTIN_OPS(enum_ops)
  CAORS_FIND_BUILTIN_OPS(indirect_ops)
  CAORS_FIND_BUILTIN_OPS(sequence_ops)
  CAORS_FIND_BUILTIN_OPS(set_ops)
  CAORS_FIND_BUILTIN_OPS(associative_ops)
  CAORS_FIND_BUILTIN_OPS(pair_ops)
  CAORS_FIND_BUILTIN_OPS(tuple_ops)
  CAORS_FIND_BUILTIN_OPS(variant_ops)

#undef CAORS_FIND_BUILTIN_OPS

  for (std::size_t index = 0; index < static_capability_count_; ++index) {
    const capability_entry& entry = static_capabilities_[index];
    if (entry.key == key && entry.data != nullptr) {
      return entry.data;
    }
  }

  return nullptr;
}

bool type_data::contains_capability(capability_key key) const noexcept {
  return find_capability(key) != nullptr;
}

std::size_t type_data::capability_count() const noexcept {
  const type_details* value = details();
  return value != nullptr ? value->capabilities.size() : 0;
}

error type_data::add_capability(capability_key key, const void* capability) {
  if (key == nullptr || capability == nullptr) {
    return {error_domain::meta, errc::invalid_argument};
  }

  if (contains_capability(key)) {
    return {error_domain::meta, errc::duplicate_component};
  }

  capability_data data{};
  data.key = key;
  data.data = capability;
  ensure_details().capabilities.push_back(std::move(data));
  return {};
}

error type_data::add_owned_capability(capability_key key,
                                      memory::unique_ptr<owned_capability_base> capability) {
  if (key == nullptr || capability == nullptr || capability->data() == nullptr) {
    return {error_domain::meta, errc::invalid_argument};
  }

  if (contains_capability(key)) {
    return {error_domain::meta, errc::duplicate_component};
  }

  capability_data data{};
  data.key = key;
  data.data = capability->data();
  data.owned = std::move(capability);
  ensure_details().capabilities.push_back(std::move(data));
  return {};
}

bool type_data::contains_extension(extension_key key) const noexcept {
  return find_extension(key) != nullptr;
}

const extension_data* type_data::find_extension(extension_key key) const noexcept {
  if (key == nullptr) {
    return nullptr;
  }

  const type_details* value = details();
  if (value == nullptr) {
    return nullptr;
  }
  for (const extension_data& extension : value->extensions) {
    if (extension.key == key) {
      return &extension;
    }
  }

  return nullptr;
}

extension_data* type_data::mutable_find_extension(extension_key key) noexcept {
  if (key == nullptr) {
    return nullptr;
  }

  for (extension_data& extension : ensure_details().extensions) {
    if (extension.key == key) {
      return &extension;
    }
  }

  return nullptr;
}

error type_data::add_extension(extension_data extension) {
  if (!extension.valid()) {
    return {error_domain::meta, errc::invalid_argument};
  }

  if (contains_extension(extension.key)) {
    return {error_domain::meta, errc::duplicate_component};
  }

  ensure_details().extensions.push_back(std::move(extension));
  return {};
}

} // namespace caors::meta::detail
