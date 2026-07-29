// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/messagepack.h"

#include "caors/memory/stack.h"

#include "caors/core/string_name.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::serialization {
namespace {

constexpr error invalid_messagepack() noexcept {
  return {error_domain::serialization, errc::invalid_argument};
}

constexpr error unsupported_messagepack() noexcept {
  return {error_domain::serialization, errc::unsupported};
}

void append_byte(io::byte_buffer& output, std::uint8_t value) {
  output.push_back(static_cast<std::byte>(value));
}

template <typename T> void append_big_endian(io::byte_buffer& output, T value) {
  static_assert(std::is_unsigned_v<T>);
  for (std::size_t shift = sizeof(T); shift != 0; --shift) {
    append_byte(output, static_cast<std::uint8_t>(value >> ((shift - 1U) * 8U)));
  }
}

void append_unsigned(io::byte_buffer& output, std::uint64_t value) {
  if (value <= 0x7FU) {
    append_byte(output, static_cast<std::uint8_t>(value));
  } else if (value <= std::numeric_limits<std::uint8_t>::max()) {
    append_byte(output, 0xCCU);
    append_big_endian(output, static_cast<std::uint8_t>(value));
  } else if (value <= std::numeric_limits<std::uint16_t>::max()) {
    append_byte(output, 0xCDU);
    append_big_endian(output, static_cast<std::uint16_t>(value));
  } else if (value <= std::numeric_limits<std::uint32_t>::max()) {
    append_byte(output, 0xCEU);
    append_big_endian(output, static_cast<std::uint32_t>(value));
  } else {
    append_byte(output, 0xCFU);
    append_big_endian(output, value);
  }
}

void append_signed(io::byte_buffer& output, std::int64_t value) {
  if (value >= 0) {
    append_unsigned(output, static_cast<std::uint64_t>(value));
  } else if (value >= -32) {
    append_byte(output, static_cast<std::uint8_t>(value));
  } else if (value >= std::numeric_limits<std::int8_t>::min()) {
    append_byte(output, 0xD0U);
    append_big_endian(output, static_cast<std::uint8_t>(value));
  } else if (value >= std::numeric_limits<std::int16_t>::min()) {
    append_byte(output, 0xD1U);
    append_big_endian(output, static_cast<std::uint16_t>(value));
  } else if (value >= std::numeric_limits<std::int32_t>::min()) {
    append_byte(output, 0xD2U);
    append_big_endian(output, static_cast<std::uint32_t>(value));
  } else {
    append_byte(output, 0xD3U);
    append_big_endian(output, static_cast<std::uint64_t>(value));
  }
}

error append_size(io::byte_buffer& output, std::size_t size, std::uint8_t fixed_marker,
                  std::uint8_t marker16, std::uint8_t marker32) {
  if (size <= 15U) {
    append_byte(output, static_cast<std::uint8_t>(fixed_marker | size));
  } else if (size <= std::numeric_limits<std::uint16_t>::max()) {
    append_byte(output, marker16);
    append_big_endian(output, static_cast<std::uint16_t>(size));
  } else if (size <= std::numeric_limits<std::uint32_t>::max()) {
    append_byte(output, marker32);
    append_big_endian(output, static_cast<std::uint32_t>(size));
  } else {
    return invalid_messagepack();
  }
  return {};
}

error append_string(io::byte_buffer& output, std::string_view value) {
  if (value.size() <= 31U) {
    append_byte(output, static_cast<std::uint8_t>(0xA0U | value.size()));
  } else if (value.size() <= std::numeric_limits<std::uint8_t>::max()) {
    append_byte(output, 0xD9U);
    append_big_endian(output, static_cast<std::uint8_t>(value.size()));
  } else if (value.size() <= std::numeric_limits<std::uint16_t>::max()) {
    append_byte(output, 0xDAU);
    append_big_endian(output, static_cast<std::uint16_t>(value.size()));
  } else if (value.size() <= std::numeric_limits<std::uint32_t>::max()) {
    append_byte(output, 0xDBU);
    append_big_endian(output, static_cast<std::uint32_t>(value.size()));
  } else {
    return invalid_messagepack();
  }

  const auto* begin = reinterpret_cast<const std::byte*>(value.data());
  output.insert(output.end(), begin, begin + value.size());
  return {};
}

error append_value(io::byte_buffer& output, const variant& value) {
  switch (value.kind()) {
  case meta::variant_kind::null:
    append_byte(output, 0xC0U);
    return {};
  case meta::variant_kind::boolean:
    append_byte(output, value.as_bool() ? 0xC3U : 0xC2U);
    return {};
  case meta::variant_kind::signed_integer:
    append_signed(output, value.as_i64());
    return {};
  case meta::variant_kind::unsigned_integer:
    append_unsigned(output, value.as_u64());
    return {};
  case meta::variant_kind::floating:
    append_byte(output, 0xCBU);
    append_big_endian(output, std::bit_cast<std::uint64_t>(value.as_f64()));
    return {};
  case meta::variant_kind::string:
    return append_string(output, value.as_string());
  case meta::variant_kind::array: {
    error status = append_size(output, value.as_array().size(), 0x90U, 0xDCU, 0xDDU);
    if (status) {
      return status;
    }
    for (const variant& element : value.as_array()) {
      status = append_value(output, element);
      if (status) {
        return status;
      }
    }
    return {};
  }
  case meta::variant_kind::dictionary: {
    error status = append_size(output, value.as_dictionary().size(), 0x80U, 0xDEU, 0xDFU);
    if (status) {
      return status;
    }
    for (const meta::variant_entry& entry : value.as_dictionary()) {
      status = append_value(output, entry.key);
      if (!status) {
        status = append_value(output, entry.value);
      }
      if (status) {
        return status;
      }
    }
    return {};
  }
  case meta::variant_kind::object: {
    error status = append_size(output, value.as_object().size(), 0x80U, 0xDEU, 0xDFU);
    if (status) {
      return status;
    }
    for (const meta::variant_member& member : value.as_object()) {
      status = append_string(output, member.name.view());
      if (!status) {
        status = append_value(output, member.value);
      }
      if (status) {
        return status;
      }
    }
    return {};
  }
  }
  return unsupported_messagepack();
}

class parser {
public:
  parser(std::span<const std::byte> bytes, messagepack_options options, memory_resource* resource,
         memory_resource* temporary_resource) noexcept
      : bytes_(bytes), options_(options),
        resource_(resource != nullptr ? resource : get_default_memory_resource()),
        temporary_resource_(temporary_resource) {}

  [[nodiscard]] variant_result parse() {
    variant_result result = parse_value(0);
    if (result.succeeded() && position_ != bytes_.size()) {
      result.status = invalid_messagepack();
    }
    return result;
  }

private:
  [[nodiscard]] bool available(std::size_t size) const noexcept {
    return size <= bytes_.size() - position_;
  }

  [[nodiscard]] bool read_byte(std::uint8_t& value) noexcept {
    if (!available(1)) {
      return false;
    }
    value = std::to_integer<std::uint8_t>(bytes_[position_++]);
    return true;
  }

  template <typename T> [[nodiscard]] bool read_big_endian(T& value) noexcept {
    static_assert(std::is_unsigned_v<T>);
    if (!available(sizeof(T))) {
      return false;
    }
    value = 0;
    for (std::size_t index = 0; index < sizeof(T); ++index) {
      value = static_cast<T>((value << 8U) | std::to_integer<std::uint8_t>(bytes_[position_++]));
    }
    return true;
  }

  [[nodiscard]] variant_result parse_string(std::size_t size) {
    if ((options_.max_string_size != 0 && size > options_.max_string_size) || !available(size)) {
      return {invalid_messagepack(), {}};
    }
    const char* data = reinterpret_cast<const char*>(bytes_.data() + position_);
    position_ += size;
    return {{}, variant::from(std::string_view{data, size}, resource_)};
  }

  [[nodiscard]] bool valid_container_size(std::size_t size) const noexcept {
    return options_.max_container_size == 0 || size <= options_.max_container_size;
  }

  [[nodiscard]] bool valid_depth(std::uint32_t depth) const noexcept {
    return options_.max_parse_depth == 0 || depth <= options_.max_parse_depth;
  }

  [[nodiscard]] variant_result parse_array(std::size_t size, std::uint32_t depth) {
    if (!valid_depth(depth)) {
      return {{error_domain::serialization, errc::recursion_limit}, {}};
    }
    if (!valid_container_size(size)) {
      return {invalid_messagepack(), {}};
    }
    variant::array_type values{variant::array_type::allocator_type{resource_}};
    values.reserve(size);
    for (std::size_t index = 0; index < size; ++index) {
      variant_result element = parse_value(depth);
      if (element.failed()) {
        return element;
      }
      values.push_back(std::move(element.value));
    }
    return {{}, variant::array(std::move(values))};
  }

  [[nodiscard]] variant_result parse_map(std::size_t size, std::uint32_t depth) {
    if (!valid_depth(depth)) {
      return {{error_domain::serialization, errc::recursion_limit}, {}};
    }
    if (!valid_container_size(size)) {
      return {invalid_messagepack(), {}};
    }

    variant::dictionary_type entries{variant::dictionary_type::allocator_type{temporary_resource_}};
    entries.reserve(size);
    bool all_string_keys = true;
    for (std::size_t index = 0; index < size; ++index) {
      variant_result key = parse_value(depth);
      if (key.failed()) {
        return key;
      }
      variant_result value = parse_value(depth);
      if (value.failed()) {
        return value;
      }
      all_string_keys = all_string_keys && key.value.is_string();
      entries.push_back({std::move(key.value), std::move(value.value)});
    }

    if (!all_string_keys) {
      variant::dictionary_type result_entries{variant::dictionary_type::allocator_type{resource_}};
      result_entries.reserve(entries.size());
      for (meta::variant_entry& entry : entries) {
        result_entries.push_back({std::move(entry.key), std::move(entry.value)});
      }
      return {{}, variant::dictionary(std::move(result_entries))};
    }

    variant::object_type members{variant::object_type::allocator_type{resource_}};
    members.reserve(entries.size());
    for (meta::variant_entry& entry : entries) {
      const string& key = entry.key.as_string();
      members.push_back(
          {string_name{std::string_view{key.data(), key.size()}}, 0, std::move(entry.value)});
    }
    return {{}, variant::object(std::move(members))};
  }

  [[nodiscard]] variant_result parse_value(std::uint32_t parent_depth) {
    std::uint8_t marker = 0;
    if (!read_byte(marker)) {
      return {invalid_messagepack(), {}};
    }

    if (marker <= 0x7FU) {
      return {{}, variant::from(static_cast<std::uint64_t>(marker), resource_)};
    }
    if ((marker & 0xF0U) == 0x80U) {
      return parse_map(marker & 0x0FU, parent_depth + 1U);
    }
    if ((marker & 0xF0U) == 0x90U) {
      return parse_array(marker & 0x0FU, parent_depth + 1U);
    }
    if ((marker & 0xE0U) == 0xA0U) {
      return parse_string(marker & 0x1FU);
    }
    if (marker >= 0xE0U) {
      return {
          {},
          variant::from(static_cast<std::int64_t>(static_cast<std::int8_t>(marker)), resource_)};
    }

    std::uint8_t u8 = 0;
    std::uint16_t u16 = 0;
    std::uint32_t u32 = 0;
    std::uint64_t u64 = 0;
    switch (marker) {
    case 0xC0U:
      return {{}, variant::null(resource_)};
    case 0xC2U:
      return {{}, variant::from(false, resource_)};
    case 0xC3U:
      return {{}, variant::from(true, resource_)};
    case 0xCAU:
      if (!read_big_endian(u32)) {
        return {invalid_messagepack(), {}};
      }
      return {{}, variant::from(static_cast<double>(std::bit_cast<float>(u32)), resource_)};
    case 0xCBU:
      if (!read_big_endian(u64)) {
        return {invalid_messagepack(), {}};
      }
      return {{}, variant::from(std::bit_cast<double>(u64), resource_)};
    case 0xCCU:
      return read_big_endian(u8)
                 ? variant_result{{}, variant::from(static_cast<std::uint64_t>(u8), resource_)}
                 : variant_result{invalid_messagepack(), {}};
    case 0xCDU:
      return read_big_endian(u16)
                 ? variant_result{{}, variant::from(static_cast<std::uint64_t>(u16), resource_)}
                 : variant_result{invalid_messagepack(), {}};
    case 0xCEU:
      return read_big_endian(u32)
                 ? variant_result{{}, variant::from(static_cast<std::uint64_t>(u32), resource_)}
                 : variant_result{invalid_messagepack(), {}};
    case 0xCFU:
      return read_big_endian(u64) ? variant_result{{}, variant::from(u64, resource_)}
                                  : variant_result{invalid_messagepack(), {}};
    case 0xD0U:
      return read_big_endian(u8) ? variant_result{{},
                                                  variant::from(static_cast<std::int64_t>(
                                                                    static_cast<std::int8_t>(u8)),
                                                                resource_)}
                                 : variant_result{invalid_messagepack(), {}};
    case 0xD1U:
      return read_big_endian(u16)
                 ? variant_result{{},
                                  variant::from(
                                      static_cast<std::int64_t>(static_cast<std::int16_t>(u16)),
                                      resource_)}
                 : variant_result{invalid_messagepack(), {}};
    case 0xD2U:
      return read_big_endian(u32)
                 ? variant_result{{},
                                  variant::from(
                                      static_cast<std::int64_t>(static_cast<std::int32_t>(u32)),
                                      resource_)}
                 : variant_result{invalid_messagepack(), {}};
    case 0xD3U:
      return read_big_endian(u64)
                 ? variant_result{{}, variant::from(static_cast<std::int64_t>(u64), resource_)}
                 : variant_result{invalid_messagepack(), {}};
    case 0xD9U:
      return read_big_endian(u8) ? parse_string(u8) : variant_result{invalid_messagepack(), {}};
    case 0xDAU:
      return read_big_endian(u16) ? parse_string(u16) : variant_result{invalid_messagepack(), {}};
    case 0xDBU:
      return read_big_endian(u32) ? parse_string(u32) : variant_result{invalid_messagepack(), {}};
    case 0xDCU:
      return read_big_endian(u16) ? parse_array(u16, parent_depth + 1U)
                                  : variant_result{invalid_messagepack(), {}};
    case 0xDDU:
      return read_big_endian(u32) ? parse_array(u32, parent_depth + 1U)
                                  : variant_result{invalid_messagepack(), {}};
    case 0xDEU:
      return read_big_endian(u16) ? parse_map(u16, parent_depth + 1U)
                                  : variant_result{invalid_messagepack(), {}};
    case 0xDFU:
      return read_big_endian(u32) ? parse_map(u32, parent_depth + 1U)
                                  : variant_result{invalid_messagepack(), {}};
    case 0xC1U:
      return {invalid_messagepack(), {}};
    default:
      return {unsupported_messagepack(), {}};
    }
  }

  std::span<const std::byte> bytes_{};
  messagepack_options options_{};
  std::size_t position_ = 0;
  memory_resource* resource_ = nullptr;
  memory_resource* temporary_resource_ = nullptr;
};

} // namespace

class messagepack_writer::implementation {
public:
  enum class frame_kind {
    object,
    array,
    dictionary,
  };

  enum class dictionary_phase {
    idle,
    entry,
    key,
    key_done,
    key_closed,
    value,
    value_done,
    value_closed,
  };

  struct frame {
    explicit frame(frame_kind frame_kind, std::size_t size, memory_resource* resource)
        : kind(frame_kind), expected_size(size),
          object(variant::object_type::allocator_type{resource}),
          array(variant::array_type::allocator_type{resource}),
          dictionary(variant::dictionary_type::allocator_type{resource}), key(resource),
          value(resource) {}

    frame_kind kind = frame_kind::object;
    std::size_t expected_size = 0;
    variant::object_type object{};
    variant::array_type array{};
    variant::dictionary_type dictionary{};
    string_name field_name{};
    bool slot_open = false;
    bool slot_written = false;
    dictionary_phase dictionary_state = dictionary_phase::idle;
    variant key{};
    variant value{};
  };

  implementation() : implementation(nullptr) {}

  explicit implementation(memory_resource* temporary_resource)
      : temporary_resource_(temporary_resource != nullptr ? temporary_resource
                                                          : &memory::library_temporary_resource()),
        frames_(vector<frame>::allocator_type{temporary_resource_}), output_(&owned_output_) {
    begin_temporary_scope();
  }

  implementation(io::byte_buffer& output, memory_resource* temporary_resource)
      : temporary_resource_(temporary_resource != nullptr ? temporary_resource
                                                          : &memory::library_temporary_resource()),
        frames_(vector<frame>::allocator_type{temporary_resource_}), output_(&output) {
    begin_temporary_scope();
    output_->clear();
  }

  [[nodiscard]] error begin_container(frame_kind kind, std::size_t expected_size) {
    if (!can_accept_value()) {
      return invalid_messagepack();
    }
    frames_.emplace_back(kind, expected_size, temporary_resource_);
    return {};
  }

  [[nodiscard]] error end_container(frame_kind kind) {
    if (frames_.empty() || frames_.back().kind != kind) {
      return invalid_messagepack();
    }

    if (!frame_complete(frames_.back())) {
      return invalid_messagepack();
    }
    frame current = std::move(frames_.back());
    frames_.pop_back();

    switch (kind) {
    case frame_kind::object:
      return accept_value(variant::object(std::move(current.object)));
    case frame_kind::array:
      return accept_value(variant::array(std::move(current.array)));
    case frame_kind::dictionary:
      return accept_value(variant::dictionary(std::move(current.dictionary)));
    }
    return invalid_messagepack();
  }

  [[nodiscard]] bool can_accept_value() const noexcept {
    if (frames_.empty()) {
      return !root_written_;
    }

    const frame& current = frames_.back();
    switch (current.kind) {
    case frame_kind::object:
    case frame_kind::array:
      return current.slot_open && !current.slot_written;
    case frame_kind::dictionary:
      return current.dictionary_state == dictionary_phase::key ||
             current.dictionary_state == dictionary_phase::value;
    }
    return false;
  }

  [[nodiscard]] error accept_value(variant value) {
    if (!can_accept_value()) {
      return invalid_messagepack();
    }

    if (frames_.empty()) {
      io::byte_buffer_result encoded =
          encode_messagepack(value, output_->get_allocator().resource());
      if (encoded.failed()) {
        return encoded.status;
      }
      *output_ = std::move(encoded.value);
      root_written_ = true;
      return {};
    }

    frame& current = frames_.back();
    switch (current.kind) {
    case frame_kind::object:
      current.object.push_back({current.field_name, 0, std::move(value)});
      current.slot_written = true;
      return {};
    case frame_kind::array:
      current.array.push_back(std::move(value));
      current.slot_written = true;
      return {};
    case frame_kind::dictionary:
      if (current.dictionary_state == dictionary_phase::key) {
        current.key = std::move(value);
        current.dictionary_state = dictionary_phase::key_done;
        return {};
      }
      if (current.dictionary_state == dictionary_phase::value) {
        current.value = std::move(value);
        current.dictionary_state = dictionary_phase::value_done;
        return {};
      }
      return invalid_messagepack();
    }
    return invalid_messagepack();
  }

  [[nodiscard]] static bool frame_complete(const frame& value) noexcept {
    switch (value.kind) {
    case frame_kind::object:
      return !value.slot_open;
    case frame_kind::array:
      return !value.slot_open && value.array.size() == value.expected_size;
    case frame_kind::dictionary:
      return value.dictionary_state == dictionary_phase::idle &&
             value.dictionary.size() == value.expected_size;
    }
    return false;
  }

  void reset() {
    output_->clear();
    frames_.clear();
    root_written_ = false;
  }

  void begin_temporary_scope() {
    if (auto* stack = dynamic_cast<memory::stack_resource*>(temporary_resource_);
        stack != nullptr) {
      temporary_scope_.emplace(*stack);
    }
  }

  memory_resource* temporary_resource_ = nullptr;
  std::optional<memory::stack_scope> temporary_scope_{};
  io::byte_buffer owned_output_{};
  io::byte_buffer* output_ = nullptr;
  vector<frame> frames_;
  bool root_written_ = false;
};

messagepack_writer::messagepack_writer() : implementation_(std::make_unique<implementation>()) {}

messagepack_writer::messagepack_writer(memory_resource* temporary_resource)
    : implementation_(std::make_unique<implementation>(temporary_resource)) {}

messagepack_writer::messagepack_writer(io::byte_buffer& output, memory_resource* temporary_resource)
    : implementation_(std::make_unique<implementation>(output, temporary_resource)) {}

messagepack_writer::~messagepack_writer() = default;

format_features messagepack_writer::features() const noexcept {
  return format_features::named_fields | format_features::ordered_fields |
         format_features::self_describing;
}

error messagepack_writer::begin_object(const type_schema&) {
  return implementation_->begin_container(implementation::frame_kind::object, 0);
}

error messagepack_writer::begin_field(const field_schema& field) {
  if (implementation_->frames_.empty()) {
    return invalid_messagepack();
  }
  implementation::frame& current = implementation_->frames_.back();
  if (current.kind != implementation::frame_kind::object || current.slot_open) {
    return invalid_messagepack();
  }
  current.field_name = field.serialized_name.empty() ? field.field_name : field.serialized_name;
  if (current.field_name.empty()) {
    return invalid_messagepack();
  }
  current.slot_open = true;
  current.slot_written = false;
  return {};
}

error messagepack_writer::begin_array(std::size_t size) {
  return implementation_->begin_container(implementation::frame_kind::array, size);
}

error messagepack_writer::begin_element(std::size_t index) {
  if (implementation_->frames_.empty()) {
    return invalid_messagepack();
  }
  implementation::frame& current = implementation_->frames_.back();
  if (current.kind != implementation::frame_kind::array || current.slot_open ||
      index != current.array.size() || index >= current.expected_size) {
    return invalid_messagepack();
  }
  current.slot_open = true;
  current.slot_written = false;
  return {};
}

error messagepack_writer::begin_dictionary(std::size_t size) {
  return implementation_->begin_container(implementation::frame_kind::dictionary, size);
}

error messagepack_writer::begin_entry(std::size_t index) {
  if (implementation_->frames_.empty()) {
    return invalid_messagepack();
  }
  implementation::frame& current = implementation_->frames_.back();
  if (current.kind != implementation::frame_kind::dictionary ||
      current.dictionary_state != implementation::dictionary_phase::idle ||
      index != current.dictionary.size() || index >= current.expected_size) {
    return invalid_messagepack();
  }
  current.dictionary_state = implementation::dictionary_phase::entry;
  return {};
}

error messagepack_writer::begin_key() {
  if (implementation_->frames_.empty()) {
    return invalid_messagepack();
  }
  implementation::frame& current = implementation_->frames_.back();
  if (current.kind != implementation::frame_kind::dictionary ||
      current.dictionary_state != implementation::dictionary_phase::entry) {
    return invalid_messagepack();
  }
  current.dictionary_state = implementation::dictionary_phase::key;
  return {};
}

error messagepack_writer::end_key() {
  if (implementation_->frames_.empty()) {
    return invalid_messagepack();
  }
  implementation::frame& current = implementation_->frames_.back();
  if (current.kind != implementation::frame_kind::dictionary ||
      current.dictionary_state != implementation::dictionary_phase::key_done) {
    return invalid_messagepack();
  }
  current.dictionary_state = implementation::dictionary_phase::key_closed;
  return {};
}

error messagepack_writer::begin_value() {
  if (implementation_->frames_.empty()) {
    return invalid_messagepack();
  }
  implementation::frame& current = implementation_->frames_.back();
  if (current.kind != implementation::frame_kind::dictionary ||
      current.dictionary_state != implementation::dictionary_phase::key_closed) {
    return invalid_messagepack();
  }
  current.dictionary_state = implementation::dictionary_phase::value;
  return {};
}

error messagepack_writer::end_value() {
  if (implementation_->frames_.empty()) {
    return invalid_messagepack();
  }
  implementation::frame& current = implementation_->frames_.back();
  if (current.kind != implementation::frame_kind::dictionary ||
      current.dictionary_state != implementation::dictionary_phase::value_done) {
    return invalid_messagepack();
  }
  current.dictionary_state = implementation::dictionary_phase::value_closed;
  return {};
}

error messagepack_writer::end_entry() {
  if (implementation_->frames_.empty()) {
    return invalid_messagepack();
  }
  implementation::frame& current = implementation_->frames_.back();
  if (current.kind != implementation::frame_kind::dictionary ||
      current.dictionary_state != implementation::dictionary_phase::value_closed) {
    return invalid_messagepack();
  }
  current.dictionary.push_back({std::move(current.key), std::move(current.value)});
  current.key = {};
  current.value = {};
  current.dictionary_state = implementation::dictionary_phase::idle;
  return {};
}

error messagepack_writer::end_dictionary() {
  return implementation_->end_container(implementation::frame_kind::dictionary);
}

error messagepack_writer::write_bool(bool value) {
  return implementation_->accept_value(variant::from(value, implementation_->temporary_resource_));
}

error messagepack_writer::write_i64(std::int64_t value) {
  return implementation_->accept_value(variant::from(value, implementation_->temporary_resource_));
}

error messagepack_writer::write_u64(std::uint64_t value) {
  return implementation_->accept_value(variant::from(value, implementation_->temporary_resource_));
}

error messagepack_writer::write_f64(double value) {
  return implementation_->accept_value(variant::from(value, implementation_->temporary_resource_));
}

error messagepack_writer::write_string(std::string_view value) {
  return implementation_->accept_value(variant::from(value, implementation_->temporary_resource_));
}

error messagepack_writer::write_null() {
  return implementation_->accept_value(variant::null(implementation_->temporary_resource_));
}

error messagepack_writer::end_element() {
  if (implementation_->frames_.empty()) {
    return invalid_messagepack();
  }
  implementation::frame& current = implementation_->frames_.back();
  if (current.kind != implementation::frame_kind::array || !current.slot_open ||
      !current.slot_written) {
    return invalid_messagepack();
  }
  current.slot_open = false;
  current.slot_written = false;
  return {};
}

error messagepack_writer::end_array() {
  return implementation_->end_container(implementation::frame_kind::array);
}

error messagepack_writer::end_field() {
  if (implementation_->frames_.empty()) {
    return invalid_messagepack();
  }
  implementation::frame& current = implementation_->frames_.back();
  if (current.kind != implementation::frame_kind::object || !current.slot_open ||
      !current.slot_written) {
    return invalid_messagepack();
  }
  current.field_name = {};
  current.slot_open = false;
  current.slot_written = false;
  return {};
}

error messagepack_writer::end_object() {
  return implementation_->end_container(implementation::frame_kind::object);
}

const io::byte_buffer& messagepack_writer::bytes() const noexcept {
  return *implementation_->output_;
}

void messagepack_writer::reset() { implementation_->reset(); }

io::byte_buffer_result encode_messagepack(const variant& value, memory_resource* resource) {
  io::byte_buffer bytes = io::make_byte_buffer(resource);
  error status = append_value(bytes, value);
  return {status, std::move(bytes)};
}

variant_result parse_messagepack(std::span<const std::byte> bytes, messagepack_options options) {
  return parse_messagepack(bytes, options, nullptr, nullptr);
}

variant_result parse_messagepack(std::span<const std::byte> bytes, messagepack_options options,
                                 memory_resource* resource) {
  return parse_messagepack(bytes, options, resource, nullptr);
}

variant_result parse_messagepack(std::span<const std::byte> bytes, messagepack_options options,
                                 memory_resource* resource, memory_resource* temporary_resource) {
  memory_resource* resolved_temporary =
      temporary_resource != nullptr ? temporary_resource : &memory::library_temporary_resource();
  std::optional<memory::stack_scope> temporary_scope;
  if (auto* stack = dynamic_cast<memory::stack_resource*>(resolved_temporary); stack != nullptr) {
    temporary_scope.emplace(*stack);
  }
  return parser{bytes, options, resource, resolved_temporary}.parse();
}

} // namespace caors::serialization
