// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_REGISTRY_H_
#define CAORS_OBJECT_REGISTRY_H_

#include "caors/meta/builder.h"
#include "caors/meta/query.h"
#include "caors/meta/registry.h"
#include "caors/meta/static_capability.h"
#include "caors/meta/type.h"
#include "caors/object/factory.h"
#include "caors/object/ptr.h"
#include "caors/object/type.h"

#include <optional>
#include <string_view>
#include <type_traits>

namespace caors::objects {

class registry;

/** @brief 获取进程内默认对象类型注册表。 */
[[nodiscard]] CAORS_API registry& get_registry() noexcept;

} // namespace caors::objects

namespace caors::objects::detail {

template <typename T, typename = void> struct object_type_name_traits {
  static constexpr bool exists = false;
  static constexpr bool valid = false;

  [[nodiscard]] static constexpr std::string_view get() noexcept { return {}; }
};

template <typename T>
struct object_type_name_traits<T, std::void_t<decltype(T::object_type_name)>> {
  static constexpr bool exists = true;
  static constexpr bool valid =
      std::is_convertible_v<decltype(T::object_type_name), std::string_view>;

  [[nodiscard]] static constexpr std::string_view get() noexcept { return T::object_type_name; }
};

} // namespace caors::objects::detail

namespace caors::objects {

/**
 * @brief 全局对象类型注册表 facade 和默认构造工厂。
 *
 * `objects::registry` 不表示彼此隔离的注册表实例。它复用全局 `meta::registry`
 * 管理基础类型信息，并把 `objects::object_factory_ops` 作为 capability 挂载到同一个 `type_id`
 * 上。普通代码应通过 `registration::type_<T>()` 注册对象类型，再通过
 * `objects::get_registry()` 和查询接口访问它。
 */
class CAORS_API registry final {
public:
  registry(const registry&) = delete;
  registry& operator=(const registry&) = delete;

  [[nodiscard]] bool contains(object_type_id id) const;
  [[nodiscard]] type find_type(object_type_id id) const;
  [[nodiscard]] type find_type(std::string_view name) const;
  template <typename T> [[nodiscard]] type find_type() const {
    const meta::type type = meta::get_registry().find_type<T>();
    if (!type) {
      return {};
    }

    return find_type(type.id());
  }

  [[nodiscard]] bool is_a(object_type_id type_id, object_type_id base_type_id) const;
  [[nodiscard]] std::size_t size() const noexcept;

  [[nodiscard]] object* try_create(object_type_id id,
                                   memory_resource* resource = nullptr) const noexcept;
  [[nodiscard]] object* try_create(std::string_view name,
                                   memory_resource* resource = nullptr) const noexcept;
  template <typename T>
  [[nodiscard]] T* try_create(memory_resource* resource = nullptr) const noexcept {
    const type object_type = find_type<T>();
    if (!object_type) {
      return nullptr;
    }

    return static_cast<T*>(try_create(object_type.id(), resource));
  }

  [[nodiscard]] object* create(object_type_id id, memory_resource* resource = nullptr) const;
  [[nodiscard]] object* create(std::string_view name, memory_resource* resource = nullptr) const;
  template <typename T> [[nodiscard]] T* create(memory_resource* resource = nullptr) const {
    const type object_type = find_type<T>();
    if (!object_type) {
      return nullptr;
    }

    return static_cast<T*>(create(object_type.id(), resource));
  }

  bool destroy(object_type_id id, object* instance,
               memory_resource* resource = nullptr) const noexcept;
  bool destroy(object* instance, memory_resource* resource = nullptr) const noexcept;

private:
  friend registry& get_registry() noexcept;

  registry();
  ~registry();

  [[nodiscard]] const object_factory_ops* find_factory_ops(object_type_id id) const;
};

} // namespace caors::objects

namespace caors::objects {

/** @brief 根据已注册对象类型名查询对象类型标识。 */
[[nodiscard]] inline object_type_id id(std::string_view name) { return meta::id(name); }

/** @brief 在全局对象注册表中按对象类型 id 查找类型。 */
[[nodiscard]] inline type find(object_type_id id) { return get_registry().find_type(id); }

/** @brief 在全局对象注册表中按对象类型名查找类型。 */
[[nodiscard]] inline type find(std::string_view name) { return get_registry().find_type(name); }

/** @brief 在全局对象注册表中按 C++ 类型查找对象类型。 */
template <typename T> [[nodiscard]] type find() { return get_registry().find_type<T>(); }

/** @brief 判断对象类型是否已注册。 */
[[nodiscard]] inline bool contains(object_type_id id) { return get_registry().contains(id); }

/** @brief 判断对象类型继承关系。 */
[[nodiscard]] inline bool is_a(object_type_id id, object_type_id base_id) {
  return get_registry().is_a(id, base_id);
}

/** @brief 获取当前对象类型数量。 */
[[nodiscard]] inline std::size_t size() noexcept { return get_registry().size(); }

} // namespace caors::objects

#endif
