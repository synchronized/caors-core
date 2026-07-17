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

std::size_t instance::property_count() const noexcept { return type().property_count(); }

property instance::property_at(std::size_t index) const { return type().property_at(index); }

property instance::find_property(string_name name) const { return type().find_property(name); }

property instance::find_property(std::string_view name) const { return type().find_property(name); }

core::vector<property> instance::properties(memory_resource* resource) const {
  core::vector<property> result{allocator<property>{resource}};
  const std::size_t count = property_count();
  result.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    result.push_back(property_at(index));
  }

  return result;
}

std::size_t instance::method_count() const noexcept { return type().method_count(); }

method_set instance::method_at(std::size_t index) const { return type().method_at(index); }

method_set instance::find_method(string_name name) const { return type().find_method(name); }

method_set instance::find_method(std::string_view name) const { return type().find_method(name); }

core::vector<method_set> instance::methods(memory_resource* resource) const {
  core::vector<method_set> result{allocator<method_set>{resource}};
  const std::size_t count = method_count();
  result.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    result.push_back(method_at(index));
  }

  return result;
}

any instance::get(std::string_view name) const {
  const property reflected_property = find_property(name);
  if (!reflected_property || !value_) {
    return {};
  }

  return reflected_property.get_value(value_.data());
}

any_ref instance::get_ref(std::string_view name) const {
  const property reflected_property = find_property(name);
  if (!reflected_property || !value_) {
    return {};
  }

  if (value_.writable()) {
    return reflected_property.get_ref(value_.mutable_data());
  }

  return reflected_property.get_ref(value_.data());
}

bool instance::set(std::string_view name, const any& source) const {
  const property reflected_property = find_property(name);
  if (!reflected_property || !value_.writable()) {
    return false;
  }

  return reflected_property.try_set_value(value_.mutable_data(), source);
}

bool instance::set(std::string_view name, const any_ref& source) const {
  const any_ref target = get_ref(name);
  return target.try_set(source);
}

any instance::call(std::string_view name, std::span<const any_ref> arguments) const {
  const method_set reflected_method = find_method(name);
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
  const objects::ptr_view pointer = objects::make_ptr_view(instance_.get_ref(name));
  return pointer ? pointer.get() : nullptr;
}

bool object::set_ptr(std::string_view name, objects::object* pointer) const {
  const objects::ptr_view object_pointer = objects::make_ptr_view(instance_.get_ref(name));
  return object_pointer && object_pointer.set(pointer);
}

object object_view(instance value) noexcept { return object{value}; }

} // namespace caors::runtime
