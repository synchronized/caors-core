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

/** @brief 可特化的类型序列化 codec。 */
template <typename T> struct codec {
  static constexpr bool can_write = false;
  static constexpr bool can_read = false;
  static constexpr bool can_make_value = false;
};

/** @brief 把运行期 C++ 值转换为序列化中间值。 */
[[nodiscard]] CAORS_API variant_result to_variant(const any& data);

/** @brief 把运行期 C++ 引用转换为序列化中间值。不可拷贝引用会返回不支持。 */
[[nodiscard]] CAORS_API variant_result to_variant(const any_ref& data);

/** @brief 按 C++ 类型注册已特化的序列化 codec。 */
template <typename T> [[nodiscard]] error register_codec();

} // namespace caors::serialization

namespace caors::serialization::detail {

using codec_write_fn = error (*)(writer& writer, const any& data);
using codec_context_write_fn = error (*)(writer& writer, const any& data,
                                         const serialization::context& context);
using codec_read_fn = error (*)(reader& reader, const reflection::field& field, void* object);
using codec_context_read_fn = error (*)(reader& reader, const reflection::field& field,
                                        void* object, const serialization::context& context);
using codec_read_value_fn = error (*)(reader& reader, const any_ref& target);
using codec_context_read_value_fn = error (*)(reader& reader, const any_ref& target,
                                              const serialization::context& context);
using codec_value_fn = variant_result (*)(const any& data);
using codec_context_value_fn = variant_result (*)(const any& data,
                                                  const serialization::context& context);

/** @brief 单个类型的序列化 codec 函数集合。 */
struct codec_data {
  codec_write_fn write = nullptr;
  codec_context_write_fn write_context = nullptr;
  codec_read_fn read = nullptr;
  codec_context_read_fn read_context = nullptr;
  codec_read_value_fn read_value = nullptr;
  codec_context_read_value_fn read_value_context = nullptr;
  codec_value_fn make_value = nullptr;
  codec_context_value_fn make_value_context = nullptr;

  [[nodiscard]] constexpr bool valid() const noexcept {
    return write != nullptr || write_context != nullptr || read != nullptr ||
           read_context != nullptr || read_value != nullptr || read_value_context != nullptr ||
           make_value != nullptr || make_value_context != nullptr;
  }
};

/** @brief 运行期序列化 codec 注册表。内部入口，公开代码优先使用
 * `serialization::register_codec<T>()`。 */
class CAORS_API codec_registry final {
public:
  [[nodiscard]] static codec_registry& instance() noexcept;

  codec_registry(const codec_registry&) = delete;
  codec_registry& operator=(const codec_registry&) = delete;

  [[nodiscard]] error register_codec(type_id id, codec_data codec);
  [[nodiscard]] std::optional<codec_data> find_codec(type_id id) const;
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
[[nodiscard]] CAORS_API error register_serialization_codec(type_id id, codec_data codec);

/** @brief 从 codec 策略类型构造类型擦除函数表。 */
template <typename Codec, typename T> [[nodiscard]] codec_data make_codec_data_from() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  codec_data data{};

  if constexpr (Codec::can_write) {
    data.write = [](serialization::writer& writer, const any& value) -> error {
      const type* stored = value.try_get<type>();
      if (stored == nullptr) {
        return {error_domain::serialization, errc::invalid_type};
      }

      return Codec::write(writer, *stored);
    };

    if constexpr (requires(serialization::writer& writer, const type& value,
                           const serialization::context& context) {
                    Codec::write(writer, value, context);
                  }) {
      data.write_context = [](serialization::writer& writer, const any& value,
                              const serialization::context& context) -> error {
        const type* stored = value.try_get<type>();
        if (stored == nullptr) {
          return {error_domain::serialization, errc::invalid_type};
        }

        return Codec::write(writer, *stored, context);
      };
    }
  }

  if constexpr (Codec::can_read) {
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

    if constexpr (requires(serialization::reader& reader, type& value,
                           const serialization::context& context) {
                    Codec::read(reader, value, context);
                  }) {
      data.read_context = [](serialization::reader& reader, const reflection::field& field,
                             void* object, const serialization::context& context) -> error {
        type decoded{};
        error status = Codec::read(reader, decoded, context);
        if (status) {
          return status;
        }

        if (!field.try_set<type>(object, decoded)) {
          return {error_domain::serialization, errc::unsupported};
        }

        return {};
      };
    }

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

    if constexpr (requires(serialization::reader& reader, type& value,
                           const serialization::context& context) {
                    Codec::read(reader, value, context);
                  }) {
      data.read_value_context = [](serialization::reader& reader, const any_ref& target,
                                   const serialization::context& context) -> error {
        type decoded{};
        error status = Codec::read(reader, decoded, context);
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
    data.make_value = [](const any& value) -> variant_result {
      const type* stored = value.try_get<type>();
      if (stored == nullptr) {
        return {{error_domain::serialization, errc::invalid_type}, {}};
      }

      return Codec::make_value(*stored);
    };

    if constexpr (requires(const type& value, const serialization::context& context) {
                    Codec::make_value(value, context);
                  }) {
      data.make_value_context = [](const any& value,
                                   const serialization::context& context) -> variant_result {
        const type* stored = value.try_get<type>();
        if (stored == nullptr) {
          return {{error_domain::serialization, errc::invalid_type}, {}};
        }

        return Codec::make_value(*stored, context);
      };
    }
  }

  return data;
}

/** @brief 从公开 `serialization::codec<T>` 特化构造类型擦除函数表。 */
template <typename T> [[nodiscard]] codec_data make_codec_data() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  return make_codec_data_from<serialization::codec<type>, type>();
}

/** @brief 查询公开 codec 特化对应的静态函数表。 */
template <typename T> [[nodiscard]] const codec_data* codec_data_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  static const codec_data data = make_codec_data<type>();
  return data.valid() ? &data : nullptr;
}

/** @brief 按 C++ 类型注册已特化的序列化 codec。内部入口。 */
template <typename T> [[nodiscard]] error register_serialization_codec() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  const codec_data* codec = codec_data_ptr_of<type>();
  if (codec == nullptr) {
    return {error_domain::serialization, errc::unsupported};
  }

  const meta::type meta_type = meta::get_registry().find_type<type>();
  if (!meta_type) {
    return {error_domain::serialization, errc::invalid_type};
  }

  return register_serialization_codec(meta_type.id(), *codec);
}

} // namespace caors::serialization::detail

namespace caors::serialization {

/** @brief 按 C++ 类型注册已特化的序列化 codec。 */
template <typename T> [[nodiscard]] error register_codec() {
  return detail::register_serialization_codec<T>();
}

} // namespace caors::serialization

#endif
