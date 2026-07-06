// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_REF_COUNTED_H_
#define CAORS_OBJECT_REF_COUNTED_H_

#include "caors/object/factory.h"
#include "caors/traits/wrapper.h"

#include <atomic>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace caors::objects {

template <typename T> class ref;
template <typename T> class weak_ref;

template <typename T, typename... Args>
[[nodiscard]] ref<T> make_ref(memory_resource* resource, Args&&... args);

namespace detail {

using ref_destroy_fn = void (*)(void* instance, memory_resource* resource) noexcept;

struct ref_control_block {
  ref_control_block(memory_resource* object_resource, memory_resource* control_resource,
                    ref_destroy_fn destroy) noexcept
      : object_resource(object_resource), control_resource(control_resource), destroy(destroy) {}

  std::atomic<std::size_t> strong_count{0};
  std::atomic<std::size_t> weak_count{1};
  memory_resource* object_resource = nullptr;
  memory_resource* control_resource = nullptr;
  ref_destroy_fn destroy = nullptr;
  void* instance = nullptr;
};

inline void destroy_ref_control_block(ref_control_block* control) noexcept {
  memory_resource* resource = control->control_resource;
  caors::destroy(control, resource);
}

inline void add_ref(ref_control_block* control) noexcept {
  control->strong_count.fetch_add(1, std::memory_order_relaxed);
}

[[nodiscard]] inline bool try_add_ref(ref_control_block* control) noexcept {
  std::size_t current = control->strong_count.load(std::memory_order_acquire);
  while (current != 0) {
    if (control->strong_count.compare_exchange_weak(current, current + 1, std::memory_order_acq_rel,
                                                    std::memory_order_acquire)) {
      return true;
    }
  }

  return false;
}

[[nodiscard]] inline bool release_ref(ref_control_block* control) noexcept {
  std::size_t current = control->strong_count.load(std::memory_order_acquire);
  while (current != 0) {
    if (control->strong_count.compare_exchange_weak(current, current - 1, std::memory_order_acq_rel,
                                                    std::memory_order_acquire)) {
      return current == 1;
    }
  }

  return false;
}

inline void add_weak_ref(ref_control_block* control) noexcept {
  control->weak_count.fetch_add(1, std::memory_order_relaxed);
}

inline void release_weak_ref(ref_control_block* control) noexcept {
  if (control->weak_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    destroy_ref_control_block(control);
  }
}

[[nodiscard]] inline std::size_t ref_count(const ref_control_block* control) noexcept {
  return control != nullptr ? control->strong_count.load(std::memory_order_acquire) : 0;
}

template <typename T>
void destroy_ref_counted_instance(void* instance, memory_resource* resource) noexcept {
  objects::destroy(static_cast<T*>(instance), resource);
}

} // namespace detail

/**
 * @brief 可选侵入式引用计数基类。
 *
 * 继承该类型的对象可以由 `objects::ref<T>` 和 `objects::weak_ref<T>` 管理引用计数。计数位于
 * 独立控制块中，因此对象销毁后弱引用仍可安全判断失效；这不表示对象内部状态具备线程安全访问能力。
 */
class ref_counted {
public:
  ref_counted() noexcept = default;
  ref_counted(const ref_counted&) noexcept {}
  ref_counted& operator=(const ref_counted&) noexcept { return *this; }

  /** @brief 当前强引用数量。 */
  [[nodiscard]] std::size_t use_count() const noexcept { return detail::ref_count(control_); }

  /** @brief 当前是否只有一个强引用。 */
  [[nodiscard]] bool unique() const noexcept { return use_count() == 1; }

protected:
  ~ref_counted() = default;

private:
  template <typename T> friend class ref;
  template <typename T> friend class weak_ref;
  template <typename T, typename... Args>
  friend ref<T> make_ref(memory_resource* resource, Args&&... args);

  void bind_control_block(detail::ref_control_block* control) noexcept { control_ = control; }

  detail::ref_control_block* control_ = nullptr;
};

/**
 * @brief 引用计数对象的强引用。
 *
 * `ref<T>` 只管理由 `objects::make_ref<T>()` 创建的对象。它不会接管 `objects::manager`
 * 已经拥有的对象，也不会改变 `objects::ptr<T>`、`objects::handle` 或
 * `objects::typed_handle<T>` 的非拥有语义。
 */
template <typename T> class ref {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");
  static_assert(std::is_base_of_v<ref_counted, T>,
                "T must derive from caors::objects::ref_counted");
  static_assert(!std::is_const_v<T>, "T must not be const");

public:
  ref() noexcept = default;
  ref(std::nullptr_t) noexcept {}

  ref(const ref& other) noexcept : instance_(other.instance_), control_(other.control_) {
    retain();
  }

  ref(ref&& other) noexcept : instance_(other.instance_), control_(other.control_) {
    other.instance_ = nullptr;
    other.control_ = nullptr;
  }

  ~ref() { release(); }

  ref& operator=(const ref& other) noexcept {
    if (this == &other) {
      return *this;
    }

    ref copy{other};
    swap(copy);
    return *this;
  }

  ref& operator=(ref&& other) noexcept {
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

  /** @brief 获取对象指针。返回值不转移所有权。 */
  [[nodiscard]] T* get() const noexcept { return instance_; }
  [[nodiscard]] T& operator*() const noexcept { return *instance_; }
  [[nodiscard]] T* operator->() const noexcept { return instance_; }
  [[nodiscard]] explicit operator bool() const noexcept { return instance_ != nullptr; }

  /** @brief 当前对象的强引用数量。空引用返回 0。 */
  [[nodiscard]] std::size_t use_count() const noexcept {
    return instance_ != nullptr ? instance_->use_count() : 0;
  }

  /** @brief 当前是否只有一个强引用。 */
  [[nodiscard]] bool unique() const noexcept { return use_count() == 1; }

  /** @brief 释放当前强引用。若这是最后一个强引用，对象会被销毁。 */
  void reset(std::nullptr_t = nullptr) noexcept { release(); }

  /** @brief 交换两个强引用。 */
  void swap(ref& other) noexcept {
    using std::swap;
    swap(instance_, other.instance_);
    swap(control_, other.control_);
  }

  friend bool operator==(const ref& lhs, std::nullptr_t) noexcept {
    return lhs.instance_ == nullptr;
  }

  friend bool operator==(std::nullptr_t, const ref& rhs) noexcept {
    return rhs.instance_ == nullptr;
  }

  friend bool operator!=(const ref& lhs, std::nullptr_t) noexcept {
    return lhs.instance_ != nullptr;
  }

  friend bool operator!=(std::nullptr_t, const ref& rhs) noexcept {
    return rhs.instance_ != nullptr;
  }

private:
  template <typename U> friend class weak_ref;

  template <typename Object, typename... Args>
  friend ref<Object> make_ref(memory_resource* resource, Args&&... args);

  struct adopt_tag {};
  struct retained_tag {};

  ref(T* instance, detail::ref_control_block* control, adopt_tag) noexcept
      : instance_(instance), control_(control) {
    retain();
  }

  ref(T* instance, detail::ref_control_block* control, retained_tag) noexcept
      : instance_(instance), control_(control) {}

  void retain() noexcept {
    if (control_ != nullptr) {
      detail::add_ref(control_);
    }
  }

  void release() noexcept {
    T* instance = instance_;
    detail::ref_control_block* control = control_;
    instance_ = nullptr;
    control_ = nullptr;

    if (control != nullptr && detail::release_ref(control)) {
      control->instance = nullptr;
      control->destroy(instance, control->object_resource);
      detail::release_weak_ref(control);
    }
  }

  T* instance_ = nullptr;
  detail::ref_control_block* control_ = nullptr;
};

template <typename T> void swap(ref<T>& lhs, ref<T>& rhs) noexcept { lhs.swap(rhs); }

/**
 * @brief 引用计数对象的弱引用。
 *
 * `weak_ref<T>` 不延长对象生命周期。对象仍存活时可通过 `lock()` 尝试提升为 `ref<T>`；
 * 最后一个强引用释放后，`lock()` 返回空强引用。
 */
template <typename T> class weak_ref {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");
  static_assert(std::is_base_of_v<ref_counted, T>,
                "T must derive from caors::objects::ref_counted");
  static_assert(!std::is_const_v<T>, "T must not be const");

public:
  weak_ref() noexcept = default;
  weak_ref(std::nullptr_t) noexcept {}

  weak_ref(const ref<T>& strong) noexcept : instance_(strong.instance_), control_(strong.control_) {
    retain();
  }

  weak_ref(const weak_ref& other) noexcept : instance_(other.instance_), control_(other.control_) {
    retain();
  }

  weak_ref(weak_ref&& other) noexcept : instance_(other.instance_), control_(other.control_) {
    other.instance_ = nullptr;
    other.control_ = nullptr;
  }

  ~weak_ref() { release(); }

  weak_ref& operator=(const ref<T>& strong) noexcept {
    weak_ref copy{strong};
    swap(copy);
    return *this;
  }

  weak_ref& operator=(const weak_ref& other) noexcept {
    if (this == &other) {
      return *this;
    }

    weak_ref copy{other};
    swap(copy);
    return *this;
  }

  weak_ref& operator=(weak_ref&& other) noexcept {
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

  /** @brief 当前对象的强引用数量。空引用或已失效引用返回 0。 */
  [[nodiscard]] std::size_t use_count() const noexcept { return detail::ref_count(control_); }

  /** @brief 判断对象是否已经失效。 */
  [[nodiscard]] bool expired() const noexcept { return use_count() == 0; }

  /** @brief 尝试提升为强引用。对象已经销毁时返回空强引用。 */
  [[nodiscard]] ref<T> lock() const noexcept {
    if (control_ == nullptr || !detail::try_add_ref(control_)) {
      return {};
    }

    return ref<T>{instance_, control_, typename ref<T>::retained_tag{}};
  }

  /** @brief 判断当前是否仍能提升为强引用。 */
  [[nodiscard]] explicit operator bool() const noexcept { return !expired(); }

  /** @brief 释放当前弱引用。 */
  void reset(std::nullptr_t = nullptr) noexcept { release(); }

  /** @brief 交换两个弱引用。 */
  void swap(weak_ref& other) noexcept {
    using std::swap;
    swap(instance_, other.instance_);
    swap(control_, other.control_);
  }

  friend bool operator==(const weak_ref& lhs, std::nullptr_t) noexcept {
    return lhs.control_ == nullptr || lhs.expired();
  }

  friend bool operator==(std::nullptr_t, const weak_ref& rhs) noexcept {
    return rhs.control_ == nullptr || rhs.expired();
  }

  friend bool operator!=(const weak_ref& lhs, std::nullptr_t) noexcept { return !(lhs == nullptr); }

  friend bool operator!=(std::nullptr_t, const weak_ref& rhs) noexcept { return !(nullptr == rhs); }

private:
  void retain() noexcept {
    if (control_ != nullptr) {
      detail::add_weak_ref(control_);
    }
  }

  void release() noexcept {
    detail::ref_control_block* control = control_;
    instance_ = nullptr;
    control_ = nullptr;

    if (control != nullptr) {
      detail::release_weak_ref(control);
    }
  }

  T* instance_ = nullptr;
  detail::ref_control_block* control_ = nullptr;
};

template <typename T> void swap(weak_ref<T>& lhs, weak_ref<T>& rhs) noexcept { lhs.swap(rhs); }

/** @brief 使用指定内存资源创建引用计数对象并返回强引用。 */
template <typename T, typename... Args>
[[nodiscard]] ref<T> make_ref(memory_resource* resource, Args&&... args) {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");
  static_assert(std::is_base_of_v<ref_counted, T>,
                "T must derive from caors::objects::ref_counted");

  memory_resource* resolved = resource != nullptr ? resource : get_default_memory_resource();
  auto* control = caors::make<detail::ref_control_block>(resolved, resolved, resolved,
                                                         &detail::destroy_ref_counted_instance<T>);

#if CAORS_HAS_EXCEPTIONS
  try {
#endif
    T* instance = objects::create<T>(resolved, std::forward<Args>(args)...);
    instance->bind_control_block(control);
    control->instance = instance;
    return ref<T>{instance, control, typename ref<T>::adopt_tag{}};
#if CAORS_HAS_EXCEPTIONS
  } catch (...) {
    caors::destroy(control, resolved);
    throw;
  }
#endif

  return {};
}

/** @brief 使用默认内存资源创建引用计数对象并返回强引用。 */
template <typename T> [[nodiscard]] ref<T> make_ref() {
  return make_ref<T>(static_cast<memory_resource*>(nullptr));
}

/** @brief 使用默认内存资源和构造参数创建引用计数对象并返回强引用。 */
template <typename T, typename First, typename... Rest>
  requires(!std::is_convertible_v<std::remove_cvref_t<First>, memory_resource*>)
[[nodiscard]] ref<T> make_ref(First&& first, Rest&&... rest) {
  return make_ref<T>(static_cast<memory_resource*>(nullptr), std::forward<First>(first),
                     std::forward<Rest>(rest)...);
}

} // namespace caors::objects

namespace caors {

template <typename T> using object_ref = objects::ref<T>;
template <typename T> using object_weak_ref = objects::weak_ref<T>;

} // namespace caors

namespace caors::traits {

/** @brief `objects::ref<T>` 按拥有型 inline wrapper 序列化内部对象。 */
template <typename T> struct wrapper_traits<caors::objects::ref<T>> {
  static constexpr bool is_wrapper = true;
  static constexpr bool nullable = true;
  static constexpr wrapper_semantics semantics = wrapper_semantics::inline_value;
  using wrapped_type = std::remove_cv_t<T>;
};

template <typename T> struct wrapper_access<caors::objects::ref<T>> {
  using wrapped_type = std::remove_cv_t<T>;

  [[nodiscard]] static const wrapped_type* value(const caors::objects::ref<T>& source) noexcept {
    return source.get();
  }

  [[nodiscard]] static wrapped_type* mutable_value(caors::objects::ref<T>& source) noexcept {
    return source.get();
  }

  [[nodiscard]] static bool has_value(const caors::objects::ref<T>& source) noexcept {
    return static_cast<bool>(source);
  }

  static bool reset(caors::objects::ref<T>& source) noexcept {
    source.reset();
    return true;
  }

  static bool ensure_value(caors::objects::ref<T>& source) {
    if (source) {
      return true;
    }

    if constexpr (std::is_default_constructible_v<wrapped_type>) {
#if CAORS_HAS_EXCEPTIONS
      try {
        source = caors::objects::make_ref<wrapped_type>();
      } catch (...) {
        return false;
      }
#else
      if constexpr (!std::is_nothrow_default_constructible_v<wrapped_type>) {
        return false;
      } else {
        source = caors::objects::make_ref<wrapped_type>();
      }
#endif
      return static_cast<bool>(source);
    } else {
      return false;
    }
  }

  static bool ensure_value(caors::objects::ref<T>& source,
                           caors::memory::memory_resource* resource) {
    if (source) {
      return true;
    }

    if constexpr (std::is_default_constructible_v<wrapped_type>) {
#if CAORS_HAS_EXCEPTIONS
      try {
        source = caors::objects::make_ref<wrapped_type>(resource);
      } catch (...) {
        return false;
      }
#else
      if constexpr (!std::is_nothrow_default_constructible_v<wrapped_type>) {
        return false;
      } else {
        source = caors::objects::make_ref<wrapped_type>(resource);
      }
#endif
      return static_cast<bool>(source);
    } else {
      return false;
    }
  }
};

/** @brief 弱引用不稳定拥有内部值，默认禁止作为普通 wrapper 序列化。 */
template <typename T> struct wrapper_traits<caors::objects::weak_ref<T>> {
  static constexpr bool is_wrapper = true;
  static constexpr bool nullable = true;
  static constexpr wrapper_semantics semantics = wrapper_semantics::unsupported;
  using wrapped_type = std::remove_cv_t<T>;
};

} // namespace caors::traits

#endif
