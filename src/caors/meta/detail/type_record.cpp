// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/detail/type_record.h"

#include "caors/core/containers.h"
#include "caors/memory/resource.h"
#include "caors/meta/detail/enum_value_record.h"
#include "caors/meta/detail/method_record.h"
#include "caors/meta/detail/property_record.h"
#include "caors/meta/metadata.h"

#include <utility>

namespace caors::meta::detail {

struct type_details {
  type_id parent_id{};
  parent_cast_ops parent_cast{};
  memory::unique_ptr<meta::metadata_map> metadata{};
  vector<property_record> properties{};
  vector<method_set_record> methods{};
  vector<nested_type_data> nested_types{};
  vector<enum_value_record> enum_values{};
  vector<capability_data> capabilities{};
};

type_record::type_record(type_id id, string_name name, type_guid guid, type_id parent_id)
    : id_(id), name_(name), guid_(guid), registered_(true),
      details_(memory::make<type_details>(&memory::library_resource())) {
  details_->parent_id = parent_id;
}

type_record::type_record(type_id id, string_name name, type_guid guid, value_storage_ops value,
                         type_semantics semantics, builtin_ops builtin,
                         const capability_entry* static_capabilities,
                         std::size_t static_capability_count, bool registered) noexcept
    : id_(id), name_(name), guid_(guid), value_(value), semantics_(semantics), builtin_(builtin),
      static_capabilities_(static_capabilities), static_capability_count_(static_capability_count),
      registered_(registered) {}

type_record::type_record(type_record&& other) noexcept
    : id_(other.id_), name_(other.name_), guid_(other.guid_), value_(other.value_),
      semantics_(other.semantics_), builtin_(other.builtin_),
      static_capabilities_(other.static_capabilities_),
      static_capability_count_(other.static_capability_count_), registered_(other.registered_),
      details_(std::exchange(other.details_, nullptr)) {}

type_record& type_record::operator=(type_record&& other) noexcept {
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

type_record::~type_record() {
  if (details_ != nullptr) {
    memory::destroy(details_, &memory::library_resource());
  }
}

bool type_record::static_model_matches(const type_record& other) const noexcept {
  if (!valid() || !other.valid() || id_ != other.id_ ||
      value_.layout.size != other.value_.layout.size ||
      value_.layout.alignment != other.value_.layout.alignment ||
      value_.traits.trivially_copyable != other.value_.traits.trivially_copyable ||
      value_.traits.trivially_destructible != other.value_.traits.trivially_destructible ||
      value_.traits.trivially_default_constructible !=
          other.value_.traits.trivially_default_constructible ||
      (value_.ops.default_construct != nullptr) !=
          (other.value_.ops.default_construct != nullptr) ||
      (value_.ops.clone != nullptr) != (other.value_.ops.clone != nullptr) ||
      (value_.ops.destroy != nullptr) != (other.value_.ops.destroy != nullptr) ||
      (value_.ops.assign != nullptr) != (other.value_.ops.assign != nullptr) ||
      (value_.ops.resource_construct != nullptr) !=
          (other.value_.ops.resource_construct != nullptr) ||
      (value_.ops.resource_destroy != nullptr) != (other.value_.ops.resource_destroy != nullptr) ||
      semantics_.kind != other.semantics_.kind || semantics_.wrapper != other.semantics_.wrapper ||
      semantics_.key_type.id != other.semantics_.key_type.id ||
      (semantics_.key_type.fallback != nullptr) !=
          (other.semantics_.key_type.fallback != nullptr) ||
      semantics_.value_type.id != other.semantics_.value_type.id ||
      (semantics_.value_type.fallback != nullptr) !=
          (other.semantics_.value_type.fallback != nullptr) ||
      semantics_.indirect != other.semantics_.indirect ||
      semantics_.nullable != other.semantics_.nullable ||
      builtin_.index() != other.builtin_.index() ||
      static_capability_count_ != other.static_capability_count_) {
    return false;
  }

  if (static_capability_count_ > 0 &&
      (static_capabilities_ == nullptr || other.static_capabilities_ == nullptr)) {
    return false;
  }

  for (std::size_t index = 0; index < static_capability_count_; ++index) {
    const capability_entry& lhs = static_capabilities_[index];
    const capability_entry& rhs = other.static_capabilities_[index];
    if (lhs.key != rhs.key || (lhs.data != nullptr) != (rhs.data != nullptr)) {
      return false;
    }
  }

  return true;
}

void type_record::set_static_data(const type_record& source) noexcept {
  value_ = source.value_;
  semantics_ = source.semantics_;
  builtin_ = source.builtin_;
  static_capabilities_ = source.static_capabilities_;
  static_capability_count_ = source.static_capability_count_;
}

type_details& type_record::ensure_details() {
  if (details_ == nullptr) {
    details_ = memory::make<type_details>(&memory::library_resource());
  }
  return *details_;
}

type_id type_record::parent_id() const noexcept {
  const type_details* value = details();
  return value != nullptr ? value->parent_id : type_id::invalid();
}

const parent_cast_ops* type_record::parent_cast() const noexcept {
  const type_details* value = details();
  return value != nullptr && value->parent_cast.valid() ? &value->parent_cast : nullptr;
}

void type_record::set_parent_id(type_id parent_id) { ensure_details().parent_id = parent_id; }

void type_record::set_parent_cast(parent_cast_ops cast) { ensure_details().parent_cast = cast; }

bool type_record::members_valid() const noexcept {
  const type_details* value = details();
  if (value == nullptr) {
    return true;
  }

  for (const property_record& property : value->properties) {
    if (!property.valid()) {
      return false;
    }
  }

  for (const method_set_record& method_set : value->methods) {
    if (!method_set.valid()) {
      return false;
    }
  }

  for (const nested_type_data& nested_type : value->nested_types) {
    if (!nested_type.valid()) {
      return false;
    }
  }

  for (const enum_value_record& enum_value : value->enum_values) {
    if (!enum_value.valid()) {
      return false;
    }
  }

  for (const capability_data& capability : value->capabilities) {
    if (!capability.valid()) {
      return false;
    }
  }

  return true;
}

const meta::metadata_map& type_record::metadata() const noexcept {
  static const meta::metadata_map empty{
      typename meta::metadata_map::allocator_type{&memory::library_resource()}};

  const type_details* value = details();
  return value != nullptr && value->metadata != nullptr ? *value->metadata : empty;
}

const meta::metadata_value* type_record::find_metadata(meta::metadata_key key) const noexcept {
  const type_details* value = details();
  return value != nullptr && value->metadata != nullptr ? meta::find_metadata(*value->metadata, key)
                                                        : nullptr;
}

void type_record::set_metadata(meta::metadata_key key, meta::metadata_value value) {
  type_details& details_value = ensure_details();
  if (details_value.metadata == nullptr) {
    details_value.metadata = memory::make_unique<meta::metadata_map>(
        &memory::library_resource(), meta::make_metadata_map(&memory::library_resource()));
  }
  meta::set_metadata(*details_value.metadata, key, std::move(value));
}

std::size_t type_record::property_count() const noexcept {
  const type_details* value = details();
  return value != nullptr ? value->properties.size() : 0;
}

const property_record* type_record::property_at(std::size_t index) const noexcept {
  const type_details* value = details();
  return value != nullptr && index < value->properties.size() ? &value->properties[index] : nullptr;
}

const property_record* type_record::find_property_ptr(core::string_name name) const noexcept {
  if (!name) {
    return nullptr;
  }

  const type_details* value = details();
  if (value == nullptr) {
    return nullptr;
  }
  for (const property_record& property : value->properties) {
    if (property.name() == name) {
      return &property;
    }
  }

  return nullptr;
}

property_record* type_record::mutable_find_property_ptr(core::string_name name) noexcept {
  if (!name) {
    return nullptr;
  }

  for (property_record& property : ensure_details().properties) {
    if (property.name() == name) {
      return &property;
    }
  }

  return nullptr;
}

void type_record::add_property(property_record property) {
  ensure_details().properties.push_back(std::move(property));
}

std::size_t type_record::method_count() const noexcept {
  const type_details* value = details();
  return value != nullptr ? value->methods.size() : 0;
}

const method_set_record* type_record::method_at(std::size_t index) const noexcept {
  const type_details* value = details();
  return value != nullptr && index < value->methods.size() ? &value->methods[index] : nullptr;
}

const method_set_record* type_record::find_method_ptr(core::string_name name) const noexcept {
  if (!name) {
    return nullptr;
  }

  const type_details* value = details();
  if (value == nullptr) {
    return nullptr;
  }
  for (const method_set_record& method_set : value->methods) {
    if (method_set.name() == name) {
      return &method_set;
    }
  }

  return nullptr;
}

method_set_record* type_record::mutable_find_method_ptr(core::string_name name) noexcept {
  if (!name) {
    return nullptr;
  }

  for (method_set_record& method_set : ensure_details().methods) {
    if (method_set.name() == name) {
      return &method_set;
    }
  }

  return nullptr;
}

std::optional<std::size_t> type_record::add_method(method_record method) {
  if (!method.valid() || !method.name()) {
    return std::nullopt;
  }

  method_set_record* existing = mutable_find_method_ptr(method.name());
  if (existing != nullptr) {
    return existing->add_overload(std::move(method));
  }

  method_set_record method_set{method.name()};
  std::optional<std::size_t> overload_index = method_set.add_overload(std::move(method));
  if (!overload_index.has_value()) {
    return std::nullopt;
  }

  ensure_details().methods.push_back(std::move(method_set));
  return overload_index;
}

std::size_t type_record::nested_type_count() const noexcept {
  const type_details* value = details();
  return value != nullptr ? value->nested_types.size() : 0;
}

const nested_type_data* type_record::nested_type_at(std::size_t index) const noexcept {
  const type_details* value = details();
  return value != nullptr && index < value->nested_types.size() ? &value->nested_types[index]
                                                                : nullptr;
}

const nested_type_data* type_record::find_nested_type_ptr(core::string_name name) const noexcept {
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

bool type_record::contains_nested_type(type_id id) const noexcept {
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

error type_record::add_nested_type(nested_type_data nested_type) {
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

std::size_t type_record::enum_value_count() const noexcept {
  const type_details* value = details();
  return value != nullptr ? value->enum_values.size() : 0;
}

const enum_value_record* type_record::enum_value_at(std::size_t index) const noexcept {
  const type_details* value = details();
  return value != nullptr && index < value->enum_values.size() ? &value->enum_values[index]
                                                               : nullptr;
}

const enum_value_record* type_record::find_enum_value_ptr(core::string_name name) const noexcept {
  if (!name) {
    return nullptr;
  }

  const type_details* details_value = details();
  if (details_value == nullptr) {
    return nullptr;
  }
  for (const enum_value_record& value : details_value->enum_values) {
    if (value.name() == name) {
      return &value;
    }
  }

  return nullptr;
}

enum_value_record* type_record::mutable_find_enum_value_ptr(core::string_name name) noexcept {
  if (!name) {
    return nullptr;
  }

  for (enum_value_record& value : ensure_details().enum_values) {
    if (value.name() == name) {
      return &value;
    }
  }

  return nullptr;
}

const enum_value_record* type_record::find_enum_value_ptr(std::int64_t value) const noexcept {
  const type_details* details_value = details();
  if (details_value == nullptr) {
    return nullptr;
  }
  for (const enum_value_record& enum_value : details_value->enum_values) {
    if (enum_value.to_i64() == value) {
      return &enum_value;
    }
  }

  return nullptr;
}

const enum_value_record* type_record::find_enum_value_ptr(std::uint64_t value) const noexcept {
  const type_details* details_value = details();
  if (details_value == nullptr) {
    return nullptr;
  }
  for (const enum_value_record& enum_value : details_value->enum_values) {
    if (enum_value.to_u64() == value) {
      return &enum_value;
    }
  }

  return nullptr;
}

void type_record::add_enum_value(enum_value_record value) {
  ensure_details().enum_values.push_back(std::move(value));
}

const void* type_record::find_capability(capability_key key) const noexcept {
  if (!key) {
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

const void* type_record::find_static_capability(capability_key key) const noexcept {
  if (!key) {
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

bool type_record::contains_capability(capability_key key) const noexcept {
  return find_capability(key) != nullptr;
}

std::size_t type_record::capability_count() const noexcept {
  const type_details* value = details();
  return value != nullptr ? value->capabilities.size() : 0;
}

error type_record::add_capability(capability_key key, const void* capability) {
  if (!key || capability == nullptr) {
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

error type_record::add_owned_capability(capability_key key,
                                        memory::unique_ptr<owned_capability_base> capability) {
  if (!key || capability == nullptr || capability->data() == nullptr) {
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

} // namespace caors::meta::detail
