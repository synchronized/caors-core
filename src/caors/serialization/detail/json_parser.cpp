// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "internal.h"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <string>
#include <string_view>

namespace caors::serialization::detail {
namespace {

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

      members.push_back(value::variant_member{core::string_name{name}, 0, std::move(value.value)});
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

variant_result parse_json_value(std::string_view text) {
  json_parser parser{text};
  return parser.parse();
}

} // namespace caors::serialization::detail
