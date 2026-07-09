// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_CODEC_H_
#define CAORS_SERIALIZATION_CODEC_H_

#include "caors/meta/capability.h"
#include "caors/reflection/field.h"
#include "caors/serialization/detail/codec_ops.h"
#include "caors/serialization/reader.h"
#include "caors/serialization/writer.h"
#include "caors/value/any_ref.h"

#include <chrono>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

namespace caors::serialization {

struct context;

class codec;

namespace detail {
struct codec_ops;
[[nodiscard]] CAORS_API const codec_ops* ops_of(codec view) noexcept;
} // namespace detail

/** @brief 可特化的类型序列化 codec。 */
template <typename T> struct codec_traits {
  static constexpr bool can_write = false;
  static constexpr bool can_read = false;
  static constexpr bool can_make_value = false;
};

namespace detail {

[[nodiscard]] inline error read_caors_string(reader& reader, string& value) {
  return reader.read_string(value);
}

[[nodiscard]] inline error read_std_string(reader& reader, std::string& value) {
  string text;
  const error status = reader.read_string(text);
  if (status) {
    return status;
  }

  value.assign(text.data(), text.size());
  return {};
}

[[nodiscard]] inline error read_guid(reader& reader, guid& value) {
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

[[nodiscard]] inline error read_type_guid(reader& reader, type_guid& value) {
  guid parsed;
  const error status = read_guid(reader, parsed);
  if (status) {
    return status;
  }

  value = type_guid{parsed};
  return {};
}

[[nodiscard]] inline error read_filesystem_path(reader& reader, std::filesystem::path& value) {
  string text;
  const error status = reader.read_string(text);
  if (status) {
    return status;
  }

  value = std::filesystem::path{std::string_view{text.data(), text.size()}};
  return {};
}

[[nodiscard]] inline error read_i64_or_u64(reader& reader, std::int64_t& value) {
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
[[nodiscard]] inline error read_chrono_duration(reader& reader, Duration& value) {
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

[[nodiscard]] inline error read_system_time_point(reader& reader,
                                                  std::chrono::system_clock::time_point& value) {
  std::int64_t milliseconds = 0;
  const error status = read_i64_or_u64(reader, milliseconds);
  if (status) {
    return status;
  }

  value = std::chrono::system_clock::time_point{std::chrono::milliseconds{milliseconds}};
  return {};
}

} // namespace detail

/** @brief `caors::string` 的字符串 codec。 */
template <> struct codec_traits<string> {
  static constexpr bool can_write = true;
  static constexpr bool can_read = true;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static error write(writer& writer, const string& value) {
    return writer.write_string(value);
  }

  [[nodiscard]] static error read(reader& reader, string& value) {
    return detail::read_caors_string(reader, value);
  }

  [[nodiscard]] static variant_result make_value(const string& value) {
    return {{}, variant::from(value)};
  }
};

/** @brief `std::string` 的字符串 codec。 */
template <> struct codec_traits<std::string> {
  static constexpr bool can_write = true;
  static constexpr bool can_read = true;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static error write(writer& writer, const std::string& value) {
    return writer.write_string(value);
  }

  [[nodiscard]] static error read(reader& reader, std::string& value) {
    return detail::read_std_string(reader, value);
  }

  [[nodiscard]] static variant_result make_value(const std::string& value) {
    return {{}, variant::from(value)};
  }
};

/** @brief `std::string_view` 的只写借用字符串 codec。 */
template <> struct codec_traits<std::string_view> {
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

/** @brief `guid` 的字符串 codec。 */
template <> struct codec_traits<guid> {
  static constexpr bool can_write = true;
  static constexpr bool can_read = true;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static error write(writer& writer, guid value) {
    return writer.write_string(value.string());
  }

  [[nodiscard]] static error read(reader& reader, guid& value) {
    return detail::read_guid(reader, value);
  }

  [[nodiscard]] static variant_result make_value(guid value) {
    return {{}, variant::from(value.string())};
  }
};

/** @brief `type_guid` 的字符串 codec。 */
template <> struct codec_traits<type_guid> {
  static constexpr bool can_write = true;
  static constexpr bool can_read = true;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static error write(writer& writer, type_guid value) {
    return writer.write_string(value.value.string());
  }

  [[nodiscard]] static error read(reader& reader, type_guid& value) {
    return detail::read_type_guid(reader, value);
  }

  [[nodiscard]] static variant_result make_value(type_guid value) {
    return {{}, variant::from(value.value.string())};
  }
};

/** @brief `std::filesystem::path` 的通用路径字符串 codec。 */
template <> struct codec_traits<std::filesystem::path> {
  static constexpr bool can_write = true;
  static constexpr bool can_read = true;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static error write(writer& writer, const std::filesystem::path& value) {
    return writer.write_string(value.generic_string());
  }

  [[nodiscard]] static error read(reader& reader, std::filesystem::path& value) {
    return detail::read_filesystem_path(reader, value);
  }

  [[nodiscard]] static variant_result make_value(const std::filesystem::path& value) {
    return {{}, variant::from(value.generic_string())};
  }
};

template <typename Duration> struct chrono_duration_codec_traits {
  static constexpr bool can_write = true;
  static constexpr bool can_read = true;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static error write(writer& writer, const Duration& value) {
    return writer.write_i64(static_cast<std::int64_t>(value.count()));
  }

  [[nodiscard]] static error read(reader& reader, Duration& value) {
    return detail::read_chrono_duration(reader, value);
  }

  [[nodiscard]] static variant_result make_value(const Duration& value) {
    return {{}, variant::from(static_cast<std::int64_t>(value.count()))};
  }
};

template <>
struct codec_traits<std::chrono::nanoseconds>
    : chrono_duration_codec_traits<std::chrono::nanoseconds> {};
template <>
struct codec_traits<std::chrono::microseconds>
    : chrono_duration_codec_traits<std::chrono::microseconds> {};
template <>
struct codec_traits<std::chrono::milliseconds>
    : chrono_duration_codec_traits<std::chrono::milliseconds> {};
template <>
struct codec_traits<std::chrono::seconds> : chrono_duration_codec_traits<std::chrono::seconds> {};
template <>
struct codec_traits<std::chrono::minutes> : chrono_duration_codec_traits<std::chrono::minutes> {};
template <>
struct codec_traits<std::chrono::hours> : chrono_duration_codec_traits<std::chrono::hours> {};

/** @brief `system_clock::time_point` 按毫秒 epoch 整数读写。 */
template <> struct codec_traits<std::chrono::system_clock::time_point> {
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
    return detail::read_system_time_point(reader, value);
  }

  [[nodiscard]] static variant_result
  make_value(const std::chrono::system_clock::time_point& value) {
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(value.time_since_epoch());
    return {{}, variant::from(static_cast<std::int64_t>(milliseconds.count()))};
  }
};

/** @brief 序列化 codec 只读视图。 */
class CAORS_API codec final {
public:
  constexpr codec() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] type_id type() const noexcept;
  [[nodiscard]] bool can_write() const noexcept;
  [[nodiscard]] bool can_read() const noexcept;
  [[nodiscard]] bool can_make_value() const noexcept;

private:
  template <typename T> friend codec codec_of();
  friend codec find_codec(meta::type type);
  friend const detail::codec_ops* detail::ops_of(codec) noexcept;

  constexpr explicit codec(const detail::codec_ops* ops) noexcept : ops_(ops) {}

  const detail::codec_ops* ops_ = nullptr;
};

/** @brief 把运行期 C++ 值转换为序列化中间值。 */
[[nodiscard]] CAORS_API variant_result to_variant(const any& data);

/** @brief 把运行期 C++ 引用转换为序列化中间值。不可拷贝引用会返回不支持。 */
[[nodiscard]] CAORS_API variant_result to_variant(const any_ref& data);

/** @brief 按类型 id 查询已注册 meta 类型的 codec 视图。 */
[[nodiscard]] CAORS_API codec find_codec(type_id id);

/** @brief 按 meta 类型查询已索引的 codec 视图。 */
[[nodiscard]] CAORS_API codec find_codec(meta::type type);

/** @brief 按 C++ 类型查询静态 codec 视图；类型不要求已经显式注册。 */
template <typename T> [[nodiscard]] codec codec_of();

} // namespace caors::serialization

namespace caors::serialization::detail {

/** @brief 从 codec 视图取回内部操作表。 */
[[nodiscard]] CAORS_API const codec_ops* ops_of(codec view) noexcept;

/** @brief 从 codec 策略类型构造类型擦除函数表。 */
template <typename Codec, typename T> [[nodiscard]] codec_ops make_codec_ops_from() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  codec_ops data{};
  data.type = meta::detail::descriptor_type_id<type>();

  if constexpr (Codec::can_write) {
    if constexpr (requires(const serialization::context& context, serialization::writer& writer,
                           const type& value) { Codec::write(context, writer, value); }) {
      data.write = [](const serialization::context& context, serialization::writer& writer,
                      const any& value) -> error {
        const type* stored = value.try_get<type>();
        if (stored == nullptr) {
          return {error_domain::serialization, errc::invalid_type};
        }

        return Codec::write(context, writer, *stored);
      };
    } else if constexpr (requires(serialization::writer& writer, const type& value) {
                           Codec::write(writer, value);
                         }) {
      data.write = [](const serialization::context&, serialization::writer& writer,
                      const any& value) -> error {
        const type* stored = value.try_get<type>();
        if (stored == nullptr) {
          return {error_domain::serialization, errc::invalid_type};
        }

        return Codec::write(writer, *stored);
      };
    }
  }

  if constexpr (Codec::can_read) {
    if constexpr (requires(const serialization::context& context, serialization::reader& reader,
                           type& value) { Codec::read(context, reader, value); }) {
      data.read = [](const serialization::context& context, serialization::reader& reader,
                     const any_ref& target) -> error {
        type decoded{};
        error status = Codec::read(context, reader, decoded);
        if (status) {
          return status;
        }

        if (!target.try_set<type>(decoded)) {
          return {error_domain::serialization, errc::unsupported};
        }

        return {};
      };
    } else if constexpr (requires(serialization::reader& reader, type& value) {
                           Codec::read(reader, value);
                         }) {
      data.read = [](const serialization::context&, serialization::reader& reader,
                     const any_ref& target) -> error {
        type decoded{};
        error status = Codec::read(reader, decoded);
        if (status) {
          return status;
        }

        if (!target.try_set<type>(decoded)) {
          return {error_domain::serialization, errc::unsupported};
        }

        return {};
      };
    }
  }

  if constexpr (Codec::can_make_value) {
    if constexpr (requires(const serialization::context& context, const type& value) {
                    Codec::make_value(context, value);
                  }) {
      data.make_value = [](const serialization::context& context,
                           const any& value) -> variant_result {
        const type* stored = value.try_get<type>();
        if (stored == nullptr) {
          return {{error_domain::serialization, errc::invalid_type}, {}};
        }

        return Codec::make_value(context, *stored);
      };
    } else if constexpr (requires(const type& value) { Codec::make_value(value); }) {
      data.make_value = [](const serialization::context&, const any& value) -> variant_result {
        const type* stored = value.try_get<type>();
        if (stored == nullptr) {
          return {{error_domain::serialization, errc::invalid_type}, {}};
        }

        return Codec::make_value(*stored);
      };
    }
  }

  return data;
}

/** @brief 从公开 `serialization::codec_traits<T>` 特化构造类型擦除函数表。 */
template <typename T> [[nodiscard]] codec_ops make_codec_ops() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  return make_codec_ops_from<serialization::codec_traits<type>, type>();
}

/** @brief 查询公开 codec 特化对应的静态操作表。 */
template <typename T> [[nodiscard]] const codec_ops* codec_ops_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  static const codec_ops data = make_codec_ops<type>();
  return data.valid() ? &data : nullptr;
}

} // namespace caors::serialization::detail

namespace caors::meta::capabilities {

/** @brief 让 `codec_traits<T>` 成为序列化 codec 的静态能力提供者。 */
template <typename T> struct static_capability<serialization::detail::codec_ops, T> {
  [[nodiscard]] static const serialization::detail::codec_ops* get() {
    return serialization::detail::codec_ops_ptr_of<T>();
  }
};

template <typename T>
struct module_static_capability_list<serialization_static_capability_module, T> {
  using type = type_list<serialization::detail::codec_ops>;
};

} // namespace caors::meta::capabilities

namespace caors::serialization {

template <typename T> [[nodiscard]] codec codec_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  return find_codec(meta::type_of<type>());
}

} // namespace caors::serialization

#endif
