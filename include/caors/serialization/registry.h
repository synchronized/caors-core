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
 * 注册表复用全局 `meta::registry`，把 `serialization::type_data` 作为组件挂载到同一个
 * `type_id` 上。当前序列化实现依赖反射字段信息读取对象数据。
 */
class CAORS_API registry final {
public:
  [[nodiscard]] static registry& instance() noexcept;

  registry(const registry&) = delete;
  registry& operator=(const registry&) = delete;

  [[nodiscard]] error register_type(type_id id, type_data data = type_data{});
  [[nodiscard]] error register_type(meta::type type, type_data data = type_data{});
  [[nodiscard]] bool contains(type_id id) const;
  [[nodiscard]] std::optional<type_data> find_type(type_id id) const;
  [[nodiscard]] std::size_t size() const noexcept;

  template <typename T> [[nodiscard]] error register_type_for(type_data data = type_data{}) {
    const meta::type type = meta::get_registry().find_type<T>();
    if (!type) {
      return {error_domain::serialization, errc::invalid_type};
    }

    return register_type(type.id(), data);
  }

private:
  registry() = default;
  ~registry() = default;
};

/** @brief 获取进程内全局序列化注册表。 */
[[nodiscard]] CAORS_API registry& get_registry() noexcept;

/** @brief 查找指定类型的序列化组件。 */
[[nodiscard]] CAORS_API std::optional<type_data> find(type_id id);

/** @brief 查找指定元类型的序列化组件。 */
[[nodiscard]] CAORS_API std::optional<type_data> find(meta::type type);

/** @brief 判断指定类型是否挂载了序列化组件。 */
[[nodiscard]] CAORS_API bool contains(type_id id);

/** @brief 判断指定元类型是否挂载了序列化组件。 */
[[nodiscard]] CAORS_API bool contains(meta::type type);

/** @brief 获取当前序列化组件数量。 */
[[nodiscard]] CAORS_API std::size_t size() noexcept;

/** @brief 向全局序列化注册表注册指定类型 id。 */
[[nodiscard]] inline error register_type(type_id id, type_data data = type_data{}) {
  return get_registry().register_type(id, std::move(data));
}

/** @brief 向已注册元类型挂载序列化组件，不会创建或修改基础类型记录。 */
[[nodiscard]] inline error register_type(meta::type type, type_data data = type_data{}) {
  return get_registry().register_type(type, std::move(data));
}

/** @brief 向全局序列化注册表注册指定 C++ 类型。 */
template <typename T> [[nodiscard]] error register_type(type_data data = type_data{}) {
  return get_registry().register_type_for<T>(std::move(data));
}

/** @brief 向全局序列化注册表注册指定 C++ 类型和字段配置。 */
template <typename T> [[nodiscard]] error register_type(std::initializer_list<field_data> fields) {
  return register_type<T>(make_type_data(fields));
}

/** @brief 已注册元类型的序列化配置构建器。 */
class class_builder {
public:
  explicit class_builder(meta::type type) : type_(type) {
    if (!type.valid()) {
      status_ = {error_domain::serialization, errc::invalid_type};
    }
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

    return register_type(type_, data_);
  }

  [[nodiscard]] const type_data& serialization_data() const noexcept { return data_; }

private:
  meta::type type_{};
  type_data data_{};
  error status_{};
};

/** @brief 为已注册元类型创建序列化配置构建器。 */
[[nodiscard]] inline class_builder class_(meta::type type) { return class_builder{type}; }

} // namespace caors::serialization

#endif
