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
  [[nodiscard]] error register_type(meta_type type,
                                    serialization_type_data data = serialization_type_data{});
  [[nodiscard]] bool contains(type_id id) const;
  [[nodiscard]] std::optional<serialization_type_data> find_type(type_id id) const;
  [[nodiscard]] std::size_t size() const noexcept;

  template <typename T>
  [[nodiscard]] error register_type_for(serialization_type_data data = serialization_type_data{}) {
    const std::optional<meta_type> type = get_meta_registry().find_type<T>();
    if (!type.has_value()) {
      return {error_domain::serialization, errc::invalid_type};
    }

    return register_type(type->id(), data);
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

/** @brief 向已注册元类型挂载序列化组件。不会创建或修改基础类型记录。 */
[[nodiscard]] CAORS_API error
register_serialization(meta_type type, serialization_type_data data = serialization_type_data{});

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

namespace serialization {

/** @brief 已注册元类型的序列化配置构建器。 */
class class_builder {
public:
  explicit class_builder(meta_type type) : type_(type) {
    if (!type.valid()) {
      status_ = {error_domain::serialization, errc::invalid_type};
    }
  }

  class_builder& field(std::string_view field_name, std::string_view serialized_name = {},
                       bool skipped = false, std::uint32_t field_id = 0) {
    if (status_) {
      return *this;
    }

    data_.fields.push_back(
        make_serialization_field(field_name, serialized_name, skipped, field_id));
    return *this;
  }

  [[nodiscard]] error commit() {
    if (status_) {
      return status_;
    }

    return caors::register_serialization(type_, data_);
  }

  [[nodiscard]] const serialization_type_data& serialization_data() const noexcept { return data_; }

private:
  meta_type type_{};
  serialization_type_data data_{};
  error status_{};
};

/** @brief 为已注册元类型创建序列化配置构建器。 */
[[nodiscard]] inline class_builder class_(meta_type type) { return class_builder{type}; }

} // namespace serialization

} // namespace caors

#endif
