// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/value/any_ref.h"

#include "caors/meta/query.h"
#include "caors/meta/type_descriptor.h"

#include <cstring>
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
  return meta.valid() ? detail::default_construct(meta) : any{};
}

any any::default_construct(type_id type, memory::memory_resource* resource) {
  const meta::type meta = caors::meta::find(type);
  return meta.valid() ? detail::default_construct(meta, resource) : any{};
}

bool any::valid() const noexcept { return type_.valid() && data_ != nullptr; }

meta::type any::get_type() const noexcept { return valid() ? type_ : meta::type{}; }

const void* any::data() const noexcept { return valid() ? data_ : nullptr; }

void* any::mutable_data() noexcept { return valid() ? data_ : nullptr; }

any_ref any::ref() { return any_ref::from_raw(get_type(), data_, true); }

any_ref any::ref() const { return any_ref::from_raw(get_type(), data_, false); }

std::size_t any::size() const noexcept { return type_.size(); }

std::size_t any::alignment() const noexcept { return type_.alignment(); }

void any::reset() noexcept {
  if (data_ != nullptr) {
    if (stores_inline()) {
      // 内联路径只接收平凡析构类型，不需要调用销毁函数。
    } else if (resource_destroy_ != nullptr) {
      resource_destroy_(resource_, data_);
    } else if (const auto destroy = caors::meta::get_destroy_value_fn(type_); destroy != nullptr) {
      destroy(data_);
    }
  }

  type_ = {};
  data_ = nullptr;
  resource_ = nullptr;
  resource_destroy_ = nullptr;
  mode_ = storage_mode::empty;
}

void any::swap(any& other) noexcept {
  if (this == &other) {
    return;
  }

  any temporary{std::move(other)};
  other = std::move(*this);
  *this = std::move(temporary);
}

bool any::matches_type(const any& other) const noexcept {
  return valid() && other.valid() && get_type().id() == other.get_type().id() &&
         size() == other.size() && alignment() == other.alignment();
}

bool any::try_set(const any& source) {
  const auto assign = caors::meta::get_assign_value_fn(type_);
  if (!matches_type(source) || assign == nullptr) {
    return false;
  }

  return assign(data_, source.data_);
}

any any::copy_from_raw(meta::type type, const void* source) {
  const meta::type_descriptor* descriptor = caors::meta::descriptor_of(type);
  if (!type || descriptor == nullptr || descriptor->type != type.id()) {
    return {};
  }

  if (source == nullptr || descriptor->ops.clone == nullptr || descriptor->ops.destroy == nullptr) {
    return {};
  }

  if (can_store_inline(descriptor)) {
    any result;
    result.type_ = type;
    result.data_ = result.inline_data();
    result.mode_ = storage_mode::inline_value;
    std::memcpy(result.data_, source, descriptor->layout.size);
    return result;
  }

  if (descriptor->ops.clone == nullptr || descriptor->ops.destroy == nullptr) {
    return {};
  }

  void* copy = descriptor->ops.clone(source);
  if (copy == nullptr) {
    return {};
  }

  any result;
  result.type_ = type;
  result.data_ = copy;
  result.mode_ = storage_mode::heap_value;
  return result;
}

void any::copy_from(const any& other) {
  if (!other.valid()) {
    return;
  }

  *this = copy_from_raw(other.get_type(), other.data_);
}

void any::move_from(any& other) noexcept {
  type_ = other.type_;
  resource_ = other.resource_;
  resource_destroy_ = other.resource_destroy_;
  mode_ = other.mode_;

  if (other.stores_inline()) {
    data_ = inline_data();
    std::memcpy(data_, other.inline_data(), other.size());
  } else {
    data_ = other.data_;
  }

  other.type_ = {};
  other.data_ = nullptr;
  other.resource_ = nullptr;
  other.resource_destroy_ = nullptr;
  other.mode_ = storage_mode::empty;
}

bool any::can_store_inline(const meta::type_descriptor* descriptor) noexcept {
  return descriptor != nullptr && descriptor->valid() &&
         descriptor->layout.size <= inline_storage_size &&
         descriptor->layout.alignment <= inline_storage_alignment &&
         descriptor->traits.trivially_copyable && descriptor->traits.trivially_destructible;
}

bool any::can_default_construct_inline(const meta::type_descriptor* descriptor) noexcept {
  return can_store_inline(descriptor) && descriptor->traits.trivially_default_constructible;
}

void* any::inline_data() noexcept { return static_cast<void*>(inline_storage_); }

const void* any::inline_data() const noexcept { return static_cast<const void*>(inline_storage_); }

bool any_ref::valid() const noexcept { return type_.valid() && data_ != nullptr; }

meta::type any_ref::get_type() const noexcept { return valid() ? type_ : meta::type{}; }

const void* any_ref::data() const noexcept { return valid() ? data_ : nullptr; }

void* any_ref::mutable_data() const noexcept {
  return valid() && writable_ ? const_cast<void*>(data_) : nullptr;
}

std::size_t any_ref::size() const noexcept { return type_.size(); }

std::size_t any_ref::alignment() const noexcept { return type_.alignment(); }

bool any_ref::writable() const noexcept { return valid() && writable_; }

bool any_ref::matches_type(const any& other) const noexcept {
  return valid() && other.valid() && get_type().id() == other.get_type().id() &&
         size() == other.size() && alignment() == other.alignment();
}

bool any_ref::matches_type(const any_ref& other) const noexcept {
  return valid() && other.valid() && get_type().id() == other.get_type().id() &&
         size() == other.size() && alignment() == other.alignment();
}

bool any_ref::try_set(const any& source) const {
  const auto assign = caors::meta::get_assign_value_fn(type_);
  if (!matches_type(source) || !writable_ || assign == nullptr) {
    return false;
  }

  return assign(const_cast<void*>(data_), source.data());
}

bool any_ref::try_set(const any_ref& source) const {
  const auto assign = caors::meta::get_assign_value_fn(type_);
  if (!matches_type(source) || !writable_ || assign == nullptr) {
    return false;
  }

  return assign(const_cast<void*>(data_), source.data_);
}

any any_ref::to_any() const { return any::copy_from_raw(get_type(), data_); }

any_ref any_ref::from_raw(meta::type type, const void* data, bool writable) noexcept {
  if (data == nullptr || !type) {
    return {};
  }

  any_ref result;
  result.type_ = type;
  result.data_ = data;
  result.writable_ = writable;
  return result;
}

namespace detail {

const meta::type_descriptor* descriptor_of(const any& value) noexcept {
  return caors::meta::descriptor_of(value.type_);
}

const meta::type_descriptor* descriptor_of(const any_ref& value) noexcept {
  return caors::meta::descriptor_of(value.type_);
}

any default_construct(meta::type type) { return default_construct(type, nullptr); }

any default_construct(meta::type type, memory::memory_resource* resource) {
  const meta::type_descriptor* descriptor = caors::meta::descriptor_of(type);
  if (!type || descriptor == nullptr || descriptor->type != type.id() ||
      caors::meta::get_default_construct_value_fn(type) == nullptr ||
      caors::meta::get_destroy_value_fn(type) == nullptr) {
    return {};
  }

  if (resource == nullptr && any::can_default_construct_inline(descriptor)) {
    any result;
    result.type_ = type;
    result.data_ = result.inline_data();
    result.mode_ = any::storage_mode::inline_value;
    std::memset(result.data_, 0, descriptor->layout.size);
    return result;
  }

  void* value = nullptr;
  meta::value_resource_destroy_fn resource_destroy = nullptr;
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
  result.type_ = type;
  result.data_ = value;
  result.resource_ = resource_destroy != nullptr ? resource : nullptr;
  result.resource_destroy_ = resource_destroy;
  result.mode_ = resource_destroy != nullptr ? any::storage_mode::resource_value
                                             : any::storage_mode::heap_value;
  return result;
}

} // namespace detail

void swap(any& lhs, any& rhs) noexcept { lhs.swap(rhs); }

} // namespace caors::value
