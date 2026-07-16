// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESOURCES_CACHE_H_
#define CAORS_RESOURCES_CACHE_H_

#include "caors/core/containers.h"
#include "caors/memory/unique_ptr.h"
#include "caors/resources/handle.h"
#include "caors/resources/manager.h"

#include <cstddef>
#include <type_traits>
#include <utility>

namespace caors::resources {

/**
 * @brief 拥有式资源缓存。
 *
 * `cache<T>` 按 `resource_guid` 持有 `T` 资源对象，并同步维护一个非拥有 `resources::manager`
 * 作为查询和引用解析索引。它不负责从文件加载资源，也不做异步调度；加载策略后续由 loader 层提供。
 */
template <typename T> class cache final {
  static_assert(std::is_base_of_v<resources::resource, T>,
                "T must derive from caors::resources::resource");

public:
  using resource_type = T;

  explicit cache(memory_resource* resource = nullptr)
      : resource_(resource != nullptr ? resource : get_default_memory_resource()),
        index_(resource_), resources_by_guid_(typename resource_map::allocator_type{resource_}) {}

  cache(const cache&) = delete;
  cache& operator=(const cache&) = delete;
  cache(cache&&) = delete;
  cache& operator=(cache&&) = delete;

  /** @brief 查询缓存内部容器和资源创建使用的内存资源。 */
  [[nodiscard]] memory_resource* resource() const noexcept { return resource_; }

  /** @brief 查询非拥有资源索引。 */
  [[nodiscard]] const manager& index() const noexcept { return index_; }

  /** @brief 查询指定 GUID 当前缓存绑定版本。未绑定时返回 0。 */
  [[nodiscard]] resource_generation generation_of(resource_guid guid) const {
    return index_.generation_of(guid);
  }

  /** @brief 查询当前缓存资源数量。 */
  [[nodiscard]] std::size_t size() const noexcept { return resources_by_guid_.size(); }
  [[nodiscard]] bool empty() const noexcept { return resources_by_guid_.empty(); }

  /** @brief 判断 GUID 是否已经存在于缓存中。 */
  [[nodiscard]] bool contains(resource_guid guid) const { return find(guid) != nullptr; }

  /** @brief 按 GUID 查找资源。未命中时返回 `nullptr`。 */
  [[nodiscard]] resource_type* find(resource_guid guid) const {
    const auto position = resources_by_guid_.find(guid);
    return position != resources_by_guid_.end() ? position->second.get() : nullptr;
  }

  /** @brief 按 GUID 查找资源强引用。未命中时返回空 handle。 */
  [[nodiscard]] handle<resource_type> find_handle(resource_guid guid) const {
    const auto position = resources_by_guid_.find(guid);
    return position != resources_by_guid_.end() ? position->second : handle<resource_type>{};
  }

  /** @brief 按 GUID 查找可转换的资源类型。类型不匹配时返回 `nullptr`。 */
  template <typename U> [[nodiscard]] U* find_as(resource_guid guid) const {
    static_assert(std::is_base_of_v<resources::resource, U>,
                  "U must derive from caors::resources::resource");
    return dynamic_cast<U*>(find(guid));
  }

  /**
   * @brief 构造并插入一个资源。
   *
   * nil GUID 或重复 GUID 会失败并返回 `nullptr`。若构造出的资源已经有非 nil GUID，必须和传入 GUID
   * 一致；若资源 GUID 为 nil，则会写入传入 GUID。
   */
  template <typename... Args>
  [[nodiscard]] resource_type* create(resource_guid guid, Args&&... args) {
    return create_handle(guid, std::forward<Args>(args)...).get();
  }

  /**
   * @brief 构造并插入一个资源，返回缓存持有的强引用句柄。
   *
   * nil GUID 或重复 GUID 会失败并返回空 handle。若构造出的资源已经有非 nil GUID，必须和传入 GUID
   * 一致；若资源 GUID 为 nil，则会写入传入 GUID。
   */
  template <typename... Args>
  [[nodiscard]] handle<resource_type> create_handle(resource_guid guid, Args&&... args) {
    if (!guid || contains(guid)) {
      return {};
    }

    handle<resource_type> owned =
        make_handle<resource_type>(resource_, std::forward<Args>(args)...);
    if (!owned) {
      return {};
    }

    const resource_guid owned_guid = owned->guid();
    if (owned_guid && owned_guid != guid) {
      return {};
    }

    if (!owned_guid) {
      owned->set_guid(guid);
    }

    resource_type* stored = owned.get();
    if (!index_.bind(guid, stored)) {
      return {};
    }

    const auto [_, inserted] = resources_by_guid_.emplace(guid, owned);
    if (!inserted) {
      index_.unbind(guid);
      return {};
    }

    return owned;
  }

  /**
   * @brief 插入已经构造好的资源并转移到强 handle。
   *
   * 传入资源必须携带非 nil GUID，且该 GUID 尚未存在于缓存中。成功后缓存持有一份强引用。
   */
  bool insert(memory::unique_ptr<resource_type> owned) {
    return insert(adopt_handle(std::move(owned), resource_));
  }

  /** @brief 空资源不能插入缓存。 */
  bool insert(std::nullptr_t) noexcept { return false; }

  /**
   * @brief 插入已经构造好的资源强引用。
   *
   * 传入资源必须携带非 nil GUID，且该 GUID 尚未存在于缓存中。成功后缓存持有一份强引用。
   */
  bool insert(handle<resource_type> owned) {
    if (!owned || !owned->has_guid() || contains(owned->guid())) {
      return false;
    }

    const resource_guid guid = owned->guid();
    resource_type* stored = owned.get();
    if (!index_.bind(guid, stored)) {
      return false;
    }

    const auto [_, inserted] = resources_by_guid_.emplace(guid, std::move(owned));
    if (!inserted) {
      index_.unbind(guid);
      return false;
    }

    return true;
  }

  /**
   * @brief 替换已经存在的资源并转移到强 handle。
   *
   * 传入资源必须携带非 nil GUID，且缓存中已经存在同 GUID 资源。替换成功后缓存索引指向新资源。
   * 旧资源会在最后一个外部强引用释放后销毁。已有外部裸指针或已解析 `resource_ref<T>`
   * 不会自动失效，调用方应避免继续使用旧指针，并在需要时重新解析引用。
   */
  bool replace(memory::unique_ptr<resource_type> owned) {
    return replace(adopt_handle(std::move(owned), resource_));
  }

  /** @brief 空资源不能替换缓存中的资源。 */
  bool replace(std::nullptr_t) noexcept { return false; }

  /**
   * @brief 替换已经存在的资源强引用。
   *
   * 传入资源必须携带非 nil GUID，且缓存中已经存在同 GUID 资源。替换成功后缓存指向新资源。
   * 旧资源会在最后一个外部强引用释放后销毁。
   */
  bool replace(handle<resource_type> owned) {
    if (!owned || !owned->has_guid()) {
      return false;
    }

    const resource_guid guid = owned->guid();
    const auto position = resources_by_guid_.find(guid);
    if (position == resources_by_guid_.end()) {
      return false;
    }

    resource_type* stored = owned.get();
    if (!index_.unbind(guid)) {
      return false;
    }

    if (!index_.bind(guid, stored)) {
      (void)index_.bind(guid, position->second.get());
      return false;
    }

    position->second = std::move(owned);
    return true;
  }

  /**
   * @brief 从缓存中移除资源。
   *
   * 成功移除时返回 true。若外部仍持有强 handle，资源会继续存活。已有外部裸指针或已解析
   * `resource_ref<T>` 不会自动置空。
   */
  bool erase(resource_guid guid) {
    const auto position = resources_by_guid_.find(guid);
    if (position == resources_by_guid_.end()) {
      return false;
    }

    index_.unbind(guid);
    resources_by_guid_.erase(position);
    return true;
  }

  /**
   * @brief 从缓存中移除资源并把强引用交给调用方。
   *
   * 返回的资源仍保留自身 GUID，但不再能通过当前缓存或内部索引查找。
   */
  [[nodiscard]] handle<resource_type> release(resource_guid guid) {
    const auto position = resources_by_guid_.find(guid);
    if (position == resources_by_guid_.end()) {
      return {};
    }

    index_.unbind(guid);
    handle<resource_type> owned = std::move(position->second);
    resources_by_guid_.erase(position);
    return owned;
  }

  /** @brief 清空缓存并释放全部缓存强引用。 */
  void clear() noexcept {
    index_.clear();
    resources_by_guid_.clear();
  }

  /** @brief 按引用保存的 GUID 解析资源。 */
  template <typename U> [[nodiscard]] bool resolve(ref<U>& reference) const {
    return index_.resolve(reference);
  }

  /** @brief 判断已解析资源引用是否仍指向当前缓存中的同一绑定版本。 */
  template <typename U> [[nodiscard]] bool is_current(const ref<U>& reference) const {
    return index_.is_current(reference);
  }

private:
  using resource_map = core::hash_map<resource_guid, handle<resource_type>, resource_guid_hash>;

  memory_resource* resource_ = nullptr;
  manager index_;
  resource_map resources_by_guid_;
};

} // namespace caors::resources

namespace caors {

template <typename T> using resource_cache = resources::cache<T>;

} // namespace caors

#endif
