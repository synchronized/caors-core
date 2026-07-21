// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/json_reader.h"

#include "detail/polymorphic.h"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <string>

namespace caors::serialization {
namespace {

[[nodiscard]] error invalid_json() { return {error_domain::serialization, errc::invalid_argument}; }

[[nodiscard]] core::string_name serialized_field_name(const field_schema& field) noexcept {
  return field.serialized_name.empty() ? field.field_name : field.serialized_name;
}

struct parsed_json_span {
  std::size_t begin = 0;
  std::size_t end = 0;
  bool valid = false;
};

class json_span_cursor {
public:
  json_span_cursor(std::string_view text, json_options options, std::size_t begin,
                   std::size_t end) noexcept
      : text_(text), options_(options), position_(begin), end_(end) {}

  [[nodiscard]] bool eof() const noexcept { return position_ >= end_; }
  [[nodiscard]] std::size_t position() const noexcept { return position_; }
  [[nodiscard]] char peek() const noexcept { return eof() ? '\0' : text_[position_]; }

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

  [[nodiscard]] bool consume(char expected) noexcept {
    if (peek() != expected) {
      return false;
    }

    ++position_;
    return true;
  }

  [[nodiscard]] bool consume_text(std::string_view expected) noexcept {
    if (end_ - position_ < expected.size() ||
        text_.substr(position_, expected.size()) != expected) {
      return false;
    }

    position_ += expected.size();
    return true;
  }

  [[nodiscard]] error parse_string(core::string& output) {
    if (!consume('"')) {
      return invalid_json();
    }

    output.clear();
    while (!eof()) {
      const char character = text_[position_++];
      if (character == '"') {
        return {};
      }

      if (character != '\\') {
        if (static_cast<unsigned char>(character) < 0x20U) {
          return invalid_json();
        }
        output.push_back(character);
        continue;
      }

      if (eof()) {
        return invalid_json();
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
        return invalid_json();
      }
    }

    return invalid_json();
  }

  [[nodiscard]] error skip_value(std::size_t depth, parsed_json_span* span = nullptr) {
    skip_space();
    const std::size_t begin = position_;
    switch (peek()) {
    case '{':
      if (at_container_depth_limit(depth)) {
        return {error_domain::serialization, errc::recursion_limit};
      }
      if (const error status = skip_object(depth + 1U); status) {
        return status;
      }
      break;
    case '[':
      if (at_container_depth_limit(depth)) {
        return {error_domain::serialization, errc::recursion_limit};
      }
      if (const error status = skip_array(depth + 1U); status) {
        return status;
      }
      break;
    case '"': {
      core::string ignored;
      if (const error status = parse_string(ignored); status) {
        return status;
      }
      break;
    }
    case 't':
      if (!consume_text("true")) {
        return invalid_json();
      }
      break;
    case 'f':
      if (!consume_text("false")) {
        return invalid_json();
      }
      break;
    case 'n':
      if (!consume_text("null")) {
        return invalid_json();
      }
      break;
    default:
      if (peek() == '-' || (peek() >= '0' && peek() <= '9')) {
        if (const error status = skip_number(); status) {
          return status;
        }
        break;
      }
      return invalid_json();
    }

    if (span != nullptr) {
      *span = parsed_json_span{begin, position_, true};
    }
    return {};
  }

  template <typename AddMember>
  [[nodiscard]] error scan_object_members(AddMember&& add_member, std::size_t depth) {
    skip_space();
    if (!consume('{')) {
      return invalid_json();
    }

    skip_space();
    if (consume('}')) {
      skip_space();
      return eof() ? error{} : invalid_json();
    }

    while (true) {
      core::string name_text;
      error status = parse_string(name_text);
      if (status) {
        return status;
      }
      core::string_name name{name_text};

      skip_space();
      if (!consume(':')) {
        return invalid_json();
      }

      parsed_json_span value{};
      status = skip_value(depth, &value);
      if (status) {
        return status;
      }

      status = add_member(name, value);
      if (status) {
        return status;
      }

      skip_space();
      if (consume('}')) {
        break;
      }

      if (!consume(',')) {
        return invalid_json();
      }

      skip_space();
      if (consume('}')) {
        if (!options_.allow_trailing_commas) {
          return invalid_json();
        }
        break;
      }
    }

    skip_space();
    return eof() ? error{} : invalid_json();
  }

  [[nodiscard]] error scan_array_elements(vector<parsed_json_span>& elements, std::size_t depth) {
    skip_space();
    if (!consume('[')) {
      return invalid_json();
    }

    skip_space();
    if (consume(']')) {
      skip_space();
      return eof() ? error{} : invalid_json();
    }

    while (true) {
      parsed_json_span value{};
      error status = skip_value(depth, &value);
      if (status) {
        return status;
      }
      elements.push_back(value);

      skip_space();
      if (consume(']')) {
        break;
      }

      if (!consume(',')) {
        return invalid_json();
      }

      skip_space();
      if (consume(']')) {
        if (!options_.allow_trailing_commas) {
          return invalid_json();
        }
        break;
      }
    }

    skip_space();
    return eof() ? error{} : invalid_json();
  }

  [[nodiscard]] error read_number(bool& floating, std::string_view& value) {
    skip_space();
    const std::size_t begin = position_;
    floating = false;

    if (consume('-')) {
      if (peek() < '0' || peek() > '9') {
        return invalid_json();
      }
    }

    if (consume('0')) {
      if (peek() >= '0' && peek() <= '9') {
        return invalid_json();
      }
    } else {
      if (peek() < '1' || peek() > '9') {
        return invalid_json();
      }
      while (peek() >= '0' && peek() <= '9') {
        ++position_;
      }
    }

    if (consume('.')) {
      floating = true;
      if (peek() < '0' || peek() > '9') {
        return invalid_json();
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
        return invalid_json();
      }
      while (peek() >= '0' && peek() <= '9') {
        ++position_;
      }
    }

    value = text_.substr(begin, position_ - begin);
    skip_space();
    return eof() ? error{} : invalid_json();
  }

private:
  [[nodiscard]] bool at_container_depth_limit(std::size_t depth) const noexcept {
    return options_.max_parse_depth != 0 && depth >= options_.max_parse_depth;
  }

  [[nodiscard]] error skip_object(std::size_t depth) {
    if (!consume('{')) {
      return invalid_json();
    }

    skip_space();
    if (consume('}')) {
      return {};
    }

    while (true) {
      core::string ignored;
      error status = parse_string(ignored);
      if (status) {
        return status;
      }

      skip_space();
      if (!consume(':')) {
        return invalid_json();
      }

      status = skip_value(depth);
      if (status) {
        return status;
      }

      skip_space();
      if (consume('}')) {
        return {};
      }

      if (!consume(',')) {
        return invalid_json();
      }

      skip_space();
      if (consume('}')) {
        return options_.allow_trailing_commas ? error{} : invalid_json();
      }
    }
  }

  [[nodiscard]] error skip_array(std::size_t depth) {
    if (!consume('[')) {
      return invalid_json();
    }

    skip_space();
    if (consume(']')) {
      return {};
    }

    while (true) {
      error status = skip_value(depth);
      if (status) {
        return status;
      }

      skip_space();
      if (consume(']')) {
        return {};
      }

      if (!consume(',')) {
        return invalid_json();
      }

      skip_space();
      if (consume(']')) {
        return options_.allow_trailing_commas ? error{} : invalid_json();
      }
    }
  }

  [[nodiscard]] error skip_number() {
    if (consume('-')) {
      if (peek() < '0' || peek() > '9') {
        return invalid_json();
      }
    }

    if (consume('0')) {
      if (peek() >= '0' && peek() <= '9') {
        return invalid_json();
      }
    } else {
      if (peek() < '1' || peek() > '9') {
        return invalid_json();
      }
      while (peek() >= '0' && peek() <= '9') {
        ++position_;
      }
    }

    if (consume('.')) {
      if (peek() < '0' || peek() > '9') {
        return invalid_json();
      }
      while (peek() >= '0' && peek() <= '9') {
        ++position_;
      }
    }

    if (peek() == 'e' || peek() == 'E') {
      ++position_;
      if (peek() == '+' || peek() == '-') {
        ++position_;
      }
      if (peek() < '0' || peek() > '9') {
        return invalid_json();
      }
      while (peek() >= '0' && peek() <= '9') {
        ++position_;
      }
    }

    return {};
  }

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
    if (position_ + 4U > end_) {
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
      return invalid_json();
    }

    if (is_high_surrogate(value)) {
      if (position_ + 6U > end_ || text_[position_] != '\\' || text_[position_ + 1U] != 'u') {
        return invalid_json();
      }

      position_ += 2U;
      std::uint32_t low = 0;
      if (!parse_hex4(low) || !is_low_surrogate(low)) {
        return invalid_json();
      }

      value = 0x10000U + (((value - 0xD800U) << 10U) | (low - 0xDC00U));
    } else if (is_low_surrogate(value)) {
      return invalid_json();
    }

    return append_utf8(output, value) ? error{} : invalid_json();
  }

  std::string_view text_{};
  json_options options_{};
  std::size_t position_ = 0;
  std::size_t end_ = 0;
};

} // namespace

json_reader::json_reader(std::string_view text, json_options options)
    : text_(text), options_(options), root_{0, text.size(), true} {}

format_features json_reader::features() const noexcept {
  return format_features::named_fields | format_features::ordered_fields |
         format_features::self_describing;
}

error json_reader::begin_object(const type_schema& type) {
  const json_span* value = current_span();
  if (value == nullptr || !value->valid) {
    return invalid_json();
  }

  object_context context{};
  context.members.reserve(type.fields.size());
  const error status = scan_object(*value, context.members, current_container_depth() + 1U);
  if (status) {
    return status;
  }

  object_stack_.push_back(std::move(context));
  return {};
}

error json_reader::begin_field(const field_schema& field) {
  if (object_stack_.empty() || object_stack_.back().current != nullptr) {
    return invalid_json();
  }

  const string_name name = serialized_field_name(field);
  object_context& context = object_stack_.back();
  if (context.next_member < context.members.size() && !name.empty() &&
      context.members[context.next_member].name == name) {
    context.current = &context.members[context.next_member];
    ++context.next_member;
    return {};
  }

  for (std::size_t index = 0; index < context.members.size(); ++index) {
    const object_member& member = context.members[index];
    if (!name.empty() && member.name == name) {
      context.current = &member;
      if (index >= context.next_member) {
        context.next_member = index + 1U;
      }
      return {};
    }
  }

  return {error_domain::serialization, errc::not_found};
}

error json_reader::begin_array(std::size_t& size) {
  const json_span* value = current_span();
  if (value == nullptr || !value->valid) {
    return invalid_json();
  }

  array_context context{};
  const error status = scan_array(*value, context.elements, current_container_depth() + 1U);
  if (status) {
    return status;
  }

  size = context.elements.size();
  array_stack_.push_back(std::move(context));
  return {};
}

error json_reader::begin_element(std::size_t index) {
  if (array_stack_.empty() || array_stack_.back().current != nullptr ||
      index >= array_stack_.back().elements.size()) {
    return invalid_json();
  }

  array_stack_.back().current = &array_stack_.back().elements[index];
  return {};
}

error json_reader::begin_dictionary(std::size_t& size) {
  const json_span* value = current_span();
  if (value == nullptr || !value->valid) {
    return invalid_json();
  }

  dictionary_context context{};
  const error status = scan_dictionary(*value, context.entries, current_container_depth() + 1U);
  if (status) {
    return status;
  }

  size = context.entries.size();
  dictionary_stack_.push_back(std::move(context));
  return {};
}

error json_reader::begin_entry(std::size_t index) {
  if (dictionary_stack_.empty() || dictionary_stack_.back().entry != nullptr ||
      dictionary_stack_.back().current.valid || index >= dictionary_stack_.back().entries.size()) {
    return invalid_json();
  }

  dictionary_stack_.back().entry = &dictionary_stack_.back().entries[index];
  return {};
}

error json_reader::begin_key() {
  if (dictionary_stack_.empty() || dictionary_stack_.back().entry == nullptr ||
      dictionary_stack_.back().current.valid) {
    return invalid_json();
  }

  dictionary_stack_.back().current = dictionary_stack_.back().entry->key;
  return {};
}

error json_reader::end_key() {
  if (dictionary_stack_.empty() || !dictionary_stack_.back().current.valid) {
    return invalid_json();
  }

  dictionary_stack_.back().current = {};
  return {};
}

error json_reader::begin_value() {
  if (dictionary_stack_.empty() || dictionary_stack_.back().entry == nullptr ||
      dictionary_stack_.back().current.valid) {
    return invalid_json();
  }

  dictionary_stack_.back().current = dictionary_stack_.back().entry->value;
  return {};
}

error json_reader::end_value() {
  if (dictionary_stack_.empty() || !dictionary_stack_.back().current.valid) {
    return invalid_json();
  }

  dictionary_stack_.back().current = {};
  return {};
}

error json_reader::end_entry() {
  if (dictionary_stack_.empty() || dictionary_stack_.back().entry == nullptr ||
      dictionary_stack_.back().current.valid) {
    return invalid_json();
  }

  dictionary_stack_.back().entry = nullptr;
  return {};
}

error json_reader::end_dictionary() {
  if (dictionary_stack_.empty() || dictionary_stack_.back().entry != nullptr ||
      dictionary_stack_.back().current.valid) {
    return invalid_json();
  }

  dictionary_stack_.pop_back();
  return {};
}

error json_reader::read_bool(bool& value) {
  const json_span* span = current_span();
  if (span == nullptr || !span->valid) {
    return invalid_json();
  }

  if (!span->valid || span->begin > span->end || span->end > text_.size()) {
    return invalid_json();
  }
  const std::string_view text = text_.substr(span->begin, span->end - span->begin);
  if (text == "true") {
    value = true;
    return {};
  }
  if (text == "false") {
    value = false;
    return {};
  }

  return {error_domain::serialization, errc::unsupported};
}

error json_reader::read_i64(std::int64_t& value) {
  const json_span* span = current_span();
  if (span == nullptr || !span->valid) {
    return invalid_json();
  }

  bool floating = false;
  std::string_view number;
  const error status = read_number_span(*span, floating, number);
  if (status) {
    return status;
  }
  if (floating) {
    return {error_domain::serialization, errc::unsupported};
  }

  const auto [_, ec] = std::from_chars(number.data(), number.data() + number.size(), value);
  return ec == std::errc{} ? error{} : error{error_domain::serialization, errc::unsupported};
}

error json_reader::read_u64(std::uint64_t& value) {
  const json_span* span = current_span();
  if (span == nullptr || !span->valid) {
    return invalid_json();
  }

  bool floating = false;
  std::string_view number;
  const error status = read_number_span(*span, floating, number);
  if (status) {
    return status;
  }
  if (floating || (!number.empty() && number.front() == '-')) {
    return {error_domain::serialization, errc::unsupported};
  }

  const auto [_, ec] = std::from_chars(number.data(), number.data() + number.size(), value);
  return ec == std::errc{} ? error{} : error{error_domain::serialization, errc::unsupported};
}

error json_reader::read_f64(double& value) {
  const json_span* span = current_span();
  if (span == nullptr || !span->valid) {
    return invalid_json();
  }

  bool floating = false;
  std::string_view number;
  const error status = read_number_span(*span, floating, number);
  if (status) {
    return status;
  }

  const std::string text{number};
  char* end = nullptr;
  value = std::strtod(text.c_str(), &end);
  if (end != text.c_str() + text.size() || !std::isfinite(value)) {
    return {error_domain::serialization, errc::unsupported};
  }

  return {};
}

error json_reader::read_string(string& value) {
  const json_span* span = current_span();
  return span != nullptr && span->valid ? read_string_span(*span, value) : invalid_json();
}

bool json_reader::is_null() const noexcept {
  const json_span* span = current_span();
  return span != nullptr && span->valid && span->begin <= span->end && span->end <= text_.size() &&
         text_.substr(span->begin, span->end - span->begin) == "null";
}

bool json_reader::is_polymorphic_envelope(const serialization::context& context) const noexcept {
  const json_span* span = current_span();
  if (span == nullptr || !span->valid) {
    return false;
  }

  vector<object_member> members;
  if (scan_object(*span, members, current_container_depth() + 1U)) {
    return false;
  }

  const string_name type_field = detail::polymorphic_type_field(context);
  const string_name value_field = detail::polymorphic_value_field(context);
  bool has_type = false;
  bool has_value = false;
  for (const object_member& member : members) {
    if (member.name == type_field) {
      string ignored;
      has_type = read_string_span(member.value, ignored).succeeded();
    } else if (member.name == value_field) {
      has_value = true;
    }
  }

  return has_type && has_value;
}

error json_reader::read_polymorphic_type_guid(const serialization::context& context,
                                              type_guid& value) {
  const json_span* span = current_span();
  if (span == nullptr || !span->valid) {
    return invalid_json();
  }

  vector<object_member> members;
  error status = scan_object(*span, members, current_container_depth() + 1U);
  if (status) {
    return status;
  }

  const string_name type_field = detail::polymorphic_type_field(context);
  for (const object_member& member : members) {
    if (member.name != type_field) {
      continue;
    }

    string text;
    status = read_string_span(member.value, text);
    if (status) {
      return status;
    }

    const std::optional<guid> parsed = guid::parse(std::string_view{text.data(), text.size()});
    if (!parsed.has_value()) {
      return {error_domain::serialization, errc::invalid_type};
    }
    value = type_guid{*parsed};
    return value ? error{} : error{error_domain::serialization, errc::invalid_type};
  }

  return {error_domain::serialization, errc::not_found};
}

error json_reader::begin_polymorphic_value(const serialization::context& context) {
  const json_span* span = current_span();
  if (span == nullptr || !span->valid) {
    return invalid_json();
  }

  vector<object_member> members;
  error status = scan_object(*span, members, current_container_depth() + 1U);
  if (status) {
    return status;
  }

  const string_name value_field = detail::polymorphic_value_field(context);
  for (const object_member& member : members) {
    if (member.name == value_field) {
      polymorphic_values_.push_back(member.value);
      polymorphic_object_depths_.push_back(object_stack_.size());
      return {};
    }
  }

  return {error_domain::serialization, errc::not_found};
}

error json_reader::end_polymorphic_value() {
  if (polymorphic_values_.empty() || polymorphic_object_depths_.empty()) {
    return invalid_json();
  }

  polymorphic_values_.pop_back();
  polymorphic_object_depths_.pop_back();
  return {};
}

error json_reader::end_element() {
  if (array_stack_.empty() || array_stack_.back().current == nullptr) {
    return invalid_json();
  }

  array_stack_.back().current = nullptr;
  return {};
}

error json_reader::end_array() {
  if (array_stack_.empty() || array_stack_.back().current != nullptr) {
    return invalid_json();
  }

  array_stack_.pop_back();
  return {};
}

error json_reader::end_field() {
  if (object_stack_.empty() || object_stack_.back().current == nullptr) {
    return invalid_json();
  }

  object_stack_.back().current = nullptr;
  return {};
}

error json_reader::end_object() {
  if (object_stack_.empty() || object_stack_.back().current != nullptr) {
    return invalid_json();
  }

  object_stack_.pop_back();
  return {};
}

const json_reader::json_span* json_reader::current_span() const noexcept {
  if (!dictionary_stack_.empty() && dictionary_stack_.back().current.valid) {
    return &dictionary_stack_.back().current;
  }

  if (!polymorphic_values_.empty() && object_stack_.size() == polymorphic_object_depths_.back()) {
    return &polymorphic_values_.back();
  }

  if (!object_stack_.empty() && object_stack_.back().current != nullptr &&
      object_stack_.size() > array_stack_.size()) {
    return &object_stack_.back().current->value;
  }

  if (!array_stack_.empty() && array_stack_.back().current != nullptr) {
    return array_stack_.back().current;
  }

  if (!object_stack_.empty() && object_stack_.back().current != nullptr) {
    return &object_stack_.back().current->value;
  }

  if (!polymorphic_values_.empty()) {
    return &polymorphic_values_.back();
  }

  return &root_;
}

error json_reader::scan_object(json_span span, vector<object_member>& members,
                               std::size_t depth) const {
  json_span_cursor cursor{text_, options_, span.begin, span.end};
  return cursor.scan_object_members(
      [&](string_name name, parsed_json_span value) -> error {
        for (object_member& member : members) {
          if (member.name != name) {
            continue;
          }

          switch (options_.duplicate_key_policy) {
          case json_duplicate_key_policy::keep_all:
            break;
          case json_duplicate_key_policy::use_first:
            return {};
          case json_duplicate_key_policy::use_last:
            member.value = json_span{value.begin, value.end, value.valid};
            return {};
          case json_duplicate_key_policy::reject:
            return {error_domain::serialization, errc::duplicate_name};
          }
          break;
        }

        members.push_back(object_member{name, json_span{value.begin, value.end, value.valid}});
        return {};
      },
      depth);
}

error json_reader::scan_array(json_span span, vector<json_span>& elements,
                              std::size_t depth) const {
  vector<parsed_json_span> parsed_elements;
  json_span_cursor cursor{text_, options_, span.begin, span.end};
  const error status = cursor.scan_array_elements(parsed_elements, depth);
  if (status) {
    return status;
  }

  elements.reserve(parsed_elements.size());
  for (parsed_json_span element : parsed_elements) {
    elements.push_back(json_span{element.begin, element.end, element.valid});
  }
  return {};
}

error json_reader::scan_dictionary(json_span span, vector<dictionary_entry>& entries,
                                   std::size_t depth) const {
  vector<json_span> elements;
  error status = scan_array(span, elements, depth);
  if (status) {
    return status;
  }

  entries.reserve(elements.size());
  for (json_span entry_span : elements) {
    vector<json_span> pair;
    status = scan_array(entry_span, pair, depth + 1U);
    if (status) {
      return status;
    }
    if (pair.size() != 2U) {
      return invalid_json();
    }

    entries.push_back(dictionary_entry{pair[0], pair[1]});
  }

  return {};
}

error json_reader::read_string_span(json_span span, string& value) const {
  core::string decoded;
  json_span_cursor cursor{text_, options_, span.begin, span.end};
  error status = cursor.parse_string(decoded);
  if (status) {
    return status;
  }
  cursor.skip_space();
  if (!cursor.eof()) {
    return invalid_json();
  }

  value = std::move(decoded);
  return {};
}

error json_reader::read_number_span(json_span span, bool& floating, std::string_view& value) const {
  json_span_cursor cursor{text_, options_, span.begin, span.end};
  return cursor.read_number(floating, value);
}

std::size_t json_reader::current_container_depth() const noexcept {
  return object_stack_.size() + array_stack_.size() + dictionary_stack_.size();
}

} // namespace caors::serialization
