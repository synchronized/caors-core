// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/reader.h"
#include "caors/serialization/writer.h"

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

} // namespace caors::serialization
