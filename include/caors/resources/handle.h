// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RESOURCES_HANDLE_H_
#define CAORS_RESOURCES_HANDLE_H_

#include "caors/memory/resource.h"
#include "caors/resources/resource.h"

#include <atomic>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace caors::resources {

template <typename T> class handle;
template <typename T> class weak_handle;

template <typename T, typename... Args>
[[nodiscard]] handle<T> make_handle(memory_resource* resource, Args&&... args);

namespace detail {

using handle_destroy_fn = void (*)(void* instance, memory_resource* resource) noexcept;

struct handle_control_block {
  handle_control_block(memory_resource* object_resource, memory_resource* control_resource,
                       handle_destroy_fn destroy) noexcept
      : object_resource(object_resource), control_resource(control_resource), destroy(destroy) {}

  std::atomic<std::size_t> strong_count{0};
  std::atomic<std::size_t> weak_count{1};
  memory_resource* object_resource = nullptr;
  memory_resource* control_resource = nullptr;
  handle_destroy_fn destroy = nullptr;
  void* instance = nullptr;
};

[[nodiscard]] inline handle_control_block*
try_make_handle_control_block(memory_resource* object_resource, memory_resource* control_resource,
                              handle_destroy_fn destroy) noexcept {
  memory_resource* resolved =
      control_resource != nullptr ? control_resource : get_default_memory_resource();
  void* storage =
      resolved->try_allocate(sizeof(handle_control_block), alignof(handle_control_block));
  if (storage == nullptr) {
    return nullptr;
  }

  return ::new (storage) handle_control_block{object_resource, resolved, destroy};
}

inline void destroy_handle_control_block(handle_control_block* control) noexcept {
  memory_resource* resource = control->control_resource;
  caors::destroy(control, resource);
}

inline void add_handle_ref(handle_control_block* control) noexcept {
  control->strong_count.fetch_add(1, std::memory_order_relaxed);
}

[[nodiscard]] inline bool try_add_handle_ref(handle_control_block* control) noexcept {
  std::size_t current = control->strong_count.load(std::memory_order_acquire);
  while (current != 0) {
    if (control->strong_count.compare_exchange_weak(current, current + 1, std::memory_order_acq_rel,
                                                    std::memory_order_acquire)) {
      return true;
    }
  }

  return false;
}

[[nodiscard]] inline bool release_handle_ref(handle_control_block* control) noexcept {
  std::size_t current = control->strong_count.load(std::memory_order_acquire);
  while (current != 0) {
    if (control->strong_count.compare_exchange_weak(current, current - 1, std::memory_order_acq_rel,
                                                    std::memory_order_acquire)) {
      return current == 1;
    }
  }

  return false;
}

inline void add_weak_handle_ref(handle_control_block* control) noexcept {
  control->weak_count.fetch_add(1, std::memory_order_relaxed);
}

inline void release_weak_handle_ref(handle_control_block* control) noexcept {
  if (control->weak_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    destroy_handle_control_block(control);
  }
}

[[nodiscard]] inline std::size_t handle_ref_count(const handle_control_block* control) noexcept {
  return control != nullptr ? control->strong_count.load(std::memory_order_acquire) : 0;
}

template <typename T>
void destroy_handle_instance(void* instance, memory_resource* resource) noexcept {
  caors::destroy(static_cast<T*>(instance), resource);
}

} // namespace detail

/**
 * @brief 资源强引用句柄。
 *
 * `handle<T>` 通过独立控制块持有资源对象。它适合运行期共享资源生命周期；稳定身份和序列化仍应使用
 * `resource_guid` / `resource_ref<T>`。
 */
template <typename T> class handle {
  static_assert(std::is_base_of_v<resource, T>, "T must derive from caors::resources::resource");
  static_assert(!std::is_const_v<T>, "T must not be const");

public:
  using resource_type = T;

  handle() noexcept = default;
  handle(std::nullptr_t) noexcept {}

  handle(const handle& other) noexcept : instance_(other.instance_), control_(other.control_) {
    retain();
  }

  template <typename U>
    requires(!std::is_same_v<U, T> && std::is_convertible_v<U*, T*>)
  handle(const handle<U>& other) noexcept : instance_(other.instance_), control_(other.control_) {
    retain();
  }

  handle(handle&& other) noexcept : instance_(other.instance_), control_(other.control_) {
    other.instance_ = nullptr;
    other.control_ = nullptr;
  }

  template <typename U>
    requires(!std::is_same_v<U, T> && std::is_convertible_v<U*, T*>)
  handle(handle<U>&& other) noexcept : instance_(other.instance_), control_(other.control_) {
    other.instance_ = nullptr;
    other.control_ = nullptr;
  }

  ~handle() { release(); }

  handle& operator=(const handle& other) noexcept {
    if (this == &other) {
      return *this;
    }

    handle copy{other};
    swap(copy);
    return *this;
  }

  handle& operator=(handle&& other) noexcept {
    if (this == &other) {
      return *this;
    }

    release();
    instance_ = other.instance_;
    control_ = other.control_;
    other.instance_ = nullptr;
    other.control_ = nullptr;
    return *this;
  }

  template <typename U>
    requires(!std::is_same_v<U, T> && std::is_convertible_v<U*, T*>)
  handle& operator=(const handle<U>& other) noexcept {
    handle copy{other};
    swap(copy);
    return *this;
  }

  template <typename U>
    requires(!std::is_same_v<U, T> && std::is_convertible_v<U*, T*>)
  handle& operator=(handle<U>&& other) noexcept {
    release();
    instance_ = other.instance_;
    control_ = other.control_;
    other.instance_ = nullptr;
    other.control_ = nullptr;
    return *this;
  }

  /** @brief 获取资源指针。返回值不转移所有权。 */
  [[nodiscard]] T* get() const noexcept { return instance_; }
  [[nodiscard]] T& operator*() const noexcept { return *instance_; }
  [[nodiscard]] T* operator->() const noexcept { return instance_; }
  [[nodiscard]] explicit operator bool() const noexcept { return instance_ != nullptr; }

  /** @brief 获取资源 GUID。空句柄返回 nil GUID。 */
  [[nodiscard]] resource_guid guid() const noexcept {
    return instance_ != nullptr ? instance_->guid() : resource_guid::nil();
  }

  /** @brief 当前资源的强引用数量。空句柄返回 0。 */
  [[nodiscard]] std::size_t use_count() const noexcept {
    return detail::handle_ref_count(control_);
  }

  /** @brief 当前是否只有一个强引用。 */
  [[nodiscard]] bool unique() const noexcept { return use_count() == 1; }

  /** @brief 释放当前强引用。若这是最后一个强引用，资源对象会被销毁。 */
  void reset(std::nullptr_t = nullptr) noexcept { release(); }

  /** @brief 交换两个强引用句柄。 */
  void swap(handle& other) noexcept {
    using std::swap;
    swap(instance_, other.instance_);
    swap(control_, other.control_);
  }

  friend bool operator==(const handle& lhs, std::nullptr_t) noexcept {
    return lhs.instance_ == nullptr;
  }

  friend bool operator==(std::nullptr_t, const handle& rhs) noexcept {
    return rhs.instance_ == nullptr;
  }

  friend bool operator!=(const handle& lhs, std::nullptr_t) noexcept {
    return lhs.instance_ != nullptr;
  }

  friend bool operator!=(std::nullptr_t, const handle& rhs) noexcept {
    return rhs.instance_ != nullptr;
  }

private:
  template <typename U> friend class handle;
  template <typename U> friend class weak_handle;
  template <typename U, typename... Args>
  friend handle<U> make_handle(memory_resource* resource, Args&&... args);

  struct adopt_tag {};
  struct retained_tag {};

  handle(T* instance, detail::handle_control_block* control, adopt_tag) noexcept
      : instance_(instance), control_(control) {
    retain();
  }

  handle(T* instance, detail::handle_control_block* control, retained_tag) noexcept
      : instance_(instance), control_(control) {}

  void retain() noexcept {
    if (control_ != nullptr) {
      detail::add_handle_ref(control_);
    }
  }

  void release() noexcept {
    T* instance = instance_;
    detail::handle_control_block* control = control_;
    instance_ = nullptr;
    control_ = nullptr;

    if (control != nullptr && detail::release_handle_ref(control)) {
      void* stored_instance = control->instance != nullptr ? control->instance : instance;
      control->instance = nullptr;
      control->destroy(stored_instance, control->object_resource);
      detail::release_weak_handle_ref(control);
    }
  }

  T* instance_ = nullptr;
  detail::handle_control_block* control_ = nullptr;
};

template <typename T> void swap(handle<T>& lhs, handle<T>& rhs) noexcept { lhs.swap(rhs); }

/**
 * @brief 资源弱引用句柄。
 *
 * `weak_handle<T>` 不延长资源生命周期。资源仍存活时可通过 `lock()` 尝试提升为 `handle<T>`。
 */
template <typename T> class weak_handle {
  static_assert(std::is_base_of_v<resource, T>, "T must derive from caors::resources::resource");
  static_assert(!std::is_const_v<T>, "T must not be const");

public:
  weak_handle() noexcept = default;
  weak_handle(std::nullptr_t) noexcept {}

  weak_handle(const handle<T>& strong) noexcept
      : instance_(strong.instance_), control_(strong.control_) {
    retain();
  }

  template <typename U>
    requires(!std::is_same_v<U, T> && std::is_convertible_v<U*, T*>)
  weak_handle(const handle<U>& strong) noexcept
      : instance_(strong.instance_), control_(strong.control_) {
    retain();
  }

  weak_handle(const weak_handle& other) noexcept
      : instance_(other.instance_), control_(other.control_) {
    retain();
  }

  template <typename U>
    requires(!std::is_same_v<U, T> && std::is_convertible_v<U*, T*>)
  weak_handle(const weak_handle<U>& other) noexcept
      : instance_(other.instance_), control_(other.control_) {
    retain();
  }

  weak_handle(weak_handle&& other) noexcept : instance_(other.instance_), control_(other.control_) {
    other.instance_ = nullptr;
    other.control_ = nullptr;
  }

  template <typename U>
    requires(!std::is_same_v<U, T> && std::is_convertible_v<U*, T*>)
  weak_handle(weak_handle<U>&& other) noexcept
      : instance_(other.instance_), control_(other.control_) {
    other.instance_ = nullptr;
    other.control_ = nullptr;
  }

  ~weak_handle() { release(); }

  weak_handle& operator=(const handle<T>& strong) noexcept {
    weak_handle copy{strong};
    swap(copy);
    return *this;
  }

  template <typename U>
    requires(!std::is_same_v<U, T> && std::is_convertible_v<U*, T*>)
  weak_handle& operator=(const handle<U>& strong) noexcept {
    weak_handle copy{strong};
    swap(copy);
    return *this;
  }

  weak_handle& operator=(const weak_handle& other) noexcept {
    if (this == &other) {
      return *this;
    }

    weak_handle copy{other};
    swap(copy);
    return *this;
  }

  weak_handle& operator=(weak_handle&& other) noexcept {
    if (this == &other) {
      return *this;
    }

    release();
    instance_ = other.instance_;
    control_ = other.control_;
    other.instance_ = nullptr;
    other.control_ = nullptr;
    return *this;
  }

  template <typename U>
    requires(!std::is_same_v<U, T> && std::is_convertible_v<U*, T*>)
  weak_handle& operator=(const weak_handle<U>& other) noexcept {
    weak_handle copy{other};
    swap(copy);
    return *this;
  }

  template <typename U>
    requires(!std::is_same_v<U, T> && std::is_convertible_v<U*, T*>)
  weak_handle& operator=(weak_handle<U>&& other) noexcept {
    release();
    instance_ = other.instance_;
    control_ = other.control_;
    other.instance_ = nullptr;
    other.control_ = nullptr;
    return *this;
  }

  /** @brief 当前资源的强引用数量。空引用或已失效引用返回 0。 */
  [[nodiscard]] std::size_t use_count() const noexcept {
    return detail::handle_ref_count(control_);
  }

  /** @brief 判断资源是否已经失效。 */
  [[nodiscard]] bool expired() const noexcept { return use_count() == 0; }

  /** @brief 尝试提升为强引用。资源已经销毁时返回空句柄。 */
  [[nodiscard]] handle<T> lock() const noexcept {
    if (control_ == nullptr || !detail::try_add_handle_ref(control_)) {
      return {};
    }

    return handle<T>{instance_, control_, typename handle<T>::retained_tag{}};
  }

  /** @brief 判断当前是否仍能提升为强引用。 */
  [[nodiscard]] explicit operator bool() const noexcept { return !expired(); }

  /** @brief 释放当前弱引用。 */
  void reset(std::nullptr_t = nullptr) noexcept { release(); }

  /** @brief 交换两个弱引用句柄。 */
  void swap(weak_handle& other) noexcept {
    using std::swap;
    swap(instance_, other.instance_);
    swap(control_, other.control_);
  }

  friend bool operator==(const weak_handle& lhs, std::nullptr_t) noexcept {
    return lhs.control_ == nullptr || lhs.expired();
  }

  friend bool operator==(std::nullptr_t, const weak_handle& rhs) noexcept {
    return rhs.control_ == nullptr || rhs.expired();
  }

  friend bool operator!=(const weak_handle& lhs, std::nullptr_t) noexcept {
    return !(lhs == nullptr);
  }

  friend bool operator!=(std::nullptr_t, const weak_handle& rhs) noexcept {
    return !(nullptr == rhs);
  }

private:
  template <typename U> friend class weak_handle;

  void retain() noexcept {
    if (control_ != nullptr) {
      detail::add_weak_handle_ref(control_);
    }
  }

  void release() noexcept {
    detail::handle_control_block* control = control_;
    instance_ = nullptr;
    control_ = nullptr;

    if (control != nullptr) {
      detail::release_weak_handle_ref(control);
    }
  }

  T* instance_ = nullptr;
  detail::handle_control_block* control_ = nullptr;
};

template <typename T> void swap(weak_handle<T>& lhs, weak_handle<T>& rhs) noexcept {
  lhs.swap(rhs);
}

/** @brief 使用指定内存资源创建资源对象并返回强引用句柄。 */
template <typename T, typename... Args>
[[nodiscard]] handle<T> make_handle(memory_resource* resource, Args&&... args) {
  static_assert(std::is_base_of_v<resources::resource, T>,
                "T must derive from caors::resources::resource");

  memory_resource* resolved = resource != nullptr ? resource : get_default_memory_resource();
  auto* control = detail::try_make_handle_control_block(resolved, resolved,
                                                        &detail::destroy_handle_instance<T>);
  if (control == nullptr) {
    return {};
  }

#if CAORS_HAS_EXCEPTIONS
  try {
#endif
    T* instance = caors::make<T>(resolved, std::forward<Args>(args)...);
    control->instance = instance;
    return handle<T>{instance, control, typename handle<T>::adopt_tag{}};
#if CAORS_HAS_EXCEPTIONS
  } catch (...) {
    detail::destroy_handle_control_block(control);
    throw;
  }
#endif

  return {};
}

/** @brief 使用默认内存资源创建资源对象并返回强引用句柄。 */
template <typename T> [[nodiscard]] handle<T> make_handle() {
  return make_handle<T>(static_cast<memory_resource*>(nullptr));
}

/** @brief 使用默认内存资源和构造参数创建资源对象并返回强引用句柄。 */
template <typename T, typename First, typename... Rest>
  requires(!std::is_convertible_v<std::remove_cvref_t<First>, memory_resource*>)
[[nodiscard]] handle<T> make_handle(First&& first, Rest&&... rest) {
  return make_handle<T>(static_cast<memory_resource*>(nullptr), std::forward<First>(first),
                        std::forward<Rest>(rest)...);
}

} // namespace caors::resources

namespace caors {

template <typename T> using resource_handle = resources::handle<T>;
template <typename T> using resource_weak_handle = resources::weak_handle<T>;

using resources::make_handle;

} // namespace caors

#endif
