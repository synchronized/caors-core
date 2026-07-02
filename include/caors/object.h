// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_H_
#define CAORS_OBJECT_H_

#include "caors/error.h"
#include "caors/export.h"
#include "caors/memory.h"
#include "caors/meta.h"

#include <cstddef>
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

/** @brief 根据已注册对象类型名查询对象类型标识。 */
[[nodiscard]] inline object_type_id make_object_type_id(std::string_view name) {
  return make_type_id(name);
}

/**
 * @brief 不拥有对象的运行期身份句柄。
 *
 * `object_handle` 只保存对象 id 和声明类型 id，不保证对象仍然存活，也不提供解引用能力。它适合在
 * 序列化、资源表或生命周期管理器之间传递对象身份；真正解析为指针需要上层对象数据库或 resolver。
 */
class object_handle {
public:
  constexpr object_handle() noexcept = default;
  constexpr object_handle(object_id id, object_type_id type) noexcept : id_(id), type_(type) {}

  [[nodiscard]] static constexpr object_handle invalid() noexcept { return {}; }
  [[nodiscard]] constexpr object_id id() const noexcept { return id_; }
  [[nodiscard]] constexpr object_type_id type() const noexcept { return type_; }
  [[nodiscard]] explicit constexpr operator bool() const noexcept { return id_ && type_; }

  friend constexpr bool operator==(object_handle lhs, object_handle rhs) noexcept {
    return lhs.id_ == rhs.id_ && lhs.type_ == rhs.type_;
  }

  friend constexpr bool operator!=(object_handle lhs, object_handle rhs) noexcept {
    return !(lhs == rhs);
  }

private:
  object_id id_{};
  object_type_id type_{};
};

class object;

using object_try_create_fn = object* (*)(memory_resource * resource) noexcept;
using object_create_fn = object* (*)(memory_resource * resource);
using object_destroy_fn = void (*)(object* instance, memory_resource* resource) noexcept;

/** @brief 对象类型底层数据。 */
struct object_type_data {
  object_try_create_fn try_create = nullptr;
  object_create_fn create = nullptr;
  object_destroy_fn destroy = nullptr;

  [[nodiscard]] constexpr bool constructible() const noexcept {
    return try_create != nullptr && create != nullptr;
  }
};

/** @brief 对象类型只读视图。 */
class CAORS_API object_type final {
public:
  object_type() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] meta_type info() const noexcept;
  [[nodiscard]] object_type_id id() const noexcept;
  [[nodiscard]] string_name name() const noexcept;
  [[nodiscard]] type_guid guid() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] std::size_t alignment() const noexcept;
  [[nodiscard]] object_type_id parent_id() const noexcept;
  [[nodiscard]] bool constructible() const noexcept;

private:
  friend class object_registry;

  object_type(meta_type type, const object_type_data* data) noexcept;
  [[nodiscard]] const object_type_data* data() const noexcept;

  meta_type type_{};
  const object_type_data* data_ = nullptr;
};

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
 * @brief 非拥有对象引用。
 *
 * `object_ref<T>` 只保存对象指针，不延长对象生命周期。序列化时它通过
 * `serialization_reference_resolver` 转换为 `object_id`，反序列化时再从 `object_id`
 * 解析回对象指针。
 */
template <typename T> class object_ref {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");

public:
  object_ref() noexcept = default;
  object_ref(std::nullptr_t) noexcept {}
  explicit object_ref(T* instance) noexcept : instance_(instance) {}

  [[nodiscard]] T* get() const noexcept { return instance_; }
  [[nodiscard]] object_id id() const noexcept {
    return instance_ != nullptr ? instance_->id() : object_id::invalid();
  }
  [[nodiscard]] explicit operator bool() const noexcept { return instance_ != nullptr; }

  void reset(T* instance = nullptr) noexcept { instance_ = instance; }

private:
  T* instance_ = nullptr;
};

/**
 * @brief 类型化弱对象句柄。
 *
 * 该类型只保存对象身份，不拥有对象、不延长生命周期，也不隐式解析指针。它为后续对象生命周期管理器、
 * 弱引用 resolver 和资源引用序列化预留稳定边界。
 */
template <typename T> class object_weak_handle {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");

public:
  object_weak_handle() noexcept = default;
  object_weak_handle(std::nullptr_t) noexcept {}
  explicit object_weak_handle(object_handle handle) noexcept : handle_(handle) {}
  explicit object_weak_handle(const T& instance) noexcept
      : handle_(instance.id(), instance.type_id()) {}

  [[nodiscard]] object_handle handle() const noexcept { return handle_; }
  [[nodiscard]] object_id id() const noexcept { return handle_.id(); }
  [[nodiscard]] object_type_id type() const noexcept { return handle_.type(); }
  [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(handle_); }

  void reset(object_handle handle = {}) noexcept { handle_ = handle; }

private:
  object_handle handle_{};
};

namespace detail {

template <typename T> struct object_ref_traits {
  static constexpr bool is_object_ref = false;
};

template <typename T> struct object_ref_traits<object_ref<T>> {
  static constexpr bool is_object_ref = true;
  using object_type = T;
};

template <typename T> struct object_ref_traits<object_weak_handle<T>> {
  static constexpr bool is_object_ref = false;
  using object_type = T;
};

template <typename T, typename = void> struct object_type_guid_traits {
  [[nodiscard]] static constexpr type_guid get() noexcept { return type_guid::nil(); }
};

template <typename T>
struct object_type_guid_traits<T, std::void_t<decltype(T::object_type_guid)>> {
  static_assert(
      std::is_same_v<std::remove_cv_t<std::remove_reference_t<decltype(T::object_type_guid)>>,
                     type_guid>,
      "object_type_guid must be caors::type_guid");

  [[nodiscard]] static constexpr type_guid get() noexcept { return T::object_type_guid; }
};

template <typename T> [[nodiscard]] constexpr type_guid object_type_guid_of() noexcept {
  return object_type_guid_traits<T>::get();
}

template <typename T, typename = void> struct object_base_type_traits {
  using type = object;
};

template <typename T> struct object_base_type_traits<T, std::void_t<typename T::base_type>> {
  using type = typename T::base_type;
};

} // namespace detail

/**
 * @brief 为派生对象提供默认运行期类型名实现。
 *
 * `Derived` 需要提供 `static constexpr std::string_view object_type_name`。可序列化、可反射或跨插件
 * 暴露的类型建议额外提供 `static constexpr caors::type_guid object_type_guid`。
 */
template <typename Derived, typename Base = object> class object_base : public Base {
  static_assert(std::is_base_of_v<object, Base>, "Base must derive from caors::object");

public:
  using base_type = Base;

  [[nodiscard]] static object_type_id static_type_id() noexcept {
#if CAORS_HAS_EXCEPTIONS
    try {
      return type_id_of<Derived>();
    } catch (...) {
      return object_type_id::invalid();
    }
#else
    return type_id_of<Derived>();
#endif
  }

  [[nodiscard]] static std::string_view static_type_name() noexcept {
    return Derived::object_type_name;
  }

  [[nodiscard]] static constexpr type_guid static_type_guid() noexcept {
    return detail::object_type_guid_of<Derived>();
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

/** @brief 创建对象类型基础元信息。 */
template <typename T>
[[nodiscard]] meta_type_data
make_object_meta_type_data(std::string_view name,
                           object_type_id parent_id = object::static_type_id()) noexcept {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");
  return make_meta_type_data<T>(name, detail::object_type_guid_of<T>(), parent_id);
}

/** @brief 创建对象类型底层数据。 */
template <typename T> [[nodiscard]] object_type_data make_object_type_data() noexcept {
  static_assert(std::is_base_of_v<object, T>, "T must derive from caors::object");

  object_type_data data{nullptr, nullptr, &destroy_object_instance<T>};
  if constexpr (std::is_default_constructible_v<T>) {
    data.try_create = &try_create_object_instance<T>;
    data.create = &create_object_instance<T>;
  }

  return data;
}

class object_registry;

/** @brief 获取进程内默认对象类型注册表。 */
[[nodiscard]] CAORS_API object_registry& get_object_registry() noexcept;

/**
 * @brief 全局对象类型注册表 facade 和默认构造工厂。
 *
 * `object_registry` 不表示彼此隔离的注册表实例。它复用全局 `meta_registry`
 * 管理基础类型信息，并把 `object_type_data` 作为组件挂载到同一个 `type_id`
 * 上。普通代码应通过 `get_object_registry()` 和全局辅助函数访问它。
 */
class CAORS_API object_registry final {
public:
  object_registry(const object_registry&) = delete;
  object_registry& operator=(const object_registry&) = delete;

  [[nodiscard]] error register_type(const meta_type_data& type, const object_type_data& object);
  template <typename T>
  [[nodiscard]] error register_type_for(const meta_type_data& type,
                                        const object_type_data& object) {
    return register_type(detail::cpp_type_key_of<T>(), type, object);
  }

  [[nodiscard]] bool contains(object_type_id id) const;
  [[nodiscard]] std::optional<object_type> find_type(object_type_id id) const;
  [[nodiscard]] std::optional<object_type> find_type(std::string_view name) const;
  template <typename T> [[nodiscard]] std::optional<object_type> find_type() const {
    const std::optional<meta_type> type = find_registered_type(detail::cpp_type_key_of<T>());
    if (!type.has_value()) {
      return std::nullopt;
    }

    return find_type(type->id());
  }

  [[nodiscard]] bool is_a(object_type_id type_id, object_type_id base_type_id) const;
  [[nodiscard]] std::size_t size() const noexcept;

  [[nodiscard]] object* try_create(object_type_id id,
                                   memory_resource* resource = nullptr) const noexcept;
  [[nodiscard]] object* try_create(std::string_view name,
                                   memory_resource* resource = nullptr) const noexcept;
  template <typename T>
  [[nodiscard]] T* try_create(memory_resource* resource = nullptr) const noexcept {
    const std::optional<object_type> type = find_type<T>();
    if (!type.has_value()) {
      return nullptr;
    }

    return static_cast<T*>(try_create(type->id(), resource));
  }

  [[nodiscard]] object* create(object_type_id id, memory_resource* resource = nullptr) const;
  [[nodiscard]] object* create(std::string_view name, memory_resource* resource = nullptr) const;
  template <typename T> [[nodiscard]] T* create(memory_resource* resource = nullptr) const {
    const std::optional<object_type> type = find_type<T>();
    if (!type.has_value()) {
      return nullptr;
    }

    return static_cast<T*>(create(type->id(), resource));
  }

  bool destroy(object* instance, memory_resource* resource = nullptr) const noexcept;

private:
  friend object_registry& get_object_registry() noexcept;

  object_registry();
  ~object_registry();

  [[nodiscard]] error register_type(detail::cpp_type_key key, const meta_type_data& type,
                                    const object_type_data& object);
  [[nodiscard]] std::optional<meta_type> find_registered_type(detail::cpp_type_key key) const;
  [[nodiscard]] const object_type_data* find_type_data(object_type_id id) const;
};

/** @brief 向全局对象注册表注册对象类型。 */
template <typename T>
[[nodiscard]] error register_object_type(std::string_view name,
                                         object_type_id parent_id = object_type_id::invalid()) {
  object_registry& registry = get_object_registry();
  if (!parent_id) {
    using base_type = typename detail::object_base_type_traits<T>::type;
    const std::optional<object_type> base = registry.find_type<base_type>();
    if (base.has_value()) {
      parent_id = base->id();
    }
  }

  return registry.register_type_for<T>(make_object_meta_type_data<T>(name, parent_id),
                                       make_object_type_data<T>());
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
