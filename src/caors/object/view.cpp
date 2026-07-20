// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/object/view.h"

#include "caors/meta/instance.h"
#include "caors/object/ptr_view.h"

#include <utility>

namespace caors::objects {

object_view::object_view(meta::any_ref value) noexcept : value_(value) {}

object_view::object_view(meta::any_ref value, object* pointer) noexcept
    : value_(value), pointer_(pointer) {}

bool object_view::valid() const noexcept { return value_.valid(); }

meta::any_ref object_view::value() const noexcept { return value_; }

meta::instance object_view::instance() const noexcept { return meta::instance{value_}; }

meta::type object_view::type() const noexcept { return valid() ? value_.get_type() : meta::type{}; }

std::size_t object_view::property_count() const noexcept { return type().property_count(); }

meta::property object_view::property_at(std::size_t index) const {
  return type().property_at(index);
}

meta::property object_view::find_property(string_name name) const {
  return type().find_property(name);
}

meta::property object_view::find_property(std::string_view name) const {
  return type().find_property(name);
}

core::vector<meta::property> object_view::properties(memory_resource* resource) const {
  core::vector<meta::property> result{allocator<meta::property>{resource}};
  const std::size_t count = property_count();
  result.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    result.push_back(property_at(index));
  }

  return result;
}

std::size_t object_view::method_count() const noexcept { return type().method_count(); }

meta::method_set object_view::method_at(std::size_t index) const { return type().method_at(index); }

meta::method_set object_view::find_method(string_name name) const {
  return type().find_method(name);
}

meta::method_set object_view::find_method(std::string_view name) const {
  return type().find_method(name);
}

core::vector<meta::method_set> object_view::methods(memory_resource* resource) const {
  core::vector<meta::method_set> result{allocator<meta::method_set>{resource}};
  const std::size_t count = method_count();
  result.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    result.push_back(method_at(index));
  }

  return result;
}

meta::value object_view::get(std::string_view name) const {
  const meta::instance object = instance();
  const meta::property reflected_property = object.find_property(name);
  if (!reflected_property || !object) {
    return {};
  }

  const ptr_view pointer = make_ptr_view(reflected_property.get_ref(object));
  if (pointer) {
    objects::object* target = pointer.get();
    return target != nullptr ? meta::value::from_object(target) : meta::value{};
  }

  return reflected_property.get_value(object);
}

any object_view::get_any(std::string_view name) const { return instance().get_any(name); }

any_ref object_view::get_ref(std::string_view name) const { return instance().get_ref(name); }

bool object_view::set(std::string_view name, const meta::value& source) const {
  const meta::instance object = instance();
  const meta::property reflected_property = object.find_property(name);
  if (!reflected_property || !object.writable()) {
    return false;
  }

  const ptr_view pointer = make_ptr_view(reflected_property.get_ref(object));
  if (pointer && (source.is_null() || source.is_object())) {
    return pointer.set(source.as_object());
  }

  return reflected_property.try_set_value(object, source);
}

bool object_view::set_ref(std::string_view name, const any_ref& source) const {
  const any_ref target = get_ref(name);
  return target.try_set(source);
}

bool object_view::set_any(std::string_view name, const any& source) const {
  return instance().set_any(name, source);
}

any object_view::call_any(std::string_view name, std::span<const meta::argument> arguments) const {
  return instance().call_any(name, arguments);
}

meta::invoke_value_result object_view::call(std::string_view name,
                                            std::span<const meta::value> arguments) const {
  return instance().call(name, arguments);
}

object* object_view::ptr() const noexcept {
  if (pointer_ != nullptr) {
    return pointer_;
  }

  if (!value_) {
    return nullptr;
  }

  if (auto* mutable_object = value_.try_get_mutable<object>()) {
    return mutable_object;
  }

  return const_cast<object*>(value_.try_get<object>());
}

object* object_view::get_ptr(std::string_view name) const {
  const ptr_view pointer = make_ptr_view(get_ref(name));
  return pointer ? pointer.get() : nullptr;
}

bool object_view::set_ptr(std::string_view name, object* pointer) const {
  const ptr_view object_pointer = make_ptr_view(get_ref(name));
  return object_pointer && object_pointer.set(pointer);
}

} // namespace caors::objects
