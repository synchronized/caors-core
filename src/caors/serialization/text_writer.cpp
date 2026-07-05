// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/writer.h"

#include <sstream>
#include <string>
#include <string_view>

namespace caors::serialization {
namespace {

[[nodiscard]] core::string_name serialized_field_name(const field_data& data) noexcept {
  return data.serialized_name.empty() ? data.field_name : data.serialized_name;
}

void append_escaped_string(core::string& output, std::string_view value) {
  output.push_back('"');
  for (const char character : value) {
    switch (character) {
    case '"':
      output += "\\\"";
      break;
    case '\\':
      output += "\\\\";
      break;
    case '\n':
      output += "\\n";
      break;
    case '\r':
      output += "\\r";
      break;
    case '\t':
      output += "\\t";
      break;
    default:
      output.push_back(character);
      break;
    }
  }
  output.push_back('"');
}

void append_text(core::string& output, std::string_view value) {
  output.append(value.data(), value.size());
}

} // namespace

text_writer::text_writer() noexcept : text_(&owned_text_) {}

text_writer::text_writer(string& output) : text_(&output) { reset(); }

format_features text_writer::features() const noexcept {
  return format_features::named_fields | format_features::ordered_fields |
         format_features::self_describing;
}

error text_writer::begin_object(const type_data&) {
  error status = prepare_value();
  if (status) {
    return status;
  }

  output().push_back('{');
  contexts_.push_back(context{context_kind::object, true});
  mark_value_written();
  return {};
}

error text_writer::begin_field(const field_data& field) {
  if (contexts_.empty() || contexts_.back().kind != context_kind::object || expecting_value_) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  const core::string_name name = serialized_field_name(field);
  if (name.empty()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  context& current = contexts_.back();
  if (!current.first) {
    output().push_back(',');
  }

  append_escaped_string(output(), name.view());
  output().push_back(':');
  current.first = false;
  expecting_value_ = true;
  return {};
}

error text_writer::begin_array(std::size_t) {
  error status = prepare_value();
  if (status) {
    return status;
  }

  output().push_back('[');
  contexts_.push_back(context{context_kind::array, true});
  mark_value_written();
  return {};
}

error text_writer::begin_element(std::size_t) {
  if (contexts_.empty() || contexts_.back().kind != context_kind::array || expecting_value_) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  context& current = contexts_.back();
  if (!current.first) {
    output().push_back(',');
  }

  current.first = false;
  expecting_value_ = true;
  return {};
}

error text_writer::begin_dictionary(std::size_t size) { return begin_array(size); }

error text_writer::begin_entry(std::size_t index) {
  error status = begin_element(index);
  if (status) {
    return status;
  }

  return begin_array(2);
}

error text_writer::begin_key() { return begin_element(0); }

error text_writer::end_key() { return end_element(); }

error text_writer::begin_value() { return begin_element(1); }

error text_writer::end_value() { return end_element(); }

error text_writer::end_entry() {
  error status = end_array();
  if (status) {
    return status;
  }

  return end_element();
}

error text_writer::end_dictionary() { return end_array(); }

error text_writer::write_bool(bool value) {
  error status = prepare_value();
  if (status) {
    return status;
  }

  output() += value ? "true" : "false";
  mark_value_written();
  return {};
}

error text_writer::write_i64(std::int64_t value) {
  error status = prepare_value();
  if (status) {
    return status;
  }

  append_text(output(), std::to_string(value));
  mark_value_written();
  return {};
}

error text_writer::write_u64(std::uint64_t value) {
  error status = prepare_value();
  if (status) {
    return status;
  }

  append_text(output(), std::to_string(value));
  mark_value_written();
  return {};
}

error text_writer::write_f64(double value) {
  error status = prepare_value();
  if (status) {
    return status;
  }

  std::ostringstream output;
  output << value;
  append_text(this->output(), output.str());
  mark_value_written();
  return {};
}

error text_writer::write_string(std::string_view value) {
  error status = prepare_value();
  if (status) {
    return status;
  }

  append_escaped_string(output(), value);
  mark_value_written();
  return {};
}

error text_writer::write_null() {
  error status = prepare_value();
  if (status) {
    return status;
  }

  output() += "null";
  mark_value_written();
  return {};
}

error text_writer::end_element() {
  if (contexts_.empty() || contexts_.back().kind != context_kind::array || expecting_value_) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  return {};
}

error text_writer::end_array() {
  if (contexts_.empty() || contexts_.back().kind != context_kind::array || expecting_value_) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  output().push_back(']');
  contexts_.pop_back();
  return {};
}

error text_writer::end_field() {
  if (contexts_.empty() || contexts_.back().kind != context_kind::object || expecting_value_) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  return {};
}

error text_writer::end_object() {
  if (contexts_.empty() || contexts_.back().kind != context_kind::object || expecting_value_) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  output().push_back('}');
  contexts_.pop_back();
  return {};
}

void text_writer::reset() {
  output().clear();
  contexts_.clear();
  expecting_value_ = false;
  root_written_ = false;
}

error text_writer::prepare_value() {
  if (contexts_.empty()) {
    if (root_written_) {
      return {error_domain::serialization, errc::invalid_argument};
    }

    return {};
  }

  if (!expecting_value_) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  return {};
}

void text_writer::mark_value_written() noexcept {
  if (contexts_.empty()) {
    root_written_ = true;
    return;
  }

  expecting_value_ = false;
}

} // namespace caors::serialization
