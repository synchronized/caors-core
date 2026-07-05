// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/value/any_ref.h"

#include "caors/meta/detail/type_descriptor.h"
#include "caors/meta/query.h"

#include <utility>

namespace caors::value {

any::~any() { reset(); }

any::any(const any& other) { copy_from(other); }

any::any(any&& other) noexcept { move_from(other); }

any& any::operator=(const any& other) {
  if (this == &other) {
    return *this;
  }

  any copy{other};
  swap(copy);
  return *this;
}

any& any::operator=(any&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  reset();
  move_from(other);
  return *this;
}

any any::default_construct(type_id type) {
  const meta::type meta = caors::meta::find(type);
  const meta::detail::type_descriptor* descriptor =
      meta ? caors::meta::get_registry().find_type_descriptor(meta.id()) : nullptr;
  return meta.valid() ? detail::default_construct(type, descriptor) : any{};
}

any any::default_construct(type_id type, memory::memory_resource* resource) {
  const meta::type meta = caors::meta::find(type);
  const meta::detail::type_descriptor* descriptor =
      meta ? caors::meta::get_registry().find_type_descriptor(meta.id()) : nullptr;
  return meta.valid() ? detail::default_construct(type, descriptor, resource) : any{};
}

bool any::valid() const noexcept {
  return descriptor_ != nullptr && descriptor_->valid() && data_ != nullptr;
}

type_id any::type() const noexcept { return get_type_id(); }

string_name any::type_name() const noexcept {
  const meta::type meta = caors::meta::find(get_type_id());
  return meta ? meta.name() : string_name{};
}

const void* any::data() const noexcept { return valid() ? data_ : nullptr; }

void* any::mutable_data() noexcept { return valid() ? data_ : nullptr; }

any_ref any::ref() { return any_ref::from_raw(get_type_id(), descriptor_, data_, true); }

any_ref any::ref() const { return any_ref::from_raw(get_type_id(), descriptor_, data_, false); }

std::size_t any::size() const noexcept {
  return descriptor_ != nullptr ? descriptor_->layout.size : 0;
}

std::size_t any::alignment() const noexcept {
  return descriptor_ != nullptr ? descriptor_->layout.alignment : 0;
}

void any::reset() noexcept {
  if (data_ != nullptr) {
    if (resource_destroy_ != nullptr) {
      resource_destroy_(resource_, data_);
    } else if (descriptor_ != nullptr && descriptor_->ops.destroy != nullptr) {
      descriptor_->ops.destroy(data_);
    }
  }

  descriptor_ = nullptr;
  data_ = nullptr;
  resource_ = nullptr;
  resource_destroy_ = nullptr;
}

void any::swap(any& other) noexcept {
  using std::swap;
  swap(descriptor_, other.descriptor_);
  swap(data_, other.data_);
  swap(resource_, other.resource_);
  swap(resource_destroy_, other.resource_destroy_);
}

bool any::matches_type(const any& other) const noexcept {
  return valid() && other.valid() && get_type_id() == other.get_type_id() &&
         size() == other.size() && alignment() == other.alignment();
}

bool any::try_set(const any& source) {
  const auto assign = descriptor_ != nullptr ? descriptor_->ops.assign : nullptr;
  if (!matches_type(source) || assign == nullptr) {
    return false;
  }

  return assign(data_, source.data_);
}

any any::copy_from_raw(type_id type, const meta::detail::type_descriptor* descriptor,
                       const void* source) {
  if (descriptor == nullptr || descriptor->type != type) {
    return {};
  }

  if (source == nullptr || descriptor->ops.clone == nullptr || descriptor->ops.destroy == nullptr) {
    return {};
  }

  void* copy = descriptor->ops.clone(source);
  if (copy == nullptr) {
    return {};
  }

  any result;
  result.descriptor_ = descriptor;
  result.data_ = copy;
  return result;
}

void any::copy_from(const any& other) {
  if (!other.valid()) {
    return;
  }

  *this = copy_from_raw(other.get_type_id(), other.descriptor_, other.data_);
}

void any::move_from(any& other) noexcept {
  descriptor_ = other.descriptor_;
  data_ = other.data_;
  resource_ = other.resource_;
  resource_destroy_ = other.resource_destroy_;

  other.descriptor_ = nullptr;
  other.data_ = nullptr;
  other.resource_ = nullptr;
  other.resource_destroy_ = nullptr;
}

type_id any::get_type_id() const noexcept {
  return descriptor_ != nullptr ? descriptor_->type : type_id{};
}

bool any_ref::valid() const noexcept {
  return descriptor_ != nullptr && descriptor_->valid() && data_ != nullptr;
}

type_id any_ref::type() const noexcept { return get_type_id(); }

string_name any_ref::type_name() const noexcept {
  const meta::type meta = caors::meta::find(get_type_id());
  return meta ? meta.name() : string_name{};
}

const void* any_ref::data() const noexcept { return valid() ? data_ : nullptr; }

void* any_ref::mutable_data() const noexcept {
  return valid() && writable_ ? const_cast<void*>(data_) : nullptr;
}

std::size_t any_ref::size() const noexcept {
  return descriptor_ != nullptr ? descriptor_->layout.size : 0;
}

std::size_t any_ref::alignment() const noexcept {
  return descriptor_ != nullptr ? descriptor_->layout.alignment : 0;
}

bool any_ref::writable() const noexcept { return valid() && writable_; }

bool any_ref::matches_type(const any& other) const noexcept {
  return valid() && other.valid() && get_type_id() == other.type() && size() == other.size() &&
         alignment() == other.alignment();
}

bool any_ref::matches_type(const any_ref& other) const noexcept {
  return valid() && other.valid() && get_type_id() == other.get_type_id() &&
         size() == other.size() && alignment() == other.alignment();
}

bool any_ref::try_set(const any& source) const {
  const auto assign = descriptor_ != nullptr ? descriptor_->ops.assign : nullptr;
  if (!matches_type(source) || !writable_ || assign == nullptr) {
    return false;
  }

  return assign(const_cast<void*>(data_), source.data());
}

bool any_ref::try_set(const any_ref& source) const {
  const auto assign = descriptor_ != nullptr ? descriptor_->ops.assign : nullptr;
  if (!matches_type(source) || !writable_ || assign == nullptr) {
    return false;
  }

  return assign(const_cast<void*>(data_), source.data_);
}

any any_ref::to_any() const { return any::copy_from_raw(get_type_id(), descriptor_, data_); }

any_ref any_ref::from_raw(type_id type, const meta::detail::type_descriptor* descriptor,
                          const void* data, bool writable) noexcept {
  if (data == nullptr || descriptor == nullptr || descriptor->type != type) {
    return {};
  }

  any_ref result;
  result.descriptor_ = descriptor;
  result.data_ = data;
  result.writable_ = writable;
  return result;
}

type_id any_ref::get_type_id() const noexcept {
  return descriptor_ != nullptr ? descriptor_->type : type_id{};
}

namespace detail {

const meta::detail::type_descriptor* descriptor_of(const any& value) noexcept {
  return value.descriptor_;
}

const meta::detail::type_descriptor* descriptor_of(const any_ref& value) noexcept {
  return value.descriptor_;
}

any default_construct(type_id type, const meta::detail::type_descriptor* descriptor) {
  return default_construct(type, descriptor, nullptr);
}

any default_construct(type_id type, const meta::detail::type_descriptor* descriptor,
                      memory::memory_resource* resource) {
  if (descriptor == nullptr || descriptor->type != type ||
      descriptor->ops.default_construct == nullptr || descriptor->ops.destroy == nullptr) {
    return {};
  }

  void* value = nullptr;
  meta::detail::value_resource_destroy_fn resource_destroy = nullptr;
  if (resource != nullptr && descriptor->ops.resource_construct != nullptr &&
      descriptor->ops.resource_destroy != nullptr) {
    value = descriptor->ops.resource_construct(resource);
    resource_destroy = descriptor->ops.resource_destroy;
  } else {
    value = descriptor->ops.default_construct();
  }

  if (value == nullptr) {
    return {};
  }

  any result;
  result.descriptor_ = descriptor;
  result.data_ = value;
  result.resource_ = resource_destroy != nullptr ? resource : nullptr;
  result.resource_destroy_ = resource_destroy;
  return result;
}

} // namespace detail

void swap(any& lhs, any& rhs) noexcept { lhs.swap(rhs); }

} // namespace caors::value
