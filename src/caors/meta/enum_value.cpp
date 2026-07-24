// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/enum_value.h"

#include "caors/meta/detail/enum_value_access.h"
#include "caors/meta/detail/enum_value_record.h"
#include "caors/meta/detail/enum_value_query.h"
#include "caors/meta/detail/type_access.h"
#include "caors/meta/detail/type_record.h"

#include <optional>

namespace caors::meta {
namespace {

[[nodiscard]] const metadata_map& empty_metadata_map() noexcept {
  static const metadata_map empty{
      typename metadata_map::allocator_type{&memory::library_resource()}};
  return empty;
}

} // namespace

enum_value_view::enum_value_view(const detail::enum_value_record* enum_value) noexcept
    : value_(enum_value) {}

bool enum_value_view::valid() const noexcept { return value_ != nullptr && value_->valid(); }

core::string_name enum_value_view::name() const noexcept {
  const detail::enum_value_record* value = info();
  return value != nullptr ? value->name() : core::string_name{};
}

std::int64_t enum_value_view::to_i64() const noexcept {
  const detail::enum_value_record* value = info();
  return value != nullptr ? value->to_i64() : 0;
}

std::uint64_t enum_value_view::to_u64() const noexcept {
  const detail::enum_value_record* value = info();
  return value != nullptr ? value->to_u64() : 0;
}

const meta::metadata_map& enum_value_view::metadata() const noexcept {
  const detail::enum_value_record* value = info();
  return value != nullptr ? value->metadata() : empty_metadata_map();
}

const meta::metadata_value* enum_value_view::find_metadata(meta::metadata_key key) const noexcept {
  const detail::enum_value_record* value = info();
  return value != nullptr ? value->find_metadata(key) : nullptr;
}

const detail::enum_value_record* enum_value_view::info() const noexcept {
  if (!valid()) {
    return nullptr;
  }

  return value_;
}

namespace detail {

std::size_t enum_value_count(const type& source) noexcept {
  const type_record* data = type_access::registered_data_of(source);
  return data != nullptr ? data->enum_value_count() : 0;
}

enum_value_view enum_value_at(const type& source, std::size_t index) {
  const type_record* data = type_access::registered_data_of(source);
  return data != nullptr ? enum_value_access::make(data->enum_value_at(index)) : enum_value_view{};
}

enum_value_view find_enum_value(const type& source, core::string_name enum_value_name) {
  if (!enum_value_name) {
    return {};
  }

  const type_record* data = type_access::registered_data_of(source);
  return data != nullptr ? enum_value_access::make(data->find_enum_value_ptr(enum_value_name))
                         : enum_value_view{};
}

enum_value_view find_enum_value(const type& source, std::string_view enum_value_name) {
  const std::optional<core::string_name> interned_name = core::string_name::find(enum_value_name);
  if (!interned_name.has_value()) {
    return {};
  }

  return find_enum_value(source, *interned_name);
}

enum_value_view find_enum_value_by_i64(const type& source, std::int64_t value) noexcept {
  const type_record* data = type_access::registered_data_of(source);
  return data != nullptr ? enum_value_access::make(data->find_enum_value_ptr(value))
                         : enum_value_view{};
}

enum_value_view find_enum_value_by_u64(const type& source, std::uint64_t value) noexcept {
  const type_record* data = type_access::registered_data_of(source);
  return data != nullptr ? enum_value_access::make(data->find_enum_value_ptr(value))
                         : enum_value_view{};
}

} // namespace detail

} // namespace caors::meta
