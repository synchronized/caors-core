// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_REF_COUNTED_H_
#define CAORS_OBJECT_REF_COUNTED_H_

#include "caors/object/factory.h"

#include <atomic>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace caors::objects {

template <typename T> class strong_ref;

/**
 * @brief 可选侵入式引用计数基类。
 *
 * 继承该类型的对象可以由 `objects::strong_ref<T>` 管理强引用计数。计数本身使用原子变量，保证强引用
 * 增减操作可跨线程执行；这不表示对象内部状态具备线程安全访问能力。
 */
class ref_counted {
public:
  ref_counted() noexcept = default;
  ref_counted(const ref_counted&) noexcept {}
  ref_counted& operator=(const ref_counted&) noexcept { return *this; }

  /** @brief 当前强引用数量。 */
  [[nodiscard]] std::size_t use_count() const noexcept {
    return strong_count_.load(std::memory_order_acquire);
  }

  /** @brief 当前是否只有一个强引用。 */
  [[nodiscard]] bool unique() const noexcept { return use_count() == 1; }

protected:
  ~ref_counted() = default;

private:
  template <typename T> friend class strong_ref;

  void add_strong_ref() const noexcept { strong_count_.fetch_add(1, std::memory_order_relaxed); }

  [[nodiscard]] bool release_strong_ref() const noexcept {
    std::size_t current = strong_count_.load(std::memory_order_acquire);
    while (current != 0) {
      if (strong_count_.compare_exchange_weak(current, current - 1, std::memory_order_acq_rel,
                                              std::memory_order_acquire)) {
        return current == 1;
      }
    }

    return false;
  }

  mutable std::atomic<std::size_t> strong_count_{0};
};

/**
 * @brief 引用计数对象的强引用。
 *
 * `strong_ref<T>` 只管理由 `objects::make_ref<T>()` 创建的对象。它不会接管 `objects::manager`
 * 已经拥有的对象，也不会改变 `objects::ref<T>`、`objects::handle` 或 `objects::weak_handle<T>`
 * 的非拥有语义。
 */
template <typename T> class strong_ref {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");
  static_assert(std::is_base_of_v<ref_counted, T>,
                "T must derive from caors::objects::ref_counted");
  static_assert(!std::is_const_v<T>, "T must not be const");

public:
  strong_ref() noexcept = default;
  strong_ref(std::nullptr_t) noexcept {}

  strong_ref(const strong_ref& other) noexcept
      : instance_(other.instance_), resource_(other.resource_) {
    retain();
  }

  strong_ref(strong_ref&& other) noexcept : instance_(other.instance_), resource_(other.resource_) {
    other.instance_ = nullptr;
    other.resource_ = nullptr;
  }

  ~strong_ref() { release(); }

  strong_ref& operator=(const strong_ref& other) noexcept {
    if (this == &other) {
      return *this;
    }

    strong_ref copy{other};
    swap(copy);
    return *this;
  }

  strong_ref& operator=(strong_ref&& other) noexcept {
    if (this == &other) {
      return *this;
    }

    release();
    instance_ = other.instance_;
    resource_ = other.resource_;
    other.instance_ = nullptr;
    other.resource_ = nullptr;
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
  void swap(strong_ref& other) noexcept {
    using std::swap;
    swap(instance_, other.instance_);
    swap(resource_, other.resource_);
  }

  friend bool operator==(const strong_ref& lhs, std::nullptr_t) noexcept {
    return lhs.instance_ == nullptr;
  }

  friend bool operator==(std::nullptr_t, const strong_ref& rhs) noexcept {
    return rhs.instance_ == nullptr;
  }

  friend bool operator!=(const strong_ref& lhs, std::nullptr_t) noexcept {
    return lhs.instance_ != nullptr;
  }

  friend bool operator!=(std::nullptr_t, const strong_ref& rhs) noexcept {
    return rhs.instance_ != nullptr;
  }

private:
  template <typename Object, typename... Args>
  friend strong_ref<Object> make_ref(memory_resource* resource, Args&&... args);

  struct adopt_tag {};

  strong_ref(T* instance, memory_resource* resource, adopt_tag) noexcept
      : instance_(instance), resource_(resource) {
    retain();
  }

  void retain() noexcept {
    if (instance_ != nullptr) {
      instance_->add_strong_ref();
    }
  }

  void release() noexcept {
    T* instance = instance_;
    memory_resource* resource = resource_;
    instance_ = nullptr;
    resource_ = nullptr;

    if (instance != nullptr && instance->release_strong_ref()) {
      objects::destroy(instance, resource);
    }
  }

  T* instance_ = nullptr;
  memory_resource* resource_ = nullptr;
};

template <typename T> void swap(strong_ref<T>& lhs, strong_ref<T>& rhs) noexcept { lhs.swap(rhs); }

/** @brief 使用指定内存资源创建引用计数对象并返回强引用。 */
template <typename T, typename... Args>
[[nodiscard]] strong_ref<T> make_ref(memory_resource* resource, Args&&... args) {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");
  static_assert(std::is_base_of_v<ref_counted, T>,
                "T must derive from caors::objects::ref_counted");

  T* instance = objects::create<T>(resource, std::forward<Args>(args)...);
  if (instance == nullptr) {
    return {};
  }

  return strong_ref<T>{instance, resource, typename strong_ref<T>::adopt_tag{}};
}

/** @brief 使用默认内存资源创建引用计数对象并返回强引用。 */
template <typename T> [[nodiscard]] strong_ref<T> make_ref() {
  return make_ref<T>(static_cast<memory_resource*>(nullptr));
}

/** @brief 使用默认内存资源和构造参数创建引用计数对象并返回强引用。 */
template <typename T, typename First, typename... Rest>
  requires(!std::is_convertible_v<std::remove_cvref_t<First>, memory_resource*>)
[[nodiscard]] strong_ref<T> make_ref(First&& first, Rest&&... rest) {
  return make_ref<T>(static_cast<memory_resource*>(nullptr), std::forward<First>(first),
                     std::forward<Rest>(rest)...);
}

} // namespace caors::objects

#endif
