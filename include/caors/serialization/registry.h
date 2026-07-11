// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_REGISTRY_H_
#define CAORS_SERIALIZATION_REGISTRY_H_

#include "caors/serialization/types.h"

#include <cstddef>
#include <optional>
#include <utility>

namespace caors::serialization {

class class_builder;

/**
 * @brief 底层全局序列化注册表。
 *
 * 注册表复用全局 `meta::registry`，把 `serialization::type_schema` 作为组件挂载到同一个
 * `type_id` 上。当前序列化实现依赖反射字段信息读取对象数据。
 */
class CAORS_API registry final {
public:
  [[nodiscard]] static registry& instance() noexcept;

  registry(const registry&) = delete;
  registry& operator=(const registry&) = delete;

  [[nodiscard]] bool contains(type_id id) const;
  [[nodiscard]] std::optional<type_schema> find_type(type_id id) const;
  [[nodiscard]] std::size_t size() const noexcept;

private:
  friend class class_builder;

  registry() = default;
  ~registry() = default;

  [[nodiscard]] error attach_schema(type_id id, type_schema data = type_schema{});
  [[nodiscard]] error attach_schema(meta::type type, type_schema data = type_schema{});
};

/** @brief 获取进程内全局序列化注册表。 */
[[nodiscard]] CAORS_API registry& get_registry() noexcept;

/** @brief 查找指定类型的序列化组件。 */
[[nodiscard]] CAORS_API std::optional<type_schema> find(type_id id);

/** @brief 查找指定元类型的序列化组件。 */
[[nodiscard]] CAORS_API std::optional<type_schema> find(meta::type type);

/** @brief 判断指定类型是否挂载了序列化组件。 */
[[nodiscard]] CAORS_API bool contains(type_id id);

/** @brief 判断指定元类型是否挂载了序列化组件。 */
[[nodiscard]] CAORS_API bool contains(meta::type type);

/** @brief 获取当前序列化组件数量。 */
[[nodiscard]] CAORS_API std::size_t size() noexcept;

/** @brief 已注册元类型的序列化配置构建器。 */
class class_builder {
public:
  explicit class_builder(meta::type type) : type_(type) {
    if (!type.registered()) {
      status_ = {error_domain::serialization, errc::invalid_type};
    }
  }

  class_builder(meta::type type, type_schema data) : class_builder(type) {
    data_ = std::move(data);
  }

  class_builder& field(std::string_view field_name, std::string_view serialized_name = {},
                       bool skipped = false, std::uint32_t field_id = 0) {
    if (status_) {
      return *this;
    }

    data_.fields.push_back(make_field(field_name, serialized_name, skipped, field_id));
    return *this;
  }

  [[nodiscard]] error commit() {
    if (status_) {
      return status_;
    }

    status_ = get_registry().attach_schema(type_, data_);
    return status_;
  }

  [[nodiscard]] const type_schema& schema() const noexcept { return data_; }

private:
  meta::type type_{};
  type_schema data_{};
  error status_{};
};

/** @brief 为已注册元类型创建序列化配置构建器。 */
[[nodiscard]] inline class_builder class_(meta::type type) { return class_builder{type}; }

/** @brief 为已注册 C++ 类型创建序列化配置构建器。 */
template <typename T> [[nodiscard]] class_builder class_() {
  return class_builder{meta::find<T>()};
}

/** @brief 为已注册 C++ 类型和完整 schema 创建序列化配置构建器。 */
template <typename T> [[nodiscard]] class_builder class_(type_schema data) {
  return class_builder{meta::find<T>(), std::move(data)};
}

/** @brief 为已注册 C++ 类型和字段配置创建序列化配置构建器。 */
template <typename T>
[[nodiscard]] class_builder class_(std::initializer_list<field_schema> fields) {
  return class_<T>(make_type_schema(fields));
}

} // namespace caors::serialization

#endif
