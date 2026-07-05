// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MEMORY_UNIQUE_PTR_H_
#define CAORS_MEMORY_UNIQUE_PTR_H_

#include "caors/memory/resource.h"

#include <memory>
#include <type_traits>
#include <utility>

namespace caors::memory {

/**
 * @brief 可保存运行期销毁回调的 unique_ptr deleter。
 *
 * 默认构造时等价于普通 `delete`。当序列化层用 `memory_resource` 构造出运行期派生对象后，
 * wrapper 适配器可以把对应销毁回调写入 deleter，保证释放时使用匹配的资源和真实派生类型。
 */
template <typename T> class erased_deleter {
public:
  using destroy_fn = void (*)(void* user_data, void* value) noexcept;

  constexpr erased_deleter() noexcept = default;
  constexpr erased_deleter(void* user_data, destroy_fn destroy) noexcept
      : user_data_(user_data), destroy_(destroy) {}

  void operator()(T* value) const noexcept {
    if (value == nullptr) {
      return;
    }

    if (destroy_ == nullptr) {
      delete value;
      return;
    }

    if constexpr (std::is_polymorphic_v<T>) {
      destroy_(user_data_, dynamic_cast<void*>(value));
    } else {
      destroy_(user_data_, value);
    }
  }

  void reset() noexcept {
    user_data_ = nullptr;
    destroy_ = nullptr;
  }

  void reset(void* user_data, destroy_fn destroy) noexcept {
    user_data_ = user_data;
    destroy_ = destroy;
  }

  [[nodiscard]] bool custom() const noexcept { return destroy_ != nullptr; }

private:
  void* user_data_ = nullptr;
  destroy_fn destroy_ = nullptr;
};

/**
 * @brief 能接管 `memory_resource` 构造对象的 unique_ptr 别名。
 *
 * 普通 `std::unique_ptr<T>` 不能在运行期替换 deleter，因此当多态反序列化需要使用
 * `serialization::context::memory_resource` 时，应使用这个别名作为字段类型。
 */
template <typename T> using unique_ptr = std::unique_ptr<T, erased_deleter<T>>;

/** @brief 使用指定资源构造对象，并返回携带匹配销毁回调的 unique_ptr。 */
template <typename T, typename... Args>
[[nodiscard]] unique_ptr<T> make_unique(memory_resource* resource, Args&&... args) {
  memory_resource* resolved = resource != nullptr ? resource : get_default_memory_resource();
  T* value = make<T>(resolved, std::forward<Args>(args)...);
  unique_ptr<T> result{value};
  result.get_deleter().reset(resolved, [](void* user_data, void* stored) noexcept {
    destroy(static_cast<T*>(stored), static_cast<memory_resource*>(user_data));
  });
  return result;
}

} // namespace caors::memory

#endif
