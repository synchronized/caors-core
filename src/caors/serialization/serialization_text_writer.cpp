// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/writer.h"

#include <sstream>
#include <string>
#include <string_view>

namespace caors {
namespace {

[[nodiscard]] string_name serialized_field_name(const serialization::field_data& data) noexcept {
  return data.serialized_name.empty() ? data.field_name : data.serialized_name;
}

void append_escaped_string(string& output, std::string_view value) {
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

void append_text(string& output, std::string_view value) {
  output.append(value.data(), value.size());
}

} // namespace

serialization::format_features serialization::text_writer::features() const noexcept {
  return serialization::format_features::named_fields |
         serialization::format_features::ordered_fields |
         serialization::format_features::self_describing;
}

error serialization::text_writer::begin_object(const serialization::type_data&) {
  error status = prepare_value();
  if (status) {
    return status;
  }

  text_.push_back('{');
  contexts_.push_back(context{context_kind::object, true});
  mark_value_written();
  return {};
}

error serialization::text_writer::begin_field(const serialization::field_data& field) {
  if (contexts_.empty() || contexts_.back().kind != context_kind::object || expecting_value_) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  const string_name name = serialized_field_name(field);
  if (name.empty()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  context& current = contexts_.back();
  if (!current.first) {
    text_.push_back(',');
  }

  append_escaped_string(text_, name.view());
  text_.push_back(':');
  current.first = false;
  expecting_value_ = true;
  return {};
}

error serialization::text_writer::begin_array(std::size_t) {
  error status = prepare_value();
  if (status) {
    return status;
  }

  text_.push_back('[');
  contexts_.push_back(context{context_kind::array, true});
  mark_value_written();
  return {};
}

error serialization::text_writer::begin_element(std::size_t) {
  if (contexts_.empty() || contexts_.back().kind != context_kind::array || expecting_value_) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  context& current = contexts_.back();
  if (!current.first) {
    text_.push_back(',');
  }

  current.first = false;
  expecting_value_ = true;
  return {};
}

error serialization::text_writer::begin_dictionary(std::size_t size) { return begin_array(size); }

error serialization::text_writer::begin_entry(std::size_t index) {
  error status = begin_element(index);
  if (status) {
    return status;
  }

  return begin_array(2);
}

error serialization::text_writer::begin_key() { return begin_element(0); }

error serialization::text_writer::end_key() { return end_element(); }

error serialization::text_writer::begin_value() { return begin_element(1); }

error serialization::text_writer::end_value() { return end_element(); }

error serialization::text_writer::end_entry() {
  error status = end_array();
  if (status) {
    return status;
  }

  return end_element();
}

error serialization::text_writer::end_dictionary() { return end_array(); }

error serialization::text_writer::write_bool(bool value) {
  error status = prepare_value();
  if (status) {
    return status;
  }

  text_ += value ? "true" : "false";
  mark_value_written();
  return {};
}

error serialization::text_writer::write_i64(std::int64_t value) {
  error status = prepare_value();
  if (status) {
    return status;
  }

  append_text(text_, std::to_string(value));
  mark_value_written();
  return {};
}

error serialization::text_writer::write_u64(std::uint64_t value) {
  error status = prepare_value();
  if (status) {
    return status;
  }

  append_text(text_, std::to_string(value));
  mark_value_written();
  return {};
}

error serialization::text_writer::write_f64(double value) {
  error status = prepare_value();
  if (status) {
    return status;
  }

  std::ostringstream output;
  output << value;
  append_text(text_, output.str());
  mark_value_written();
  return {};
}

error serialization::text_writer::write_string(std::string_view value) {
  error status = prepare_value();
  if (status) {
    return status;
  }

  append_escaped_string(text_, value);
  mark_value_written();
  return {};
}

error serialization::text_writer::write_null() {
  error status = prepare_value();
  if (status) {
    return status;
  }

  text_ += "null";
  mark_value_written();
  return {};
}

error serialization::text_writer::end_element() {
  if (contexts_.empty() || contexts_.back().kind != context_kind::array || expecting_value_) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  return {};
}

error serialization::text_writer::end_array() {
  if (contexts_.empty() || contexts_.back().kind != context_kind::array || expecting_value_) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  text_.push_back(']');
  contexts_.pop_back();
  return {};
}

error serialization::text_writer::end_field() {
  if (contexts_.empty() || contexts_.back().kind != context_kind::object || expecting_value_) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  return {};
}

error serialization::text_writer::end_object() {
  if (contexts_.empty() || contexts_.back().kind != context_kind::object || expecting_value_) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  text_.push_back('}');
  contexts_.pop_back();
  return {};
}

void serialization::text_writer::reset() {
  text_.clear();
  contexts_.clear();
  expecting_value_ = false;
  root_written_ = false;
}

error serialization::text_writer::prepare_value() {
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

void serialization::text_writer::mark_value_written() noexcept {
  if (contexts_.empty()) {
    root_written_ = true;
    return;
  }

  expecting_value_ = false;
}

} // namespace caors
