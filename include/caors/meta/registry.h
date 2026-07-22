// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_REGISTRY_H_
#define CAORS_META_REGISTRY_H_

#include "caors/adapters/stl_wrapper.h"
#include "caors/error.h"
#include "caors/export.h"
#include "caors/meta/detail/extension_data.h"
#include "caors/meta/detail/parent_cast_ops.h"
#include "caors/meta/extension_view.h"
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

namespace caors::meta {

class registry;

} // namespace caors::meta

namespace caors::meta::detail {

class registry_state;
class registry_access;
class type_data;
struct type_descriptor;
template <typename T> [[nodiscard]] error ensure_type_descriptor_for();

} // namespace caors::meta::detail

namespace caors::registration {

template <typename T> class enum_builder;
template <typename T> class type_builder;

} // namespace caors::registration

namespace caors::meta {

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

  /** @brief 判断指定类型是否挂载了某份扩展数据。 */
  template <typename Extension> [[nodiscard]] bool has_extension(type_id id) const;

  /** @brief 获取指定类型上的扩展数据副本。 */
  template <typename Extension>
  [[nodiscard]] std::optional<Extension> find_extension(type_id id) const;

  /**
   * @brief 获取指定类型上的扩展数据只读指针。
   *
   * 扩展数据由对应 `type_data` 以 append-only
   * 方式保存。返回指针不拥有对象，只要扩展数据已注册且进程未退出， 指针地址保持稳定。
   */
  template <typename Extension> [[nodiscard]] const Extension* find_extension_ptr(type_id id) const;

  /** @brief 统计挂载了指定扩展数据的类型数量。 */
  template <typename Extension> [[nodiscard]] std::size_t extension_count() const noexcept;

  /** @brief 查询同时拥有指定扩展数据集合的类型。 */
  template <typename... Extensions>
  [[nodiscard]] ::caors::meta::extension_view<Extensions...> extension_view() const;

private:
  template <typename T> friend class ::caors::registration::enum_builder;
  template <typename T> friend class ::caors::registration::type_builder;
  friend class detail::registry_access;
  friend class type;

  [[nodiscard]] error register_type(type_id id, string_name name, type_guid guid,
                                    type_id parent_id);
  [[nodiscard]] error commit_type(detail::type_data type, const std::type_info& cpp_type);
  template <typename T>
  [[nodiscard]] error register_type_for(string_name name, type_guid guid = type_guid::nil(),
                                        type_id parent_id = type_id::invalid());
  [[nodiscard]] error ensure_type_descriptor(const detail::type_descriptor& descriptor);
  [[nodiscard]] error set_type_guid(type_id id, type_guid guid);
  [[nodiscard]] error set_type_parent(type_id id, type_id parent_id);
  [[nodiscard]] error set_type_parent(type_id id, type_id parent_id, detail::parent_cast_ops cast);
  template <typename Child, typename Parent> [[nodiscard]] error set_type_parent_for();

  [[nodiscard]] error register_cpp_type_info(const std::type_info& info, type_id id);
  [[nodiscard]] error add_extension(type_id id, detail::extension_data extension);
  [[nodiscard]] bool has_extension(type_id id, detail::extension_key key) const;
  [[nodiscard]] const detail::extension_data* find_extension_data(type_id id,
                                                                  detail::extension_key key) const;

  [[nodiscard]] vector<type_id> view_extension_ids(const detail::extension_key* keys,
                                                   std::size_t count) const;

  memory::memory_resource* state_resource_ = nullptr;
  detail::registry_state* state_ = nullptr;
};

namespace detail {

/** @brief registry 内部写入入口。公开代码应使用 registration/meta type API。 */
class registry_access final {
public:
  [[nodiscard]] static registry_state* state(registry& registry) noexcept {
    return registry.state_;
  }

  [[nodiscard]] static const registry_state* state(const registry& registry) noexcept {
    return registry.state_;
  }

  [[nodiscard]] static memory::memory_resource* state_resource(registry& registry) noexcept {
    return registry.state_resource_;
  }

  [[nodiscard]] static error ensure_type_descriptor(registry& registry,
                                                    const type_descriptor& descriptor) {
    return registry.ensure_type_descriptor(descriptor);
  }

  [[nodiscard]] static error add_extension(registry& registry, type_id id,
                                           extension_data extension) {
    return registry.add_extension(id, std::move(extension));
  }
};

} // namespace detail

/** @brief 获取进程内全局类型元数据注册表。 */
[[nodiscard]] CAORS_API registry& get_registry() noexcept;

} // namespace caors::meta

#include "caors/meta/detail/registry_extension_templates.h"
#include "caors/meta/detail/registry_templates.h"
#include "caors/meta/detail/registry_view_templates.h"

#endif
