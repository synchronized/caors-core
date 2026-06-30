// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_REGISTRY_H_
#define CAORS_SERIALIZATION_REGISTRY_H_

#include "caors/serialization/types.h"

#include <cstddef>
#include <optional>

namespace caors {

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

} // namespace caors

#endif
