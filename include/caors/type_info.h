// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TYPE_INFO_H_
#define CAORS_TYPE_INFO_H_

#include "caors/export.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace caors {

/**
 * @brief 运行期类型标识。
 *
 * 类型标识由稳定类型名哈希得到，适合运行期查找和注册表索引。它不是跨版本序列化 ID。
 */
class type_id {
public:
  constexpr type_id() noexcept = default;
  explicit constexpr type_id(std::uint64_t value) noexcept : value_(value) {}

  [[nodiscard]] static constexpr type_id invalid() noexcept { return type_id{}; }
  [[nodiscard]] constexpr std::uint64_t value() const noexcept { return value_; }
  [[nodiscard]] explicit constexpr operator bool() const noexcept { return value_ != 0; }

  friend constexpr bool operator==(type_id lhs, type_id rhs) noexcept {
    return lhs.value_ == rhs.value_;
  }

  friend constexpr bool operator!=(type_id lhs, type_id rhs) noexcept { return !(lhs == rhs); }

private:
  std::uint64_t value_ = 0;
};

/** @brief 根据类型名生成运行期类型标识。 */
[[nodiscard]] constexpr type_id make_type_id(std::string_view name) noexcept {
  std::uint64_t hash = 14695981039346656037ull;
  for (const char character : name) {
    hash ^= static_cast<unsigned char>(character);
    hash *= 1099511628211ull;
  }

  return type_id{hash};
}

/**
 * @brief 对象系统和反射系统共享的基础类型描述。
 *
 * 该结构只保存类型身份、名称、布局和父类型关系，不包含对象生命周期或字段/方法反射信息。
 */
struct type_info {
  type_id id{};
  std::string_view name{};
  std::size_t size = 0;
  std::size_t alignment = 0;
  type_id parent_id{};

  [[nodiscard]] constexpr bool valid() const noexcept {
    return static_cast<bool>(id) && !name.empty();
  }
};

/** @brief 创建基础类型描述。 */
template <typename T>
[[nodiscard]] constexpr type_info make_type_info(std::string_view name,
                                                 type_id parent_id = type_id::invalid()) noexcept {
  return type_info{
      make_type_id(name), name, sizeof(T), alignof(T), parent_id,
  };
}

/** @brief 基础类型注册结果。 */
enum class type_register_result {
  success,
  invalid_type,
  duplicate_id,
  duplicate_name,
  hash_collision,
  missing_parent,
};

/** @brief 判断基础类型注册是否成功。 */
[[nodiscard]] constexpr bool type_register_succeeded(type_register_result result) noexcept {
  return result == type_register_result::success;
}

class type_registry_state;

/**
 * @brief 对象系统和反射系统共享的基础类型注册表。
 *
 * 注册表拥有类型名存储。`find_type()` 返回的 `type_info::name` 在注册表生命周期内有效。
 */
class CAORS_API type_registry final {
public:
  type_registry();
  ~type_registry();

  type_registry(const type_registry&) = delete;
  type_registry& operator=(const type_registry&) = delete;

  [[nodiscard]] type_register_result register_type(const type_info& type);
  [[nodiscard]] bool contains(type_id id) const;
  [[nodiscard]] std::optional<type_info> find_type(type_id id) const;
  [[nodiscard]] std::optional<type_info> find_type(std::string_view name) const;
  [[nodiscard]] bool is_a(type_id id, type_id base_id) const;
  [[nodiscard]] std::size_t size() const noexcept;
  void clear() noexcept;

private:
  type_registry_state* state_ = nullptr;
};

/** @brief 获取进程内默认基础类型注册表。 */
[[nodiscard]] CAORS_API type_registry& get_type_registry() noexcept;

/** @brief 向指定注册表注册基础类型。 */
template <typename T>
[[nodiscard]] type_register_result register_type(type_registry& registry, std::string_view name,
                                                 type_id parent_id = type_id::invalid()) {
  return registry.register_type(make_type_info<T>(name, parent_id));
}

/** @brief 向默认基础类型注册表注册基础类型。 */
template <typename T>
[[nodiscard]] type_register_result register_type(std::string_view name,
                                                 type_id parent_id = type_id::invalid()) {
  return register_type<T>(get_type_registry(), name, parent_id);
}

}

#endif
