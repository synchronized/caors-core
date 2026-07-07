// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_EVENTS_DISPATCHER_H_
#define CAORS_EVENTS_DISPATCHER_H_

#include "caors/core/containers.h"
#include "caors/events/sink.h"
#include "caors/memory/resource.h"

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace caors::events {

namespace detail {

using event_key = const void*;

template <typename T> struct event_type_key_storage {
  inline static const int key = 0;
};

template <typename T> [[nodiscard]] event_key event_type_key_of() noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  return &event_type_key_storage<type>::key;
}

} // namespace detail

/**
 * @brief 按事件类型存储并批量派发的同步 dispatcher。
 *
 * `dispatcher`
 * 适合把事件先入队，再在明确的更新时间点批量派发。第一版只保证单线程使用，不做跨线程同步、
 * 优先级、异步调度或监听对象生命周期管理。
 */
class dispatcher final {
public:
  explicit dispatcher(memory_resource* resource = nullptr)
      : resource_(resource != nullptr ? resource : get_default_memory_resource()),
        queues_(typename queue_map::allocator_type{resource_}),
        queue_order_(typename queue_order::allocator_type{resource_}) {}

  dispatcher(const dispatcher&) = delete;
  dispatcher& operator=(const dispatcher&) = delete;
  dispatcher(dispatcher&&) = delete;
  dispatcher& operator=(dispatcher&&) = delete;

  /** @brief 查询 dispatcher 内部容器和队列使用的内存资源。 */
  [[nodiscard]] memory_resource* resource() const noexcept { return resource_; }

  /** @brief 返回所有事件类型队列中的待派发事件总数。 */
  [[nodiscard]] std::size_t size() const noexcept {
    std::size_t count = 0;
    for (const auto& [_, queue] : queues_) {
      count += queue->size();
    }
    return count;
  }

  /** @brief 返回指定事件类型队列中的待派发事件数量。 */
  template <typename Event> [[nodiscard]] std::size_t size() const noexcept {
    const event_queue<Event>* queue = find_queue<Event>();
    return queue != nullptr ? queue->size() : 0;
  }

  /** @brief 判断是否没有任何待派发事件。 */
  [[nodiscard]] bool empty() const noexcept { return size() == 0; }

  /** @brief 判断指定事件类型队列是否没有待派发事件。 */
  template <typename Event> [[nodiscard]] bool empty() const noexcept { return size<Event>() == 0; }

  /** @brief 获取指定事件类型的订阅入口。 */
  template <typename Event> [[nodiscard]] events::sink<void(const Event&)> sink() {
    return assure_queue<Event>().sink();
  }

  /** @brief 将事件复制入队，等待后续 `update()` 派发。 */
  template <typename Event> void enqueue(const Event& event) {
    assure_queue<Event>().enqueue(event);
  }

  /** @brief 将事件移动入队，等待后续 `update()` 派发。 */
  template <typename Event> void enqueue(Event&& event) {
    assure_queue<Event>().enqueue(std::move(event));
  }

  /** @brief 在对应事件队列中原地构造事件。 */
  template <typename Event, typename... Args> Event& emplace(Args&&... args) {
    return assure_queue<Event>().emplace(std::forward<Args>(args)...);
  }

  /** @brief 立即同步触发一个事件，不经过队列。 */
  template <typename Event> void trigger(const Event& event) {
    assure_queue<Event>().trigger(event);
  }

  /** @brief 派发指定事件类型当前已经入队的事件。派发期间新增事件会留到下一次 update。 */
  template <typename Event> void update() {
    event_queue<Event>* queue = find_queue<Event>();
    if (queue != nullptr) {
      queue->update();
    }
  }

  /** @brief 按事件类型首次使用顺序派发全部当前已经入队的事件。 */
  void update() {
    const std::size_t initial_size = queue_order_.size();
    for (std::size_t index = 0; index < initial_size; ++index) {
      const auto position = queues_.find(queue_order_[index]);
      if (position != queues_.end()) {
        position->second->update();
      }
    }
  }

  /** @brief 丢弃指定事件类型的待派发事件，不触发监听者。 */
  template <typename Event> void clear() noexcept {
    event_queue<Event>* queue = find_queue<Event>();
    if (queue != nullptr) {
      queue->clear();
    }
  }

  /** @brief 丢弃全部待派发事件，不触发监听者。 */
  void clear() noexcept {
    for (auto& [_, queue] : queues_) {
      queue->clear();
    }
  }

private:
  class queue_base {
  public:
    virtual ~queue_base() = default;
    virtual void update() = 0;
    virtual void clear() noexcept = 0;
    [[nodiscard]] virtual std::size_t size() const noexcept = 0;
  };

  template <typename Event> class event_queue final : public queue_base {
  public:
    explicit event_queue(memory_resource* resource)
        : resource_(resource != nullptr ? resource : get_default_memory_resource()),
          events_(typename event_list::allocator_type{resource_}), listeners_(resource_) {}

    [[nodiscard]] events::sink<void(const Event&)> sink() noexcept {
      return events::sink<void(const Event&)>{listeners_};
    }

    void enqueue(const Event& event) { events_.push_back(event); }
    void enqueue(Event&& event) { events_.push_back(std::move(event)); }

    template <typename... Args> Event& emplace(Args&&... args) {
      return events_.emplace_back(std::forward<Args>(args)...);
    }

    void trigger(const Event& event) { listeners_.emit(event); }

    void update() override {
      event_list pending{typename event_list::allocator_type{resource_}};
      pending.swap(events_);
      for (const Event& event : pending) {
        listeners_.emit(event);
      }
    }

    void clear() noexcept override { events_.clear(); }
    [[nodiscard]] std::size_t size() const noexcept override { return events_.size(); }

  private:
    using event_list = core::vector<Event>;

    memory_resource* resource_ = nullptr;
    event_list events_;
    signal<void(const Event&)> listeners_;
  };

  struct queue_deleter {
    using destroy_fn = void (*)(memory_resource* resource, queue_base* queue) noexcept;

    void operator()(queue_base* queue) const noexcept {
      if (queue != nullptr && destroy != nullptr) {
        destroy(resource, queue);
      }
    }

    memory_resource* resource = nullptr;
    destroy_fn destroy = nullptr;
  };

  using queue_ptr = std::unique_ptr<queue_base, queue_deleter>;
  using queue_map = core::hash_map<detail::event_key, queue_ptr>;
  using queue_order = core::vector<detail::event_key>;

  template <typename Event>
  [[nodiscard]] static queue_ptr make_queue_ptr(memory_resource* resource) {
    auto* queue = make<event_queue<Event>>(resource, resource);
    return queue_ptr{queue, queue_deleter{resource, &destroy_queue<Event>}};
  }

  template <typename Event>
  static void destroy_queue(memory_resource* resource, queue_base* queue) noexcept {
    destroy(static_cast<event_queue<Event>*>(queue), resource);
  }

  template <typename Event> [[nodiscard]] event_queue<Event>* find_queue() noexcept {
    const auto position = queues_.find(detail::event_type_key_of<Event>());
    return position != queues_.end() ? static_cast<event_queue<Event>*>(position->second.get())
                                     : nullptr;
  }

  template <typename Event> [[nodiscard]] const event_queue<Event>* find_queue() const noexcept {
    const auto position = queues_.find(detail::event_type_key_of<Event>());
    return position != queues_.end()
               ? static_cast<const event_queue<Event>*>(position->second.get())
               : nullptr;
  }

  template <typename Event> [[nodiscard]] event_queue<Event>& assure_queue() {
    const detail::event_key key = detail::event_type_key_of<Event>();
    const auto found = queues_.find(key);
    if (found != queues_.end()) {
      return *static_cast<event_queue<Event>*>(found->second.get());
    }

    queue_ptr queue = make_queue_ptr<Event>(resource_);
    auto* raw = static_cast<event_queue<Event>*>(queue.get());
    queues_.emplace(key, std::move(queue));
    queue_order_.push_back(key);
    return *raw;
  }

  memory_resource* resource_ = nullptr;
  queue_map queues_;
  queue_order queue_order_;
};

} // namespace caors::events

namespace caors {

using event_dispatcher = events::dispatcher;

} // namespace caors

#endif
