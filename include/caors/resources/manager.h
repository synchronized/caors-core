// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESOURCES_MANAGER_H_
#define CAORS_RESOURCES_MANAGER_H_

#include "caors/core/containers.h"
#include "caors/resources/ref.h"

#include <cstddef>
#include <type_traits>

namespace caors::resources {

/**
 * @brief 非拥有资源索引。
 *
 * `manager` 只保存 `resource_guid -> resource*` 映射，不创建、不销毁资源，也不延长资源生命周期。
 * 它适合第一阶段作为资源数据库、加载器和缓存系统的底层索引。跨线程访问时调用方需要自行同步。
 */
class manager final {
public:
  explicit manager(memory_resource* resource = nullptr)
      : resource_(resource != nullptr ? resource : get_default_memory_resource()),
        resources_by_guid_(typename resource_map::allocator_type{resource_}),
        guids_by_resource_(typename pointer_map::allocator_type{resource_}) {}

  manager(const manager&) = delete;
  manager& operator=(const manager&) = delete;
  manager(manager&&) = delete;
  manager& operator=(manager&&) = delete;

  /** @brief 查询索引内部容器使用的内存资源。 */
  [[nodiscard]] memory_resource* resource() const noexcept { return resource_; }

  /** @brief 查询当前绑定数量。 */
  [[nodiscard]] std::size_t size() const noexcept { return resources_by_guid_.size(); }
  [[nodiscard]] bool empty() const noexcept { return resources_by_guid_.empty(); }

  /** @brief 判断 GUID 是否已经绑定资源。 */
  [[nodiscard]] bool contains(resource_guid guid) const { return find(guid) != nullptr; }

  /** @brief 判断资源指针是否已经绑定。 */
  [[nodiscard]] bool contains(const resources::resource* instance) const {
    return instance != nullptr && guids_by_resource_.find(instance) != guids_by_resource_.end();
  }

  /**
   * @brief 使用资源对象自身 GUID 绑定资源。
   *
   * 空指针或 nil GUID 会失败。
   */
  bool bind(resources::resource* instance) {
    return instance != nullptr ? bind(instance->guid(), instance) : false;
  }

  /**
   * @brief 绑定 GUID 和资源对象。
   *
   * 空指针或 nil GUID 会失败。若资源对象已经有非 nil GUID，必须和传入 GUID 一致；若资源对象 GUID
   * 为 nil，则会写入传入 GUID。同一对 GUID/资源重复绑定视为成功。
   */
  bool bind(resource_guid guid, resources::resource* instance) {
    if (!guid || instance == nullptr) {
      return false;
    }

    const resource_guid instance_guid = instance->guid();
    if (instance_guid && instance_guid != guid) {
      return false;
    }

    const auto guid_position = resources_by_guid_.find(guid);
    if (guid_position != resources_by_guid_.end()) {
      return guid_position->second == instance;
    }

    const auto resource_position = guids_by_resource_.find(instance);
    if (resource_position != guids_by_resource_.end()) {
      return resource_position->second == guid;
    }

    if (!instance_guid) {
      instance->set_guid(guid);
    }

    resources_by_guid_.emplace(guid, instance);
    guids_by_resource_.emplace(instance, guid);
    return true;
  }

  /** @brief 按 GUID 解绑资源。 */
  bool unbind(resource_guid guid) {
    const auto position = resources_by_guid_.find(guid);
    if (position == resources_by_guid_.end()) {
      return false;
    }

    guids_by_resource_.erase(position->second);
    resources_by_guid_.erase(position);
    return true;
  }

  /** @brief 按资源指针解绑资源。 */
  bool unbind(const resources::resource* instance) {
    const auto position = guids_by_resource_.find(instance);
    if (position == guids_by_resource_.end()) {
      return false;
    }

    resources_by_guid_.erase(position->second);
    guids_by_resource_.erase(position);
    return true;
  }

  /** @brief 清空所有绑定；不会销毁资源对象。 */
  void clear() noexcept {
    resources_by_guid_.clear();
    guids_by_resource_.clear();
  }

  /** @brief 按 GUID 查找资源。未命中时返回 `nullptr`。 */
  [[nodiscard]] resources::resource* find(resource_guid guid) const {
    if (!guid) {
      return nullptr;
    }

    const auto position = resources_by_guid_.find(guid);
    return position != resources_by_guid_.end() ? position->second : nullptr;
  }

  /** @brief 按 GUID 查找指定资源类型。类型不匹配时返回 `nullptr`。 */
  template <typename T> [[nodiscard]] T* find(resource_guid guid) const {
    static_assert(std::is_base_of_v<resources::resource, T>,
                  "T must derive from caors::resources::resource");
    return dynamic_cast<T*>(find(guid));
  }

  /** @brief 通过资源指针查询已绑定 GUID。未绑定时返回 nil GUID。 */
  [[nodiscard]] resource_guid guid_of(const resources::resource* instance) const {
    const auto position = guids_by_resource_.find(instance);
    return position != guids_by_resource_.end() ? position->second : resource_guid::nil();
  }

  /**
   * @brief 按引用保存的 GUID 解析资源。
   *
   * 未命中或类型不匹配时会清除引用里的已解析指针，但保留 GUID。
   */
  template <typename T> [[nodiscard]] bool resolve(ref<T>& reference) const {
    if (!reference.has_guid()) {
      return false;
    }

    T* instance = find<T>(reference.guid());
    if (instance == nullptr) {
      reference.unresolve();
      return false;
    }

    return reference.resolve(instance);
  }

private:
  using resource_map = core::hash_map<resource_guid, resources::resource*, resource_guid_hash>;
  using pointer_map = core::hash_map<const resources::resource*, resource_guid>;

  memory_resource* resource_ = nullptr;
  resource_map resources_by_guid_;
  pointer_map guids_by_resource_;
};

} // namespace caors::resources

namespace caors {

using resource_manager = resources::manager;

} // namespace caors

#endif
