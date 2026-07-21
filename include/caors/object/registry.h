// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_REGISTRY_H_
#define CAORS_OBJECT_REGISTRY_H_

#include "caors/error.h"
#include "caors/meta/context.h"
#include "caors/meta/query.h"
#include "caors/meta/registry.h"
#include "caors/meta/type.h"
#include "caors/object/ptr.h"
#include "caors/object/type.h"

#include <string_view>
#include <type_traits>

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

/** @brief 确保指定 meta registry 中已注册基础 `caors::object` 类型。 */
[[nodiscard]] CAORS_API error ensure_object_base_registered(meta::registry& registry);

} // namespace caors::objects::detail

namespace caors::objects {

/** @brief 根据已注册对象类型名查询对象类型标识。 */
[[nodiscard]] inline object_type_id id(std::string_view name) { return meta::id(name); }

/** @brief 根据指定类型上下文中的已注册对象类型名查询对象类型标识。 */
[[nodiscard]] inline object_type_id id(meta::context& context, std::string_view name) {
  return meta::id(context, name);
}

/** @brief 在指定类型上下文中按对象类型 id 查找对象类型。 */
[[nodiscard]] CAORS_API type find(meta::context& context, object_type_id id);

/** @brief 在默认类型上下文中按对象类型 id 查找对象类型。 */
[[nodiscard]] inline type find(object_type_id id) {
  return ::caors::objects::find(meta::default_context(), id);
}

/** @brief 在指定类型上下文中按对象类型名查找对象类型。 */
[[nodiscard]] CAORS_API type find(meta::context& context, std::string_view name);

/** @brief 在默认类型上下文中按对象类型名查找对象类型。 */
[[nodiscard]] inline type find(std::string_view name) {
  return ::caors::objects::find(meta::default_context(), name);
}

/** @brief 在指定类型上下文中按 C++ 类型查找对象类型。 */
template <typename T> [[nodiscard]] type find(meta::context& context) {
  static_assert(std::is_base_of_v<object, std::remove_cv_t<std::remove_reference_t<T>>>,
                "T must derive from caors::object");
  const type object_type{meta::find<T>(context)};
  return object_type ? object_type : type{};
}

/** @brief 在默认类型上下文中按 C++ 类型查找对象类型。 */
template <typename T> [[nodiscard]] type find() {
  return ::caors::objects::find<T>(meta::default_context());
}

/** @brief 判断指定类型上下文中是否已注册对象类型。 */
[[nodiscard]] CAORS_API bool contains(meta::context& context, object_type_id id);

/** @brief 判断默认类型上下文中是否已注册对象类型。 */
[[nodiscard]] inline bool contains(object_type_id id) {
  return contains(meta::default_context(), id);
}

/** @brief 判断指定类型上下文中的对象类型继承关系。 */
[[nodiscard]] CAORS_API bool is_a(meta::context& context, object_type_id id,
                                  object_type_id base_id);

/** @brief 判断默认类型上下文中的对象类型继承关系。 */
[[nodiscard]] inline bool is_a(object_type_id id, object_type_id base_id) {
  return ::caors::objects::is_a(meta::default_context(), id, base_id);
}

/** @brief 获取指定类型上下文中已注册对象类型数量。 */
[[nodiscard]] CAORS_API std::size_t size(meta::context& context) noexcept;

/** @brief 获取默认类型上下文中已注册对象类型数量。 */
[[nodiscard]] inline std::size_t size() noexcept { return size(meta::default_context()); }

/** @brief 按对象类型 id 尝试默认构造对象；类型未注册、不可构造或分配失败时返回空指针。 */
[[nodiscard]] CAORS_API object* try_create(meta::context& context, object_type_id id,
                                           memory_resource* resource = nullptr) noexcept;

/** @brief 在默认类型上下文中按对象类型 id 尝试默认构造对象。 */
[[nodiscard]] inline object* try_create(object_type_id id,
                                        memory_resource* resource = nullptr) noexcept {
  return try_create(meta::default_context(), id, resource);
}

/** @brief 按对象类型名尝试默认构造对象；类型未注册、不可构造或分配失败时返回空指针。 */
[[nodiscard]] CAORS_API object* try_create(meta::context& context, std::string_view name,
                                           memory_resource* resource = nullptr) noexcept;

/** @brief 在默认类型上下文中按对象类型名尝试默认构造对象。 */
[[nodiscard]] inline object* try_create(std::string_view name,
                                        memory_resource* resource = nullptr) noexcept {
  return try_create(meta::default_context(), name, resource);
}

/** @brief 按对象类型 id 默认构造对象；类型未注册或不可构造时返回空指针。 */
[[nodiscard]] CAORS_API object* create(meta::context& context, object_type_id id,
                                       memory_resource* resource = nullptr);

/** @brief 在默认类型上下文中按对象类型 id 默认构造对象。 */
[[nodiscard]] inline object* create(object_type_id id, memory_resource* resource = nullptr) {
  return create(meta::default_context(), id, resource);
}

/** @brief 按对象类型名默认构造对象；类型未注册或不可构造时返回空指针。 */
[[nodiscard]] CAORS_API object* create(meta::context& context, std::string_view name,
                                       memory_resource* resource = nullptr);

/** @brief 在默认类型上下文中按对象类型名默认构造对象。 */
[[nodiscard]] inline object* create(std::string_view name, memory_resource* resource = nullptr) {
  return create(meta::default_context(), name, resource);
}

/** @brief 销毁通过对象类型工厂创建的对象。 */
CAORS_API bool destroy(meta::context& context, object_type_id id, object* instance,
                       memory_resource* resource = nullptr) noexcept;

/** @brief 在默认类型上下文中销毁通过对象类型工厂创建的对象。 */
inline bool destroy(object_type_id id, object* instance,
                    memory_resource* resource = nullptr) noexcept {
  return destroy(meta::default_context(), id, instance, resource);
}

} // namespace caors::objects

#endif
