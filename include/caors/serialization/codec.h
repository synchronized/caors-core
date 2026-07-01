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

namespace caors {

using serialization_codec_write_fn = error (*)(serialization_writer& writer, const any& data);
using serialization_codec_read_fn = error (*)(serialization_reader& reader,
                                              const reflection_field& field, void* object);
using serialization_codec_value_fn = variant_result (*)(const any& data);

/** @brief 单个类型的序列化 codec 函数集合。 */
struct serialization_codec_data {
  serialization_codec_write_fn write = nullptr;
  serialization_codec_read_fn read = nullptr;
  serialization_codec_value_fn make_value = nullptr;

  [[nodiscard]] constexpr bool valid() const noexcept {
    return write != nullptr || read != nullptr || make_value != nullptr;
  }
};

/** @brief 可特化的类型序列化 codec。 */
template <typename T> struct serialization_codec {
  static constexpr bool can_write = false;
  static constexpr bool can_read = false;
  static constexpr bool can_make_value = false;
};

/** @brief 运行期序列化 codec 注册表。 */
class CAORS_API serialization_codec_registry final {
public:
  [[nodiscard]] static serialization_codec_registry& instance() noexcept;

  serialization_codec_registry(const serialization_codec_registry&) = delete;
  serialization_codec_registry& operator=(const serialization_codec_registry&) = delete;

  [[nodiscard]] error register_codec(type_id id, serialization_codec_data codec);
  [[nodiscard]] std::optional<serialization_codec_data> find_codec(type_id id) const;
  [[nodiscard]] bool contains(type_id id) const;
  [[nodiscard]] std::size_t size() const noexcept;

private:
  serialization_codec_registry() = default;
  ~serialization_codec_registry() = default;
};

/** @brief 获取进程内全局序列化 codec 注册表。 */
[[nodiscard]] CAORS_API serialization_codec_registry& get_serialization_codec_registry() noexcept;

/** @brief 把运行期 C++ 值转换为序列化中间值。 */
[[nodiscard]] CAORS_API variant_result to_variant(const any& data);

/** @brief 把运行期 C++ 引用转换为序列化中间值。不可拷贝引用会返回不支持。 */
[[nodiscard]] CAORS_API variant_result to_variant(const any_ref& data);

/** @brief 按类型 id 注册序列化 codec。 */
[[nodiscard]] CAORS_API error register_serialization_codec(type_id id,
                                                           serialization_codec_data codec);

/** @brief 按 C++ 类型注册已特化的序列化 codec。 */
template <typename T> [[nodiscard]] error register_serialization_codec() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  serialization_codec_data codec{};

  if constexpr (serialization_codec<type>::can_write) {
    codec.write = [](serialization_writer& writer, const any& data) -> error {
      const type* stored = data.try_get<type>();
      if (stored == nullptr) {
        return {error_domain::serialization, errc::invalid_type};
      }

      return serialization_codec<type>::write(writer, *stored);
    };
  }

  if constexpr (serialization_codec<type>::can_read) {
    codec.read = [](serialization_reader& reader, const reflection_field& field,
                    void* object) -> error {
      type decoded{};
      error status = serialization_codec<type>::read(reader, decoded);
      if (status) {
        return status;
      }

      if (!field.try_set<type>(object, decoded)) {
        return {error_domain::serialization, errc::unsupported};
      }

      return {};
    };
  }

  if constexpr (serialization_codec<type>::can_make_value) {
    codec.make_value = [](const any& data) -> variant_result {
      const type* stored = data.try_get<type>();
      if (stored == nullptr) {
        return {{error_domain::serialization, errc::invalid_type}, {}};
      }

      return serialization_codec<type>::make_value(*stored);
    };
  }

  if (!codec.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  if constexpr (detail::has_type_name_v<type>) {
    const error result = caors::register_type<type>(type_name<type>::value);
    if (result && result.value() != errc::duplicate_id) {
      return {error_domain::serialization, errc::invalid_type};
    }
  }

  const meta_type meta = get_meta_registry().ensure_type_for<type>();
  if (!meta.valid()) {
    return {error_domain::serialization, errc::invalid_type};
  }

  return register_serialization_codec(meta.id(), codec);
}

} // namespace caors

#endif
