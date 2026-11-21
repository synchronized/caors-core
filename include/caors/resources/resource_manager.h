// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#ifndef CAORS_RESOURCES_RESOURCE_MANAGER_H_
#define CAORS_RESOURCES_RESOURCE_MANAGER_H_

#include "caors/resources/async.h"
#include <string>
#include <typeindex>

namespace caors::resources {
/** @brief 宿主执行环境；管理器不创建线程，执行器及分配器须覆盖全部在途操作和结果。 */
struct resource_execution_options {
  tasks::executor worker;
  tasks::executor completion;
  memory_resource* resource = nullptr;
};

/**
 * @brief 按宿主键管理异步加载与类型化缓存，不创建文件来源或项目装配。
 * @note 注册、查询、请求发起和 close 必须与完成发布统一串行化；loader 可从宿主线程交付结果。
 * loader 为借用，须覆盖真实在途清理；其实现模块还须覆盖结果对象的析构。
 * close 不阻塞，已发操作持有类型存储，可在管理器销毁后完成。
 */
template <typename Key = std::string, typename Hash = std::hash<Key>,
          typename Equal = std::equal_to<Key>>
class resource_manager final {
  struct entry_base {
    virtual ~entry_base() = default;
  };
  template <typename T> struct entry final : entry_base {
    entry(memory_resource* resource, loader<T, Key>& source, Hash hash, Equal equal)
        : values(resource, hash, equal), requests(resource, std::move(hash), std::move(equal)),
          source(&source) {}
    cache<T, Key, Hash, Equal> values;
    load_request_table<T, Key, Hash, Equal> requests;
    loader<T, Key>* source;
  };
  using entry_map = core::hash_map<std::type_index, std::shared_ptr<entry_base>>;

public:
  explicit resource_manager(resource_execution_options options, Hash hash = {}, Equal equal = {})
      : options_(resolve_options(std::move(options))), hash_(std::move(hash)),
        equal_(std::move(equal)), entries_(typename entry_map::allocator_type{resource()}) {}
  resource_manager(const resource_manager&) = delete;
  resource_manager& operator=(const resource_manager&) = delete;
  resource_manager(resource_manager&&) = delete;
  resource_manager& operator=(resource_manager&&) = delete;
  ~resource_manager() { close(); }

  /** @brief 注册借用 loader，同类型重复注册返回 already_exists。 */
  template <typename T> [[nodiscard]] error register_loader(loader<T, Key>& source) {
    if (closed_)
      return {error_domain::resources, errc::cancelled};
    auto key = std::type_index(typeid(T));
    if (entries_.contains(key))
      return {error_domain::resources, errc::already_exists};
    auto value = std::allocate_shared<entry<T>>(allocator<entry<T>>{resource()}, resource(), source,
                                                hash_, equal_);
    entries_.emplace(key, std::move(value));
    return {};
  }
  /** @brief 加载或加入同键在途操作；缓存命中直接返回已有句柄。 */
  template <typename T> [[nodiscard]] load_request<T> request_load(Key key) {
    auto value = lookup<T>();
    if (!value)
      return load_request<T>::failed(
          {error_domain::resources, closed_ ? errc::cancelled : errc::not_found}, resource());
    if (!options_.worker || !options_.completion)
      return load_request<T>::failed({error_domain::resources, errc::invalid_argument}, resource());
    return detail::request_load_into_impl<T>(value->values, *value->source, std::move(key),
                                             options_.worker, options_.completion, &value->requests,
                                             true, {}, {}, value);
  }
  /** @brief 独立重载，最新请求决定缓存绑定；失败保留旧值。 */
  template <typename T> [[nodiscard]] load_request<T> request_reload(Key key) {
    auto value = lookup<T>();
    if (!value)
      return load_request<T>::failed(
          {error_domain::resources, closed_ ? errc::cancelled : errc::not_found}, resource());
    if (!options_.worker || !options_.completion)
      return load_request<T>::failed({error_domain::resources, errc::invalid_argument}, resource());
    return detail::request_reload_into_impl<T>(value->values, *value->source, std::move(key),
                                               options_.worker, options_.completion,
                                               &value->requests, value);
  }
  /** @brief 查询当前缓存句柄，不启动加载。 */
  template <typename T> [[nodiscard]] handle<T> find(const Key& key) const {
    auto value = lookup<T>();
    return value ? value->values.find_handle(key) : handle<T>{};
  }
  /** @brief 按引用的宿主键解析当前绑定；未注册或关闭时清除指针并保留键。 */
  template <typename T> [[nodiscard]] bool resolve(ref<T, Key>& reference) const {
    auto value = lookup<T>();
    if (!value) {
      reference.unresolve();
      return false;
    }
    return value->values.resolve(reference);
  }
  /** @brief 检查引用是否仍属于当前类型存储及其绑定代次。 */
  template <typename T> [[nodiscard]] bool is_current(const ref<T, Key>& reference) const {
    auto value = lookup<T>();
    return value && value->values.is_current(reference);
  }
  /** @brief 停止新请求并释放管理器绑定；不取消其他持有者的在途订阅。 */
  void close() noexcept {
    if (closed_)
      return;
    closed_ = true;
    entry_map retired{typename entry_map::allocator_type{resource()}};
    retired.swap(entries_);
  }
  [[nodiscard]] bool closed() const noexcept { return closed_; }
  [[nodiscard]] memory_resource* resource() const noexcept { return options_.resource; }

private:
  static resource_execution_options resolve_options(resource_execution_options options) {
    if (!options.resource)
      options.resource = get_default_memory_resource();
    return options;
  }
  template <typename T> std::shared_ptr<entry<T>> lookup() const {
    if (closed_)
      return {};
    auto found = entries_.find(std::type_index(typeid(T)));
    return found == entries_.end() ? nullptr : std::static_pointer_cast<entry<T>>(found->second);
  }
  resource_execution_options options_;
  Hash hash_;
  Equal equal_;
  entry_map entries_;
  bool closed_ = false;
};
} // namespace caors::resources
#endif
