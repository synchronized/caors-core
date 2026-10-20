// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_REGISTRY_H_
#define CAORS_META_REGISTRY_H_

#include "caors/adapters/stl_wrapper.h"
#include "caors/error.h"
#include "caors/export.h"
#include "caors/meta/detail/parent_cast_ops.h"
#include "caors/meta/registration.h"
#include "caors/meta/type.h"

#include "caors/core/detail/shared_snapshot.h"

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

namespace caors::plugins {

class plugin_library_lease;

} // namespace caors::plugins

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

/** @brief registry 注册和查询同步策略。 */
enum class registry_management_policy {
  /** @brief 默认策略：注册、发布、卸载和查询都由 registry 内部同步，类型提交后不隐式替换。 */
  locked_append_only,
  /** @brief 初始化期可写，冻结后查询走只读优化路径；当前仍按默认同步实现。 */
  frozen_read_optimized,
  /** @brief 调用方自行保证注册/发布/卸载与查询之间没有数据竞争；当前仍按默认同步实现。 */
  manual_synchronization,
};

/** @brief registry 创建选项。 */
struct registry_options {
  registry_management_policy management = registry_management_policy::locked_append_only;
  /** @brief registry 内部状态使用的资源；为空时使用 CAORS process/library 资源。 */
  memory::memory_resource* internal_resource = nullptr;
};

/** @brief registry layer 生命周期策略。 */
enum class registry_layer_lifetime {
  /** @brief 跟随进程或宿主生命周期，不支持提前 detach，调用 guard 不做 in-flight 计数。 */
  pinned_process,
  /** @brief 可从 registry snapshot 中 detach，进入函数指针调用时需要 in-flight guard。 */
  detachable,
};

/** @brief registry layer 创建选项。 */
struct registry_layer_options {
  registry_layer_lifetime lifetime = registry_layer_lifetime::detachable;
  /** @brief layer 完整注册数据使用的资源；为空时继承宿主 registry 资源。 */
  memory::memory_resource* internal_resource = nullptr;
};

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
  explicit registry(registry_options options);
  ~registry();

  registry(const registry&) = delete;
  registry& operator=(const registry&) = delete;

  [[nodiscard]] error add_registration(registration_fn fn);
  [[nodiscard]] error run_registrations();
  [[nodiscard]] error init();
  /** @brief 执行延迟注册、校验并冻结 primary registry，冻结后 primary 写入返回 `errc::frozen`。 */
  [[nodiscard]] error freeze();
  [[nodiscard]] error validate() const;
  /** @brief 查询 primary registry 是否已经冻结。 */
  [[nodiscard]] bool frozen() const noexcept;

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
  /** @brief 返回 registry 的管理策略。 */
  [[nodiscard]] registry_management_policy management_policy() const noexcept;
  /** @brief 返回 registry 内部长期数据使用的资源。 */
  [[nodiscard]] memory::memory_resource* internal_resource() const noexcept;

  /** @brief 创建查询可回退到当前 snapshot、但写入保持隔离的 layer draft。 */
  [[nodiscard]] std::unique_ptr<registry_layer> create_layer() const;
  /** @brief 按指定生命周期创建查询可回退到当前 snapshot、但写入保持隔离的 layer draft。 */
  [[nodiscard]] std::unique_ptr<registry_layer>
  create_layer(registry_layer_lifetime lifetime) const;
  /** @brief 使用独立生命周期和内存资源创建隔离 layer draft。 */
  [[nodiscard]] std::unique_ptr<registry_layer> create_layer(registry_layer_options options) const;
  /** @brief 校验、冻结并原子发布 layer；失败时当前 registry 保持不变。 */
  [[nodiscard]] error publish_layer(registry_layer& layer);
  /** @brief 从新 snapshot 原子移除已发布 layer，并使旧句柄失效。 */
  [[nodiscard]] error detach_layer(registry_layer& layer);

private:
  template <typename T> friend class ::caors::registration::enum_builder;
  template <typename T> friend class ::caors::registration::type_builder;
  friend class detail::registry_access;
  friend class registry_layer;

  registry(std::shared_ptr<detail::registry_snapshot> base_snapshot,
           registry_layer_lifetime layer_lifetime, memory::memory_resource* internal_resource);

  [[nodiscard]] error register_type(type_id id, string_name name, type_guid guid,
                                    type_id parent_id);
  [[nodiscard]] error validate_type(const detail::pending_type_record& pending,
                                    const std::type_info& cpp_type) const;
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
  [[nodiscard]] std::shared_ptr<detail::registry_snapshot> capture_snapshot() const noexcept;

  memory::memory_resource* state_resource_ = nullptr;
  registry_options options_{};
  std::shared_ptr<detail::registry_layer_state> primary_layer_{};
  ::caors::detail::shared_snapshot<detail::registry_snapshot> snapshot_{};
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
  /** @brief 返回当前 layer 的生命周期策略。 */
  [[nodiscard]] registry_layer_lifetime lifetime() const noexcept;
  /** @brief 返回当前仍在执行的类型擦除调用数量。 */
  [[nodiscard]] std::size_t in_flight_calls() const noexcept;
  /** @brief 返回当前 layer 自身注册的类型数量，不包含基础 snapshot。 */
  [[nodiscard]] std::size_t local_size() const noexcept;
  /** @brief 返回内部 layer 状态的共享所有者数量，供安全卸载门禁使用。 */
  [[nodiscard]] std::size_t lease_count() const noexcept;

private:
  friend class registry;
  friend class ::caors::plugins::plugin_library_lease;
  registry_layer(std::shared_ptr<detail::registry_snapshot> base_snapshot,
                 registry_layer_lifetime lifetime, memory::memory_resource* internal_resource);

  [[nodiscard]] error attach_lifetime(std::shared_ptr<void> owner);

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
};

} // namespace detail

/** @brief 获取进程内全局类型元数据注册表。 */
[[nodiscard]] CAORS_API registry& get_registry() noexcept;

} // namespace caors::meta

#include "caors/meta/detail/registry_templates.h"

#endif
