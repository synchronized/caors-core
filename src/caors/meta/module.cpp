// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/module.h"

#include "caors/meta/detail/method_access.h"
#include "caors/meta/detail/module_access.h"
#include "caors/meta/detail/property_access.h"

#include "caors/core/containers.h"
#include "caors/memory/resource.h"
#include "caors/memory/unique_ptr.h"

#include <mutex>
#include <optional>
#include <utility>

namespace caors::meta {
namespace {

class module_registry {
public:
  module_registry()
      : modules_(typename core::hash_map<core::string_name,
                                         memory::unique_ptr<detail::module_data>>::allocator_type{
            &memory::library_resource()}) {}

  detail::module_data* ensure(core::string_name name) {
    if (!name) {
      return nullptr;
    }

    std::lock_guard lock{mutex_};
    auto position = modules_.find(name);
    if (position != modules_.end()) {
      return position->second.get();
    }

    auto data = memory::make_unique<detail::module_data>(&memory::library_resource(), name);
    detail::module_data* result = data.get();
    modules_.try_emplace(name, std::move(data));
    return result;
  }

  const detail::module_data* find(core::string_name name) const {
    if (!name) {
      return nullptr;
    }

    std::lock_guard lock{mutex_};
    auto position = modules_.find(name);
    return position != modules_.end() ? position->second.get() : nullptr;
  }

private:
  mutable std::mutex mutex_;
  core::hash_map<core::string_name, memory::unique_ptr<detail::module_data>> modules_;
};

module_registry& modules() {
  static module_registry registry;
  return registry;
}

} // namespace

namespace detail {

bool module_data::valid() const noexcept {
  if (!name_) {
    return false;
  }

  for (const property_data& property : properties_) {
    if (!property.valid()) {
      return false;
    }
  }

  for (const method_set_data& method : methods_) {
    if (!method.valid()) {
      return false;
    }
  }

  return true;
}

std::size_t module_data::property_count() const noexcept { return properties_.size(); }

const property_data* module_data::property_at(std::size_t index) const noexcept {
  return index < properties_.size() ? &properties_[index] : nullptr;
}

property_data* module_data::mutable_find_property_ptr(core::string_name name) noexcept {
  if (!name) {
    return nullptr;
  }

  for (property_data& property : properties_) {
    if (property.name() == name) {
      return &property;
    }
  }

  return nullptr;
}

const property_data* module_data::find_property_ptr(core::string_name name) const noexcept {
  if (!name) {
    return nullptr;
  }

  for (const property_data& property : properties_) {
    if (property.name() == name) {
      return &property;
    }
  }

  return nullptr;
}

error module_data::add_property(property_data property) {
  if (!property.valid()) {
    return {error_domain::meta, errc::invalid_argument};
  }

  if (find_property_ptr(property.name()) != nullptr) {
    return {error_domain::meta, errc::duplicate_field};
  }

  properties_.push_back(std::move(property));
  return {};
}

std::size_t module_data::method_count() const noexcept { return methods_.size(); }

const method_set_data* module_data::method_at(std::size_t index) const noexcept {
  return index < methods_.size() ? &methods_[index] : nullptr;
}

method_set_data* module_data::mutable_find_method_ptr(core::string_name name) noexcept {
  if (!name) {
    return nullptr;
  }

  for (method_set_data& method : methods_) {
    if (method.name() == name) {
      return &method;
    }
  }

  return nullptr;
}

const method_set_data* module_data::find_method_ptr(core::string_name name) const noexcept {
  if (!name) {
    return nullptr;
  }

  for (const method_set_data& method : methods_) {
    if (method.name() == name) {
      return &method;
    }
  }

  return nullptr;
}

std::optional<std::size_t> module_data::add_method(method_data method) {
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

  methods_.push_back(std::move(method_set));
  return overload_index;
}

} // namespace detail

module_view::module_view(const detail::module_data* data) noexcept : data_(data) {}

bool module_view::valid() const noexcept { return data_ != nullptr && data_->valid(); }

core::string_name module_view::name() const noexcept {
  const detail::module_data* module = detail::module_access::data_of(*this);
  return module != nullptr ? module->name() : core::string_name{};
}

std::size_t module_view::property_count() const noexcept {
  const detail::module_data* module = detail::module_access::data_of(*this);
  return module != nullptr ? module->property_count() : 0;
}

property module_view::property_at(std::size_t index) const {
  const detail::module_data* module = detail::module_access::data_of(*this);
  return module != nullptr ? detail::property_access::make(module->property_at(index)) : property{};
}

property module_view::find_property(core::string_name name) const {
  const detail::module_data* module = detail::module_access::data_of(*this);
  return module != nullptr ? detail::property_access::make(module->find_property_ptr(name))
                           : property{};
}

property module_view::find_property(std::string_view name) const {
  const std::optional<core::string_name> interned_name = core::string_name::find(name);
  return interned_name.has_value() ? find_property(*interned_name) : property{};
}

std::size_t module_view::method_count() const noexcept {
  const detail::module_data* module = detail::module_access::data_of(*this);
  return module != nullptr ? module->method_count() : 0;
}

method_set module_view::method_at(std::size_t index) const {
  const detail::module_data* module = detail::module_access::data_of(*this);
  return module != nullptr ? detail::method_access::make_set(module->method_at(index))
                           : method_set{};
}

method_set module_view::find_method(core::string_name name) const {
  const detail::module_data* module = detail::module_access::data_of(*this);
  return module != nullptr ? detail::method_access::make_set(module->find_method_ptr(name))
                           : method_set{};
}

method_set module_view::find_method(std::string_view name) const {
  const std::optional<core::string_name> interned_name = core::string_name::find(name);
  return interned_name.has_value() ? find_method(*interned_name) : method_set{};
}

module_builder::module_builder(std::string_view name) : name_(name) {
  refresh();
  if (module_ == nullptr) {
    status_ = {error_domain::meta, errc::invalid_argument};
  }
}

error module_builder::commit_property(detail::property_data property) {
  if (module_ == nullptr) {
    return {error_domain::meta, errc::invalid_argument};
  }

  return module_->add_property(std::move(property));
}

error module_builder::commit_method(detail::method_data method) {
  if (module_ == nullptr || !module_->add_method(std::move(method)).has_value()) {
    return {error_domain::meta, errc::duplicate_method};
  }

  return {};
}

void module_builder::refresh() noexcept { module_ = modules().ensure(name_); }

module_view module_builder::get() const noexcept { return detail::module_access::make(module_); }

module_builder module(std::string_view name) { return module_builder{name}; }

module_view find_module(std::string_view name) {
  const std::optional<core::string_name> interned_name = core::string_name::find(name);
  return interned_name.has_value() ? find_module(*interned_name) : module_view{};
}

module_view find_module(core::string_name name) {
  return detail::module_access::make(modules().find(name));
}

} // namespace caors::meta
