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

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <utility>

namespace caors::meta {

class registry;
class registry_layer;

} // namespace caors::meta

namespace caors::meta::detail {

class registry_state;
class registry_snapshot;
class registry_layer_state;
class registry_access;
class pending_type_record;
class type_record;

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

  /** @brief 创建查询可回退到当前 snapshot、但写入保持隔离的 layer draft。 */
  [[nodiscard]] std::unique_ptr<registry_layer> create_layer() const;
  /** @brief 校验、冻结并原子发布 layer；失败时当前 registry 保持不变。 */
  [[nodiscard]] error publish_layer(registry_layer& layer);
  /** @brief 从新 snapshot 原子移除已发布 layer，并使旧句柄失效。 */
  [[nodiscard]] error detach_layer(registry_layer& layer);

  /** @brief 判断指定类型是否挂载了某份扩展数据。 */
  template <typename Extension> [[nodiscard]] bool has_extension(type_id id) const;

  /** @brief 获取指定类型上的扩展数据副本。 */
  template <typename Extension>
  [[nodiscard]] std::optional<Extension> find_extension(type_id id) const;

  /**
   * @brief 获取指定类型上的扩展数据只读指针。
   *
   * 仅查询当前 registry 自己的 primary layer，不跨 overlay 回退。返回指针不拥有对象，
   * 生命周期不超过当前 registry；跨 layer 查询应使用返回副本的 `find_extension()`。
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
  friend class registry_layer;

  explicit registry(std::shared_ptr<detail::registry_snapshot> base_snapshot);

  [[nodiscard]] error register_type(type_id id, string_name name, type_guid guid,
                                    type_id parent_id);
  [[nodiscard]] error commit_type(detail::pending_type_record pending,
                                  const std::type_info& cpp_type);
  template <typename T>
  [[nodiscard]] error register_type_for(string_name name, type_guid guid = type_guid::nil(),
                                        type_id parent_id = type_id::invalid());
  [[nodiscard]] error ensure_static_type_record(const detail::type_record& fallback);
  [[nodiscard]] error set_type_guid(type_id id, type_guid guid);
  [[nodiscard]] error set_type_parent(type_id id, type_id parent_id);
  [[nodiscard]] error set_type_parent(type_id id, type_id parent_id, detail::parent_cast_ops cast);
  template <typename Child, typename Parent> [[nodiscard]] error set_type_parent_for();

  [[nodiscard]] error register_cpp_type_info(const std::type_info& info, type_id id);
  [[nodiscard]] error add_extension(type_id id, detail::extension_data extension);
  [[nodiscard]] bool has_extension(type_id id, detail::extension_key key) const;
  [[nodiscard]] const detail::extension_data* find_extension_data(type_id id,
                                                                  detail::extension_key key) const;
  using extension_copy_fn = void (*)(void*, const detail::extension_data&);
  [[nodiscard]] bool copy_extension_data(type_id id, detail::extension_key key, void* destination,
                                         extension_copy_fn copy) const;

  [[nodiscard]] vector<type_id>
  view_extension_ids(const std::shared_ptr<detail::registry_snapshot>& snapshot,
                     const detail::extension_key* keys, std::size_t count) const;
  [[nodiscard]] std::shared_ptr<detail::registry_snapshot> capture_snapshot() const noexcept;

  memory::memory_resource* state_resource_ = nullptr;
  std::shared_ptr<detail::registry_layer_state> primary_layer_{};
  std::atomic<std::shared_ptr<detail::registry_snapshot>> snapshot_{};
  detail::registry_state* state_ = nullptr;
};

/** @brief 尚未发布或已经冻结的独立 registry layer 所有者。 */
class CAORS_API registry_layer final {
public:
  registry_layer(const registry_layer&) = delete;
  registry_layer& operator=(const registry_layer&) = delete;
  ~registry_layer();

  [[nodiscard]] registry& get_registry() noexcept;
  [[nodiscard]] const registry& get_registry() const noexcept;
  [[nodiscard]] bool published() const noexcept;
  [[nodiscard]] bool active() const noexcept;
  /** @brief 返回当前仍在执行的类型擦除调用数量。 */
  [[nodiscard]] std::size_t in_flight_calls() const noexcept;

private:
  friend class registry;
  explicit registry_layer(std::shared_ptr<detail::registry_snapshot> base_snapshot);

  registry* registry_ = nullptr;
  bool published_ = false;
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

  [[nodiscard]] static std::shared_ptr<registry_snapshot>
  snapshot(const registry& registry) noexcept {
    return registry.capture_snapshot();
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
