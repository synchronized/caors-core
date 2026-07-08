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

#include <optional>
#include <type_traits>

namespace caors::serialization {

struct context;

class codec;

namespace detail {
struct codec_ops;
class codec_registry;
[[nodiscard]] CAORS_API const codec_ops* ops_of(codec view) noexcept;
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
  friend const detail::codec_ops* detail::ops_of(codec) noexcept;

  constexpr explicit codec(const detail::codec_ops* ops) noexcept : ops_(ops) {}

  const detail::codec_ops* ops_ = nullptr;
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

/** @brief 运行期序列化 codec 能力索引包装。内部入口，公开代码优先使用
 * `serialization::register_codec<T>()`。 */
class CAORS_API codec_registry final {
public:
  [[nodiscard]] static codec_registry& instance() noexcept;

  codec_registry(const codec_registry&) = delete;
  codec_registry& operator=(const codec_registry&) = delete;

  [[nodiscard]] error register_codec(type_id id, const codec_ops& codec);
  [[nodiscard]] codec find_codec(type_id id) const;
  [[nodiscard]] bool contains(type_id id) const;
  [[nodiscard]] std::size_t size() const noexcept;

private:
  codec_registry() = default;
  ~codec_registry() = default;
};

/** @brief 获取进程内全局序列化 codec 能力索引包装。内部入口。 */
[[nodiscard]] CAORS_API codec_registry& get_codec_registry() noexcept;

/** @brief 按类型 id 注册序列化 codec。内部入口，公开代码优先使用
 * `serialization::register_codec<T>()`。 */
[[nodiscard]] CAORS_API error register_serialization_codec(type_id id,
                                                           const codec_ops& codec);

/** @brief 从 codec 视图取回内部操作表。 */
[[nodiscard]] CAORS_API const codec_ops* ops_of(codec view) noexcept;

/** @brief 从 codec 策略类型构造类型擦除函数表。 */
template <typename Codec, typename T> [[nodiscard]] codec_ops make_codec_ops_from() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  codec_ops data{};
  data.type = meta::detail::descriptor_type_id<type>();

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

/** @brief 按 C++ 类型注册已特化的序列化 codec。内部入口。 */
template <typename T> [[nodiscard]] error register_serialization_codec() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  const codec_ops* codec = codec_ops_ptr_of<type>();
  if (codec == nullptr) {
    return {error_domain::serialization, errc::unsupported};
  }

  return register_serialization_codec(codec->type, *codec);
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
  if (const detail::codec_ops* ops =
          meta::capabilities::get<detail::codec_ops, type>();
      ops != nullptr) {
    return codec{ops};
  }

  return find_codec(meta::detail::type_id_of_cpp_type<type>());
}

/** @brief 按 C++ 类型注册已特化的序列化 codec。 */
template <typename T> [[nodiscard]] error register_codec() {
  return detail::register_serialization_codec<T>();
}

} // namespace caors::serialization

#endif
