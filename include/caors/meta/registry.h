// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_REGISTRY_H_
#define CAORS_META_REGISTRY_H_

#include "caors/adapters/stl_wrapper.h"
#include "caors/error.h"
#include "caors/export.h"
#include "caors/meta/detail/component_storage.h"
#include "caors/meta/detail/parent_cast_ops.h"
#include "caors/meta/registration.h"
#include "caors/meta/type.h"
#include "caors/meta/type_registration.h"
#include "caors/meta/view.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <mutex>
#include <new>
#include <optional>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <utility>

namespace caors::meta::detail {

class registry_state;
class type_data;
struct type_descriptor;
template <typename T> [[nodiscard]] error ensure_type_descriptor_for();
[[nodiscard]] error ensure_type_descriptor_registered(const type_descriptor& descriptor);

} // namespace caors::meta::detail

namespace caors::meta {

template <typename T> class class_builder;
template <typename T> class enum_builder;

/**
 * @brief 底层全局类型元数据注册表。
 *
 * `meta::registry` 以 `type_id` 为 key，先注册内部基础类型数据，再按类型挂载父类、
 * 反射描述、序列化描述等元数据组件。普通代码应使用 `meta::class_<T>()`、`meta::resolve<T>()`、
 * `meta::find<T>()` 和 `meta::find()` 入口，不要创建或管理注册表实例。
 */
class CAORS_API registry final {
public:
  [[nodiscard]] static registry& instance() noexcept;

  registry(const registry&) = delete;
  registry& operator=(const registry&) = delete;

  [[nodiscard]] error add_registration(registration_fn fn);
  [[nodiscard]] error run_registrations();
  [[nodiscard]] error init();
  [[nodiscard]] error validate() const;

  [[nodiscard]] bool contains(type_id id) const;
  template <typename T> [[nodiscard]] type find_type() const;

  [[nodiscard]] type find_type(type_id id) const;
  [[nodiscard]] type find_type(type_guid guid) const;
  [[nodiscard]] type find_type(const std::type_info& info) const;
  [[nodiscard]] type find_type(std::string_view name) const;
  [[nodiscard]] type find_type(string_name name) const;
  [[nodiscard]] bool is_a(type_id id, type_id base_id) const;
  [[nodiscard]] const void* upcast(type_id id, type_id base_id, const void* value) const;
  [[nodiscard]] void* upcast(type_id id, type_id base_id, void* value) const;
  [[nodiscard]] std::size_t size() const noexcept;

  /** @brief 给已注册类型挂载一个元数据组件。 */
  template <typename Component, typename... Args>
  [[nodiscard]] error emplace_component(type_id id, Args&&... args);

  /**
   * @brief 确保指定类型拥有组件。
   *
   * 如果组件已经存在，直接返回现有组件指针；如果不存在，则用传入参数创建。返回指针不拥有组件，
   * 生命周期由全局注册表管理。
   */
  template <typename Component, typename... Args>
  [[nodiscard]] const Component* ensure_component(type_id id, Args&&... args);

  /** @brief 判断指定类型是否挂载了某个元数据组件。 */
  template <typename Component> [[nodiscard]] bool has_component(type_id id) const;

  /** @brief 获取指定类型上的元数据组件副本。 */
  template <typename Component>
  [[nodiscard]] std::optional<Component> find_component(type_id id) const;

  /**
   * @brief 获取指定类型上的元数据组件只读指针。
   *
   * 组件由全局注册表以 append-only 方式保存。返回指针不拥有对象，只要组件已注册且进程未退出，
   * 指针地址保持稳定。
   */
  template <typename Component> [[nodiscard]] const Component* find_component_ptr(type_id id) const;

  /** @brief 获取指定类型上的元数据组件可写指针，仅供注册构建阶段使用。 */
  template <typename Component> [[nodiscard]] Component* find_component_mutable_ptr(type_id id);

  /** @brief 统计挂载了指定组件的类型数量。 */
  template <typename Component> [[nodiscard]] std::size_t component_count() const noexcept;

  /** @brief 查询同时拥有指定组件集合的类型。 */
  template <typename... Components> [[nodiscard]] view<Components...> component_view() const;

private:
  registry();
  ~registry();

  template <typename T> friend class class_builder;
  template <typename T> friend class enum_builder;
  friend class type;
  friend error detail::ensure_type_descriptor_registered(const detail::type_descriptor& descriptor);

  [[nodiscard]] error register_type(const type_registration_data& type);
  template <typename T> [[nodiscard]] error register_type_for(const type_registration_data& data);
  [[nodiscard]] error ensure_type_descriptor(const detail::type_descriptor& descriptor);
  [[nodiscard]] error set_type_guid(type_id id, type_guid guid);
  [[nodiscard]] error set_type_parent(type_id id, type_id parent_id);
  [[nodiscard]] error set_type_parent(type_id id, type_id parent_id, detail::parent_cast_ops cast);
  template <typename Child, typename Parent> [[nodiscard]] error set_type_parent_for();

  [[nodiscard]] error register_cpp_type_info(const std::type_info& info, type_id id);
  [[nodiscard]] detail::type_data* find_mutable_type_data(type_id id);

  [[nodiscard]] detail::meta_component_storage_base*
  find_component_storage(detail::meta_component_key key);
  [[nodiscard]] const detail::meta_component_storage_base*
  find_component_storage(detail::meta_component_key key) const;
  detail::meta_component_storage_base&
  ensure_component_storage(detail::meta_component_key key,
                           memory::unique_ptr<detail::meta_component_storage_base> storage);
  [[nodiscard]] vector<type_id> view_component_ids(const detail::meta_component_key* keys,
                                                   std::size_t count) const;

  memory::memory_resource* state_resource_ = nullptr;
  detail::registry_state* state_ = nullptr;
};

/** @brief 获取进程内全局类型元数据注册表。 */
[[nodiscard]] CAORS_API registry& get_registry() noexcept;

} // namespace caors::meta

#include "caors/meta/detail/registry_component_templates.h"
#include "caors/meta/detail/registry_templates.h"
#include "caors/meta/detail/registry_view_templates.h"

#endif
