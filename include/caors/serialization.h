// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_H_
#define CAORS_SERIALIZATION_H_

#include "caors/error.h"
#include "caors/export.h"
#include "caors/meta.h"
#include "caors/reflection.h"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace caors {

/** @brief 序列化格式。当前 MVP 仅提供简单文本格式。 */
enum class serialization_format {
  text,
};

/** @brief 单个字段的序列化配置。 */
struct serialization_field_data {
  string_name field_name{};
  string_name serialized_name{};
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
  std::vector<serialization_field_data> fields{};

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
                         bool skipped = false) {
  return serialization_field_data{
      string_name{field_name},
      serialized_name.empty() ? string_name{field_name} : string_name{serialized_name},
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
                                 std::vector<serialization_field_data>{fields}};
}

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
  std::string text{};

  [[nodiscard]] bool succeeded() const noexcept { return status.succeeded(); }
  [[nodiscard]] bool failed() const noexcept { return status.failed(); }
  [[nodiscard]] explicit operator bool() const noexcept { return succeeded(); }
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

/** @brief 通过反射类型视图把对象序列化为简单文本格式。 */
[[nodiscard]] CAORS_API serialization_text_result serialize_text(const reflection_type& type,
                                                                 const void* object);

/** @brief 通过类型 id 查找反射信息并把对象序列化为简单文本格式。 */
[[nodiscard]] CAORS_API serialization_text_result serialize_text(type_id id, const void* object);

/** @brief 把已注册序列化和反射信息的 C++ 对象序列化为简单文本格式。 */
template <typename T> [[nodiscard]] serialization_text_result serialize_text(const T& object) {
  const std::optional<meta_type> type = caors::find_type<T>();
  if (!type.has_value()) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  return serialize_text(type->id(), &object);
}

} // namespace caors

#endif
