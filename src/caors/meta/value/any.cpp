// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/value/any_ref.h"

#include "caors/meta/query.h"

#include <cstring>
#include <utility>

namespace caors::meta {
namespace {

[[nodiscard]] bool can_store_inline(const meta::value_storage_ops& value) noexcept {
  constexpr std::size_t inline_storage_size = sizeof(void*) * 3;
  constexpr std::size_t inline_storage_alignment = alignof(std::max_align_t);
  return value.valid() && value.layout.size <= inline_storage_size &&
         value.layout.alignment <= inline_storage_alignment && value.traits.trivially_copyable &&
         value.traits.trivially_destructible;
}

[[nodiscard]] bool can_default_construct_inline(const meta::value_storage_ops& value) noexcept {
  return can_store_inline(value) && value.traits.trivially_default_constructible;
}

[[nodiscard]] meta::type_layout layout_of(meta::type type) noexcept {
  const meta::value_storage_ops* value_ops = type.get_raw_ops<meta::value_storage_ops>();
  return value_ops != nullptr ? value_ops->layout : meta::type_layout{};
}

[[nodiscard]] const meta::value_ops* value_ops_of(meta::type type) noexcept {
  const meta::value_storage_ops* storage = type.get_raw_ops<meta::value_storage_ops>();
  return storage != nullptr ? &storage->ops : nullptr;
}

} // namespace

owned_value::~owned_value() { reset(); }

owned_value::owned_value(owned_value&& other) noexcept {
  type_ = other.type_;
  data_ = other.data_;
  resource_ = other.resource_;
  resource_destroy_ = other.resource_destroy_;
  default_destroy_ = other.default_destroy_;
  other.clear_released();
}

owned_value& owned_value::operator=(owned_value&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  reset();
  type_ = other.type_;
  data_ = other.data_;
  resource_ = other.resource_;
  resource_destroy_ = other.resource_destroy_;
  default_destroy_ = other.default_destroy_;
  other.clear_released();
  return *this;
}

bool owned_value::valid() const noexcept {
  return type_.valid() && data_ != nullptr &&
         (resource_destroy_ != nullptr || default_destroy_ != nullptr);
}

meta::type owned_value::get_type() const noexcept { return valid() ? type_ : meta::type{}; }

const void* owned_value::data() const noexcept { return valid() ? data_ : nullptr; }

void* owned_value::mutable_data() noexcept { return valid() ? data_ : nullptr; }

any_ref owned_value::ref() { return detail::any_ref_access::from_erased(get_type(), data_, true); }

any_ref owned_value::ref() const {
  return detail::any_ref_access::from_erased(get_type(), data_, false);
}

any owned_value::into_any() && { return any::take_ownership(std::move(*this)); }

released_owned_value owned_value::release() && noexcept {
  released_owned_value released{
      type_, data_, resource_, resource_destroy_, default_destroy_,
  };
  clear_released();
  return released;
}

void owned_value::reset() noexcept {
  if (data_ != nullptr) {
    if (resource_destroy_ != nullptr) {
      resource_destroy_(resource_, data_);
    } else if (default_destroy_ != nullptr) {
      default_destroy_(data_);
    }
  }

  clear_released();
}

void owned_value::swap(owned_value& other) noexcept {
  using std::swap;
  swap(type_, other.type_);
  swap(data_, other.data_);
  swap(resource_, other.resource_);
  swap(resource_destroy_, other.resource_destroy_);
  swap(default_destroy_, other.default_destroy_);
}

void owned_value::clear_released() noexcept {
  type_ = {};
  data_ = nullptr;
  resource_ = nullptr;
  resource_destroy_ = nullptr;
  default_destroy_ = nullptr;
}

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
  return meta.valid() ? default_construct(meta) : any{};
}

any any::default_construct(meta::type type) { return default_construct(type, nullptr); }

any any::default_construct(type_id type, memory::memory_resource* resource) {
  const meta::type meta = caors::meta::find(type);
  return meta.valid() ? default_construct(meta, resource) : any{};
}

bool any::valid() const noexcept { return type_.valid() && data_ != nullptr; }

meta::type any::get_type() const noexcept { return valid() ? type_ : meta::type{}; }

const void* any::data() const noexcept { return valid() ? data_ : nullptr; }

void* any::mutable_data() noexcept { return valid() ? data_ : nullptr; }

any_ref any::ref() { return detail::any_ref_access::from_erased(get_type(), data_, true); }

any_ref any::ref() const { return detail::any_ref_access::from_erased(get_type(), data_, false); }

std::size_t any::size() const noexcept { return layout_of(type_).size; }

std::size_t any::alignment() const noexcept { return layout_of(type_).alignment; }

void any::reset() noexcept {
  if (data_ != nullptr) {
    if (stores_inline()) {
      // 内联路径只接收平凡析构类型，不需要调用销毁函数。
    } else if (resource_destroy_ != nullptr) {
      resource_destroy_(resource_, data_);
    } else if (destroy_ != nullptr) {
      destroy_(data_);
    }
  }

  type_ = {};
  data_ = nullptr;
  resource_ = nullptr;
  resource_destroy_ = nullptr;
  destroy_ = nullptr;
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
  const meta::value_ops* ops = value_ops_of(type_);
  if (!matches_type(source) || ops == nullptr || ops->assign == nullptr) {
    return false;
  }

  return ops->assign(data_, source.data_);
}

any any::copy_from_raw(meta::type type, const void* source) {
  const meta::value_storage_ops* value_ops = type.get_raw_ops<meta::value_storage_ops>();
  if (!type || value_ops == nullptr) {
    return {};
  }

  if (source == nullptr || value_ops->ops.clone == nullptr || value_ops->ops.destroy == nullptr) {
    return {};
  }

  if (can_store_inline(*value_ops)) {
    any result;
    result.type_ = type;
    result.data_ = result.inline_data();
    result.mode_ = storage_mode::inline_value;
    std::memcpy(result.data_, source, value_ops->layout.size);
    return result;
  }

  if (value_ops->ops.clone == nullptr || value_ops->ops.destroy == nullptr) {
    return {};
  }

  void* copy = value_ops->ops.clone(source);
  if (copy == nullptr) {
    return {};
  }

  any result;
  result.type_ = type;
  result.data_ = copy;
  result.destroy_ = value_ops->ops.destroy;
  result.mode_ = storage_mode::heap_value;
  return result;
}

any any::take_ownership(owned_value&& value) {
  if (!value.valid()) {
    return {};
  }

  const meta::value_storage_ops* value_ops = value.type_.get_raw_ops<meta::value_storage_ops>();
  if (value_ops == nullptr) {
    return {};
  }

  const bool uses_resource_destroy = value.resource_destroy_ != nullptr;
  const meta::destroy_fn destroy =
      value.default_destroy_ != nullptr ? value.default_destroy_ : value_ops->ops.destroy;
  if (!uses_resource_destroy && destroy == nullptr) {
    return {};
  }

  any result;
  result.type_ = value.type_;
  result.data_ = value.data_;
  if (value.resource_destroy_ != nullptr) {
    result.resource_ = value.resource_;
    result.resource_destroy_ = value.resource_destroy_;
    result.mode_ = any::storage_mode::resource_value;
  } else {
    result.destroy_ = destroy;
    result.mode_ = any::storage_mode::heap_value;
  }

  value.clear_released();
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
  destroy_ = other.destroy_;
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
  other.destroy_ = nullptr;
  other.mode_ = storage_mode::empty;
}

void* any::inline_data() noexcept { return static_cast<void*>(inline_storage_); }

const void* any::inline_data() const noexcept { return static_cast<const void*>(inline_storage_); }

bool any_ref::valid() const noexcept { return type_.valid() && data_ != nullptr; }

meta::type any_ref::get_type() const noexcept { return valid() ? type_ : meta::type{}; }

const void* any_ref::data() const noexcept { return valid() ? data_ : nullptr; }

void* any_ref::mutable_data() const noexcept {
  return valid() && writable_ ? const_cast<void*>(data_) : nullptr;
}

std::size_t any_ref::size() const noexcept { return layout_of(type_).size; }

std::size_t any_ref::alignment() const noexcept { return layout_of(type_).alignment; }

bool any_ref::writable() const noexcept { return valid() && writable_; }

bool any_ref::assignable() const noexcept {
  const meta::value_ops* ops = value_ops_of(type_);
  return writable() && ops != nullptr && ops->assign != nullptr;
}

bool any_ref::matches_type(const any& other) const noexcept {
  return valid() && other.valid() && get_type().id() == other.get_type().id() &&
         size() == other.size() && alignment() == other.alignment();
}

bool any_ref::matches_type(const any_ref& other) const noexcept {
  return valid() && other.valid() && get_type().id() == other.get_type().id() &&
         size() == other.size() && alignment() == other.alignment();
}

bool any_ref::try_set(const any& source) const {
  const meta::value_ops* ops = value_ops_of(type_);
  if (!matches_type(source) || !writable_ || ops == nullptr || ops->assign == nullptr) {
    return false;
  }

  return ops->assign(const_cast<void*>(data_), source.data());
}

bool any_ref::try_set(const any_ref& source) const {
  const meta::value_ops* ops = value_ops_of(type_);
  if (!matches_type(source) || !writable_ || ops == nullptr || ops->assign == nullptr) {
    return false;
  }

  return ops->assign(const_cast<void*>(data_), source.data_);
}

any any_ref::to_any() const { return any::copy_from_raw(get_type(), data_); }

any_ref any_ref::from_erased(meta::type type, const void* data, bool writable) noexcept {
  if (data == nullptr || !type) {
    return {};
  }

  any_ref result;
  result.type_ = type;
  result.data_ = data;
  result.writable_ = writable;
  return result;
}

any any::default_construct(meta::type type, memory::memory_resource* resource) {
  const meta::value_storage_ops* value_ops = type.get_raw_ops<meta::value_storage_ops>();
  if (!type || value_ops == nullptr || value_ops->ops.default_construct == nullptr ||
      value_ops->ops.destroy == nullptr) {
    return {};
  }

  if (resource == nullptr && can_default_construct_inline(*value_ops)) {
    any result;
    result.type_ = type;
    result.data_ = result.inline_data();
    result.mode_ = any::storage_mode::inline_value;
    std::memset(result.data_, 0, value_ops->layout.size);
    return result;
  }

  void* value = nullptr;
  meta::resource_destroy_fn resource_destroy = nullptr;
  if (resource != nullptr && value_ops->ops.resource_construct != nullptr &&
      value_ops->ops.resource_destroy != nullptr) {
    value = value_ops->ops.resource_construct(resource);
    resource_destroy = value_ops->ops.resource_destroy;
  } else {
    value = value_ops->ops.default_construct();
  }

  if (value == nullptr) {
    return {};
  }

  any result;
  result.type_ = type;
  result.data_ = value;
  result.resource_ = resource_destroy != nullptr ? resource : nullptr;
  result.resource_destroy_ = resource_destroy;
  result.destroy_ = resource_destroy == nullptr ? value_ops->ops.destroy : nullptr;
  result.mode_ = resource_destroy != nullptr ? any::storage_mode::resource_value
                                             : any::storage_mode::heap_value;
  return result;
}

void swap(any& lhs, any& rhs) noexcept { lhs.swap(rhs); }

void swap(owned_value& lhs, owned_value& rhs) noexcept { lhs.swap(rhs); }

} // namespace caors::meta
