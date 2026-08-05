// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "internal.h"

#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace caors::serialization::detail {
namespace {

class json_text_token_reader {
public:
  explicit json_text_token_reader(std::string_view text) noexcept : text_(text) {}

  [[nodiscard]] bool eof() const noexcept { return position_ >= text_.size(); }
  [[nodiscard]] error status() const noexcept { return {}; }

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
      case 'u': {
        const error status = parse_unicode_escape(output);
        if (status) {
          return status;
        }
        break;
      }
      default:
        return {error_domain::serialization, errc::invalid_argument};
      }
    }

    return {error_domain::serialization, errc::invalid_argument};
  }

  [[nodiscard]] variant_result parse_number(memory::memory_resource* result_resource) {
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
      return {{}, variant::from(value, result_resource)};
    }

    if (!number.empty() && number.front() == '-') {
      std::int64_t value = 0;
      const auto [_, ec] = std::from_chars(number.data(), number.data() + number.size(), value);
      if (ec != std::errc{}) {
        return {{error_domain::serialization, errc::invalid_argument}, {}};
      }
      return {{}, variant::from(value, result_resource)};
    }

    std::uint64_t value = 0;
    const auto [_, ec] = std::from_chars(number.data(), number.data() + number.size(), value);
    if (ec != std::errc{}) {
      return {{error_domain::serialization, errc::invalid_argument}, {}};
    }
    return {{}, variant::from(value, result_resource)};
  }

private:
  [[nodiscard]] static bool is_high_surrogate(std::uint32_t value) noexcept {
    return value >= 0xD800U && value <= 0xDBFFU;
  }

  [[nodiscard]] static bool is_low_surrogate(std::uint32_t value) noexcept {
    return value >= 0xDC00U && value <= 0xDFFFU;
  }

  [[nodiscard]] static bool append_utf8(core::string& output, std::uint32_t code_point) {
    if (code_point <= 0x7FU) {
      output.push_back(static_cast<char>(code_point));
      return true;
    }

    if (code_point <= 0x7FFU) {
      output.push_back(static_cast<char>(0xC0U | (code_point >> 6U)));
      output.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
      return true;
    }

    if (code_point <= 0xFFFFU) {
      output.push_back(static_cast<char>(0xE0U | (code_point >> 12U)));
      output.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU)));
      output.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
      return true;
    }

    if (code_point <= 0x10FFFFU) {
      output.push_back(static_cast<char>(0xF0U | (code_point >> 18U)));
      output.push_back(static_cast<char>(0x80U | ((code_point >> 12U) & 0x3FU)));
      output.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU)));
      output.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
      return true;
    }

    return false;
  }

  [[nodiscard]] bool parse_hex4(std::uint32_t& value) noexcept {
    if (position_ + 4U > text_.size()) {
      return false;
    }

    value = 0;
    for (std::size_t index = 0; index < 4U; ++index) {
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

    return true;
  }

  [[nodiscard]] error parse_unicode_escape(core::string& output) {
    std::uint32_t value = 0;
    if (!parse_hex4(value)) {
      return {error_domain::serialization, errc::invalid_argument};
    }

    if (is_high_surrogate(value)) {
      if (position_ + 6U > text_.size() || text_[position_] != '\\' ||
          text_[position_ + 1U] != 'u') {
        return {error_domain::serialization, errc::invalid_argument};
      }

      position_ += 2U;
      std::uint32_t low = 0;
      if (!parse_hex4(low) || !is_low_surrogate(low)) {
        return {error_domain::serialization, errc::invalid_argument};
      }

      value = 0x10000U + (((value - 0xD800U) << 10U) | (low - 0xDC00U));
    } else if (is_low_surrogate(value)) {
      return {error_domain::serialization, errc::invalid_argument};
    }

    return append_utf8(output, value) ? error{}
                                      : error{error_domain::serialization, errc::invalid_argument};
  }

  std::string_view text_{};
  std::size_t position_ = 0;
};

class json_stream_token_reader {
public:
  explicit json_stream_token_reader(io::input_stream& stream) noexcept : stream_(&stream) {}

  [[nodiscard]] bool eof() {
    fill();
    return !current_loaded_ && !status_;
  }

  [[nodiscard]] error status() const noexcept { return status_; }

  [[nodiscard]] char peek() {
    fill();
    return current_loaded_ ? current_ : '\0';
  }

  [[nodiscard]] bool consume(char expected) {
    if (peek() != expected) {
      return false;
    }

    advance();
    return true;
  }

  [[nodiscard]] bool consume_text(std::string_view expected) {
    for (char character : expected) {
      if (!consume(character)) {
        return false;
      }
    }
    return true;
  }

  void skip_space() {
    while (!eof()) {
      switch (peek()) {
      case ' ':
      case '\n':
      case '\r':
      case '\t':
        advance();
        break;
      default:
        return;
      }
    }
  }

  [[nodiscard]] error parse_string(core::string& output) {
    if (!consume('"')) {
      return fail_status_or_invalid();
    }

    output.clear();
    while (!eof()) {
      const char character = peek();
      advance();
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
        return fail_status_or_invalid();
      }

      const char escaped = peek();
      advance();
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
      case 'u': {
        const error status = parse_unicode_escape(output);
        if (status) {
          return status;
        }
        break;
      }
      default:
        return {error_domain::serialization, errc::invalid_argument};
      }
    }

    return fail_status_or_invalid();
  }

  [[nodiscard]] variant_result parse_number(memory::memory_resource* result_resource) {
    core::string number;
    bool floating = false;

    if (consume('-')) {
      number.push_back('-');
      if (peek() < '0' || peek() > '9') {
        return {fail_status_or_invalid(), {}};
      }
    }

    if (consume('0')) {
      number.push_back('0');
      if (peek() >= '0' && peek() <= '9') {
        return {{error_domain::serialization, errc::invalid_argument}, {}};
      }
    } else {
      if (peek() < '1' || peek() > '9') {
        return {fail_status_or_invalid(), {}};
      }
      while (peek() >= '0' && peek() <= '9') {
        number.push_back(peek());
        advance();
      }
    }

    if (consume('.')) {
      floating = true;
      number.push_back('.');
      if (peek() < '0' || peek() > '9') {
        return {fail_status_or_invalid(), {}};
      }
      while (peek() >= '0' && peek() <= '9') {
        number.push_back(peek());
        advance();
      }
    }

    if (peek() == 'e' || peek() == 'E') {
      floating = true;
      number.push_back(peek());
      advance();
      if (peek() == '+' || peek() == '-') {
        number.push_back(peek());
        advance();
      }
      if (peek() < '0' || peek() > '9') {
        return {fail_status_or_invalid(), {}};
      }
      while (peek() >= '0' && peek() <= '9') {
        number.push_back(peek());
        advance();
      }
    }

    if (floating) {
      double value = 0.0;
      const std::string text{number.data(), number.size()};
      char* end = nullptr;
      value = std::strtod(text.c_str(), &end);
      if (end != text.c_str() + text.size() || !std::isfinite(value)) {
        return {{error_domain::serialization, errc::invalid_argument}, {}};
      }
      return {{}, variant::from(value, result_resource)};
    }

    if (!number.empty() && number.front() == '-') {
      std::int64_t value = 0;
      const auto [_, ec] = std::from_chars(number.data(), number.data() + number.size(), value);
      if (ec != std::errc{}) {
        return {{error_domain::serialization, errc::invalid_argument}, {}};
      }
      return {{}, variant::from(value, result_resource)};
    }

    std::uint64_t value = 0;
    const auto [_, ec] = std::from_chars(number.data(), number.data() + number.size(), value);
    if (ec != std::errc{}) {
      return {{error_domain::serialization, errc::invalid_argument}, {}};
    }
    return {{}, variant::from(value, result_resource)};
  }

private:
  [[nodiscard]] static bool is_high_surrogate(std::uint32_t value) noexcept {
    return value >= 0xD800U && value <= 0xDBFFU;
  }

  [[nodiscard]] static bool is_low_surrogate(std::uint32_t value) noexcept {
    return value >= 0xDC00U && value <= 0xDFFFU;
  }

  [[nodiscard]] static bool append_utf8(core::string& output, std::uint32_t code_point) {
    if (code_point <= 0x7FU) {
      output.push_back(static_cast<char>(code_point));
      return true;
    }

    if (code_point <= 0x7FFU) {
      output.push_back(static_cast<char>(0xC0U | (code_point >> 6U)));
      output.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
      return true;
    }

    if (code_point <= 0xFFFFU) {
      output.push_back(static_cast<char>(0xE0U | (code_point >> 12U)));
      output.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU)));
      output.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
      return true;
    }

    if (code_point <= 0x10FFFFU) {
      output.push_back(static_cast<char>(0xF0U | (code_point >> 18U)));
      output.push_back(static_cast<char>(0x80U | ((code_point >> 12U) & 0x3FU)));
      output.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU)));
      output.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
      return true;
    }

    return false;
  }

  [[nodiscard]] error fail_status_or_invalid() const noexcept {
    return status_ ? status_ : error{error_domain::serialization, errc::invalid_argument};
  }

  void fill() {
    if (current_loaded_ || status_) {
      return;
    }

    std::byte byte{};
    io::size_result read = stream_->read(std::span<std::byte>{&byte, 1U});
    if (read.failed()) {
      status_ = rethrow_as(read.error(), error_domain::serialization);
      return;
    }

    if (read.value == 0U) {
      return;
    }

    current_ = static_cast<char>(std::to_integer<unsigned char>(byte));
    current_loaded_ = true;
  }

  void advance() {
    fill();
    current_loaded_ = false;
  }

  [[nodiscard]] bool parse_hex4(std::uint32_t& value) {
    value = 0;
    for (std::size_t index = 0; index < 4U; ++index) {
      if (eof()) {
        return false;
      }

      const char digit = peek();
      advance();
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

    return true;
  }

  [[nodiscard]] error parse_unicode_escape(core::string& output) {
    std::uint32_t value = 0;
    if (!parse_hex4(value)) {
      return fail_status_or_invalid();
    }

    if (is_high_surrogate(value)) {
      if (!consume('\\') || !consume('u')) {
        return fail_status_or_invalid();
      }

      std::uint32_t low = 0;
      if (!parse_hex4(low) || !is_low_surrogate(low)) {
        return fail_status_or_invalid();
      }

      value = 0x10000U + (((value - 0xD800U) << 10U) | (low - 0xDC00U));
    } else if (is_low_surrogate(value)) {
      return {error_domain::serialization, errc::invalid_argument};
    }

    return append_utf8(output, value) ? error{}
                                      : error{error_domain::serialization, errc::invalid_argument};
  }

  io::input_stream* stream_ = nullptr;
  error status_{};
  char current_ = '\0';
  bool current_loaded_ = false;
};

template <typename TokenReader> class json_parser {
public:
  json_parser(TokenReader input, json_options options,
              memory::memory_resource* result_resource) noexcept
      : input_(std::move(input)), options_(options), result_resource_(result_resource) {}

  [[nodiscard]] variant_result parse() {
    input_.skip_space();
    variant_result value = parse_value(0);
    if (value.failed()) {
      return value;
    }

    input_.skip_space();
    if (input_.status()) {
      return {input_.status(), {}};
    }

    if (!input_.eof()) {
      return {{error_domain::serialization, errc::invalid_argument}, {}};
    }

    return value;
  }

private:
  [[nodiscard]] bool at_container_depth_limit(std::uint32_t depth) const noexcept {
    return options_.max_parse_depth != 0 && depth >= options_.max_parse_depth;
  }

  [[nodiscard]] variant_result parse_value(std::uint32_t depth) {
    input_.skip_space();
    switch (input_.peek()) {
    case '{':
      if (at_container_depth_limit(depth)) {
        return {{error_domain::serialization, errc::recursion_limit}, {}};
      }
      return parse_object(depth + 1U);
    case '[':
      if (at_container_depth_limit(depth)) {
        return {{error_domain::serialization, errc::recursion_limit}, {}};
      }
      return parse_array(depth + 1U);
    case '"': {
      core::string value;
      const error status = input_.parse_string(value);
      return status ? variant_result{status, {}}
                    : variant_result{{}, variant::from(value, result_resource_)};
    }
    case 't':
      if (input_.consume_text("true")) {
        return {{}, variant::from(true, result_resource_)};
      }
      break;
    case 'f':
      if (input_.consume_text("false")) {
        return {{}, variant::from(false, result_resource_)};
      }
      break;
    case 'n':
      if (input_.consume_text("null")) {
        return {{}, variant::null(result_resource_)};
      }
      break;
    default:
      if (input_.peek() == '-' || (input_.peek() >= '0' && input_.peek() <= '9')) {
        return input_.parse_number(result_resource_);
      }
      break;
    }

    return {{error_domain::serialization, errc::invalid_argument}, {}};
  }

  [[nodiscard]] variant_result parse_object(std::uint32_t depth) {
    if (!input_.consume('{')) {
      return {{error_domain::serialization, errc::invalid_argument}, {}};
    }

    variant::object_type members{variant::object_type::allocator_type{result_resource_}};
    input_.skip_space();
    if (input_.consume('}')) {
      return {{}, variant::object(std::move(members))};
    }

    while (true) {
      core::string name_text;
      error status = input_.parse_string(name_text);
      if (status) {
        return {status, {}};
      }
      core::string_name name{name_text};

      input_.skip_space();
      if (!input_.consume(':')) {
        return {{error_domain::serialization, errc::invalid_argument}, {}};
      }

      variant_result value = parse_value(depth);
      if (value.failed()) {
        return value;
      }

      status = add_object_member(members, name, std::move(value.value));
      if (status) {
        return {status, {}};
      }

      input_.skip_space();
      if (input_.consume('}')) {
        break;
      }

      if (!input_.consume(',')) {
        return {{error_domain::serialization, errc::invalid_argument}, {}};
      }

      input_.skip_space();
      if (input_.consume('}')) {
        if (!options_.allow_trailing_commas) {
          return {{error_domain::serialization, errc::invalid_argument}, {}};
        }
        break;
      }
    }

    return {{}, variant::object(std::move(members))};
  }

  [[nodiscard]] variant_result parse_array(std::uint32_t depth) {
    if (!input_.consume('[')) {
      return {{error_domain::serialization, errc::invalid_argument}, {}};
    }

    variant::array_type values{variant::array_type::allocator_type{result_resource_}};
    input_.skip_space();
    if (input_.consume(']')) {
      return {{}, variant::array(std::move(values))};
    }

    while (true) {
      variant_result value = parse_value(depth);
      if (value.failed()) {
        return value;
      }

      values.push_back(std::move(value.value));
      input_.skip_space();
      if (input_.consume(']')) {
        break;
      }

      if (!input_.consume(',')) {
        return {{error_domain::serialization, errc::invalid_argument}, {}};
      }

      input_.skip_space();
      if (input_.consume(']')) {
        if (!options_.allow_trailing_commas) {
          return {{error_domain::serialization, errc::invalid_argument}, {}};
        }
        break;
      }
    }

    return {{}, variant::array(std::move(values))};
  }

  [[nodiscard]] error add_object_member(variant::object_type& members, core::string_name name,
                                        variant value) const {
    for (meta::variant_member& member : members) {
      if (member.name != name) {
        continue;
      }

      switch (options_.duplicate_key_policy) {
      case json_duplicate_key_policy::keep_all:
        break;
      case json_duplicate_key_policy::use_first:
        return {};
      case json_duplicate_key_policy::use_last:
        member.value = std::move(value);
        return {};
      case json_duplicate_key_policy::reject:
        return {error_domain::serialization, errc::duplicate_name};
      }
      break;
    }

    members.push_back(meta::variant_member{name, 0, std::move(value)});
    return {};
  }

  TokenReader input_;
  json_options options_{};
  memory::memory_resource* result_resource_ = nullptr;
};

} // namespace

variant_result parse_json_value(std::string_view text, json_options options,
                                memory::memory_resource* result_resource) {
  json_parser parser{json_text_token_reader{text}, options, result_resource};
  return parser.parse();
}

variant_result parse_json_stream(io::input_stream& stream, json_options options,
                                 memory::memory_resource* result_resource) {
  json_parser parser{json_stream_token_reader{stream}, options, result_resource};
  return parser.parse();
}

} // namespace caors::serialization::detail
