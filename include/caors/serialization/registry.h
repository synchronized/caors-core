// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_REGISTRY_H_
#define CAORS_SERIALIZATION_REGISTRY_H_

#include "caors/serialization/types.h"

#include <cstddef>
#include <optional>
#include <utility>

namespace caors::serialization {

/**
 * @brief 底层全局序列化注册表。
 *
 * 注册表复用全局 `meta::registry`，把 `serialization::type_schema` 作为 extension 挂载到同一个
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
  registry() = default;
  ~registry() = default;
};

/** @brief 获取进程内全局序列化注册表。 */
[[nodiscard]] CAORS_API registry& get_registry() noexcept;

/** @brief 查找指定类型的序列化 extension。 */
[[nodiscard]] CAORS_API std::optional<type_schema> find(type_id id);

/** @brief 查找指定元类型的序列化 extension。 */
[[nodiscard]] CAORS_API std::optional<type_schema> find(meta::type type);

/** @brief 查找指定元类型的序列化 schema 视图，不复制 schema 数据。 */
[[nodiscard]] CAORS_API schema_view find_schema(meta::type type) noexcept;

/** @brief 判断指定类型是否挂载了序列化 extension。 */
[[nodiscard]] CAORS_API bool contains(type_id id);

/** @brief 判断指定元类型是否挂载了序列化 extension。 */
[[nodiscard]] CAORS_API bool contains(meta::type type);

/** @brief 获取当前序列化 extension 数量。 */
[[nodiscard]] CAORS_API std::size_t size() noexcept;

} // namespace caors::serialization

#endif
