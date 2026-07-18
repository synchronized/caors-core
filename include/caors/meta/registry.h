// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_REGISTRY_H_
#define CAORS_META_REGISTRY_H_

#include "caors/adapters/stl_wrapper.h"
#include "caors/error.h"
#include "caors/export.h"
#include "caors/meta/detail/extension_data.h"
#include "caors/meta/extension_view.h"
#include "caors/meta/detail/parent_cast_ops.h"
#include "caors/meta/registration.h"
#include "caors/meta/type.h"

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
class owned_capability_base;
class type_data;
struct type_descriptor;
template <typename T> [[nodiscard]] error ensure_type_descriptor_for();
[[nodiscard]] error ensure_type_descriptor_registered(const type_descriptor& descriptor);
[[nodiscard]] error register_capability(registry& registry, type_id id, capability_key key,
                                        const void* capability);
[[nodiscard]] error register_capability(type_id id, capability_key key, const void* capability);
[[nodiscard]] error register_owned_capability(registry& registry, type_id id, capability_key key,
                                              memory::unique_ptr<owned_capability_base> capability);
[[nodiscard]] error register_owned_capability(type_id id, capability_key key,
                                              memory::unique_ptr<owned_capability_base> capability);
[[nodiscard]] const void* find_capability(const registry& registry, type_id id,
                                          capability_key key) noexcept;
[[nodiscard]] const void* find_capability(type_id id, capability_key key) noexcept;
[[nodiscard]] bool contains_capability(const registry& registry, type_id id,
                                       capability_key key) noexcept;
[[nodiscard]] bool contains_capability(type_id id, capability_key key) noexcept;
[[nodiscard]] std::size_t capability_count(const registry& registry, capability_key key) noexcept;
[[nodiscard]] std::size_t capability_count(capability_key key) noexcept;

} // namespace caors::meta::detail

namespace caors::meta {

template <typename T> class class_builder;
template <typename T> class enum_builder;

/**
 * @brief 类型元数据注册表。
 *
 * `meta::registry` 以 `type_id` 为 key，先注册内部基础类型数据，再按类型挂载父类、
 * 反射描述、序列化描述等扩展数据。普通代码使用默认上下文即可；需要测试隔离、插件隔离或自定义
 * 生命周期时，可以创建独立 registry 并通过 `meta::context` 显式传递。
 */
class CAORS_API registry final {
public:
  [[nodiscard]] static registry& instance() noexcept;

  registry();
  ~registry();

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

  /** @brief 给已注册类型挂载一份扩展数据。 */
  template <typename Extension, typename... Args>
  [[nodiscard]] error emplace_extension(type_id id, Args&&... args);

  /** @brief 判断指定类型是否挂载了某份扩展数据。 */
  template <typename Extension> [[nodiscard]] bool has_extension(type_id id) const;

  /** @brief 获取指定类型上的扩展数据副本。 */
  template <typename Extension>
  [[nodiscard]] std::optional<Extension> find_extension(type_id id) const;

  /**
   * @brief 获取指定类型上的扩展数据只读指针。
   *
   * 扩展数据由对应 `type_data` 以 append-only 方式保存。返回指针不拥有对象，只要扩展数据已注册且进程未退出，
   * 指针地址保持稳定。
   */
  template <typename Extension> [[nodiscard]] const Extension* find_extension_ptr(type_id id) const;

  /** @brief 获取指定类型上的扩展数据可写指针，仅供注册构建阶段使用。 */
  template <typename Extension> [[nodiscard]] Extension* find_extension_mutable_ptr(type_id id);

  /** @brief 统计挂载了指定扩展数据的类型数量。 */
  template <typename Extension> [[nodiscard]] std::size_t extension_count() const noexcept;

  /** @brief 查询同时拥有指定扩展数据集合的类型。 */
  template <typename... Extensions>
  [[nodiscard]] ::caors::meta::extension_view<Extensions...> extension_view() const;

private:
  template <typename T> friend class class_builder;
  template <typename T> friend class enum_builder;
  friend class type;
  friend error detail::ensure_type_descriptor_registered(const detail::type_descriptor& descriptor);
  friend error detail::register_capability(registry& registry, type_id id, capability_key key,
                                           const void* capability);
  friend error detail::register_capability(type_id id, capability_key key, const void* capability);
  friend error detail::register_owned_capability(
      registry& registry, type_id id, capability_key key,
      memory::unique_ptr<detail::owned_capability_base> capability);
  friend error
  detail::register_owned_capability(type_id id, capability_key key,
                                    memory::unique_ptr<detail::owned_capability_base> capability);
  friend const void* detail::find_capability(const registry& registry, type_id id,
                                             capability_key key) noexcept;
  friend const void* detail::find_capability(type_id id, capability_key key) noexcept;
  friend bool detail::contains_capability(const registry& registry, type_id id,
                                          capability_key key) noexcept;
  friend bool detail::contains_capability(type_id id, capability_key key) noexcept;
  friend std::size_t detail::capability_count(const registry& registry, capability_key key) noexcept;
  friend std::size_t detail::capability_count(capability_key key) noexcept;

  [[nodiscard]] error register_type(type_id id, string_name name, type_guid guid,
                                    type_id parent_id);
  template <typename T>
  [[nodiscard]] error register_type_for(string_name name, type_guid guid = type_guid::nil(),
                                        type_id parent_id = type_id::invalid());
  [[nodiscard]] error ensure_type_descriptor(const detail::type_descriptor& descriptor);
  [[nodiscard]] error set_type_guid(type_id id, type_guid guid);
  [[nodiscard]] error set_type_parent(type_id id, type_id parent_id);
  [[nodiscard]] error set_type_parent(type_id id, type_id parent_id, detail::parent_cast_ops cast);
  template <typename Child, typename Parent> [[nodiscard]] error set_type_parent_for();

  [[nodiscard]] error register_cpp_type_info(const std::type_info& info, type_id id);
  [[nodiscard]] const detail::type_data* find_type_data(type_id id) const;
  [[nodiscard]] detail::type_data* find_mutable_type_data(type_id id);
  [[nodiscard]] error add_extension(type_id id, detail::extension_data extension);
  [[nodiscard]] bool has_extension(type_id id, detail::extension_key key) const;
  [[nodiscard]] const detail::extension_data* find_extension_data(type_id id,
                                                                  detail::extension_key key) const;
  [[nodiscard]] detail::extension_data* find_extension_mutable_data(type_id id,
                                                                    detail::extension_key key);

  [[nodiscard]] error index_extension(type_id id, detail::extension_key key);
  [[nodiscard]] vector<type_id> view_extension_ids(const detail::extension_key* keys,
                                                   std::size_t count) const;

  memory::memory_resource* state_resource_ = nullptr;
  detail::registry_state* state_ = nullptr;
};

/** @brief 获取进程内全局类型元数据注册表。 */
[[nodiscard]] CAORS_API registry& get_registry() noexcept;

} // namespace caors::meta

#include "caors/meta/detail/registry_extension_templates.h"
#include "caors/meta/detail/registry_templates.h"
#include "caors/meta/detail/registry_view_templates.h"

#endif
