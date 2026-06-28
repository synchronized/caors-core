// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_H_
#define CAORS_OBJECT_H_

#include "caors/export.h"
#include "caors/memory.h"
#include "caors/meta.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors {

/**
 * @brief 运行期对象实例标识。
 *
 * `0` 保留为无效值。该标识只表达一次进程运行期间的对象身份，不承诺跨进程或跨存档稳定。
 */
class object_id {
public:
  constexpr object_id() noexcept = default;
  explicit constexpr object_id(std::uint64_t value) noexcept : value_(value) {}

  [[nodiscard]] static constexpr object_id invalid() noexcept { return object_id{}; }
  [[nodiscard]] constexpr std::uint64_t value() const noexcept { return value_; }
  [[nodiscard]] explicit constexpr operator bool() const noexcept { return value_ != 0; }

  friend constexpr bool operator==(object_id lhs, object_id rhs) noexcept {
    return lhs.value_ == rhs.value_;
  }

  friend constexpr bool operator!=(object_id lhs, object_id rhs) noexcept { return !(lhs == rhs); }

private:
  std::uint64_t value_ = 0;
};

/** @brief 生成新的运行期对象实例标识。 */
[[nodiscard]] CAORS_API object_id generate_object_id() noexcept;

using object_type_id = type_id;

/** @brief 根据类型名生成对象类型标识。 */
[[nodiscard]] constexpr object_type_id make_object_type_id(std::string_view name) noexcept {
  return make_type_id(name);
}

class object;

using object_try_create_fn = object* (*)(memory_resource * resource) noexcept;
using object_create_fn = object* (*)(memory_resource * resource);
using object_destroy_fn = void (*)(object* instance, memory_resource* resource) noexcept;

/** @brief 注册表中保存的对象类型描述。 */
struct object_type {
  type_info info{};
  object_try_create_fn try_create = nullptr;
  object_create_fn create = nullptr;
  object_destroy_fn destroy = nullptr;

  [[nodiscard]] constexpr bool valid() const noexcept { return info.valid(); }
  [[nodiscard]] constexpr bool constructible() const noexcept {
    return try_create != nullptr && create != nullptr;
  }
};

/** @brief 对象类型注册结果。 */
enum class object_register_result {
  success,
  invalid_type,
  duplicate_id,
  duplicate_name,
  hash_collision,
  missing_parent,
};

/** @brief 判断对象类型注册是否成功。 */
[[nodiscard]] constexpr bool object_register_succeeded(object_register_result result) noexcept {
  return result == object_register_result::success;
}

/**
 * @brief CAORS 对象基类。
 *
 * 该类型只提供对象身份和运行期类型入口，不负责引用计数、GC、脚本或属性反射。
 */
class CAORS_API object {
public:
  object() noexcept;
  virtual ~object();

  object(const object&) = delete;
  object& operator=(const object&) = delete;
  object(object&&) = delete;
  object& operator=(object&&) = delete;

  [[nodiscard]] object_id id() const noexcept;
  [[nodiscard]] virtual object_type_id type_id() const noexcept;
  [[nodiscard]] virtual std::string_view type_name() const noexcept;

  [[nodiscard]] static object_type_id static_type_id() noexcept;
  [[nodiscard]] static std::string_view static_type_name() noexcept;

private:
  object_id id_;
};

/**
 * @brief 为派生对象提供默认运行期类型实现。
 *
 * `Derived` 需要提供 `static constexpr std::string_view object_type_name`。
 */
template <typename Derived, typename Base = object> class object_base : public Base {
  static_assert(std::is_base_of_v<object, Base>, "Base must derive from caors::object");

public:
  using base_type = Base;

  [[nodiscard]] static object_type_id static_type_id() noexcept {
    return make_object_type_id(static_type_name());
  }

  [[nodiscard]] static std::string_view static_type_name() noexcept {
    return Derived::object_type_name;
  }

  [[nodiscard]] object_type_id type_id() const noexcept override { return static_type_id(); }

  [[nodiscard]] std::string_view type_name() const noexcept override { return static_type_name(); }
};

template <typename T>
[[nodiscard]] object* try_create_object_instance(memory_resource* resource) noexcept {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");
  static_assert(std::is_default_constructible_v<T>, "T must be default constructible");

  memory_resource* resolved = resource != nullptr ? resource : get_default_memory_resource();
  void* storage = resolved->try_allocate(sizeof(T), alignof(T));
  if (storage == nullptr) {
    return nullptr;
  }

#if CAORS_HAS_EXCEPTIONS
  try {
    return ::new (storage) T();
  } catch (...) {
    resolved->deallocate(storage, sizeof(T), alignof(T));
    return nullptr;
  }
#else
  return ::new (storage) T();
#endif
}

template <typename T> [[nodiscard]] object* create_object_instance(memory_resource* resource) {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");
  static_assert(std::is_default_constructible_v<T>, "T must be default constructible");

  return make<T>(resource);
}

template <typename T>
void destroy_object_instance(object* instance, memory_resource* resource) noexcept {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");

  destroy(static_cast<T*>(instance), resource);
}

/** @brief 创建对象类型描述。 */
template <typename T>
[[nodiscard]] object_type
make_object_type(std::string_view name,
                 object_type_id parent_id = object::static_type_id()) noexcept {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");

  object_type type{
      make_type_info<T>(name, parent_id),
      nullptr,
      nullptr,
      &destroy_object_instance<T>,
  };

  if constexpr (std::is_default_constructible_v<T>) {
    type.try_create = &try_create_object_instance<T>;
    type.create = &create_object_instance<T>;
  }

  return type;
}

class object_registry_state;

/**
 * @brief 对象类型注册表和默认构造工厂。
 *
 * 注册表复用 `meta_registry` 管理基础类型信息。`find_type()` 返回的 `object_type::info.name`
 * 在注册表生命周期内有效。
 */
class CAORS_API object_registry final {
public:
  object_registry();
  ~object_registry();

  object_registry(const object_registry&) = delete;
  object_registry& operator=(const object_registry&) = delete;

  [[nodiscard]] object_register_result register_type(const object_type& type);
  [[nodiscard]] bool contains(object_type_id id) const;
  [[nodiscard]] std::optional<object_type> find_type(object_type_id id) const;
  [[nodiscard]] std::optional<object_type> find_type(std::string_view name) const;
  [[nodiscard]] bool is_a(object_type_id type_id, object_type_id base_type_id) const;
  [[nodiscard]] std::size_t size() const noexcept;

  [[nodiscard]] object* try_create(object_type_id id,
                                   memory_resource* resource = nullptr) const noexcept;
  [[nodiscard]] object* create(object_type_id id, memory_resource* resource = nullptr) const;
  bool destroy(object* instance, memory_resource* resource = nullptr) const noexcept;
  void clear() noexcept;

private:
  object_registry_state* state_ = nullptr;
};

/** @brief 获取进程内默认对象类型注册表。 */
[[nodiscard]] CAORS_API object_registry& get_object_registry() noexcept;

/** @brief 向指定注册表注册对象类型。 */
template <typename T>
[[nodiscard]] object_register_result
register_object_type(object_registry& registry, std::string_view name,
                     object_type_id parent_id = object::static_type_id()) {
  return registry.register_type(make_object_type<T>(name, parent_id));
}

/** @brief 向默认注册表注册对象类型。 */
template <typename T>
[[nodiscard]] object_register_result
register_object_type(std::string_view name, object_type_id parent_id = object::static_type_id()) {
  return register_object_type<T>(get_object_registry(), name, parent_id);
}

/** @brief 直接创建指定 C++ 类型的对象。 */
template <typename T, typename... Args>
[[nodiscard]] T* create_object(memory_resource* resource, Args&&... args) {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");
  return make<T>(resource, std::forward<Args>(args)...);
}

template <typename T> [[nodiscard]] T* create_object() {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");
  return create_object<T>(static_cast<memory_resource*>(nullptr));
}

template <typename T, typename First, typename... Rest>
  requires(!std::is_convertible_v<std::remove_cvref_t<First>, memory_resource*>)
[[nodiscard]] T* create_object(First&& first, Rest&&... rest) {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");
  return create_object<T>(static_cast<memory_resource*>(nullptr), std::forward<First>(first),
                          std::forward<Rest>(rest)...);
}

/** @brief 销毁通过 CAORS 内存资源创建的对象。 */
template <typename T>
void destroy_object(T* instance, memory_resource* resource = nullptr) noexcept {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");
  destroy(instance, resource);
}

}

#endif
