// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/codec.h"

#include <chrono>
#include <filesystem>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

namespace caors {
namespace {

struct codec_registry_state {
  mutable std::mutex mutex;
  hash_map<std::uint64_t, serialization::codec_data> codecs;
};

[[nodiscard]] codec_registry_state& codec_state() {
  static codec_registry_state state;
  return state;
}

template <typename T> [[nodiscard]] type_id registered_codec_type_id() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (detail::has_type_name_v<type>) {
    const error result = caors::detail::register_type<type>(type_name<type>::value);
    if (result && result.value() != errc::duplicate_id) {
      return type_id::invalid();
    }
  }

  const std::optional<meta_type> meta = get_meta_registry().find_type<type>();
  return meta.has_value() ? meta->id() : type_id::invalid();
}

[[nodiscard]] error read_caors_string(serialization_reader& reader, string& value) {
  return reader.read_string(value);
}

[[nodiscard]] error read_std_string(serialization_reader& reader, std::string& value) {
  string text;
  const error status = reader.read_string(text);
  if (status) {
    return status;
  }

  value.assign(text.data(), text.size());
  return {};
}

[[nodiscard]] error read_guid(serialization_reader& reader, guid& value) {
  string text;
  const error status = reader.read_string(text);
  if (status) {
    return status;
  }

  const std::optional<guid> parsed = guid::parse(std::string_view{text.data(), text.size()});
  if (!parsed.has_value()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  value = *parsed;
  return {};
}

[[nodiscard]] error read_type_guid(serialization_reader& reader, type_guid& value) {
  guid parsed;
  const error status = read_guid(reader, parsed);
  if (status) {
    return status;
  }

  value = type_guid{parsed};
  return {};
}

[[nodiscard]] error read_filesystem_path(serialization_reader& reader,
                                         std::filesystem::path& value) {
  string text;
  const error status = reader.read_string(text);
  if (status) {
    return status;
  }

  value = std::filesystem::path{std::string_view{text.data(), text.size()}};
  return {};
}

[[nodiscard]] error read_i64_or_u64(serialization_reader& reader, std::int64_t& value) {
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
[[nodiscard]] error read_chrono_duration(serialization_reader& reader, Duration& value) {
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

[[nodiscard]] error read_system_time_point(serialization_reader& reader,
                                           std::chrono::system_clock::time_point& value) {
  std::int64_t milliseconds = 0;
  const error status = read_i64_or_u64(reader, milliseconds);
  if (status) {
    return status;
  }

  value = std::chrono::system_clock::time_point{std::chrono::milliseconds{milliseconds}};
  return {};
}

template <typename T>
[[nodiscard]] error read_codec_payload(serialization_reader& reader, T& decoded) {
  if constexpr (std::is_same_v<T, string>) {
    return read_caors_string(reader, decoded);
  } else if constexpr (std::is_same_v<T, std::string>) {
    return read_std_string(reader, decoded);
  } else if constexpr (std::is_same_v<T, guid>) {
    return read_guid(reader, decoded);
  } else if constexpr (std::is_same_v<T, type_guid>) {
    return read_type_guid(reader, decoded);
  } else if constexpr (std::is_same_v<T, std::filesystem::path>) {
    return read_filesystem_path(reader, decoded);
  } else if constexpr (std::is_same_v<T, std::chrono::nanoseconds> ||
                       std::is_same_v<T, std::chrono::microseconds> ||
                       std::is_same_v<T, std::chrono::milliseconds> ||
                       std::is_same_v<T, std::chrono::seconds> ||
                       std::is_same_v<T, std::chrono::minutes> ||
                       std::is_same_v<T, std::chrono::hours>) {
    return read_chrono_duration(reader, decoded);
  } else if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>) {
    return read_system_time_point(reader, decoded);
  } else {
    return {error_domain::serialization, errc::unsupported};
  }
}

template <typename T>
[[nodiscard]] error read_codec_value(serialization_reader& reader, const reflection::field& field,
                                     void* object) {
  T decoded{};
  const error status = read_codec_payload<T>(reader, decoded);

  if (status) {
    return status;
  }

  if (!field.try_set<T>(object, decoded)) {
    return {error_domain::serialization, errc::unsupported};
  }

  return {};
}

template <typename T>
[[nodiscard]] error read_codec_ref(serialization_reader& reader, const any_ref& target) {
  T decoded{};
  const error status = read_codec_payload<T>(reader, decoded);
  if (status) {
    return status;
  }

  if (!target.try_set<T>(decoded)) {
    return {error_domain::serialization, errc::unsupported};
  }

  return {};
}

template <typename T> [[nodiscard]] const T* codec_value(const any& data) noexcept {
  return data.try_get<T>();
}

[[nodiscard]] error write_caors_string(serialization_writer& writer, const any& data) {
  const string* stored = codec_value<string>(data);
  return stored != nullptr ? writer.write_string(*stored)
                           : error{error_domain::serialization, errc::invalid_type};
}

[[nodiscard]] error write_std_string(serialization_writer& writer, const any& data) {
  const std::string* stored = codec_value<std::string>(data);
  return stored != nullptr ? writer.write_string(*stored)
                           : error{error_domain::serialization, errc::invalid_type};
}

[[nodiscard]] error write_string_view(serialization_writer& writer, const any& data) {
  const std::string_view* stored = codec_value<std::string_view>(data);
  return stored != nullptr ? writer.write_string(*stored)
                           : error{error_domain::serialization, errc::invalid_type};
}

[[nodiscard]] error write_guid(serialization_writer& writer, const any& data) {
  const guid* stored = codec_value<guid>(data);
  return stored != nullptr ? writer.write_string(stored->string())
                           : error{error_domain::serialization, errc::invalid_type};
}

[[nodiscard]] error write_type_guid(serialization_writer& writer, const any& data) {
  const type_guid* stored = codec_value<type_guid>(data);
  return stored != nullptr ? writer.write_string(stored->value.string())
                           : error{error_domain::serialization, errc::invalid_type};
}

[[nodiscard]] error write_filesystem_path(serialization_writer& writer, const any& data) {
  const std::filesystem::path* stored = codec_value<std::filesystem::path>(data);
  return stored != nullptr ? writer.write_string(stored->generic_string())
                           : error{error_domain::serialization, errc::invalid_type};
}

template <typename Duration>
[[nodiscard]] error write_chrono_duration(serialization_writer& writer, const any& data) {
  const Duration* stored = codec_value<Duration>(data);
  return stored != nullptr ? writer.write_i64(static_cast<std::int64_t>(stored->count()))
                           : error{error_domain::serialization, errc::invalid_type};
}

[[nodiscard]] error write_system_time_point(serialization_writer& writer, const any& data) {
  const auto* stored = codec_value<std::chrono::system_clock::time_point>(data);
  if (stored == nullptr) {
    return {error_domain::serialization, errc::invalid_type};
  }

  const auto milliseconds =
      std::chrono::duration_cast<std::chrono::milliseconds>(stored->time_since_epoch());
  return writer.write_i64(static_cast<std::int64_t>(milliseconds.count()));
}

[[nodiscard]] variant_result make_caors_string_value(const any& data) {
  const string* stored = codec_value<string>(data);
  return stored != nullptr ? variant_result{{}, variant::from(*stored)}
                           : variant_result{{error_domain::serialization, errc::invalid_type}, {}};
}

[[nodiscard]] variant_result make_std_string_value(const any& data) {
  const std::string* stored = codec_value<std::string>(data);
  return stored != nullptr ? variant_result{{}, variant::from(*stored)}
                           : variant_result{{error_domain::serialization, errc::invalid_type}, {}};
}

[[nodiscard]] variant_result make_string_view_value(const any& data) {
  const std::string_view* stored = codec_value<std::string_view>(data);
  return stored != nullptr ? variant_result{{}, variant::from(*stored)}
                           : variant_result{{error_domain::serialization, errc::invalid_type}, {}};
}

[[nodiscard]] variant_result make_guid_value(const any& data) {
  const guid* stored = codec_value<guid>(data);
  return stored != nullptr ? variant_result{{}, variant::from(stored->string())}
                           : variant_result{{error_domain::serialization, errc::invalid_type}, {}};
}

[[nodiscard]] variant_result make_type_guid_value(const any& data) {
  const type_guid* stored = codec_value<type_guid>(data);
  return stored != nullptr ? variant_result{{}, variant::from(stored->value.string())}
                           : variant_result{{error_domain::serialization, errc::invalid_type}, {}};
}

[[nodiscard]] variant_result make_filesystem_path_value(const any& data) {
  const std::filesystem::path* stored = codec_value<std::filesystem::path>(data);
  return stored != nullptr ? variant_result{{}, variant::from(stored->generic_string())}
                           : variant_result{{error_domain::serialization, errc::invalid_type}, {}};
}

template <typename Duration>
[[nodiscard]] variant_result make_chrono_duration_value(const any& data) {
  const Duration* stored = codec_value<Duration>(data);
  return stored != nullptr
             ? variant_result{{}, variant::from(static_cast<std::int64_t>(stored->count()))}
             : variant_result{{error_domain::serialization, errc::invalid_type}, {}};
}

[[nodiscard]] variant_result make_system_time_point_value(const any& data) {
  const auto* stored = codec_value<std::chrono::system_clock::time_point>(data);
  if (stored == nullptr) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  const auto milliseconds =
      std::chrono::duration_cast<std::chrono::milliseconds>(stored->time_since_epoch());
  return {{}, variant::from(static_cast<std::int64_t>(milliseconds.count()))};
}

template <typename T> void register_builtin_codec(serialization::codec_data codec) {
  const type_id id = registered_codec_type_id<T>();
  if (id) {
    static_cast<void>(serialization::codec_registry::instance().register_codec(id, codec));
  }
}

void register_builtin_codecs() {
  register_builtin_codec<string>(serialization::codec_data{
      &write_caors_string,
      &read_codec_value<string>,
      &read_codec_ref<string>,
      &make_caors_string_value,
  });
  register_builtin_codec<std::string>(serialization::codec_data{
      &write_std_string,
      &read_codec_value<std::string>,
      &read_codec_ref<std::string>,
      &make_std_string_value,
  });
  register_builtin_codec<std::string_view>(serialization::codec_data{
      &write_string_view,
      nullptr,
      nullptr,
      &make_string_view_value,
  });
  register_builtin_codec<guid>(serialization::codec_data{
      &write_guid,
      &read_codec_value<guid>,
      &read_codec_ref<guid>,
      &make_guid_value,
  });
  register_builtin_codec<type_guid>(serialization::codec_data{
      &write_type_guid,
      &read_codec_value<type_guid>,
      &read_codec_ref<type_guid>,
      &make_type_guid_value,
  });
  register_builtin_codec<std::filesystem::path>(serialization::codec_data{
      &write_filesystem_path,
      &read_codec_value<std::filesystem::path>,
      &read_codec_ref<std::filesystem::path>,
      &make_filesystem_path_value,
  });
  register_builtin_codec<std::chrono::nanoseconds>(serialization::codec_data{
      &write_chrono_duration<std::chrono::nanoseconds>,
      &read_codec_value<std::chrono::nanoseconds>,
      &read_codec_ref<std::chrono::nanoseconds>,
      &make_chrono_duration_value<std::chrono::nanoseconds>,
  });
  register_builtin_codec<std::chrono::microseconds>(serialization::codec_data{
      &write_chrono_duration<std::chrono::microseconds>,
      &read_codec_value<std::chrono::microseconds>,
      &read_codec_ref<std::chrono::microseconds>,
      &make_chrono_duration_value<std::chrono::microseconds>,
  });
  register_builtin_codec<std::chrono::milliseconds>(serialization::codec_data{
      &write_chrono_duration<std::chrono::milliseconds>,
      &read_codec_value<std::chrono::milliseconds>,
      &read_codec_ref<std::chrono::milliseconds>,
      &make_chrono_duration_value<std::chrono::milliseconds>,
  });
  register_builtin_codec<std::chrono::seconds>(serialization::codec_data{
      &write_chrono_duration<std::chrono::seconds>,
      &read_codec_value<std::chrono::seconds>,
      &read_codec_ref<std::chrono::seconds>,
      &make_chrono_duration_value<std::chrono::seconds>,
  });
  register_builtin_codec<std::chrono::minutes>(serialization::codec_data{
      &write_chrono_duration<std::chrono::minutes>,
      &read_codec_value<std::chrono::minutes>,
      &read_codec_ref<std::chrono::minutes>,
      &make_chrono_duration_value<std::chrono::minutes>,
  });
  register_builtin_codec<std::chrono::hours>(serialization::codec_data{
      &write_chrono_duration<std::chrono::hours>,
      &read_codec_value<std::chrono::hours>,
      &read_codec_ref<std::chrono::hours>,
      &make_chrono_duration_value<std::chrono::hours>,
  });
  register_builtin_codec<std::chrono::system_clock::time_point>(serialization::codec_data{
      &write_system_time_point,
      &read_codec_value<std::chrono::system_clock::time_point>,
      &read_codec_ref<std::chrono::system_clock::time_point>,
      &make_system_time_point_value,
  });
}

void ensure_builtin_codecs_registered() {
  static std::once_flag once;
  std::call_once(once, register_builtin_codecs);
}

} // namespace

serialization::codec_registry& serialization::codec_registry::instance() noexcept {
  static serialization::codec_registry registry;
  return registry;
}

error serialization::codec_registry::register_codec(type_id id, serialization::codec_data codec) {
  if (!id) {
    return {error_domain::serialization, errc::invalid_type};
  }

  if (!codec.valid()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  codec_registry_state& state = codec_state();
  std::lock_guard lock(state.mutex);
  const auto [_, inserted] = state.codecs.emplace(id.value(), codec);
  if (!inserted) {
    return {error_domain::serialization, errc::duplicate_component};
  }

  return {};
}

std::optional<serialization::codec_data>
serialization::codec_registry::find_codec(type_id id) const {
  if (!id) {
    return std::nullopt;
  }

  ensure_builtin_codecs_registered();

  codec_registry_state& state = codec_state();
  std::lock_guard lock(state.mutex);
  const auto position = state.codecs.find(id.value());
  if (position == state.codecs.end()) {
    return std::nullopt;
  }

  return position->second;
}

bool serialization::codec_registry::contains(type_id id) const {
  if (!id) {
    return false;
  }

  ensure_builtin_codecs_registered();

  codec_registry_state& state = codec_state();
  std::lock_guard lock(state.mutex);
  return state.codecs.contains(id.value());
}

std::size_t serialization::codec_registry::size() const noexcept {
  ensure_builtin_codecs_registered();

  codec_registry_state& state = codec_state();
  std::lock_guard lock(state.mutex);
  return state.codecs.size();
}

serialization::codec_registry& serialization::get_codec_registry() noexcept {
  ensure_builtin_codecs_registered();
  return serialization::codec_registry::instance();
}

namespace detail {

error register_serialization_codec(type_id id, serialization::codec_data codec) {
  return serialization::get_codec_registry().register_codec(id, codec);
}

} // namespace detail

} // namespace caors
