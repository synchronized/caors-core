// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_CODEC_H_
#define CAORS_SERIALIZATION_CODEC_H_

#include "caors/reflection/field.h"
#include "caors/serialization/reader.h"
#include "caors/serialization/writer.h"
#include "caors/value/any_ref.h"

#include <optional>
#include <type_traits>

namespace caors::serialization {

struct context;

class codec;

namespace detail {
struct codec_descriptor;
class codec_registry;
[[nodiscard]] CAORS_API const codec_descriptor* descriptor_of(codec view) noexcept;
} // namespace detail

/** @brief 可特化的类型序列化 codec。 */
template <typename T> struct codec_traits {
  static constexpr bool can_write = false;
  static constexpr bool can_read = false;
  static constexpr bool can_make_value = false;
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
  friend class detail::codec_registry;
  friend const detail::codec_descriptor* detail::descriptor_of(codec) noexcept;

  constexpr explicit codec(const detail::codec_descriptor* descriptor) noexcept
      : descriptor_(descriptor) {}

  const detail::codec_descriptor* descriptor_ = nullptr;
};

/** @brief 把运行期 C++ 值转换为序列化中间值。 */
[[nodiscard]] CAORS_API variant_result to_variant(const any& data);

/** @brief 把运行期 C++ 引用转换为序列化中间值。不可拷贝引用会返回不支持。 */
[[nodiscard]] CAORS_API variant_result to_variant(const any_ref& data);

/** @brief 按类型 id 查询已索引的 codec 视图。 */
[[nodiscard]] CAORS_API codec find_codec(type_id id);

/** @brief 按 meta 类型查询已索引的 codec 视图。 */
[[nodiscard]] CAORS_API codec find_codec(meta::type type);

/** @brief 按 C++ 类型查询静态 codec 视图；类型不要求已经显式注册。 */
template <typename T> [[nodiscard]] codec codec_of();

/** @brief 按 C++ 类型注册已特化的序列化 codec。 */
template <typename T> [[nodiscard]] error register_codec();

} // namespace caors::serialization

namespace caors::serialization::detail {

using codec_write_fn = error (*)(writer& writer, const any& data);
using codec_context_write_fn = error (*)(const serialization::context& context, writer& writer,
                                         const any& data);
using codec_read_fn = error (*)(reader& reader, const reflection::field& field, void* object);
using codec_context_read_fn = error (*)(const serialization::context& context, reader& reader,
                                        const reflection::field& field, void* object);
using codec_read_value_fn = error (*)(reader& reader, const any_ref& target);
using codec_context_read_value_fn = error (*)(const serialization::context& context, reader& reader,
                                              const any_ref& target);
using codec_value_fn = variant_result (*)(const any& data);
using codec_context_value_fn = variant_result (*)(const serialization::context& context,
                                                  const any& data);

/** @brief 单个类型的静态序列化 codec 描述。 */
struct codec_descriptor {
  type_id type{};
  const meta::detail::type_descriptor* value_type = nullptr;
  codec_write_fn write = nullptr;
  codec_context_write_fn write_context = nullptr;
  codec_read_fn read = nullptr;
  codec_context_read_fn read_context = nullptr;
  codec_read_value_fn read_value = nullptr;
  codec_context_read_value_fn read_value_context = nullptr;
  codec_value_fn make_value = nullptr;
  codec_context_value_fn make_value_context = nullptr;

  [[nodiscard]] constexpr bool valid() const noexcept {
    return type && value_type != nullptr &&
           (write != nullptr || write_context != nullptr || read != nullptr ||
            read_context != nullptr || read_value != nullptr || read_value_context != nullptr ||
            make_value != nullptr || make_value_context != nullptr);
  }
};

/** @brief 运行期序列化 codec 注册表。内部入口，公开代码优先使用
 * `serialization::register_codec<T>()`。 */
class CAORS_API codec_registry final {
public:
  [[nodiscard]] static codec_registry& instance() noexcept;

  codec_registry(const codec_registry&) = delete;
  codec_registry& operator=(const codec_registry&) = delete;

  [[nodiscard]] error register_codec(type_id id, const codec_descriptor& codec);
  [[nodiscard]] codec find_codec(type_id id) const;
  [[nodiscard]] bool contains(type_id id) const;
  [[nodiscard]] std::size_t size() const noexcept;

private:
  codec_registry() = default;
  ~codec_registry() = default;
};

/** @brief 获取进程内全局序列化 codec 注册表。内部入口。 */
[[nodiscard]] CAORS_API codec_registry& get_codec_registry() noexcept;

/** @brief 按类型 id 注册序列化 codec。内部入口，公开代码优先使用
 * `serialization::register_codec<T>()`。 */
[[nodiscard]] CAORS_API error register_serialization_codec(type_id id,
                                                           const codec_descriptor& codec);

/** @brief 从 codec 视图取回内部 descriptor。 */
[[nodiscard]] CAORS_API const codec_descriptor* descriptor_of(codec view) noexcept;

/** @brief 从 codec 策略类型构造类型擦除函数表。 */
template <typename Codec, typename T> [[nodiscard]] codec_descriptor make_codec_descriptor_from() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  codec_descriptor data{};
  data.type = meta::detail::descriptor_type_id<type>();
  data.value_type = meta::detail::descriptor_ptr_of<type>();

  if constexpr (Codec::can_write) {
    if constexpr (requires(serialization::writer& writer, const type& value) {
                    Codec::write(writer, value);
                  }) {
      data.write = [](serialization::writer& writer, const any& value) -> error {
        const type* stored = value.try_get<type>();
        if (stored == nullptr) {
          return {error_domain::serialization, errc::invalid_type};
        }

        return Codec::write(writer, *stored);
      };
    }

    if constexpr (requires(const serialization::context& context, serialization::writer& writer,
                           const type& value) { Codec::write(context, writer, value); }) {
      data.write_context = [](const serialization::context& context, serialization::writer& writer,
                              const any& value) -> error {
        const type* stored = value.try_get<type>();
        if (stored == nullptr) {
          return {error_domain::serialization, errc::invalid_type};
        }

        return Codec::write(context, writer, *stored);
      };
    }
  }

  if constexpr (Codec::can_read) {
    if constexpr (requires(serialization::reader& reader, type& value) {
                    Codec::read(reader, value);
                  }) {
      data.read = [](serialization::reader& reader, const reflection::field& field,
                     void* object) -> error {
        type decoded{};
        error status = Codec::read(reader, decoded);
        if (status) {
          return status;
        }

        if (!field.try_set<type>(object, decoded)) {
          return {error_domain::serialization, errc::unsupported};
        }

        return {};
      };
    }

    if constexpr (requires(const serialization::context& context, serialization::reader& reader,
                           type& value) { Codec::read(context, reader, value); }) {
      data.read_context = [](const serialization::context& context, serialization::reader& reader,
                             const reflection::field& field, void* object) -> error {
        type decoded{};
        error status = Codec::read(context, reader, decoded);
        if (status) {
          return status;
        }

        if (!field.try_set<type>(object, decoded)) {
          return {error_domain::serialization, errc::unsupported};
        }

        return {};
      };
    }

    if constexpr (requires(serialization::reader& reader, type& value) {
                    Codec::read(reader, value);
                  }) {
      data.read_value = [](serialization::reader& reader, const any_ref& target) -> error {
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

    if constexpr (requires(const serialization::context& context, serialization::reader& reader,
                           type& value) { Codec::read(context, reader, value); }) {
      data.read_value_context = [](const serialization::context& context,
                                   serialization::reader& reader, const any_ref& target) -> error {
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
    }
  }

  if constexpr (Codec::can_make_value) {
    if constexpr (requires(const type& value) { Codec::make_value(value); }) {
      data.make_value = [](const any& value) -> variant_result {
        const type* stored = value.try_get<type>();
        if (stored == nullptr) {
          return {{error_domain::serialization, errc::invalid_type}, {}};
        }

        return Codec::make_value(*stored);
      };
    }

    if constexpr (requires(const serialization::context& context, const type& value) {
                    Codec::make_value(context, value);
                  }) {
      data.make_value_context = [](const serialization::context& context,
                                   const any& value) -> variant_result {
        const type* stored = value.try_get<type>();
        if (stored == nullptr) {
          return {{error_domain::serialization, errc::invalid_type}, {}};
        }

        return Codec::make_value(context, *stored);
      };
    }
  }

  return data;
}

/** @brief 从公开 `serialization::codec_traits<T>` 特化构造类型擦除函数表。 */
template <typename T> [[nodiscard]] codec_descriptor make_codec_descriptor() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  return make_codec_descriptor_from<serialization::codec_traits<type>, type>();
}

/** @brief 查询公开 codec 特化对应的静态 descriptor。 */
template <typename T> [[nodiscard]] const codec_descriptor* codec_descriptor_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  static const codec_descriptor data = make_codec_descriptor<type>();
  return data.valid() ? &data : nullptr;
}

/** @brief 按 C++ 类型注册已特化的序列化 codec。内部入口。 */
template <typename T> [[nodiscard]] error register_serialization_codec() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  const codec_descriptor* codec = codec_descriptor_ptr_of<type>();
  if (codec == nullptr) {
    return {error_domain::serialization, errc::unsupported};
  }

  return register_serialization_codec(codec->type, *codec);
}

} // namespace caors::serialization::detail

namespace caors::serialization {

template <typename T> [[nodiscard]] codec codec_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if (const detail::codec_descriptor* descriptor = detail::codec_descriptor_ptr_of<type>();
      descriptor != nullptr) {
    return codec{descriptor};
  }

  return find_codec(meta::type_of<type>());
}

/** @brief 按 C++ 类型注册已特化的序列化 codec。 */
template <typename T> [[nodiscard]] error register_codec() {
  return detail::register_serialization_codec<T>();
}

} // namespace caors::serialization

#endif
