// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_H_
#define CAORS_META_H_

#include "caors/core/guid.h"
#include "caors/core/string_name.h"
#include "caors/export.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace caors {

/**
 * @brief 运行期类型标识。
 *
 * 类型标识由 `string_name` 的运行期 id 派生，适合运行期查找和注册表索引。它不是跨版本序列化
 * ID。
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

/**
 * @brief 稳定类型标识。
 *
 * `type_guid` 用于序列化、存档、资源引用和插件边界等需要跨进程、跨版本稳定识别类型的场景。
 * 它和运行期 `type_id` 不同，不应由 `string_name::id()` 派生。
 */
struct type_guid {
  guid value{};

  constexpr type_guid() noexcept = default;
  explicit constexpr type_guid(guid guid_value) noexcept : value(guid_value) {}

  [[nodiscard]] static constexpr type_guid nil() noexcept { return type_guid{}; }
  [[nodiscard]] bool is_nil() const noexcept { return value.is_nil(); }
  [[nodiscard]] explicit operator bool() const noexcept { return !is_nil(); }

  friend constexpr bool operator==(const type_guid& lhs, const type_guid& rhs) noexcept {
    return lhs.value == rhs.value;
  }

  friend constexpr bool operator!=(const type_guid& lhs, const type_guid& rhs) noexcept {
    return !(lhs == rhs);
  }
};

/** @brief 根据类型名生成运行期类型标识。 */
[[nodiscard]] constexpr type_id make_type_id(string_name name) noexcept {
  return type_id{name.id()};
}

/** @brief 根据类型名生成运行期类型标识。 */
[[nodiscard]] inline type_id make_type_id(std::string_view name) {
  return make_type_id(string_name{name});
}

/**
 * @brief 对象系统和反射系统共享的基础类型描述。
 *
 * 该结构只保存类型身份、名称、稳定 GUID、布局和父类型关系，不包含对象生命周期或字段/方法反射
 * 信息。
 */
struct type_info {
  type_id id{};
  string_name name{};
  type_guid guid{};
  std::size_t size = 0;
  std::size_t alignment = 0;
  type_id parent_id{};

  [[nodiscard]] constexpr bool valid() const noexcept {
    return static_cast<bool>(id) && !name.empty();
  }
};

/** @brief 创建基础类型描述。 */
template <typename T>
[[nodiscard]] constexpr type_info make_type_info(string_name name,
                                                 type_id parent_id = type_id::invalid()) noexcept {
  return type_info{
      make_type_id(name), name, type_guid::nil(), sizeof(T), alignof(T), parent_id,
  };
}

/** @brief 创建带稳定 GUID 的基础类型描述。 */
template <typename T>
[[nodiscard]] constexpr type_info make_type_info(string_name name, type_guid guid,
                                                 type_id parent_id = type_id::invalid()) noexcept {
  return type_info{
      make_type_id(name), name, guid, sizeof(T), alignof(T), parent_id,
  };
}

/** @brief 创建基础类型描述。 */
template <typename T>
[[nodiscard]] type_info make_type_info(std::string_view name,
                                       type_id parent_id = type_id::invalid()) {
  return make_type_info<T>(string_name{name}, parent_id);
}

/** @brief 创建带稳定 GUID 的基础类型描述。 */
template <typename T>
[[nodiscard]] type_info make_type_info(std::string_view name, type_guid guid,
                                       type_id parent_id = type_id::invalid()) {
  return make_type_info<T>(string_name{name}, guid, parent_id);
}

/** @brief 基础类型注册结果。 */
enum class type_register_result {
  success,
  invalid_type,
  duplicate_id,
  duplicate_name,
  duplicate_guid,
  hash_collision,
  missing_parent,
};

/** @brief 判断基础类型注册是否成功。 */
[[nodiscard]] constexpr bool type_register_succeeded(type_register_result result) noexcept {
  return result == type_register_result::success;
}

/** @brief 元数据组件注册结果。 */
enum class meta_component_result {
  success,
  invalid_type,
  missing_type,
  duplicate_component,
};

/** @brief 判断元数据组件注册是否成功。 */
[[nodiscard]] constexpr bool meta_component_succeeded(meta_component_result result) noexcept {
  return result == meta_component_result::success;
}

namespace detail {

using meta_component_key = const void*;

template <typename Component> [[nodiscard]] meta_component_key meta_component_key_of() noexcept {
  static const int key = 0;
  return &key;
}

class meta_component_storage_base {
public:
  virtual ~meta_component_storage_base() = default;

  [[nodiscard]] virtual bool contains(type_id id) const = 0;
  virtual bool erase(type_id id) = 0;
  [[nodiscard]] virtual std::size_t size() const noexcept = 0;
  [[nodiscard]] virtual std::vector<type_id> ids() const = 0;
  virtual void clear() noexcept = 0;
};

template <typename Component>
class meta_component_storage final : public meta_component_storage_base {
public:
  static_assert(std::is_copy_constructible_v<Component>,
                "Meta components must be copy constructible");

  template <typename... Args> [[nodiscard]] bool emplace(type_id id, Args&&... args) {
    std::lock_guard lock(mutex_);
    return components_.try_emplace(id.value(), std::forward<Args>(args)...).second;
  }

  [[nodiscard]] std::optional<Component> find(type_id id) const {
    std::lock_guard lock(mutex_);
    const auto position = components_.find(id.value());
    if (position == components_.end()) {
      return std::nullopt;
    }

    return position->second;
  }

  [[nodiscard]] bool contains(type_id id) const override {
    std::lock_guard lock(mutex_);
    return components_.contains(id.value());
  }

  bool erase(type_id id) override {
    std::lock_guard lock(mutex_);
    return components_.erase(id.value()) != 0;
  }

  [[nodiscard]] std::size_t size() const noexcept override {
    std::lock_guard lock(mutex_);
    return components_.size();
  }

  [[nodiscard]] std::vector<type_id> ids() const override {
    std::lock_guard lock(mutex_);
    std::vector<type_id> result;
    result.reserve(components_.size());
    for (const auto& [id, component] : components_) {
      (void)component;
      result.emplace_back(id);
    }

    return result;
  }

  void clear() noexcept override {
    std::lock_guard lock(mutex_);
    components_.clear();
  }

private:
  mutable std::mutex mutex_;
  std::unordered_map<std::uint64_t, Component> components_;
};

} // namespace detail

template <typename... Components> class meta_view;
class meta_registry_state;

/**
 * @brief 类型元数据注册表。
 *
 * `meta_registry` 以 `type_id` 为 key，先注册基础 `type_info`，再按类型挂载对象工厂、
 * 反射描述、序列化描述等元数据组件。它不是运行期游戏 ECS。
 */
class CAORS_API meta_registry final {
public:
  meta_registry();
  ~meta_registry();

  meta_registry(const meta_registry&) = delete;
  meta_registry& operator=(const meta_registry&) = delete;

  [[nodiscard]] type_register_result register_type(const type_info& type);
  [[nodiscard]] bool contains(type_id id) const;
  [[nodiscard]] std::optional<type_info> find_type(type_id id) const;
  [[nodiscard]] std::optional<type_info> find_type(type_guid guid) const;
  [[nodiscard]] std::optional<type_info> find_type(std::string_view name) const;
  [[nodiscard]] std::optional<type_info> find_type(string_name name) const;
  [[nodiscard]] bool is_a(type_id id, type_id base_id) const;
  [[nodiscard]] std::size_t size() const noexcept;
  void clear() noexcept;

  /** @brief 给已注册类型挂载一个元数据组件。 */
  template <typename Component, typename... Args>
  [[nodiscard]] meta_component_result emplace(type_id id, Args&&... args) {
    using component_type = std::decay_t<Component>;

    if (!id) {
      return meta_component_result::invalid_type;
    }

    if (!contains(id)) {
      return meta_component_result::missing_type;
    }

    detail::meta_component_storage_base* storage_base =
        find_component_storage(detail::meta_component_key_of<component_type>());
    if (storage_base == nullptr) {
      storage_base = &ensure_component_storage(
          detail::meta_component_key_of<component_type>(),
          std::make_unique<detail::meta_component_storage<component_type>>());
    }

    auto& storage = static_cast<detail::meta_component_storage<component_type>&>(*storage_base);
    if (!storage.emplace(id, std::forward<Args>(args)...)) {
      return meta_component_result::duplicate_component;
    }

    return meta_component_result::success;
  }

  /** @brief 判断指定类型是否挂载了某个元数据组件。 */
  template <typename Component> [[nodiscard]] bool has(type_id id) const {
    using component_type = std::decay_t<Component>;
    const detail::meta_component_storage_base* storage_base =
        find_component_storage(detail::meta_component_key_of<component_type>());
    if (storage_base == nullptr) {
      return false;
    }

    return storage_base->contains(id);
  }

  /** @brief 获取指定类型上的元数据组件副本。 */
  template <typename Component> [[nodiscard]] std::optional<Component> find(type_id id) const {
    using component_type = std::decay_t<Component>;
    const detail::meta_component_storage_base* storage_base =
        find_component_storage(detail::meta_component_key_of<component_type>());
    if (storage_base == nullptr) {
      return std::nullopt;
    }

    const auto& storage =
        static_cast<const detail::meta_component_storage<component_type>&>(*storage_base);
    return storage.find(id);
  }

  /** @brief 移除指定类型上的元数据组件。 */
  template <typename Component> bool remove(type_id id) {
    using component_type = std::decay_t<Component>;
    detail::meta_component_storage_base* storage_base =
        find_component_storage(detail::meta_component_key_of<component_type>());
    if (storage_base == nullptr) {
      return false;
    }

    return storage_base->erase(id);
  }

  /** @brief 统计挂载了指定组件的类型数量。 */
  template <typename Component> [[nodiscard]] std::size_t count() const noexcept {
    using component_type = std::decay_t<Component>;
    const detail::meta_component_storage_base* storage_base =
        find_component_storage(detail::meta_component_key_of<component_type>());
    if (storage_base == nullptr) {
      return 0;
    }

    return storage_base->size();
  }

  /** @brief 查询同时拥有指定组件集合的类型。 */
  template <typename... Components> [[nodiscard]] meta_view<Components...> view() const;

private:
  [[nodiscard]] detail::meta_component_storage_base*
  find_component_storage(detail::meta_component_key key);
  [[nodiscard]] const detail::meta_component_storage_base*
  find_component_storage(detail::meta_component_key key) const;
  detail::meta_component_storage_base&
  ensure_component_storage(detail::meta_component_key key,
                           std::unique_ptr<detail::meta_component_storage_base> storage);
  [[nodiscard]] std::vector<type_id> view_component_ids(const detail::meta_component_key* keys,
                                                        std::size_t count) const;

  meta_registry_state* state_ = nullptr;
};

/** @brief 获取进程内默认类型元数据注册表。 */
[[nodiscard]] CAORS_API meta_registry& get_meta_registry() noexcept;

/** @brief 向指定元数据注册表注册基础类型。 */
template <typename T>
[[nodiscard]] type_register_result register_type(meta_registry& registry, string_name name,
                                                 type_id parent_id = type_id::invalid()) {
  return registry.register_type(make_type_info<T>(name, parent_id));
}

/** @brief 向指定元数据注册表注册带稳定 GUID 的基础类型。 */
template <typename T>
[[nodiscard]] type_register_result register_type(meta_registry& registry, string_name name,
                                                 type_guid guid,
                                                 type_id parent_id = type_id::invalid()) {
  return registry.register_type(make_type_info<T>(name, guid, parent_id));
}

/** @brief 向指定元数据注册表注册基础类型。 */
template <typename T>
[[nodiscard]] type_register_result register_type(meta_registry& registry, std::string_view name,
                                                 type_id parent_id = type_id::invalid()) {
  return register_type<T>(registry, string_name{name}, parent_id);
}

/** @brief 向指定元数据注册表注册带稳定 GUID 的基础类型。 */
template <typename T>
[[nodiscard]] type_register_result register_type(meta_registry& registry, std::string_view name,
                                                 type_guid guid,
                                                 type_id parent_id = type_id::invalid()) {
  return register_type<T>(registry, string_name{name}, guid, parent_id);
}

/** @brief 向默认元数据注册表注册基础类型。 */
template <typename T>
[[nodiscard]] type_register_result register_type(string_name name,
                                                 type_id parent_id = type_id::invalid()) {
  return register_type<T>(get_meta_registry(), name, parent_id);
}

/** @brief 向默认元数据注册表注册带稳定 GUID 的基础类型。 */
template <typename T>
[[nodiscard]] type_register_result register_type(string_name name, type_guid guid,
                                                 type_id parent_id = type_id::invalid()) {
  return register_type<T>(get_meta_registry(), name, guid, parent_id);
}

/** @brief 向默认元数据注册表注册基础类型。 */
template <typename T>
[[nodiscard]] type_register_result register_type(std::string_view name,
                                                 type_id parent_id = type_id::invalid()) {
  return register_type<T>(get_meta_registry(), string_name{name}, parent_id);
}

/** @brief 向默认元数据注册表注册带稳定 GUID 的基础类型。 */
template <typename T>
[[nodiscard]] type_register_result register_type(std::string_view name, type_guid guid,
                                                 type_id parent_id = type_id::invalid()) {
  return register_type<T>(get_meta_registry(), string_name{name}, guid, parent_id);
}

/**
 * @brief 元数据组件查询视图。
 *
 * 视图保存查询时匹配的 `type_id` 列表，不持有组件引用。需要组件内容时可通过 `find<T>()`
 * 获取副本。
 */
template <typename... Components> class meta_view final {
public:
  using const_iterator = typename std::vector<type_id>::const_iterator;

  meta_view() = default;

  [[nodiscard]] const_iterator begin() const noexcept { return ids_.begin(); }
  [[nodiscard]] const_iterator end() const noexcept { return ids_.end(); }
  [[nodiscard]] bool empty() const noexcept { return ids_.empty(); }
  [[nodiscard]] std::size_t size() const noexcept { return ids_.size(); }
  [[nodiscard]] const std::vector<type_id>& ids() const noexcept { return ids_; }

  /** @brief 获取视图中某个类型上的组件副本。 */
  template <typename Component> [[nodiscard]] std::optional<Component> find(type_id id) const {
    if (registry_ == nullptr) {
      return std::nullopt;
    }

    return registry_->template find<Component>(id);
  }

private:
  friend class meta_registry;

  meta_view(const meta_registry* registry, std::vector<type_id> ids)
      : registry_(registry), ids_(std::move(ids)) {}

  const meta_registry* registry_ = nullptr;
  std::vector<type_id> ids_;
};

template <typename... Components>
[[nodiscard]] meta_view<Components...> meta_registry::view() const {
  static_assert(sizeof...(Components) > 0, "Meta view requires at least one component type");

  detail::meta_component_key keys[] = {
      detail::meta_component_key_of<std::decay_t<Components>>()...};
  return meta_view<Components...>{this, view_component_ids(keys, sizeof...(Components))};
}

}

#endif
