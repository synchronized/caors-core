// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/operations.h"
#include "detail/internal.h"

#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <sstream>
#include <string_view>
#include <system_error>

namespace caors::serialization {
namespace {

[[nodiscard]] bool needs_json_escape(std::string_view value) noexcept {
  for (const unsigned char character : value) {
    if (character == '"' || character == '\\' || character < 0x20U) {
      return true;
    }
  }

  return false;
}

} // namespace

serialization::json_writer::json_writer() noexcept
    : text_(&owned_text_), contexts_(memory::allocator<context>{nullptr}) {}

serialization::json_writer::json_writer(json_options options) noexcept
    : json_writer(options, nullptr) {}

serialization::json_writer::json_writer(json_options options,
                                        memory::memory_resource* temporary_resource) noexcept
    : options_(options), text_(&owned_text_),
      contexts_(memory::allocator<context>{temporary_resource}) {}

serialization::json_writer::json_writer(string& output) : json_writer(output, json_options{}) {}

serialization::json_writer::json_writer(string& output, json_options options)
    : json_writer(output, options, nullptr) {}

serialization::json_writer::json_writer(string& output, json_options options,
                                        memory::memory_resource* temporary_resource)
    : options_(options), text_(&output), contexts_(memory::allocator<context>{temporary_resource}) {
  reset();
}

serialization::json_writer::json_writer(io::output_stream& output) noexcept
    : json_writer(output, json_options{}) {}

serialization::json_writer::json_writer(io::output_stream& output, json_options options) noexcept
    : json_writer(output, options, nullptr) {}

serialization::json_writer::json_writer(io::output_stream& output, json_options options,
                                        memory::memory_resource* temporary_resource) noexcept
    : options_(options), text_(&owned_text_), stream_(&output),
      contexts_(memory::allocator<context>{temporary_resource}) {
  reset();
}

serialization::format_features serialization::json_writer::features() const noexcept {
  return serialization::format_features::named_fields |
         serialization::format_features::ordered_fields |
         serialization::format_features::self_describing;
}

error serialization::json_writer::begin_object(const serialization::type_schema&) {
  error status = prepare_value();
  if (status) {
    return status;
  }

  status = write_char('{');
  if (status) {
    return status;
  }

  contexts_.push_back(context{context_kind::object, true});
  mark_value_written();
  return {};
}

error serialization::json_writer::begin_field(const serialization::field_schema& field) {
  if (contexts_.empty() || contexts_.back().kind != context_kind::object || expecting_value_) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  const core::string_name name = detail::serialized_field_name(field);
  if (name.empty()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  context& current = contexts_.back();
  if (!current.first) {
    error status = write_char(',');
    if (status) {
      return status;
    }
  }

  error status = write_newline_and_indent(contexts_.size());
  if (status) {
    return status;
  }
  status = write_json_string(name.view());
  if (status) {
    return status;
  }
  status = write_char(':');
  if (status) {
    return status;
  }
  if (options_.pretty) {
    status = write_char(' ');
    if (status) {
      return status;
    }
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

  status = write_char('[');
  if (status) {
    return status;
  }

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
    error status = write_char(',');
    if (status) {
      return status;
    }
  }

  error status = write_newline_and_indent(contexts_.size());
  if (status) {
    return status;
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

  status = write_text(value ? "true" : "false");
  if (status) {
    return status;
  }
  mark_value_written();
  return {};
}

error serialization::json_writer::write_i64(std::int64_t value) {
  error status = prepare_value();
  if (status) {
    return status;
  }

  std::array<char, 32> buffer{};
  const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
  if (result.ec != std::errc{}) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  status = write_text(
      std::string_view{buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data())});
  if (status) {
    return status;
  }
  mark_value_written();
  return {};
}

error serialization::json_writer::write_u64(std::uint64_t value) {
  error status = prepare_value();
  if (status) {
    return status;
  }

  std::array<char, 32> buffer{};
  const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
  if (result.ec != std::errc{}) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  status = write_text(
      std::string_view{buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data())});
  if (status) {
    return status;
  }
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

  std::array<char, 64> buffer{};
  const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value,
                                    std::chars_format::general, 6);
  if (result.ec == std::errc{}) {
    status = write_text(
        std::string_view{buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data())});
  } else {
    std::ostringstream output;
    output << value;
    status = write_text(output.str());
  }
  if (status) {
    return status;
  }
  mark_value_written();
  return {};
}

error serialization::json_writer::write_string(std::string_view value) {
  error status = prepare_value();
  if (status) {
    return status;
  }

  status = write_json_string(value);
  if (status) {
    return status;
  }
  mark_value_written();
  return {};
}

error serialization::json_writer::write_null() {
  error status = prepare_value();
  if (status) {
    return status;
  }

  status = write_text("null");
  if (status) {
    return status;
  }
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
    error status = write_newline_and_indent(contexts_.size() - 1);
    if (status) {
      return status;
    }
  }
  error status = write_char(']');
  if (status) {
    return status;
  }
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
    error status = write_newline_and_indent(contexts_.size() - 1);
    if (status) {
      return status;
    }
  }
  error status = write_char('}');
  if (status) {
    return status;
  }
  contexts_.pop_back();
  return {};
}

void serialization::json_writer::reset() {
  if (stream_ == nullptr) {
    output().clear();
  }
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

error serialization::json_writer::write_char(char value) {
  if (stream_ == nullptr) {
    output().push_back(value);
    return {};
  }

  return stream_->write_all(std::string_view{&value, 1});
}

error serialization::json_writer::write_text(std::string_view value) {
  if (stream_ == nullptr) {
    output().append(value.data(), value.size());
    return {};
  }

  return stream_->write_all(value);
}

error serialization::json_writer::write_json_string(std::string_view value) {
  error status = write_char('"');
  if (status) {
    return status;
  }

  if (!needs_json_escape(value)) {
    status = write_text(value);
    if (status) {
      return status;
    }
    return write_char('"');
  }

  constexpr char digits[] = "0123456789abcdef";
  std::size_t segment_begin = 0;
  for (std::size_t index = 0; index < value.size(); ++index) {
    const auto character = static_cast<unsigned char>(value[index]);
    std::string_view escaped{};
    char unicode_escape[6]{};

    switch (character) {
    case '"':
      escaped = "\\\"";
      break;
    case '\\':
      escaped = "\\\\";
      break;
    case '\b':
      escaped = "\\b";
      break;
    case '\f':
      escaped = "\\f";
      break;
    case '\n':
      escaped = "\\n";
      break;
    case '\r':
      escaped = "\\r";
      break;
    case '\t':
      escaped = "\\t";
      break;
    default:
      if (character < 0x20U) {
        unicode_escape[0] = '\\';
        unicode_escape[1] = 'u';
        unicode_escape[2] = digits[(character >> 12U) & 0x0FU];
        unicode_escape[3] = digits[(character >> 8U) & 0x0FU];
        unicode_escape[4] = digits[(character >> 4U) & 0x0FU];
        unicode_escape[5] = digits[character & 0x0FU];
        escaped = std::string_view{unicode_escape, sizeof(unicode_escape)};
      }
      break;
    }

    if (escaped.empty()) {
      continue;
    }

    if (segment_begin < index) {
      status = write_text(value.substr(segment_begin, index - segment_begin));
      if (status) {
        return status;
      }
    }

    status = write_text(escaped);
    if (status) {
      return status;
    }
    segment_begin = index + 1;
  }

  if (segment_begin < value.size()) {
    status = write_text(value.substr(segment_begin));
    if (status) {
      return status;
    }
  }

  return write_char('"');
}

error serialization::json_writer::write_newline_and_indent(std::size_t depth) {
  if (!options_.pretty) {
    return {};
  }

  error status = write_char('\n');
  if (status) {
    return status;
  }

  const std::size_t spaces = depth * static_cast<std::size_t>(options_.indent);
  for (std::size_t index = 0; index < spaces; ++index) {
    status = write_char(' ');
    if (status) {
      return status;
    }
  }

  return {};
}

variant_result parse_json(std::string_view text) { return parse_json(text, json_options{}); }

variant_result parse_json(std::string_view text, json_options options) {
  return detail::parse_json_value(text, options);
}

variant_result parse_json(io::input_stream& stream, memory_resource* resource) {
  return parse_json(stream, json_options{}, resource);
}

variant_result parse_json(io::input_stream& stream, json_options options, memory_resource*) {
  return detail::parse_json_stream(stream, options);
}

variant_result parse_json_from(io::file_system& file_system, io::path_view path,
                               memory_resource* resource) {
  return parse_json_from(file_system, path, json_options{}, resource);
}

variant_result parse_json_from(io::file_system& file_system, io::path_view path,
                               json_options options, memory_resource* resource) {
  io::input_stream_result stream = file_system.open_read(path, resource);
  if (stream.failed()) {
    return {rethrow_as(stream.error(), error_domain::serialization), {}};
  }

  return parse_json(*stream.value, options, resource);
}

} // namespace caors::serialization
