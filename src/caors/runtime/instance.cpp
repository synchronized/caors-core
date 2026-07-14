// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/runtime/instance.h"

#include "caors/object/ptr_view.h"

namespace caors::runtime {

instance::instance(meta::any_ref value) noexcept : value_(value) {}

bool instance::valid() const noexcept { return value_.valid(); }

meta::any_ref instance::value() const noexcept { return value_; }

runtime::type instance::type() const noexcept {
  return valid() ? runtime::type{value_.get_type()} : runtime::type{};
}

reflection::type instance::reflection() const noexcept { return type().reflection(); }

std::size_t instance::field_count() const noexcept { return type().field_count(); }

field instance::field_at(std::size_t index) const { return type().field_at(index); }

field instance::find_field(string_name name) const { return type().find_field(name); }

field instance::find_field(std::string_view name) const { return type().find_field(name); }

core::vector<field> instance::fields(memory_resource* resource) const {
  core::vector<field> result{allocator<field>{resource}};
  const std::size_t count = field_count();
  result.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    result.push_back(field_at(index));
  }

  return result;
}

std::size_t instance::method_count() const noexcept { return type().method_count(); }

method instance::method_at(std::size_t index) const { return type().method_at(index); }

method instance::find_method(string_name name) const { return type().find_method(name); }

method instance::find_method(std::string_view name) const { return type().find_method(name); }

core::vector<method> instance::methods(memory_resource* resource) const {
  core::vector<method> result{allocator<method>{resource}};
  const std::size_t count = method_count();
  result.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    result.push_back(method_at(index));
  }

  return result;
}

any instance::get(std::string_view name) const {
  const field reflected_field = find_field(name);
  if (!reflected_field || !value_) {
    return {};
  }

  return reflected_field.get_value(value_.data());
}

any_ref instance::get_ref(std::string_view name) const {
  const field reflected_field = find_field(name);
  if (!reflected_field || !value_) {
    return {};
  }

  if (value_.writable()) {
    return reflected_field.get_ref(value_.mutable_data());
  }

  return reflected_field.get_ref(value_.data());
}

bool instance::set(std::string_view name, const any& source) const {
  const field reflected_field = find_field(name);
  if (!reflected_field || !value_.writable()) {
    return false;
  }

  return reflected_field.try_set_value(value_.mutable_data(), source);
}

bool instance::set(std::string_view name, const any_ref& source) const {
  const any_ref target = get_ref(name);
  return target.try_set(source);
}

any instance::call(std::string_view name, std::span<const any_ref> arguments) const {
  const method reflected_method = find_method(name);
  if (!reflected_method || !value_) {
    return {};
  }

  if (value_.writable()) {
    return reflected_method.invoke(value_.mutable_data(), arguments);
  }

  return reflected_method.invoke(value_.data(), arguments);
}

object::object(instance value) noexcept : instance_(value) {}

object::object(instance value, objects::object* pointer) noexcept
    : instance_(value), pointer_(pointer) {}

bool object::valid() const noexcept { return instance_.valid(); }

instance object::as_instance() const noexcept { return instance_; }

meta::any_ref object::value() const noexcept { return instance_.value(); }

runtime::type object::type() const noexcept { return instance_.type(); }

any object::get(std::string_view name) const { return instance_.get(name); }

any_ref object::get_ref(std::string_view name) const { return instance_.get_ref(name); }

bool object::set(std::string_view name, const any& source) const {
  return instance_.set(name, source);
}

bool object::set(std::string_view name, const any_ref& source) const {
  return instance_.set(name, source);
}

objects::object* object::ptr() const noexcept {
  if (pointer_ != nullptr) {
    return pointer_;
  }

  meta::any_ref ref = instance_.value();
  if (!ref) {
    return nullptr;
  }

  if (auto* mutable_object = ref.try_get_mutable<objects::object>()) {
    return mutable_object;
  }

  return const_cast<objects::object*>(ref.try_get<objects::object>());
}

objects::object* object::get_ptr(std::string_view name) const {
  const objects::ptr_view pointer = objects::ptr_view_of(instance_.get_ref(name));
  return pointer ? pointer.get() : nullptr;
}

bool object::set_ptr(std::string_view name, objects::object* pointer) const {
  const objects::ptr_view object_pointer = objects::ptr_view_of(instance_.get_ref(name));
  return object_pointer && object_pointer.set(pointer);
}

object object_view(instance value) noexcept { return object{value}; }

} // namespace caors::runtime
