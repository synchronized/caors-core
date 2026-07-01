// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/reader.h"
#include "caors/serialization/writer.h"

namespace caors {

serialization_writer::~serialization_writer() = default;

error serialization_writer::begin_dictionary(std::size_t) {
  return {error_domain::serialization, errc::unsupported};
}

error serialization_writer::begin_entry(std::size_t) {
  return {error_domain::serialization, errc::unsupported};
}

error serialization_writer::begin_key() { return {error_domain::serialization, errc::unsupported}; }

error serialization_writer::end_key() { return {error_domain::serialization, errc::unsupported}; }

error serialization_writer::begin_value() {
  return {error_domain::serialization, errc::unsupported};
}

error serialization_writer::end_value() { return {error_domain::serialization, errc::unsupported}; }

error serialization_writer::end_entry() { return {error_domain::serialization, errc::unsupported}; }

error serialization_writer::end_dictionary() {
  return {error_domain::serialization, errc::unsupported};
}

error serialization_writer::write_null() {
  return {error_domain::serialization, errc::unsupported};
}

serialization_reader::~serialization_reader() = default;

error serialization_reader::begin_dictionary(std::size_t&) {
  return {error_domain::serialization, errc::unsupported};
}

error serialization_reader::begin_entry(std::size_t) {
  return {error_domain::serialization, errc::unsupported};
}

error serialization_reader::begin_key() { return {error_domain::serialization, errc::unsupported}; }

error serialization_reader::end_key() { return {error_domain::serialization, errc::unsupported}; }

error serialization_reader::begin_value() {
  return {error_domain::serialization, errc::unsupported};
}

error serialization_reader::end_value() { return {error_domain::serialization, errc::unsupported}; }

error serialization_reader::end_entry() { return {error_domain::serialization, errc::unsupported}; }

error serialization_reader::end_dictionary() {
  return {error_domain::serialization, errc::unsupported};
}

bool serialization_reader::is_null() const noexcept { return false; }

} // namespace caors
