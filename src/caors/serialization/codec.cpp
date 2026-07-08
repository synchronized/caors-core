// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/codec.h"

#include "caors/meta/capability.h"

#include <chrono>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

namespace caors::serialization {

bool codec::valid() const noexcept { return ops_ != nullptr && ops_->valid(); }

type_id codec::type() const noexcept {
  return ops_ != nullptr ? ops_->type : type_id::invalid();
}

bool codec::can_write() const noexcept {
  return ops_ != nullptr && (ops_->write != nullptr || ops_->write_context != nullptr);
}

bool codec::can_read() const noexcept {
  return ops_ != nullptr &&
         (ops_->read != nullptr || ops_->read_context != nullptr ||
          ops_->read_value != nullptr || ops_->read_value_context != nullptr);
}

bool codec::can_make_value() const noexcept {
  return ops_ != nullptr && (ops_->make_value != nullptr || ops_->make_value_context != nullptr);
}

codec find_codec(type_id id) {
  if (!id) {
    return {};
  }

  return detail::get_codec_registry().find_codec(id);
}

codec find_codec(meta::type type) { return type ? find_codec(type.id()) : codec{}; }

} // namespace caors::serialization

namespace caors::serialization::detail {

const codec_ops* ops_of(codec view) noexcept { return view.ops_; }

namespace {

template <typename T> [[nodiscard]] type_id registered_codec_type_id() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  return meta::detail::type_id_of_cpp_type<type>();
}

[[nodiscard]] error read_caors_string(reader& reader, core::string& value) {
  return reader.read_string(value);
}

[[nodiscard]] error read_std_string(reader& reader, std::string& value) {
  core::string text;
  const error status = reader.read_string(text);
  if (status) {
    return status;
  }

  value.assign(text.data(), text.size());
  return {};
}

[[nodiscard]] error read_guid(reader& reader, core::guid& value) {
  core::string text;
  const error status = reader.read_string(text);
  if (status) {
    return status;
  }

  const std::optional<core::guid> parsed =
      core::guid::parse(std::string_view{text.data(), text.size()});
  if (!parsed.has_value()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  value = *parsed;
  return {};
}

[[nodiscard]] error read_type_guid(reader& reader, core::type_guid& value) {
  core::guid parsed;
  const error status = read_guid(reader, parsed);
  if (status) {
    return status;
  }

  value = core::type_guid{parsed};
  return {};
}

[[nodiscard]] error read_filesystem_path(reader& reader, std::filesystem::path& value) {
  core::string text;
  const error status = reader.read_string(text);
  if (status) {
    return status;
  }

  value = std::filesystem::path{std::string_view{text.data(), text.size()}};
  return {};
}

[[nodiscard]] error read_i64_or_u64(reader& reader, std::int64_t& value) {
  error status = reader.read_i64(value);
  if (!status) {
    return {};
  }

  std::uint64_t unsigned_value = 0;
  status = reader.read_u64(unsigned_value);
  if (status) {
    return status;
  }

  if (unsigned_value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  value = static_cast<std::int64_t>(unsigned_value);
  return {};
}

template <typename Duration>
[[nodiscard]] error read_chrono_duration(reader& reader, Duration& value) {
  std::int64_t count = 0;
  const error status = read_i64_or_u64(reader, count);
  if (status) {
    return status;
  }

  using rep = typename Duration::rep;
  if (count < static_cast<std::int64_t>(std::numeric_limits<rep>::min()) ||
      count > static_cast<std::int64_t>(std::numeric_limits<rep>::max())) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  value = Duration{static_cast<rep>(count)};
  return {};
}

[[nodiscard]] error read_system_time_point(reader& reader,
                                           std::chrono::system_clock::time_point& value) {
  std::int64_t milliseconds = 0;
  const error status = read_i64_or_u64(reader, milliseconds);
  if (status) {
    return status;
  }

  value = std::chrono::system_clock::time_point{std::chrono::milliseconds{milliseconds}};
  return {};
}

template <typename T> struct builtin_codec {
  static constexpr bool can_write = false;
  static constexpr bool can_read = false;
  static constexpr bool can_make_value = false;
};

template <> struct builtin_codec<core::string> {
  static constexpr bool can_write = true;
  static constexpr bool can_read = true;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static error write(writer& writer, const core::string& value) {
    return writer.write_string(value);
  }

  [[nodiscard]] static error read(reader& reader, core::string& value) {
    return read_caors_string(reader, value);
  }

  [[nodiscard]] static variant_result make_value(const core::string& value) {
    return {{}, variant::from(value)};
  }
};

template <> struct builtin_codec<std::string> {
  static constexpr bool can_write = true;
  static constexpr bool can_read = true;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static error write(writer& writer, const std::string& value) {
    return writer.write_string(value);
  }

  [[nodiscard]] static error read(reader& reader, std::string& value) {
    return read_std_string(reader, value);
  }

  [[nodiscard]] static variant_result make_value(const std::string& value) {
    return {{}, variant::from(value)};
  }
};

template <> struct builtin_codec<std::string_view> {
  static constexpr bool can_write = true;
  static constexpr bool can_read = false;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static error write(writer& writer, std::string_view value) {
    return writer.write_string(value);
  }

  [[nodiscard]] static variant_result make_value(std::string_view value) {
    return {{}, variant::from(value)};
  }
};

template <> struct builtin_codec<core::guid> {
  static constexpr bool can_write = true;
  static constexpr bool can_read = true;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static error write(writer& writer, const core::guid& value) {
    return writer.write_string(value.string());
  }

  [[nodiscard]] static error read(reader& reader, core::guid& value) {
    return read_guid(reader, value);
  }

  [[nodiscard]] static variant_result make_value(const core::guid& value) {
    return {{}, variant::from(value.string())};
  }
};

template <> struct builtin_codec<core::type_guid> {
  static constexpr bool can_write = true;
  static constexpr bool can_read = true;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static error write(writer& writer, const core::type_guid& value) {
    return writer.write_string(value.value.string());
  }

  [[nodiscard]] static error read(reader& reader, core::type_guid& value) {
    return read_type_guid(reader, value);
  }

  [[nodiscard]] static variant_result make_value(const core::type_guid& value) {
    return {{}, variant::from(value.value.string())};
  }
};

template <> struct builtin_codec<std::filesystem::path> {
  static constexpr bool can_write = true;
  static constexpr bool can_read = true;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static error write(writer& writer, const std::filesystem::path& value) {
    return writer.write_string(value.generic_string());
  }

  [[nodiscard]] static error read(reader& reader, std::filesystem::path& value) {
    return read_filesystem_path(reader, value);
  }

  [[nodiscard]] static variant_result make_value(const std::filesystem::path& value) {
    return {{}, variant::from(value.generic_string())};
  }
};

template <typename Duration> struct chrono_duration_codec {
  static constexpr bool can_write = true;
  static constexpr bool can_read = true;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static error write(writer& writer, const Duration& value) {
    return writer.write_i64(static_cast<std::int64_t>(value.count()));
  }

  [[nodiscard]] static error read(reader& reader, Duration& value) {
    return read_chrono_duration(reader, value);
  }

  [[nodiscard]] static variant_result make_value(const Duration& value) {
    return {{}, variant::from(static_cast<std::int64_t>(value.count()))};
  }
};

template <>
struct builtin_codec<std::chrono::nanoseconds> : chrono_duration_codec<std::chrono::nanoseconds> {};
template <>
struct builtin_codec<std::chrono::microseconds> : chrono_duration_codec<std::chrono::microseconds> {
};
template <>
struct builtin_codec<std::chrono::milliseconds> : chrono_duration_codec<std::chrono::milliseconds> {
};
template <>
struct builtin_codec<std::chrono::seconds> : chrono_duration_codec<std::chrono::seconds> {};
template <>
struct builtin_codec<std::chrono::minutes> : chrono_duration_codec<std::chrono::minutes> {};
template <> struct builtin_codec<std::chrono::hours> : chrono_duration_codec<std::chrono::hours> {};

template <> struct builtin_codec<std::chrono::system_clock::time_point> {
  static constexpr bool can_write = true;
  static constexpr bool can_read = true;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static error write(writer& writer,
                                   const std::chrono::system_clock::time_point& value) {
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(value.time_since_epoch());
    return writer.write_i64(static_cast<std::int64_t>(milliseconds.count()));
  }

  [[nodiscard]] static error read(reader& reader, std::chrono::system_clock::time_point& value) {
    return read_system_time_point(reader, value);
  }

  [[nodiscard]] static variant_result
  make_value(const std::chrono::system_clock::time_point& value) {
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(value.time_since_epoch());
    return {{}, variant::from(static_cast<std::int64_t>(milliseconds.count()))};
  }
};

template <typename T> [[nodiscard]] const codec_ops* builtin_codec_ops_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  static const codec_ops data = make_codec_ops_from<builtin_codec<type>, type>();
  return data.valid() ? &data : nullptr;
}

template <typename T> void register_builtin_codec() {
  const type_id id = registered_codec_type_id<T>();
  const codec_ops* codec = builtin_codec_ops_ptr_of<T>();
  if (id && codec != nullptr) {
    static_cast<void>(codec_registry::instance().register_codec(id, *codec));
  }
}

void register_builtin_codecs() {
  register_builtin_codec<core::string>();
  register_builtin_codec<std::string>();
  register_builtin_codec<std::string_view>();
  register_builtin_codec<core::guid>();
  register_builtin_codec<core::type_guid>();
  register_builtin_codec<std::filesystem::path>();
  register_builtin_codec<std::chrono::nanoseconds>();
  register_builtin_codec<std::chrono::microseconds>();
  register_builtin_codec<std::chrono::milliseconds>();
  register_builtin_codec<std::chrono::seconds>();
  register_builtin_codec<std::chrono::minutes>();
  register_builtin_codec<std::chrono::hours>();
  register_builtin_codec<std::chrono::system_clock::time_point>();
}

void ensure_builtin_codecs_registered() { register_builtin_codecs(); }

} // namespace

codec_registry& codec_registry::instance() noexcept {
  static codec_registry registry;
  return registry;
}

error codec_registry::register_codec(type_id id, const codec_ops& codec) {
  if (!id) {
    return {error_domain::serialization, errc::invalid_type};
  }

  if (!codec.valid()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  return rethrow_as(meta::capabilities::register_for<codec_ops>(id, &codec),
                    error_domain::serialization);
}

codec codec_registry::find_codec(type_id id) const {
  if (!id) {
    return {};
  }

  ensure_builtin_codecs_registered();
  return codec{meta::capabilities::find<codec_ops>(id)};
}

bool codec_registry::contains(type_id id) const {
  if (!id) {
    return false;
  }

  ensure_builtin_codecs_registered();
  return meta::capabilities::contains<codec_ops>(id);
}

std::size_t codec_registry::size() const noexcept {
  ensure_builtin_codecs_registered();
  return meta::capabilities::size<codec_ops>();
}

codec_registry& get_codec_registry() noexcept {
  ensure_builtin_codecs_registered();
  return codec_registry::instance();
}

error register_serialization_codec(type_id id, const codec_ops& codec) {
  return get_codec_registry().register_codec(id, codec);
}

} // namespace caors::serialization::detail
