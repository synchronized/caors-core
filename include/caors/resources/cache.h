// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESOURCES_CACHE_H_
#define CAORS_RESOURCES_CACHE_H_

#include "caors/core/containers.h"
#include "caors/memory/unique_ptr.h"
#include "caors/resources/handle.h"
#include "caors/resources/manager.h"

#include <cstddef>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>

namespace caors::resources {
namespace detail {
template <typename T> class load_request_access;
struct refresh_selection {
  std::mutex mutex;
  std::weak_ptr<void> request;
};
}

/**
 * @brief 按宿主键拥有对象的缓存；每个实例具有独立的类型与代次空间。
 * @note 不规范化键，不写入对象身份；Key 必须拥有其数据。访问与析构须由调用方同步。
 * resource 须覆盖缓存、句柄、引用身份凭据和弱控制块的清理。Hash/Equal 必须满足标准容器契约。
 */
template <typename T, typename Key, typename Hash, typename Equal> class cache final {
  struct entry {
    handle<T> value;
    resource_generation generation;
  };
  using map_type = core::hash_map<Key, entry, Hash, Equal>;
  static_assert(!std::is_same_v<Key, std::string_view>, "Key must own its data");

public:
  using resource_type = T;
  using key_type = Key;
  static constexpr bool uses_embedded_identity = false;
  explicit cache(memory_resource* resource = nullptr, Hash hash = {}, Equal equal = {})
      : resource_(resource ? resource : get_default_memory_resource()),
        identity_(std::allocate_shared<unsigned char>(allocator<unsigned char>{resource_}, 0)),
        values_(0, std::move(hash), std::move(equal),
                typename map_type::allocator_type{resource_}) {}
  cache(const cache&) = delete;
  cache& operator=(const cache&) = delete;
  cache(cache&&) = delete;
  cache& operator=(cache&&) = delete;
  [[nodiscard]] memory_resource* resource() const noexcept { return resource_; }
  [[nodiscard]] std::size_t size() const noexcept { return values_.size(); }
  [[nodiscard]] bool empty() const noexcept { return values_.empty(); }
  [[nodiscard]] handle<T> find_handle(const Key& key) const {
    auto found = values_.find(key);
    return found == values_.end() ? handle<T>{} : found->second.value;
  }
  [[nodiscard]] T* find(const Key& key) const { return find_handle(key).get(); }
  [[nodiscard]] bool contains(const Key& key) const { return values_.contains(key); }
  [[nodiscard]] resource_generation generation_of(const Key& key) const {
    auto found = values_.find(key);
    return found == values_.end() ? 0 : found->second.generation;
  }
  /** @brief 新建键绑定；空对象或已有键失败，失败不替换现有对象。 */
  bool insert(Key key, handle<T> value) {
    if constexpr (std::is_same_v<Key, resource_guid>) {
      if (!key)
        return false;
    }
    if (!value || contains(key))
      return false;
    auto inserted =
        values_.emplace(std::move(key), entry{std::move(value), next_generation_}).second;
    if (inserted)
      advance_generation();
    return inserted;
  }
  bool insert(Key key, memory::unique_ptr<T> value) {
    return insert(std::move(key), adopt_handle(std::move(value), resource_));
  }
  template <typename... Args> [[nodiscard]] handle<T> create_handle(Key key, Args&&... args) {
    if (contains(key))
      return {};
    auto value = make_handle<T>(resource_, std::forward<Args>(args)...);
    return insert(std::move(key), value) ? value : handle<T>{};
  }
  template <typename... Args> [[nodiscard]] T* create(Key key, Args&&... args) {
    return create_handle(std::move(key), std::forward<Args>(args)...).get();
  }
  /** @brief 替换已有键；先发布新对象与代次，再释放旧句柄，旧外部句柄仍有效。 */
  bool replace(const Key& key, handle<T> value) {
    if (!value)
      return false;
    auto found = values_.find(key);
    if (found == values_.end())
      return false;
    auto retired = std::move(found->second.value);
    found->second = entry{std::move(value), next_generation_};
    advance_generation();
    return true;
  }
  bool replace(const Key& key, memory::unique_ptr<T> value) {
    return replace(key, adopt_handle(std::move(value), resource_));
  }
  [[nodiscard]] handle<T> release(const Key& key) {
    auto found = values_.find(key);
    if (found == values_.end())
      return {};
    auto value = std::move(found->second.value);
    values_.erase(found);
    return value;
  }
  bool erase(const Key& key) { return static_cast<bool>(release(key)); }
  /** @brief 先移除全部绑定再释放对象；保留代次计数，避免旧引用误认新对象。 */
  void clear() {
    map_type retired(0, values_.hash_function(), values_.key_eq(), values_.get_allocator());
    retired.swap(values_);
  }
  template <typename U>
    requires std::is_convertible_v<T*, U*>
  [[nodiscard]] bool resolve(ref<U, Key>& reference) const {
    if (!reference.key())
      return false;
    auto found = values_.find(*reference.key());
    if (found == values_.end()) {
      reference.unresolve();
      return false;
    }
    if (!reference.resolve(found->second.value.get(), found->second.generation))
      return false;
    if constexpr (requires { reference.owner_; })
      reference.owner_ = identity_;
    return true;
  }
  template <typename U>
    requires std::is_convertible_v<T*, U*>
  [[nodiscard]] bool is_current(const ref<U, Key>& reference) const {
    if (!reference.key() || !reference.has_generation())
      return false;
    if constexpr (requires { reference.owner_; }) {
      if (reference.owner_.owner_before(identity_) || identity_.owner_before(reference.owner_))
        return false;
    }
    auto found = values_.find(*reference.key());
    return found != values_.end() && found->second.generation == reference.generation() &&
           found->second.value.get() == reference.get();
  }

private:
  friend class detail::load_request_access<T>;
  using publication_map =
      core::hash_map<Key, std::shared_ptr<detail::refresh_selection>, Hash, Equal>;
  void advance_generation() noexcept {
    if (++next_generation_ == 0)
      ++next_generation_;
  }
  memory_resource* resource_;
  std::shared_ptr<const void> identity_;
  map_type values_;
  resource_generation next_generation_ = 1;
  publication_map publications_{0, values_.hash_function(), values_.key_eq(),
                                typename publication_map::allocator_type{resource_}};
};

/**
 * @brief 拥有式资源缓存。
 *
 * `cache<T>` 按 `resource_guid` 持有 `T` 资源对象，并同步维护一个非拥有 `resources::manager`
 * 作为查询和引用解析索引。它不负责从文件加载资源，也不做异步调度；加载策略后续由 loader 层提供。
 */
template <typename T>
  requires std::is_base_of_v<resources::resource, T>
class cache<T, resource_guid, std::hash<resource_guid>, std::equal_to<resource_guid>> final {
  static_assert(std::is_base_of_v<resources::resource, T>,
                "T must derive from caors::resources::resource");

public:
  using resource_type = T;
  using key_type = resource_guid;
  static constexpr bool uses_embedded_identity = true;

  explicit cache(memory_resource* resource = nullptr, std::hash<resource_guid> = {},
                 std::equal_to<resource_guid> = {})
      : resource_(resource != nullptr ? resource : get_default_memory_resource()),
        index_(resource_), resources_by_guid_(typename resource_map::allocator_type{resource_}),
        publications_(typename publication_map::allocator_type{resource_}) {}

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

    if (!insert(owned))
      return {};

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
    const auto [_, inserted] = resources_by_guid_.emplace(guid, owned);
    if (!inserted)
      return false;
    struct rollback {
      resource_map& values;
      resource_guid guid;
      bool committed = false;
      ~rollback() {
        if (!committed)
          values.erase(guid);
      }
    } guard{resources_by_guid_, guid};
    if (!index_.bind(guid, owned.get()))
      return false;
    guard.committed = true;

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

    if (!index_.replace_binding(guid, owned.get()))
      return false;
    // 完成两张索引的切换后再析构旧对象，避免析构重入观察到半发布状态。
    auto retired = std::move(position->second);
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
  friend class detail::load_request_access<T>;
  using publication_map =
      core::hash_map<resource_guid, std::shared_ptr<detail::refresh_selection>, resource_guid_hash>;
  using resource_map = core::hash_map<resource_guid, handle<resource_type>, resource_guid_hash>;

  memory_resource* resource_ = nullptr;
  manager index_;
  resource_map resources_by_guid_;
  // 每个刷新过的 GUID 保留当前凭据，旧操作全部结束前不能因 erase/clear 恢复其发布资格。
  publication_map publications_;
};

} // namespace caors::resources

namespace caors {

template <typename T, typename Key = resource_guid, typename Hash = std::hash<Key>,
          typename Equal = std::equal_to<Key>>
using resource_cache = resources::cache<T, Key, Hash, Equal>;

} // namespace caors

#endif
