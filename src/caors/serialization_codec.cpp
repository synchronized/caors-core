// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization.h"

#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

namespace caors {
namespace {

struct serialization_codec_registry_state {
  mutable std::mutex mutex;
  std::unordered_map<std::uint64_t, serialization_codec_data> codecs;
};

[[nodiscard]] serialization_codec_registry_state& codec_state() {
  static serialization_codec_registry_state state;
  return state;
}

template <typename T> [[nodiscard]] type_id ensure_codec_type() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (detail::has_type_name_v<type>) {
    const error result = caors::register_type<type>(type_name<type>::value);
    if (result && result.value() != errc::duplicate_id) {
      return type_id::invalid();
    }
  }

  const meta_type type_info = get_meta_registry().ensure_type_for<type>();
  return type_info.valid() ? type_info.id() : type_id::invalid();
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

template <typename T>
[[nodiscard]] error read_codec_value(serialization_reader& reader, const reflection_field& field,
                                     void* object) {
  T decoded{};
  const error status = [&]() -> error {
    if constexpr (std::is_same_v<T, string>) {
      return read_caors_string(reader, decoded);
    } else if constexpr (std::is_same_v<T, std::string>) {
      return read_std_string(reader, decoded);
    } else if constexpr (std::is_same_v<T, guid>) {
      return read_guid(reader, decoded);
    } else if constexpr (std::is_same_v<T, type_guid>) {
      return read_type_guid(reader, decoded);
    } else {
      return {error_domain::serialization, errc::unsupported};
    }
  }();

  if (status) {
    return status;
  }

  if (!field.try_set<T>(object, decoded)) {
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

[[nodiscard]] serialization_value_result make_caors_string_value(const any& data) {
  const string* stored = codec_value<string>(data);
  return stored != nullptr
             ? serialization_value_result{{}, variant::from(*stored)}
             : serialization_value_result{{error_domain::serialization, errc::invalid_type}, {}};
}

[[nodiscard]] serialization_value_result make_std_string_value(const any& data) {
  const std::string* stored = codec_value<std::string>(data);
  return stored != nullptr
             ? serialization_value_result{{}, variant::from(*stored)}
             : serialization_value_result{{error_domain::serialization, errc::invalid_type}, {}};
}

[[nodiscard]] serialization_value_result make_string_view_value(const any& data) {
  const std::string_view* stored = codec_value<std::string_view>(data);
  return stored != nullptr
             ? serialization_value_result{{}, variant::from(*stored)}
             : serialization_value_result{{error_domain::serialization, errc::invalid_type}, {}};
}

[[nodiscard]] serialization_value_result make_guid_value(const any& data) {
  const guid* stored = codec_value<guid>(data);
  return stored != nullptr
             ? serialization_value_result{{}, variant::from(stored->string())}
             : serialization_value_result{{error_domain::serialization, errc::invalid_type}, {}};
}

[[nodiscard]] serialization_value_result make_type_guid_value(const any& data) {
  const type_guid* stored = codec_value<type_guid>(data);
  return stored != nullptr
             ? serialization_value_result{{}, variant::from(stored->value.string())}
             : serialization_value_result{{error_domain::serialization, errc::invalid_type}, {}};
}

template <typename T> void register_builtin_codec(serialization_codec_data codec) {
  const type_id id = ensure_codec_type<T>();
  if (id) {
    static_cast<void>(serialization_codec_registry::instance().register_codec(id, codec));
  }
}

void register_builtin_codecs() {
  register_builtin_codec<string>(serialization_codec_data{
      &write_caors_string,
      &read_codec_value<string>,
      &make_caors_string_value,
  });
  register_builtin_codec<std::string>(serialization_codec_data{
      &write_std_string,
      &read_codec_value<std::string>,
      &make_std_string_value,
  });
  register_builtin_codec<std::string_view>(serialization_codec_data{
      &write_string_view,
      nullptr,
      &make_string_view_value,
  });
  register_builtin_codec<guid>(serialization_codec_data{
      &write_guid,
      &read_codec_value<guid>,
      &make_guid_value,
  });
  register_builtin_codec<type_guid>(serialization_codec_data{
      &write_type_guid,
      &read_codec_value<type_guid>,
      &make_type_guid_value,
  });
}

void ensure_builtin_codecs_registered() {
  static std::once_flag once;
  std::call_once(once, register_builtin_codecs);
}

} // namespace

serialization_codec_registry& serialization_codec_registry::instance() noexcept {
  static serialization_codec_registry registry;
  return registry;
}

error serialization_codec_registry::register_codec(type_id id, serialization_codec_data codec) {
  if (!id) {
    return {error_domain::serialization, errc::invalid_type};
  }

  if (!codec.valid()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  serialization_codec_registry_state& state = codec_state();
  std::lock_guard lock(state.mutex);
  const auto [_, inserted] = state.codecs.emplace(id.value(), codec);
  if (!inserted) {
    return {error_domain::serialization, errc::duplicate_component};
  }

  return {};
}

std::optional<serialization_codec_data> serialization_codec_registry::find_codec(type_id id) const {
  if (!id) {
    return std::nullopt;
  }

  ensure_builtin_codecs_registered();

  serialization_codec_registry_state& state = codec_state();
  std::lock_guard lock(state.mutex);
  const auto position = state.codecs.find(id.value());
  if (position == state.codecs.end()) {
    return std::nullopt;
  }

  return position->second;
}

bool serialization_codec_registry::contains(type_id id) const {
  if (!id) {
    return false;
  }

  ensure_builtin_codecs_registered();

  serialization_codec_registry_state& state = codec_state();
  std::lock_guard lock(state.mutex);
  return state.codecs.contains(id.value());
}

std::size_t serialization_codec_registry::size() const noexcept {
  ensure_builtin_codecs_registered();

  serialization_codec_registry_state& state = codec_state();
  std::lock_guard lock(state.mutex);
  return state.codecs.size();
}

serialization_codec_registry& get_serialization_codec_registry() noexcept {
  ensure_builtin_codecs_registered();
  return serialization_codec_registry::instance();
}

error register_serialization_codec(type_id id, serialization_codec_data codec) {
  return get_serialization_codec_registry().register_codec(id, codec);
}

} // namespace caors
