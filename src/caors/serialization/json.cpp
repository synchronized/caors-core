// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/operations.h"
#include "detail/internal.h"

#include <cmath>
#include <sstream>
#include <string_view>

namespace caors::serialization {
namespace {

void append_hex4(core::string& output, std::uint32_t value) {
  constexpr char digits[] = "0123456789abcdef";
  output += "\\u";
  output.push_back(digits[(value >> 12U) & 0x0FU]);
  output.push_back(digits[(value >> 8U) & 0x0FU]);
  output.push_back(digits[(value >> 4U) & 0x0FU]);
  output.push_back(digits[value & 0x0FU]);
}

void append_json_string(core::string& output, std::string_view value) {
  output.push_back('"');
  for (const unsigned char character : value) {
    switch (character) {
    case '"':
      output += "\\\"";
      break;
    case '\\':
      output += "\\\\";
      break;
    case '\b':
      output += "\\b";
      break;
    case '\f':
      output += "\\f";
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
      if (character < 0x20U) {
        append_hex4(output, character);
      } else {
        output.push_back(static_cast<char>(character));
      }
      break;
    }
  }
  output.push_back('"');
}

void append_text(core::string& output, std::string_view value) {
  output.append(value.data(), value.size());
}

} // namespace

serialization::json_writer::json_writer() noexcept : text_(&owned_text_) {}

serialization::json_writer::json_writer(json_options options) noexcept
    : options_(options), text_(&owned_text_) {}

serialization::json_writer::json_writer(string& output) : json_writer(output, json_options{}) {}

serialization::json_writer::json_writer(string& output, json_options options)
    : options_(options), text_(&output) {
  reset();
}

serialization::format_features serialization::json_writer::features() const noexcept {
  return serialization::format_features::named_fields |
         serialization::format_features::ordered_fields |
         serialization::format_features::self_describing;
}

error serialization::json_writer::begin_object(const serialization::type_data&) {
  error status = prepare_value();
  if (status) {
    return status;
  }

  output().push_back('{');
  contexts_.push_back(context{context_kind::object, true});
  mark_value_written();
  return {};
}

error serialization::json_writer::begin_field(const serialization::field_data& field) {
  if (contexts_.empty() || contexts_.back().kind != context_kind::object || expecting_value_) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  const core::string_name name = detail::serialized_field_name(field);
  if (name.empty()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  context& current = contexts_.back();
  if (!current.first) {
    output().push_back(',');
  }

  write_newline_and_indent(contexts_.size());
  append_json_string(output(), name.view());
  output().push_back(':');
  if (options_.pretty) {
    output().push_back(' ');
  }
  current.first = false;
  expecting_value_ = true;
  return {};
}

error serialization::json_writer::begin_array(std::size_t) {
  error status = prepare_value();
  if (status) {
    return status;
  }

  output().push_back('[');
  contexts_.push_back(context{context_kind::array, true});
  mark_value_written();
  return {};
}

error serialization::json_writer::begin_element(std::size_t) {
  if (contexts_.empty() || contexts_.back().kind != context_kind::array || expecting_value_) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  context& current = contexts_.back();
  if (!current.first) {
    output().push_back(',');
  }

  write_newline_and_indent(contexts_.size());
  current.first = false;
  expecting_value_ = true;
  return {};
}

error serialization::json_writer::begin_dictionary(std::size_t size) { return begin_array(size); }

error serialization::json_writer::begin_entry(std::size_t index) {
  error status = begin_element(index);
  if (status) {
    return status;
  }

  return begin_array(2);
}

error serialization::json_writer::begin_key() { return begin_element(0); }

error serialization::json_writer::end_key() { return end_element(); }

error serialization::json_writer::begin_value() { return begin_element(1); }

error serialization::json_writer::end_value() { return end_element(); }

error serialization::json_writer::end_entry() {
  error status = end_array();
  if (status) {
    return status;
  }

  return end_element();
}

error serialization::json_writer::end_dictionary() { return end_array(); }

error serialization::json_writer::write_bool(bool value) {
  error status = prepare_value();
  if (status) {
    return status;
  }

  output() += value ? "true" : "false";
  mark_value_written();
  return {};
}

error serialization::json_writer::write_i64(std::int64_t value) {
  error status = prepare_value();
  if (status) {
    return status;
  }

  append_text(output(), std::to_string(value));
  mark_value_written();
  return {};
}

error serialization::json_writer::write_u64(std::uint64_t value) {
  error status = prepare_value();
  if (status) {
    return status;
  }

  append_text(output(), std::to_string(value));
  mark_value_written();
  return {};
}

error serialization::json_writer::write_f64(double value) {
  error status = prepare_value();
  if (status) {
    return status;
  }

  if (!std::isfinite(value)) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  std::ostringstream output;
  output << value;
  append_text(this->output(), output.str());
  mark_value_written();
  return {};
}

error serialization::json_writer::write_string(std::string_view value) {
  error status = prepare_value();
  if (status) {
    return status;
  }

  append_json_string(output(), value);
  mark_value_written();
  return {};
}

error serialization::json_writer::write_null() {
  error status = prepare_value();
  if (status) {
    return status;
  }

  output() += "null";
  mark_value_written();
  return {};
}

error serialization::json_writer::end_element() {
  if (contexts_.empty() || contexts_.back().kind != context_kind::array || expecting_value_) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  return {};
}

error serialization::json_writer::end_array() {
  if (contexts_.empty() || contexts_.back().kind != context_kind::array || expecting_value_) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  const context current = contexts_.back();
  if (!current.first) {
    write_newline_and_indent(contexts_.size() - 1);
  }
  output().push_back(']');
  contexts_.pop_back();
  return {};
}

error serialization::json_writer::end_field() {
  if (contexts_.empty() || contexts_.back().kind != context_kind::object || expecting_value_) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  return {};
}

error serialization::json_writer::end_object() {
  if (contexts_.empty() || contexts_.back().kind != context_kind::object || expecting_value_) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  const context current = contexts_.back();
  if (!current.first) {
    write_newline_and_indent(contexts_.size() - 1);
  }
  output().push_back('}');
  contexts_.pop_back();
  return {};
}

void serialization::json_writer::reset() {
  output().clear();
  contexts_.clear();
  expecting_value_ = false;
  root_written_ = false;
}

error serialization::json_writer::prepare_value() {
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

void serialization::json_writer::mark_value_written() noexcept {
  if (contexts_.empty()) {
    root_written_ = true;
    return;
  }

  expecting_value_ = false;
}

void serialization::json_writer::write_newline_and_indent(std::size_t depth) {
  if (!options_.pretty) {
    return;
  }

  output().push_back('\n');
  const std::size_t spaces = depth * static_cast<std::size_t>(options_.indent);
  for (std::size_t index = 0; index < spaces; ++index) {
    output().push_back(' ');
  }
}

namespace detail {

serialization_text_result serialize_json(const reflection::type& type, const void* object) {
  return serialize_json(type, object, serialization::context{});
}

serialization_text_result serialize_json(const reflection::type& type, const void* object,
                                         const serialization::context& context) {
  serialization::json_writer writer;
  const error status = serialize(type, object, writer, context);
  if (status) {
    return {status, {}};
  }

  return {status, writer.text()};
}

serialization_text_result serialize_json(type_id id, const void* object) {
  return serialize_json(id, object, serialization::context{});
}

serialization_text_result serialize_json(type_id id, const void* object,
                                         const serialization::context& context) {
  serialization::json_writer writer;
  const error status = serialize(id, object, writer, context);
  if (status) {
    return {status, {}};
  }

  return {status, writer.text()};
}

} // namespace detail

variant_result parse_json(std::string_view text) { return parse_json(text, json_options{}); }

variant_result parse_json(std::string_view text, json_options) {
  return detail::parse_json_value(text);
}

} // namespace caors::serialization
