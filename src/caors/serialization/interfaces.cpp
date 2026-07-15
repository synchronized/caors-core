// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/reader.h"
#include "caors/serialization/writer.h"

#include "detail/polymorphic.h"

namespace caors::serialization {

writer::~writer() = default;

error writer::begin_dictionary(std::size_t) {
  return {error_domain::serialization, errc::unsupported};
}

error writer::begin_entry(std::size_t) { return {error_domain::serialization, errc::unsupported}; }

error writer::begin_key() { return {error_domain::serialization, errc::unsupported}; }

error writer::end_key() { return {error_domain::serialization, errc::unsupported}; }

error writer::begin_value() { return {error_domain::serialization, errc::unsupported}; }

error writer::end_value() { return {error_domain::serialization, errc::unsupported}; }

error writer::end_entry() { return {error_domain::serialization, errc::unsupported}; }

error writer::end_dictionary() { return {error_domain::serialization, errc::unsupported}; }

error writer::write_null() { return {error_domain::serialization, errc::unsupported}; }

reader::~reader() = default;

error reader::begin_dictionary(std::size_t&) {
  return {error_domain::serialization, errc::unsupported};
}

error reader::begin_entry(std::size_t) { return {error_domain::serialization, errc::unsupported}; }

error reader::begin_key() { return {error_domain::serialization, errc::unsupported}; }

error reader::end_key() { return {error_domain::serialization, errc::unsupported}; }

error reader::begin_value() { return {error_domain::serialization, errc::unsupported}; }

error reader::end_value() { return {error_domain::serialization, errc::unsupported}; }

error reader::end_entry() { return {error_domain::serialization, errc::unsupported}; }

error reader::end_dictionary() { return {error_domain::serialization, errc::unsupported}; }

bool reader::is_null() const noexcept { return false; }

const variant* reader::current_variant() const noexcept { return nullptr; }

bool reader::is_polymorphic_envelope(const serialization::context& context) const noexcept {
  const variant* current = current_variant();
  return current != nullptr && detail::is_polymorphic_envelope(*current, context);
}

error reader::read_polymorphic_type_guid(const serialization::context& context, type_guid& value) {
  const variant* current = current_variant();
  if (current == nullptr) {
    return {error_domain::serialization, errc::unsupported};
  }

  result<type_guid> parsed = detail::read_polymorphic_type_guid(*current, context);
  if (parsed.failed()) {
    return parsed.status;
  }

  value = parsed.value;
  return {};
}

error reader::begin_polymorphic_value(const serialization::context&) {
  return {error_domain::serialization, errc::unsupported};
}

error reader::end_polymorphic_value() { return {error_domain::serialization, errc::unsupported}; }

} // namespace caors::serialization
