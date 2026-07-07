// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "internal.h"

#include <limits>
#include <string>
#include <string_view>

namespace caors::serialization::detail {

namespace {

template <typename T> [[nodiscard]] bool try_write_i64(writer& writer, const any& data) {
  const T* stored = data.try_get<T>();
  if (stored == nullptr) {
    return false;
  }

  return writer.write_i64(static_cast<std::int64_t>(*stored)).succeeded();
}

template <typename T> [[nodiscard]] bool try_write_u64(writer& writer, const any& data) {
  const T* stored = data.try_get<T>();
  if (stored == nullptr) {
    return false;
  }

  return writer.write_u64(static_cast<std::uint64_t>(*stored)).succeeded();
}

template <typename T> [[nodiscard]] bool try_write_f64(writer& writer, const any& data) {
  const T* stored = data.try_get<T>();
  if (stored == nullptr) {
    return false;
  }

  return writer.write_f64(static_cast<double>(*stored)).succeeded();
}

template <typename T>
[[nodiscard]] bool try_read_signed_value(reader& reader, const any_ref& target) {
  std::int64_t value = 0;
  if (reader.read_i64(value).succeeded()) {
    if (value < static_cast<std::int64_t>(std::numeric_limits<T>::min()) ||
        value > static_cast<std::int64_t>(std::numeric_limits<T>::max())) {
      return false;
    }

    return target.try_set<T>(static_cast<T>(value));
  }

  std::uint64_t unsigned_value = 0;
  if (reader.read_u64(unsigned_value).failed() ||
      unsigned_value > static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
    return false;
  }

  return target.try_set<T>(static_cast<T>(unsigned_value));
}

template <typename T>
[[nodiscard]] bool try_read_unsigned_value(reader& reader, const any_ref& target) {
  std::uint64_t value = 0;
  if (reader.read_u64(value).succeeded()) {
    if (value > static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
      return false;
    }

    return target.try_set<T>(static_cast<T>(value));
  }

  std::int64_t signed_value = 0;
  if (reader.read_i64(signed_value).failed() || signed_value < 0 ||
      static_cast<std::uint64_t>(signed_value) >
          static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
    return false;
  }

  return target.try_set<T>(static_cast<T>(signed_value));
}

template <typename T>
[[nodiscard]] bool try_read_float_value(reader& reader, const any_ref& target) {
  double value = 0.0;
  if (reader.read_f64(value).succeeded()) {
    return target.try_set<T>(static_cast<T>(value));
  }

  std::int64_t signed_value = 0;
  if (reader.read_i64(signed_value).succeeded()) {
    return target.try_set<T>(static_cast<T>(signed_value));
  }

  std::uint64_t unsigned_value = 0;
  if (reader.read_u64(unsigned_value).succeeded()) {
    return target.try_set<T>(static_cast<T>(unsigned_value));
  }

  return false;
}

[[nodiscard]] bool try_read_char_value(reader& reader, const any_ref& target) {
  core::string value;
  if (reader.read_string(value).failed() || value.size() != 1) {
    return false;
  }

  return target.try_set<char>(value[0]);
}

[[nodiscard]] bool try_read_string_view_value(reader& reader, const any_ref& target,
                                              const serialization::context& context) {
  string_storage* storage = context.effective_string_view_storage();
  if (storage == nullptr || !target.matches_type<std::string_view>()) {
    return false;
  }

  core::string value;
  if (reader.read_string(value).failed()) {
    return false;
  }

  return target.try_set<std::string_view>(storage->store(std::move(value)));
}

} // namespace

bool try_write_scalar_value(writer& writer, const any& data) {
  if (const bool* stored = data.try_get<bool>()) {
    return writer.write_bool(*stored).succeeded();
  }

  if (const char* stored = data.try_get<char>()) {
    return writer.write_string(std::string_view{stored, 1}).succeeded();
  }

  if (try_write_i64<signed char>(writer, data) || try_write_i64<short>(writer, data) ||
      try_write_i64<int>(writer, data) || try_write_i64<long>(writer, data) ||
      try_write_i64<long long>(writer, data) || try_write_u64<unsigned char>(writer, data) ||
      try_write_u64<unsigned short>(writer, data) || try_write_u64<unsigned int>(writer, data) ||
      try_write_u64<unsigned long>(writer, data) ||
      try_write_u64<unsigned long long>(writer, data) || try_write_f64<float>(writer, data) ||
      try_write_f64<double>(writer, data) || try_write_f64<long double>(writer, data)) {
    return true;
  }

  if (const std::string* stored = data.try_get<std::string>()) {
    return writer.write_string(*stored).succeeded();
  }

  if (const std::string_view* stored = data.try_get<std::string_view>()) {
    return writer.write_string(*stored).succeeded();
  }

  return false;
}

error read_scalar_value(reader& reader, const any_ref& target,
                        const serialization::context& context) {
  bool bool_value = false;
  bool written = reader.read_bool(bool_value).succeeded() && target.try_set<bool>(bool_value);

  if (!written) {
    written = try_read_char_value(reader, target);
  }

  if (!written) {
    written = try_read_signed_value<signed char>(reader, target) ||
              try_read_signed_value<short>(reader, target) ||
              try_read_signed_value<int>(reader, target) ||
              try_read_signed_value<long>(reader, target) ||
              try_read_signed_value<long long>(reader, target);
  }

  if (!written) {
    written = try_read_unsigned_value<unsigned char>(reader, target) ||
              try_read_unsigned_value<unsigned short>(reader, target) ||
              try_read_unsigned_value<unsigned int>(reader, target) ||
              try_read_unsigned_value<unsigned long>(reader, target) ||
              try_read_unsigned_value<unsigned long long>(reader, target);
  }

  if (!written) {
    written = try_read_float_value<float>(reader, target) ||
              try_read_float_value<double>(reader, target) ||
              try_read_float_value<long double>(reader, target);
  }

  if (!written) {
    written = try_read_string_view_value(reader, target, context);
  }

  if (!written) {
    core::string string_value;
    if (reader.read_string(string_value).succeeded()) {
      const std::string standard_string{string_value.data(), string_value.size()};
      written = target.try_set<core::string>(string_value) ||
                target.try_set<std::string>(standard_string);
    }
  }

  if (!written) {
    return {error_domain::serialization, errc::unsupported};
  }

  return {};
}

} // namespace caors::serialization::detail
