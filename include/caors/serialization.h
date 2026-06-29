// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_H_
#define CAORS_SERIALIZATION_H_

#include "caors/core/containers.h"
#include "caors/error.h"
#include "caors/export.h"
#include "caors/meta.h"
#include "caors/reflection.h"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string_view>
#include <type_traits>

namespace caors {

/** @brief 序列化格式。当前 MVP 仅提供简单文本格式。 */
enum class serialization_format {
  text,
};

/** @brief 序列化后端能力。 */
enum class serialization_format_features : std::uint32_t {
  none = 0,
  named_fields = 1U << 0,
  numeric_fields = 1U << 1,
  ordered_fields = 1U << 2,
  self_describing = 1U << 3,
  schema_required = 1U << 4,
};

[[nodiscard]] constexpr serialization_format_features
operator|(serialization_format_features lhs, serialization_format_features rhs) noexcept {
  return static_cast<serialization_format_features>(static_cast<std::uint32_t>(lhs) |
                                                    static_cast<std::uint32_t>(rhs));
}

[[nodiscard]] constexpr serialization_format_features
operator&(serialization_format_features lhs, serialization_format_features rhs) noexcept {
  return static_cast<serialization_format_features>(static_cast<std::uint32_t>(lhs) &
                                                    static_cast<std::uint32_t>(rhs));
}

[[nodiscard]] constexpr bool has_feature(serialization_format_features features,
                                         serialization_format_features feature) noexcept {
  return (features & feature) != serialization_format_features::none;
}

/** @brief 单个字段的序列化配置。 */
struct serialization_field_data {
  string_name field_name{};
  string_name serialized_name{};
  std::uint32_t field_id = 0;
  std::uint32_t version_added = 0;
  std::uint32_t version_removed = 0;
  bool required = false;
  bool skipped = false;

  [[nodiscard]] constexpr bool valid() const noexcept { return !field_name.empty(); }
};

/**
 * @brief 序列化类型元信息。
 *
 * 字段配置按字段名引用反射字段。注册序列化组件时不要求反射字段已经注册，实际序列化时再解析
 * 字段配置和反射字段。
 */
struct serialization_type_data {
  serialization_format default_format = serialization_format::text;
  vector<serialization_field_data> fields{};

  [[nodiscard]] bool valid() const noexcept {
    if (default_format != serialization_format::text) {
      return false;
    }

    for (const serialization_field_data& field : fields) {
      if (!field.valid()) {
        return false;
      }
    }

    return true;
  }
};

/** @brief 创建字段序列化配置。 */
[[nodiscard]] inline serialization_field_data
make_serialization_field(std::string_view field_name, std::string_view serialized_name = {},
                         bool skipped = false, std::uint32_t field_id = 0) {
  return serialization_field_data{
      string_name{field_name},
      serialized_name.empty() ? string_name{field_name} : string_name{serialized_name},
      field_id,
      0,
      0,
      false,
      skipped,
  };
}

/** @brief 创建类型序列化配置。 */
[[nodiscard]] inline serialization_type_data
make_serialization_type_data(std::initializer_list<serialization_field_data> fields) {
  return serialization_type_data{serialization_format::text,
                                 vector<serialization_field_data>{fields}};
}

/**
 * @brief 单次序列化调度上下文。
 *
 * `schema_version == 0` 表示不启用字段版本过滤。传入非 0 版本时，调度层只处理当前版本可见的
 * 字段。
 */
struct serialization_context {
  std::uint32_t schema_version = 0;
};

/**
 * @brief 底层全局序列化注册表。
 *
 * 注册表复用全局 `meta_registry`，把 `serialization_type_data` 作为组件挂载到同一个
 * `type_id` 上。当前序列化实现依赖反射字段信息读取对象数据。
 */
class CAORS_API serialization_registry final {
public:
  [[nodiscard]] static serialization_registry& instance() noexcept;

  serialization_registry(const serialization_registry&) = delete;
  serialization_registry& operator=(const serialization_registry&) = delete;

  [[nodiscard]] error register_type(type_id id,
                                    serialization_type_data data = serialization_type_data{});
  [[nodiscard]] bool contains(type_id id) const;
  [[nodiscard]] std::optional<serialization_type_data> find_type(type_id id) const;
  [[nodiscard]] std::size_t size() const noexcept;

  template <typename T>
  [[nodiscard]] error register_type_for(serialization_type_data data = serialization_type_data{}) {
    return register_type(get_meta_registry().ensure_type_for<T>().id(), data);
  }

private:
  serialization_registry() = default;
  ~serialization_registry() = default;
};

/** @brief 文本序列化结果。 */
struct serialization_text_result {
  error status{};
  string text{};

  [[nodiscard]] bool succeeded() const noexcept { return status.succeeded(); }
  [[nodiscard]] bool failed() const noexcept { return status.failed(); }
  [[nodiscard]] explicit operator bool() const noexcept { return succeeded(); }
};

/** @brief 序列化写出后端接口。 */
class CAORS_API serialization_writer {
public:
  serialization_writer() = default;
  virtual ~serialization_writer();

  serialization_writer(const serialization_writer&) = delete;
  serialization_writer& operator=(const serialization_writer&) = delete;
  serialization_writer(serialization_writer&&) = delete;
  serialization_writer& operator=(serialization_writer&&) = delete;

  [[nodiscard]] virtual serialization_format_features features() const noexcept = 0;
  [[nodiscard]] virtual error begin_object(const serialization_type_data& type) = 0;
  [[nodiscard]] virtual error begin_field(const serialization_field_data& field) = 0;
  [[nodiscard]] virtual error begin_array(std::size_t size) = 0;
  [[nodiscard]] virtual error begin_element(std::size_t index) = 0;
  [[nodiscard]] virtual error write_bool(bool value) = 0;
  [[nodiscard]] virtual error write_i64(std::int64_t value) = 0;
  [[nodiscard]] virtual error write_u64(std::uint64_t value) = 0;
  [[nodiscard]] virtual error write_f64(double value) = 0;
  [[nodiscard]] virtual error write_string(std::string_view value) = 0;
  [[nodiscard]] virtual error end_element() = 0;
  [[nodiscard]] virtual error end_array() = 0;
  [[nodiscard]] virtual error end_field() = 0;
  [[nodiscard]] virtual error end_object() = 0;
};

/** @brief 简单文本序列化写出后端。 */
class CAORS_API text_serialization_writer final : public serialization_writer {
public:
  text_serialization_writer() = default;

  [[nodiscard]] serialization_format_features features() const noexcept override;
  [[nodiscard]] error begin_object(const serialization_type_data& type) override;
  [[nodiscard]] error begin_field(const serialization_field_data& field) override;
  [[nodiscard]] error begin_array(std::size_t size) override;
  [[nodiscard]] error begin_element(std::size_t index) override;
  [[nodiscard]] error write_bool(bool value) override;
  [[nodiscard]] error write_i64(std::int64_t value) override;
  [[nodiscard]] error write_u64(std::uint64_t value) override;
  [[nodiscard]] error write_f64(double value) override;
  [[nodiscard]] error write_string(std::string_view value) override;
  [[nodiscard]] error end_element() override;
  [[nodiscard]] error end_array() override;
  [[nodiscard]] error end_field() override;
  [[nodiscard]] error end_object() override;

  [[nodiscard]] const string& text() const noexcept { return text_; }
  void reset();

private:
  enum class context_kind {
    object,
    array,
  };

  struct context {
    context_kind kind = context_kind::object;
    bool first = true;
  };

  [[nodiscard]] error prepare_value();
  void mark_value_written() noexcept;

  string text_{};
  vector<context> contexts_{};
  bool expecting_value_ = false;
  bool root_written_ = false;
};

/** @brief 序列化读入后端接口。 */
class CAORS_API serialization_reader {
public:
  serialization_reader() = default;
  virtual ~serialization_reader();

  serialization_reader(const serialization_reader&) = delete;
  serialization_reader& operator=(const serialization_reader&) = delete;
  serialization_reader(serialization_reader&&) = delete;
  serialization_reader& operator=(serialization_reader&&) = delete;

  [[nodiscard]] virtual serialization_format_features features() const noexcept = 0;
  [[nodiscard]] virtual error begin_object(const serialization_type_data& type) = 0;
  [[nodiscard]] virtual error begin_field(const serialization_field_data& field) = 0;
  [[nodiscard]] virtual error read_bool(bool& value) = 0;
  [[nodiscard]] virtual error read_i64(std::int64_t& value) = 0;
  [[nodiscard]] virtual error read_u64(std::uint64_t& value) = 0;
  [[nodiscard]] virtual error read_f64(double& value) = 0;
  [[nodiscard]] virtual error read_string(string& value) = 0;
  [[nodiscard]] virtual error end_field() = 0;
  [[nodiscard]] virtual error end_object() = 0;
};

/** @brief 序列化中间值类型。 */
enum class serialization_value_kind {
  null,
  boolean,
  signed_integer,
  unsigned_integer,
  floating,
  string,
  array,
  object,
};

class serialization_value;
struct serialization_member;

/** @brief 序列化 DOM 中间对象。 */
class CAORS_API serialization_value final {
public:
  using array_type = vector<serialization_value>;
  using object_type = vector<serialization_member>;

  serialization_value();
  ~serialization_value();

  serialization_value(const serialization_value& other);
  serialization_value(serialization_value&& other) noexcept;
  serialization_value& operator=(const serialization_value& other);
  serialization_value& operator=(serialization_value&& other) noexcept;

  [[nodiscard]] static serialization_value null();
  [[nodiscard]] static serialization_value from(bool value);
  [[nodiscard]] static serialization_value from(std::int64_t value);
  [[nodiscard]] static serialization_value from(std::uint64_t value);
  [[nodiscard]] static serialization_value from(double value);
  [[nodiscard]] static serialization_value from(std::string_view value);
  [[nodiscard]] static serialization_value array(array_type values);
  [[nodiscard]] static serialization_value object(object_type members);

  [[nodiscard]] serialization_value_kind kind() const noexcept { return kind_; }
  [[nodiscard]] bool is_null() const noexcept { return kind_ == serialization_value_kind::null; }
  [[nodiscard]] bool as_bool() const noexcept { return bool_value_; }
  [[nodiscard]] std::int64_t as_i64() const noexcept { return i64_value_; }
  [[nodiscard]] std::uint64_t as_u64() const noexcept { return u64_value_; }
  [[nodiscard]] double as_f64() const noexcept { return f64_value_; }
  [[nodiscard]] const string& as_string() const noexcept { return string_value_; }
  [[nodiscard]] const array_type& as_array() const noexcept { return array_value_; }
  [[nodiscard]] const object_type& as_object() const noexcept { return object_value_; }

  [[nodiscard]] array_type& mutable_array() noexcept { return array_value_; }
  [[nodiscard]] object_type& mutable_object() noexcept { return object_value_; }

private:
  serialization_value_kind kind_ = serialization_value_kind::null;
  bool bool_value_ = false;
  std::int64_t i64_value_ = 0;
  std::uint64_t u64_value_ = 0;
  double f64_value_ = 0.0;
  string string_value_{};
  array_type array_value_{};
  object_type object_value_{};
};

/** @brief 序列化对象成员，保留字段名、字段 id 和字段值。 */
struct serialization_member {
  string_name name{};
  std::uint32_t field_id = 0;
  serialization_value value{};
};

/** @brief 序列化中间值构造结果。 */
struct serialization_value_result {
  error status{};
  serialization_value value{};

  [[nodiscard]] bool succeeded() const noexcept { return status.succeeded(); }
  [[nodiscard]] bool failed() const noexcept { return status.failed(); }
  [[nodiscard]] explicit operator bool() const noexcept { return succeeded(); }
};

using serialization_codec_write_fn = error (*)(serialization_writer& writer, const value& data);
using serialization_codec_read_fn = error (*)(serialization_reader& reader,
                                              const reflection_field& field, void* object);
using serialization_codec_value_fn = serialization_value_result (*)(const value& data);

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

/** @brief 按类型 id 注册序列化 codec。 */
[[nodiscard]] CAORS_API error register_serialization_codec(type_id id,
                                                           serialization_codec_data codec);

/** @brief 按 C++ 类型注册已特化的序列化 codec。 */
template <typename T> [[nodiscard]] error register_serialization_codec() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  serialization_codec_data codec{};

  if constexpr (serialization_codec<type>::can_write) {
    codec.write = [](serialization_writer& writer, const value& data) -> error {
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
      if (status.failed()) {
        return status;
      }

      if (!field.try_set<type>(object, decoded)) {
        return {error_domain::serialization, errc::unsupported};
      }

      return {};
    };
  }

  if constexpr (serialization_codec<type>::can_make_value) {
    codec.make_value = [](const value& data) -> serialization_value_result {
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
    const type_register_result result = caors::register_type<type>(type_name<type>::value);
    if (!type_register_succeeded(result) && result != type_register_result::duplicate_id) {
      return {error_domain::serialization, errc::invalid_type};
    }
  }

  const meta_type type_info = get_meta_registry().ensure_type_for<type>();
  if (!type_info.valid()) {
    return {error_domain::serialization, errc::invalid_type};
  }

  return register_serialization_codec(type_info.id(), codec);
}

/** @brief 从序列化 DOM 中间对象读取数据的后端。 */
class CAORS_API value_serialization_reader final : public serialization_reader {
public:
  explicit value_serialization_reader(const serialization_value& value) noexcept;

  [[nodiscard]] serialization_format_features features() const noexcept override;
  [[nodiscard]] error begin_object(const serialization_type_data& type) override;
  [[nodiscard]] error begin_field(const serialization_field_data& field) override;
  [[nodiscard]] error read_bool(bool& value) override;
  [[nodiscard]] error read_i64(std::int64_t& value) override;
  [[nodiscard]] error read_u64(std::uint64_t& value) override;
  [[nodiscard]] error read_f64(double& value) override;
  [[nodiscard]] error read_string(string& value) override;
  [[nodiscard]] error end_field() override;
  [[nodiscard]] error end_object() override;

private:
  [[nodiscard]] const serialization_member*
  find_member(const serialization_field_data& field) const;
  [[nodiscard]] const serialization_value* current_value() const noexcept;

  const serialization_value* root_ = nullptr;
  const serialization_member* current_member_ = nullptr;
  bool object_open_ = false;
};

/** @brief 获取进程内全局序列化注册表。 */
[[nodiscard]] CAORS_API serialization_registry& get_serialization_registry() noexcept;

/** @brief 向全局序列化注册表注册类型。 */
[[nodiscard]] CAORS_API error
register_serialization(type_id id, serialization_type_data data = serialization_type_data{});

/** @brief 向全局序列化注册表注册指定 C++ 类型。 */
template <typename T>
[[nodiscard]] error
register_serialization(serialization_type_data data = serialization_type_data{}) {
  return get_serialization_registry().register_type_for<T>(data);
}

/** @brief 向全局序列化注册表注册指定 C++ 类型和字段配置。 */
template <typename T>
[[nodiscard]] error register_serialization(std::initializer_list<serialization_field_data> fields) {
  return register_serialization<T>(make_serialization_type_data(fields));
}

/** @brief 通过反射类型视图和指定后端序列化对象。 */
[[nodiscard]] CAORS_API error serialize(const reflection_type& type, const void* object,
                                        serialization_writer& writer);

/** @brief 通过反射类型视图、指定后端和上下文序列化对象。 */
[[nodiscard]] CAORS_API error serialize(const reflection_type& type, const void* object,
                                        serialization_writer& writer,
                                        const serialization_context& context);

/** @brief 通过类型 id 查找反射信息并使用指定后端序列化对象。 */
[[nodiscard]] CAORS_API error serialize(type_id id, const void* object,
                                        serialization_writer& writer);

/** @brief 通过类型 id 查找反射信息并使用指定后端和上下文序列化对象。 */
[[nodiscard]] CAORS_API error serialize(type_id id, const void* object,
                                        serialization_writer& writer,
                                        const serialization_context& context);

/** @brief 把已注册序列化和反射信息的 C++ 对象写入指定后端。 */
template <typename T> [[nodiscard]] error serialize(const T& object, serialization_writer& writer) {
  const std::optional<meta_type> type = caors::find_type<T>();
  if (!type.has_value()) {
    return {error_domain::serialization, errc::missing_type};
  }

  return serialize(type->id(), &object, writer);
}

/** @brief 把已注册序列化和反射信息的 C++ 对象按上下文写入指定后端。 */
template <typename T>
[[nodiscard]] error serialize(const T& object, serialization_writer& writer,
                              const serialization_context& context) {
  const std::optional<meta_type> type = caors::find_type<T>();
  if (!type.has_value()) {
    return {error_domain::serialization, errc::missing_type};
  }

  return serialize(type->id(), &object, writer, context);
}

/** @brief 将序列化中间值写入指定后端。 */
[[nodiscard]] CAORS_API error write(serialization_writer& writer, const serialization_value& value);

/** @brief 通过反射类型视图和指定后端反序列化到对象。 */
[[nodiscard]] CAORS_API error deserialize(const reflection_type& type, void* object,
                                          serialization_reader& reader);

/** @brief 通过反射类型视图、指定后端和上下文反序列化到对象。 */
[[nodiscard]] CAORS_API error deserialize(const reflection_type& type, void* object,
                                          serialization_reader& reader,
                                          const serialization_context& context);

/** @brief 通过类型 id 查找反射信息并使用指定后端反序列化到对象。 */
[[nodiscard]] CAORS_API error deserialize(type_id id, void* object, serialization_reader& reader);

/** @brief 通过类型 id 查找反射信息并使用指定后端和上下文反序列化到对象。 */
[[nodiscard]] CAORS_API error deserialize(type_id id, void* object, serialization_reader& reader,
                                          const serialization_context& context);

/** @brief 把已注册序列化和反射信息的 C++ 对象从指定后端读入。 */
template <typename T> [[nodiscard]] error deserialize(T& object, serialization_reader& reader) {
  const std::optional<meta_type> type = caors::find_type<T>();
  if (!type.has_value()) {
    return {error_domain::serialization, errc::missing_type};
  }

  return deserialize(type->id(), &object, reader);
}

/** @brief 把已注册序列化和反射信息的 C++ 对象按上下文从指定后端读入。 */
template <typename T>
[[nodiscard]] error deserialize(T& object, serialization_reader& reader,
                                const serialization_context& context) {
  const std::optional<meta_type> type = caors::find_type<T>();
  if (!type.has_value()) {
    return {error_domain::serialization, errc::missing_type};
  }

  return deserialize(type->id(), &object, reader, context);
}

/** @brief 从序列化 DOM 中间对象反序列化到对象。 */
template <typename T> [[nodiscard]] error deserialize(T& object, const serialization_value& value) {
  value_serialization_reader reader{value};
  return deserialize(object, reader);
}

/** @brief 按上下文从序列化 DOM 中间对象反序列化到对象。 */
template <typename T>
[[nodiscard]] error deserialize(T& object, const serialization_value& value,
                                const serialization_context& context) {
  value_serialization_reader reader{value};
  return deserialize(object, reader, context);
}

/** @brief 通过反射类型视图构造序列化中间值。 */
[[nodiscard]] CAORS_API serialization_value_result
to_serialization_value(const reflection_type& type, const void* object);

/** @brief 通过反射类型视图和上下文构造序列化中间值。 */
[[nodiscard]] CAORS_API serialization_value_result to_serialization_value(
    const reflection_type& type, const void* object, const serialization_context& context);

/** @brief 通过类型 id 查找反射信息并构造序列化中间值。 */
[[nodiscard]] CAORS_API serialization_value_result to_serialization_value(type_id id,
                                                                          const void* object);

/** @brief 通过类型 id 查找反射信息并按上下文构造序列化中间值。 */
[[nodiscard]] CAORS_API serialization_value_result
to_serialization_value(type_id id, const void* object, const serialization_context& context);

/** @brief 把已注册序列化和反射信息的 C++ 对象转换为序列化中间值。 */
template <typename T>
[[nodiscard]] serialization_value_result to_serialization_value(const T& object) {
  const std::optional<meta_type> type = caors::find_type<T>();
  if (!type.has_value()) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  return to_serialization_value(type->id(), &object);
}

/** @brief 把已注册序列化和反射信息的 C++ 对象按上下文转换为序列化中间值。 */
template <typename T>
[[nodiscard]] serialization_value_result
to_serialization_value(const T& object, const serialization_context& context) {
  const std::optional<meta_type> type = caors::find_type<T>();
  if (!type.has_value()) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  return to_serialization_value(type->id(), &object, context);
}

/** @brief 通过反射类型视图把对象序列化为简单文本格式。 */
[[nodiscard]] CAORS_API serialization_text_result serialize_text(const reflection_type& type,
                                                                 const void* object);

/** @brief 通过反射类型视图和上下文把对象序列化为简单文本格式。 */
[[nodiscard]] CAORS_API serialization_text_result serialize_text(
    const reflection_type& type, const void* object, const serialization_context& context);

/** @brief 通过类型 id 查找反射信息并把对象序列化为简单文本格式。 */
[[nodiscard]] CAORS_API serialization_text_result serialize_text(type_id id, const void* object);

/** @brief 通过类型 id 查找反射信息并按上下文序列化为简单文本格式。 */
[[nodiscard]] CAORS_API serialization_text_result
serialize_text(type_id id, const void* object, const serialization_context& context);

/** @brief 把已注册序列化和反射信息的 C++ 对象序列化为简单文本格式。 */
template <typename T> [[nodiscard]] serialization_text_result serialize_text(const T& object) {
  const std::optional<meta_type> type = caors::find_type<T>();
  if (!type.has_value()) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  return serialize_text(type->id(), &object);
}

/** @brief 把已注册序列化和反射信息的 C++ 对象按上下文序列化为简单文本格式。 */
template <typename T>
[[nodiscard]] serialization_text_result serialize_text(const T& object,
                                                       const serialization_context& context) {
  const std::optional<meta_type> type = caors::find_type<T>();
  if (!type.has_value()) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  return serialize_text(type->id(), &object, context);
}

} // namespace caors

#endif
