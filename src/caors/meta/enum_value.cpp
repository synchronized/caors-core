// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/enum_value.h"

#include "caors/meta/detail/enum_value_data.h"

namespace caors::meta {
namespace {

[[nodiscard]] const metadata_map& empty_metadata_map() noexcept {
  static const metadata_map empty{
      typename metadata_map::allocator_type{&memory::library_resource()}};
  return empty;
}

} // namespace

enum_value_view::enum_value_view(const detail::enum_value_data* enum_value) noexcept
    : value_(enum_value) {}

bool enum_value_view::valid() const noexcept {
  return value_ != nullptr && value_->valid();
}

core::string_name enum_value_view::name() const noexcept {
  const detail::enum_value_data* value = info();
  return value != nullptr ? value->name() : core::string_name{};
}

std::int64_t enum_value_view::to_i64() const noexcept {
  const detail::enum_value_data* value = info();
  return value != nullptr ? value->to_i64() : 0;
}

std::uint64_t enum_value_view::to_u64() const noexcept {
  const detail::enum_value_data* value = info();
  return value != nullptr ? value->to_u64() : 0;
}

const meta::metadata_map& enum_value_view::metadata() const noexcept {
  const detail::enum_value_data* value = info();
  return value != nullptr ? value->metadata() : empty_metadata_map();
}

const meta::metadata_value* enum_value_view::find_metadata(meta::metadata_key key) const noexcept {
  const detail::enum_value_data* value = info();
  return value != nullptr ? value->find_metadata(key) : nullptr;
}

const detail::enum_value_data* enum_value_view::info() const noexcept {
  if (!valid()) {
    return nullptr;
  }

  return value_;
}

namespace detail {

enum_value_view make_enum_value_view(const enum_value_data* enum_value) noexcept {
  return enum_value_view{enum_value};
}

} // namespace detail

} // namespace caors::meta
