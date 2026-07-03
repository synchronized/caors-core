// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/operations.h"

#include <charconv>
#include <cmath>
#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>

namespace caors::serialization {
namespace {

[[nodiscard]] core::string_name
serialized_field_name(const serialization::field_data& data) noexcept {
  return data.serialized_name.empty() ? data.field_name : data.serialized_name;
}

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

class json_parser {
public:
  explicit json_parser(std::string_view text) noexcept : text_(text) {}

  [[nodiscard]] variant_result parse() {
    skip_space();
    variant_result value = parse_value();
    if (value.failed()) {
      return value;
    }

    skip_space();
    if (!eof()) {
      return {{error_domain::serialization, errc::invalid_argument}, {}};
    }

    return value;
  }

private:
  [[nodiscard]] bool eof() const noexcept { return position_ >= text_.size(); }

  [[nodiscard]] char peek() const noexcept { return eof() ? '\0' : text_[position_]; }

  [[nodiscard]] bool consume(char expected) noexcept {
    if (peek() != expected) {
      return false;
    }

    ++position_;
    return true;
  }

  [[nodiscard]] bool consume_text(std::string_view expected) noexcept {
    if (text_.substr(position_, expected.size()) != expected) {
      return false;
    }

    position_ += expected.size();
    return true;
  }

  void skip_space() noexcept {
    while (!eof()) {
      switch (peek()) {
      case ' ':
      case '\n':
      case '\r':
      case '\t':
        ++position_;
        break;
      default:
        return;
      }
    }
  }

  [[nodiscard]] variant_result parse_value() {
    skip_space();
    switch (peek()) {
    case '{':
      return parse_object();
    case '[':
      return parse_array();
    case '"': {
      core::string value;
      const error status = parse_string(value);
      return status ? variant_result{status, {}} : variant_result{{}, variant::from(value)};
    }
    case 't':
      if (consume_text("true")) {
        return {{}, variant::from(true)};
      }
      break;
    case 'f':
      if (consume_text("false")) {
        return {{}, variant::from(false)};
      }
      break;
    case 'n':
      if (consume_text("null")) {
        return {{}, variant::null()};
      }
      break;
    default:
      if (peek() == '-' || (peek() >= '0' && peek() <= '9')) {
        return parse_number();
      }
      break;
    }

    return {{error_domain::serialization, errc::invalid_argument}, {}};
  }

  [[nodiscard]] variant_result parse_object() {
    if (!consume('{')) {
      return {{error_domain::serialization, errc::invalid_argument}, {}};
    }

    variant::object_type members;
    skip_space();
    if (consume('}')) {
      return {{}, variant::object(std::move(members))};
    }

    while (true) {
      core::string name;
      error status = parse_string(name);
      if (status) {
        return {status, {}};
      }

      skip_space();
      if (!consume(':')) {
        return {{error_domain::serialization, errc::invalid_argument}, {}};
      }

      variant_result value = parse_value();
      if (value.failed()) {
        return value;
      }

      members.push_back(variant_member{core::string_name{name}, 0, std::move(value.value)});
      skip_space();
      if (consume('}')) {
        break;
      }

      if (!consume(',')) {
        return {{error_domain::serialization, errc::invalid_argument}, {}};
      }
      skip_space();
    }

    return {{}, variant::object(std::move(members))};
  }

  [[nodiscard]] variant_result parse_array() {
    if (!consume('[')) {
      return {{error_domain::serialization, errc::invalid_argument}, {}};
    }

    variant::array_type values;
    skip_space();
    if (consume(']')) {
      return {{}, variant::array(std::move(values))};
    }

    while (true) {
      variant_result value = parse_value();
      if (value.failed()) {
        return value;
      }

      values.push_back(std::move(value.value));
      skip_space();
      if (consume(']')) {
        break;
      }

      if (!consume(',')) {
        return {{error_domain::serialization, errc::invalid_argument}, {}};
      }
      skip_space();
    }

    return {{}, variant::array(std::move(values))};
  }

  [[nodiscard]] error parse_string(core::string& output) {
    if (!consume('"')) {
      return {error_domain::serialization, errc::invalid_argument};
    }

    output.clear();
    while (!eof()) {
      const char character = text_[position_++];
      if (character == '"') {
        return {};
      }

      if (character != '\\') {
        if (static_cast<unsigned char>(character) < 0x20U) {
          return {error_domain::serialization, errc::invalid_argument};
        }
        output.push_back(character);
        continue;
      }

      if (eof()) {
        return {error_domain::serialization, errc::invalid_argument};
      }

      const char escaped = text_[position_++];
      switch (escaped) {
      case '"':
      case '\\':
      case '/':
        output.push_back(escaped);
        break;
      case 'b':
        output.push_back('\b');
        break;
      case 'f':
        output.push_back('\f');
        break;
      case 'n':
        output.push_back('\n');
        break;
      case 'r':
        output.push_back('\r');
        break;
      case 't':
        output.push_back('\t');
        break;
      case 'u':
        if (!parse_ascii_unicode_escape(output)) {
          return {error_domain::serialization, errc::unsupported};
        }
        break;
      default:
        return {error_domain::serialization, errc::invalid_argument};
      }
    }

    return {error_domain::serialization, errc::invalid_argument};
  }

  [[nodiscard]] bool parse_ascii_unicode_escape(core::string& output) {
    if (position_ + 4 > text_.size()) {
      return false;
    }

    std::uint32_t value = 0;
    for (std::size_t index = 0; index < 4; ++index) {
      const char digit = text_[position_++];
      value <<= 4U;
      if (digit >= '0' && digit <= '9') {
        value += static_cast<std::uint32_t>(digit - '0');
      } else if (digit >= 'a' && digit <= 'f') {
        value += static_cast<std::uint32_t>(digit - 'a' + 10);
      } else if (digit >= 'A' && digit <= 'F') {
        value += static_cast<std::uint32_t>(digit - 'A' + 10);
      } else {
        return false;
      }
    }

    if (value > 0x7FU) {
      return false;
    }

    output.push_back(static_cast<char>(value));
    return true;
  }

  [[nodiscard]] variant_result parse_number() {
    const std::size_t begin = position_;
    if (consume('-')) {
      if (peek() < '0' || peek() > '9') {
        return {{error_domain::serialization, errc::invalid_argument}, {}};
      }
    }

    if (consume('0')) {
      if (peek() >= '0' && peek() <= '9') {
        return {{error_domain::serialization, errc::invalid_argument}, {}};
      }
    } else {
      if (peek() < '1' || peek() > '9') {
        return {{error_domain::serialization, errc::invalid_argument}, {}};
      }
      while (peek() >= '0' && peek() <= '9') {
        ++position_;
      }
    }

    bool floating = false;
    if (consume('.')) {
      floating = true;
      if (peek() < '0' || peek() > '9') {
        return {{error_domain::serialization, errc::invalid_argument}, {}};
      }
      while (peek() >= '0' && peek() <= '9') {
        ++position_;
      }
    }

    if (peek() == 'e' || peek() == 'E') {
      floating = true;
      ++position_;
      if (peek() == '+' || peek() == '-') {
        ++position_;
      }
      if (peek() < '0' || peek() > '9') {
        return {{error_domain::serialization, errc::invalid_argument}, {}};
      }
      while (peek() >= '0' && peek() <= '9') {
        ++position_;
      }
    }

    const std::string_view number = text_.substr(begin, position_ - begin);
    if (floating) {
      double value = 0.0;
      const std::string text{number};
      char* end = nullptr;
      value = std::strtod(text.c_str(), &end);
      if (end != text.c_str() + text.size() || !std::isfinite(value)) {
        return {{error_domain::serialization, errc::invalid_argument}, {}};
      }
      return {{}, variant::from(value)};
    }

    if (!number.empty() && number.front() == '-') {
      std::int64_t value = 0;
      const auto [_, ec] = std::from_chars(number.data(), number.data() + number.size(), value);
      if (ec != std::errc{}) {
        return {{error_domain::serialization, errc::invalid_argument}, {}};
      }
      return {{}, variant::from(value)};
    }

    std::uint64_t value = 0;
    const auto [_, ec] = std::from_chars(number.data(), number.data() + number.size(), value);
    if (ec != std::errc{}) {
      return {{error_domain::serialization, errc::invalid_argument}, {}};
    }
    return {{}, variant::from(value)};
  }

  std::string_view text_{};
  std::size_t position_ = 0;
};

} // namespace

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

  text_.push_back('{');
  contexts_.push_back(context{context_kind::object, true});
  mark_value_written();
  return {};
}

error serialization::json_writer::begin_field(const serialization::field_data& field) {
  if (contexts_.empty() || contexts_.back().kind != context_kind::object || expecting_value_) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  const core::string_name name = serialized_field_name(field);
  if (name.empty()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  context& current = contexts_.back();
  if (!current.first) {
    text_.push_back(',');
  }

  append_json_string(text_, name.view());
  text_.push_back(':');
  current.first = false;
  expecting_value_ = true;
  return {};
}

error serialization::json_writer::begin_array(std::size_t) {
  error status = prepare_value();
  if (status) {
    return status;
  }

  text_.push_back('[');
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
    text_.push_back(',');
  }

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

  text_ += value ? "true" : "false";
  mark_value_written();
  return {};
}

error serialization::json_writer::write_i64(std::int64_t value) {
  error status = prepare_value();
  if (status) {
    return status;
  }

  append_text(text_, std::to_string(value));
  mark_value_written();
  return {};
}

error serialization::json_writer::write_u64(std::uint64_t value) {
  error status = prepare_value();
  if (status) {
    return status;
  }

  append_text(text_, std::to_string(value));
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
  append_text(text_, output.str());
  mark_value_written();
  return {};
}

error serialization::json_writer::write_string(std::string_view value) {
  error status = prepare_value();
  if (status) {
    return status;
  }

  append_json_string(text_, value);
  mark_value_written();
  return {};
}

error serialization::json_writer::write_null() {
  error status = prepare_value();
  if (status) {
    return status;
  }

  text_ += "null";
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

  text_.push_back(']');
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

  text_.push_back('}');
  contexts_.pop_back();
  return {};
}

void serialization::json_writer::reset() {
  text_.clear();
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

variant_result parse_json(std::string_view text) {
  json_parser parser{text};
  return parser.parse();
}

} // namespace caors::serialization
