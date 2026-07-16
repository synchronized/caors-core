// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_ASSETS_SERIALIZATION_H_
#define CAORS_ASSETS_SERIALIZATION_H_

#include "caors/assets/asset_manager.h"
#include "caors/resources/io.h"
#include "caors/resources/serialization.h"
#include "caors/serialization/operations.h"

#include <type_traits>
#include <utility>

namespace caors::assets {

/** @brief 使用 asset_manager 内部 database 创建资源引用 resolver。 */
[[nodiscard]] inline resources::resource_ref_resolver_ops
make_resource_resolver(asset_manager& manager) noexcept {
  return resources::make_resource_resolver(manager.database());
}

/**
 * @brief 从序列化 reader 构造资源。
 *
 * 该 helper 面向简单资源类型：先用指定内存资源默认构造 `T`，写入 GUID，再通过现有
 * `serialization::deserialize()` 读回资源字段。非默认可构造或有自定义加载流程的资源应实现
 * `resources::loader<T>`。
 */
template <typename T>
[[nodiscard]] resources::load_result<T>
load_serialized(const serialization::context& context, resources::resource_guid guid,
                serialization::reader& reader, memory_resource* resource = nullptr);

template <typename T>
[[nodiscard]] resources::load_result<T> load_serialized(resources::resource_guid guid,
                                                        serialization::reader& reader,
                                                        memory_resource* resource = nullptr) {
  return load_serialized<T>(serialization::context{}, guid, reader, resource);
}

template <typename T>
[[nodiscard]] resources::load_result<T>
load_serialized(const serialization::context& context, resources::resource_guid guid,
                serialization::reader& reader, memory_resource* resource) {
  static_assert(std::is_base_of_v<resources::resource, T>,
                "T must derive from caors::resources::resource");
  static_assert(std::is_default_constructible_v<T>,
                "load_serialized<T>() requires default constructible resources");

  if (!guid) {
    return {{error_domain::resources, errc::invalid_argument}, {}};
  }

  memory::unique_ptr<T> value = memory::make_unique<T>(resource);
  if (value == nullptr) {
    return {{error_domain::resources, errc::out_of_memory}, {}};
  }

  if (value->has_guid() && value->guid() != guid) {
    return {{error_domain::resources, errc::inconsistent_type}, {}};
  }
  if (!value->has_guid()) {
    value->set_guid(guid);
  }

  const error status = serialization::deserialize(context, *value, reader);
  if (status) {
    return {status, {}};
  }

  return {{}, std::move(value)};
}

/**
 * @brief 使用序列化 writer 保存资源字段。
 *
 * 资源必须携带非 nil GUID。当前 helper 只写资源对象本体字段，资源身份由调用方的数据库、路径或包索引
 * 维护。
 */
template <typename T>
[[nodiscard]] error save_serialized(const serialization::context& context, const T& value,
                                    serialization::writer& writer);

template <typename T>
[[nodiscard]] error save_serialized(const T& value, serialization::writer& writer) {
  return save_serialized(serialization::context{}, value, writer);
}

template <typename T>
[[nodiscard]] error save_serialized(const serialization::context& context, const T& value,
                                    serialization::writer& writer) {
  static_assert(std::is_base_of_v<resources::resource, T>,
                "T must derive from caors::resources::resource");

  if (!value.has_guid()) {
    return {error_domain::resources, errc::invalid_argument};
  }

  return serialization::serialize(context, value, writer);
}

} // namespace caors::assets

#endif
