// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_TYPE_H_
#define CAORS_META_TYPE_H_

#include "caors/export.h"
#include "caors/meta/type_id.h"

#include <cstddef>
#include <string_view>

namespace caors {

class meta_registry;

/**
 * @brief 元类型底层数据。
 *
 * 该类型用于注册表内部存储和低层注册描述。对外查询应使用 `meta_type` 视图，避免直接依赖
 * 注册表内部数据布局。
 */
class meta_type_data {
public:
  constexpr meta_type_data() noexcept = default;

  constexpr meta_type_data(type_id id, string_name name, type_guid guid, std::size_t size,
                           std::size_t alignment, type_id parent_id) noexcept
      : id_(id), name_(name), guid_(guid), size_(size), alignment_(alignment),
        parent_id_(parent_id) {}

  [[nodiscard]] constexpr type_id id() const noexcept { return id_; }
  [[nodiscard]] constexpr string_name name() const noexcept { return name_; }
  [[nodiscard]] constexpr type_guid guid() const noexcept { return guid_; }
  [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }
  [[nodiscard]] constexpr std::size_t alignment() const noexcept { return alignment_; }
  [[nodiscard]] constexpr type_id parent_id() const noexcept { return parent_id_; }

  [[nodiscard]] constexpr bool valid() const noexcept { return size_ > 0 && alignment_ > 0; }

  [[nodiscard]] constexpr bool registered() const noexcept {
    return static_cast<bool>(id_) && valid();
  }

  constexpr void set_id(type_id id) noexcept { id_ = id; }
  constexpr void set_name(string_name name) noexcept { name_ = name; }
  constexpr void set_guid(type_guid guid) noexcept { guid_ = guid; }
  constexpr void set_parent_id(type_id parent_id) noexcept { parent_id_ = parent_id; }

private:
  type_id id_{};
  string_name name_{};
  type_guid guid_{};
  std::size_t size_ = 0;
  std::size_t alignment_ = 0;
  type_id parent_id_{};
};

/**
 * @brief 元类型只读视图。
 *
 * `meta_type` 不拥有类型数据，只引用全局 `meta_registry` 中 append-only 保存的
 * `meta_type_data`。视图复制成本低，适合作为对外查询结果。
 */
class CAORS_API meta_type {
public:
  constexpr meta_type() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] type_id id() const noexcept;
  [[nodiscard]] string_name name() const noexcept;
  [[nodiscard]] type_guid guid() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] std::size_t alignment() const noexcept;
  [[nodiscard]] type_id parent_id() const noexcept;
  [[nodiscard]] meta_type parent() const noexcept;

private:
  friend class meta_registry;

  constexpr meta_type(const meta_registry* registry, const meta_type_data* data) noexcept
      : registry_(registry), data_(data) {}

  const meta_registry* registry_ = nullptr;
  const meta_type_data* data_ = nullptr;
};

/** @brief 创建基础元类型注册描述。 */
template <typename T>
[[nodiscard]] constexpr meta_type_data
make_meta_type_data(string_name name, type_id parent_id = type_id::invalid()) noexcept {
  return meta_type_data{
      type_id::invalid(), name, type_guid::nil(), sizeof(T), alignof(T), parent_id,
  };
}

/** @brief 创建带稳定 GUID 的基础元类型注册描述。 */
template <typename T>
[[nodiscard]] constexpr meta_type_data
make_meta_type_data(string_name name, type_guid guid,
                    type_id parent_id = type_id::invalid()) noexcept {
  return meta_type_data{
      type_id::invalid(), name, guid, sizeof(T), alignof(T), parent_id,
  };
}

/** @brief 创建基础元类型注册描述。 */
template <typename T>
[[nodiscard]] meta_type_data make_meta_type_data(std::string_view name,
                                                 type_id parent_id = type_id::invalid()) {
  return make_meta_type_data<T>(string_name{name}, parent_id);
}

/** @brief 创建带稳定 GUID 的基础元类型注册描述。 */
template <typename T>
[[nodiscard]] meta_type_data make_meta_type_data(std::string_view name, type_guid guid,
                                                 type_id parent_id = type_id::invalid()) {
  return make_meta_type_data<T>(string_name{name}, guid, parent_id);
}

} // namespace caors

#endif
