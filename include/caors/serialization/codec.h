// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_CODEC_H_
#define CAORS_SERIALIZATION_CODEC_H_

#include "caors/reflection/view.h"
#include "caors/serialization/reader.h"
#include "caors/serialization/writer.h"
#include "caors/value/any.h"

#include <optional>
#include <type_traits>

namespace caors::serialization {

using codec_write_fn = error (*)(writer& writer, const any& data);
using codec_read_fn = error (*)(reader& reader, const reflection::field& field, void* object);
using codec_read_value_fn = error (*)(reader& reader, const any_ref& target);
using codec_value_fn = variant_result (*)(const any& data);

/** @brief 单个类型的序列化 codec 函数集合。 */
struct codec_data {
  codec_write_fn write = nullptr;
  codec_read_fn read = nullptr;
  codec_read_value_fn read_value = nullptr;
  codec_value_fn make_value = nullptr;

  [[nodiscard]] constexpr bool valid() const noexcept {
    return write != nullptr || read != nullptr || read_value != nullptr || make_value != nullptr;
  }
};

/** @brief 可特化的类型序列化 codec。 */
template <typename T> struct codec {
  static constexpr bool can_write = false;
  static constexpr bool can_read = false;
  static constexpr bool can_make_value = false;
};

/** @brief 运行期序列化 codec 注册表。 */
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

/** @brief 获取进程内全局序列化 codec 注册表。 */
[[nodiscard]] CAORS_API codec_registry& get_codec_registry() noexcept;

/** @brief 把运行期 C++ 值转换为序列化中间值。 */
[[nodiscard]] CAORS_API variant_result to_variant(const any& data);

/** @brief 把运行期 C++ 引用转换为序列化中间值。不可拷贝引用会返回不支持。 */
[[nodiscard]] CAORS_API variant_result to_variant(const any_ref& data);

/** @brief 按 C++ 类型注册已特化的序列化 codec。 */
template <typename T> [[nodiscard]] error register_codec();

} // namespace caors::serialization

namespace caors::serialization::detail {

/** @brief 按类型 id 注册序列化 codec。内部入口，公开代码优先使用
 * `serialization::register_codec<T>()`。 */
[[nodiscard]] CAORS_API error register_serialization_codec(type_id id,
                                                           serialization::codec_data codec);

/** @brief 按 C++ 类型注册已特化的序列化 codec。内部入口。 */
template <typename T> [[nodiscard]] error register_serialization_codec() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  serialization::codec_data codec{};

  if constexpr (serialization::codec<type>::can_write) {
    codec.write = [](serialization::writer& writer, const any& data) -> error {
      const type* stored = data.try_get<type>();
      if (stored == nullptr) {
        return {error_domain::serialization, errc::invalid_type};
      }

      return serialization::codec<type>::write(writer, *stored);
    };
  }

  if constexpr (serialization::codec<type>::can_read) {
    codec.read = [](serialization::reader& reader, const reflection::field& field,
                    void* object) -> error {
      type decoded{};
      error status = serialization::codec<type>::read(reader, decoded);
      if (status) {
        return status;
      }

      if (!field.try_set<type>(object, decoded)) {
        return {error_domain::serialization, errc::unsupported};
      }

      return {};
    };

    codec.read_value = [](serialization::reader& reader, const any_ref& target) -> error {
      type decoded{};
      error status = serialization::codec<type>::read(reader, decoded);
      if (status) {
        return status;
      }

      if (!target.try_set<type>(decoded)) {
        return {error_domain::serialization, errc::unsupported};
      }

      return {};
    };
  }

  if constexpr (serialization::codec<type>::can_make_value) {
    codec.make_value = [](const any& data) -> variant_result {
      const type* stored = data.try_get<type>();
      if (stored == nullptr) {
        return {{error_domain::serialization, errc::invalid_type}, {}};
      }

      return serialization::codec<type>::make_value(*stored);
    };
  }

  if (!codec.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  const meta::type_view meta_type = meta::get_registry().find_type<type>();
  if (!meta_type) {
    return {error_domain::serialization, errc::invalid_type};
  }

  return register_serialization_codec(meta_type.id(), codec);
}

} // namespace caors::serialization::detail

namespace caors::serialization {

/** @brief 按 C++ 类型注册已特化的序列化 codec。 */
template <typename T> [[nodiscard]] error register_codec() {
  return detail::register_serialization_codec<T>();
}

} // namespace caors::serialization

#endif
