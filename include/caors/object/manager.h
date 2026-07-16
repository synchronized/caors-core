// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_MANAGER_H_
#define CAORS_OBJECT_MANAGER_H_

#include "caors/core/containers.h"
#include "caors/object/handle.h"
#include "caors/object/index.h"
#include "caors/object/registry.h"

#include <cstddef>
#include <type_traits>
#include <utility>

namespace caors::objects {

/**
 * @brief 对象实例生命周期管理器。
 *
 * `manager` 拥有通过它创建的对象实例，按 `object_id` 建立运行期索引，并记录每个对象的注册类型和
 * 分配资源。该类型不提供内部线程同步；跨线程访问时调用方需要自行加锁。
 */
class CAORS_API manager final {
public:
  explicit manager(memory_resource* resource = nullptr);
  explicit manager(object_index& index, memory_resource* resource = nullptr);
  ~manager();

  manager(const manager&) = delete;
  manager& operator=(const manager&) = delete;
  manager(manager&&) = delete;
  manager& operator=(manager&&) = delete;

  /** @brief 查询对象和索引使用的内存资源。 */
  [[nodiscard]] memory_resource* resource() const noexcept { return resource_; }

  /** @brief 查询当前关联的持久对象索引。未关联时返回 `nullptr`。 */
  [[nodiscard]] object_index* index() noexcept { return index_; }
  [[nodiscard]] const object_index* index() const noexcept { return index_; }

  /** @brief 查询当前存活对象数量。 */
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] bool empty() const noexcept { return size() == 0; }

  /** @brief 查询已逻辑销毁、等待物理释放的对象数量。 */
  [[nodiscard]] std::size_t pending_destroy_count() const noexcept;

  /** @brief 判断对象 id 当前是否由该管理器持有。 */
  [[nodiscard]] bool contains(object_id id) const;

  /** @brief 按对象 id 查找对象。返回值不转移所有权。 */
  [[nodiscard]] object* find(object_id id) const;

  /** @brief 按持久 GUID 查找对象。未关联 `object_index` 或未命中时返回 `nullptr`。 */
  [[nodiscard]] object* find(object_guid guid) const;

  /** @brief 按身份句柄查找对象，并校验声明类型。返回值不转移所有权。 */
  [[nodiscard]] object* find(handle target) const;

  /** @brief 按对象 id 查找并动态转换为指定 C++ 类型。返回值不转移所有权。 */
  template <typename T> [[nodiscard]] T* find(object_id id) const {
    static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");

    const entry* stored = find_entry(id);
    if (stored == nullptr) {
      return nullptr;
    }

    const type expected = objects::find<T>();
    if (!expected || !objects::is_a(stored->type, expected.id())) {
      return nullptr;
    }

    return dynamic_cast<T*>(stored->instance);
  }

  /** @brief 按身份句柄查找并动态转换为指定 C++ 类型。返回值不转移所有权。 */
  template <typename T> [[nodiscard]] T* find(handle target) const {
    object* instance = find(target);
    return instance != nullptr ? dynamic_cast<T*>(instance) : nullptr;
  }

  /** @brief 按类型化身份句柄查找对象。返回值不转移所有权。 */
  template <typename T> [[nodiscard]] T* find(typed_handle<T> target) const {
    return find<T>(target.handle());
  }

  /** @brief 判断身份句柄当前是否仍可解析。 */
  [[nodiscard]] bool alive(handle target) const { return find(target) != nullptr; }

  /** @brief 创建默认构造对象。对象类型必须已注册且支持默认构造。 */
  [[nodiscard]] object* create(object_type_id id);

  /** @brief 创建默认构造对象并绑定持久 GUID。需要 manager 关联 `object_index`。 */
  [[nodiscard]] object* create_with_guid(object_type_id id, object_guid guid);

  /** @brief 创建指定 C++ 类型对象。类型必须已注册，可使用构造参数。 */
  template <typename T, typename... Args> [[nodiscard]] T* create(Args&&... args) {
    static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");

    const type object_type = objects::find<T>();
    if (!object_type) {
      return nullptr;
    }

    T* instance = objects::create<T>(resource_, std::forward<Args>(args)...);
    if (instance == nullptr) {
      return nullptr;
    }

#if CAORS_HAS_EXCEPTIONS
    try {
#endif
      if (!store(instance, object_type.id(), resource_)) {
        objects::destroy(instance, resource_);
        return nullptr;
      }

      return instance;
#if CAORS_HAS_EXCEPTIONS
    } catch (...) {
      objects::destroy(instance, resource_);
      throw;
    }
#endif
  }

  /** @brief 创建指定 C++ 类型对象并绑定持久 GUID。需要 manager 关联 `object_index`。 */
  template <typename T, typename... Args>
  [[nodiscard]] T* create_with_guid(object_guid guid, Args&&... args) {
    if (index_ == nullptr || !guid) {
      return nullptr;
    }

    T* instance = create<T>(std::forward<Args>(args)...);
    if (instance == nullptr) {
      return nullptr;
    }

    if (!index_->bind(guid, instance->id())) {
      (void)destroy(instance);
      return nullptr;
    }

    return instance;
  }

  /** @brief 按 id 销毁对象。销毁后已有裸指针、`objects::ptr<T>` 和类型化句柄都会失效。 */
  bool destroy(object_id id) noexcept;

  /** @brief 按对象指针销毁对象。指针必须来自该管理器。 */
  bool destroy(object* instance) noexcept;

  /**
   * @brief 延迟销毁对象。
   *
   * 成功后对象会立即从 manager 查找表和持久索引中移除，`handle` 不再可解析；对象内存会保留到
   * `flush_destroy_queue()`、`clear()` 或 manager 析构时释放。
   */
  bool destroy_later(object_id id);

  /** @brief 按对象指针延迟销毁对象。指针必须来自该管理器。 */
  bool destroy_later(object* instance);

  /** @brief 释放所有等待延迟销毁的对象。 */
  void flush_destroy_queue() noexcept;

  /** @brief 销毁所有仍存活对象。 */
  void clear() noexcept;

  /** @brief 获取当前对象身份句柄。未找到时返回无效句柄。 */
  [[nodiscard]] handle handle_of(object_id id) const;

  /** @brief 给当前由 manager 持有的对象绑定持久 GUID。 */
  bool bind_guid(object_id id, object_guid guid);

  /** @brief 查询当前对象绑定的持久 GUID。未关联索引或未绑定时返回 nil GUID。 */
  [[nodiscard]] object_guid guid_of(object_id id) const;

  /** @brief 获取当前对象的类型化身份句柄。未找到或类型不匹配时返回无效句柄。 */
  template <typename T> [[nodiscard]] typed_handle<T> handle_of(object_id id) const {
    const entry* stored = find_entry(id);
    if (stored == nullptr) {
      return {};
    }

    const type expected = objects::find<T>();
    if (!expected || !objects::is_a(stored->type, expected.id())) {
      return {};
    }

    return typed_handle<T>{handle{stored->id, stored->type}};
  }

private:
  struct entry {
    object_id id{};
    object_type_id type{};
    object* instance = nullptr;
    memory_resource* resource = nullptr;
  };

  using entry_map = core::hash_map<object_id, entry, object_id_hash>;
  using entry_queue = core::vector<entry>;

  [[nodiscard]] const entry* find_entry(object_id id) const;
  [[nodiscard]] entry* find_entry(object_id id);
  bool store(object* instance, object_type_id type, memory_resource* resource);
  void destroy_entry(const entry& stored) noexcept;

  memory_resource* resource_ = nullptr;
  object_index* index_ = nullptr;
  entry_map entries_;
  entry_queue pending_destroy_;
};

} // namespace caors::objects

#endif
