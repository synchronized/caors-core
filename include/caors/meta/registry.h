// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_REGISTRY_H_
#define CAORS_META_REGISTRY_H_

#include "caors/error.h"
#include "caors/export.h"
#include "caors/meta/container.h"
#include "caors/meta/container_traits.h"
#include "caors/meta/type.h"
#include "caors/meta/type_key.h"
#include "caors/meta/view.h"
#include "caors/meta/wrapper.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors {

namespace detail {

using meta_component_key = const void*;

template <typename T> [[nodiscard]] error ensure_type_semantics_for(type_id id);
template <typename T> [[nodiscard]] error register_container_data_for(type_id id);

template <typename Component> [[nodiscard]] meta_component_key meta_component_key_of() noexcept {
  static const int key = 0;
  return &key;
}

class meta_component_storage_base {
public:
  virtual ~meta_component_storage_base() = default;

  [[nodiscard]] virtual bool contains(type_id id) const = 0;
  [[nodiscard]] virtual std::size_t size() const noexcept = 0;
  [[nodiscard]] virtual vector<type_id> ids() const = 0;
};

template <typename Component>
class meta_component_storage final : public meta_component_storage_base {
public:
  static_assert(std::is_copy_constructible_v<Component>,
                "Meta components must be copy constructible");

  template <typename... Args> [[nodiscard]] bool emplace(type_id id, Args&&... args) {
    std::lock_guard lock(mutex_);
    auto component = std::make_unique<Component>(std::forward<Args>(args)...);
    return components_.try_emplace(id.value(), std::move(component)).second;
  }

  [[nodiscard]] std::optional<Component> find(type_id id) const {
    std::lock_guard lock(mutex_);
    const auto position = components_.find(id.value());
    if (position == components_.end()) {
      return std::nullopt;
    }

    return *position->second;
  }

  [[nodiscard]] const Component* find_ptr(type_id id) const {
    std::lock_guard lock(mutex_);
    const auto position = components_.find(id.value());
    if (position == components_.end()) {
      return nullptr;
    }

    return position->second.get();
  }

  [[nodiscard]] bool contains(type_id id) const override {
    std::lock_guard lock(mutex_);
    return components_.contains(id.value());
  }

  [[nodiscard]] std::size_t size() const noexcept override {
    std::lock_guard lock(mutex_);
    return components_.size();
  }

  [[nodiscard]] vector<type_id> ids() const override {
    std::lock_guard lock(mutex_);
    vector<type_id> result;
    result.reserve(components_.size());
    for (const auto& [id, component] : components_) {
      (void)component;
      result.emplace_back(id);
    }

    return result;
  }

private:
  mutable std::mutex mutex_;
  hash_map<std::uint64_t, std::unique_ptr<Component>> components_;
};

} // namespace detail

class meta_registry_state;

/**
 * @brief 底层全局类型元数据注册表。
 *
 * `meta_registry` 以 `type_id` 为 key，先注册基础 `meta_type_data`，再按类型挂载对象工厂、
 * 反射描述、序列化描述等元数据组件。普通代码应使用全局 `register_type()` 和
 * `find_type()` 入口，不要创建或管理注册表实例。
 */
class CAORS_API meta_registry final {
public:
  [[nodiscard]] static meta_registry& instance() noexcept;

  meta_registry(const meta_registry&) = delete;
  meta_registry& operator=(const meta_registry&) = delete;

  [[nodiscard]] error register_type(const meta_type_data& type);
  [[nodiscard]] error register_cpp_type(detail::cpp_type_key key, const meta_type_data& type);
  [[nodiscard]] meta_type ensure_cpp_type(detail::cpp_type_key key, std::size_t size,
                                          std::size_t alignment);
  template <typename T> [[nodiscard]] error register_type_for(const meta_type_data& type) {
    using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
    const detail::cpp_type_key key = detail::cpp_type_key_of<value_type>();
    const error status = register_cpp_type(key, type);
    if (status && status.value() != errc::duplicate_id) {
      return status;
    }

    const std::optional<meta_type> registered = find_cpp_type(key);
    if (registered.has_value()) {
      (void)detail::ensure_type_semantics_for<value_type>(registered->id());
      (void)detail::register_container_data_for<value_type>(registered->id());
    }

    return status;
  }

  template <typename T> [[nodiscard]] meta_type ensure_type_for() {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    const detail::cpp_type_key key = detail::cpp_type_key_of<type>();
    std::optional<meta_type> existing = find_cpp_type(key);
    if (existing.has_value()) {
      (void)detail::ensure_type_semantics_for<type>(existing->id());
      (void)detail::register_container_data_for<type>(existing->id());
      return *existing;
    }

    const error status = register_cpp_type(key, make_meta_type_data<type>(string_name{}));
    if (status && status.value() != errc::duplicate_id) {
      return {};
    }

    existing = find_cpp_type(key);
    if (existing.has_value()) {
      (void)detail::ensure_type_semantics_for<type>(existing->id());
      (void)detail::register_container_data_for<type>(existing->id());
    }

    return existing.value_or(meta_type{});
  }

  [[nodiscard]] bool contains(type_id id) const;
  [[nodiscard]] std::optional<meta_type> find_cpp_type(detail::cpp_type_key key) const;
  template <typename T> [[nodiscard]] std::optional<meta_type> find_type() const {
    return find_cpp_type(detail::cpp_type_key_of<T>());
  }

  [[nodiscard]] std::optional<meta_type> find_type(type_id id) const;
  [[nodiscard]] std::optional<meta_type> find_type(type_guid guid) const;
  [[nodiscard]] std::optional<meta_type> find_type(std::string_view name) const;
  [[nodiscard]] std::optional<meta_type> find_type(string_name name) const;
  [[nodiscard]] bool is_a(type_id id, type_id base_id) const;
  [[nodiscard]] std::size_t size() const noexcept;

  /** @brief 给已注册类型挂载一个元数据组件。 */
  template <typename Component, typename... Args>
  [[nodiscard]] error emplace(type_id id, Args&&... args) {
    using component_type = std::decay_t<Component>;

    if (!id) {
      return {error_domain::meta, errc::invalid_type};
    }

    if (!contains(id)) {
      return {error_domain::meta, errc::missing_type};
    }

    detail::meta_component_storage_base* storage_base =
        find_component_storage(detail::meta_component_key_of<component_type>());
    if (storage_base == nullptr) {
      storage_base = &ensure_component_storage(
          detail::meta_component_key_of<component_type>(),
          std::make_unique<detail::meta_component_storage<component_type>>());
    }

    auto& storage = static_cast<detail::meta_component_storage<component_type>&>(*storage_base);
    if (!storage.emplace(id, std::forward<Args>(args)...)) {
      return {error_domain::meta, errc::duplicate_component};
    }

    return {};
  }

  /** @brief 判断指定类型是否挂载了某个元数据组件。 */
  template <typename Component> [[nodiscard]] bool has(type_id id) const {
    using component_type = std::decay_t<Component>;
    const detail::meta_component_storage_base* storage_base =
        find_component_storage(detail::meta_component_key_of<component_type>());
    if (storage_base == nullptr) {
      return false;
    }

    return storage_base->contains(id);
  }

  /** @brief 获取指定类型上的元数据组件副本。 */
  template <typename Component> [[nodiscard]] std::optional<Component> find(type_id id) const {
    using component_type = std::decay_t<Component>;
    const detail::meta_component_storage_base* storage_base =
        find_component_storage(detail::meta_component_key_of<component_type>());
    if (storage_base == nullptr) {
      return std::nullopt;
    }

    const auto& storage =
        static_cast<const detail::meta_component_storage<component_type>&>(*storage_base);
    return storage.find(id);
  }

  /**
   * @brief 获取指定类型上的元数据组件只读指针。
   *
   * 组件由全局注册表以 append-only 方式保存。返回指针不拥有对象，只要组件已注册且进程未退出，
   * 指针地址保持稳定。
   */
  template <typename Component> [[nodiscard]] const Component* find_ptr(type_id id) const {
    using component_type = std::decay_t<Component>;
    const detail::meta_component_storage_base* storage_base =
        find_component_storage(detail::meta_component_key_of<component_type>());
    if (storage_base == nullptr) {
      return nullptr;
    }

    const auto& storage =
        static_cast<const detail::meta_component_storage<component_type>&>(*storage_base);
    return storage.find_ptr(id);
  }

  /** @brief 统计挂载了指定组件的类型数量。 */
  template <typename Component> [[nodiscard]] std::size_t count() const noexcept {
    using component_type = std::decay_t<Component>;
    const detail::meta_component_storage_base* storage_base =
        find_component_storage(detail::meta_component_key_of<component_type>());
    if (storage_base == nullptr) {
      return 0;
    }

    return storage_base->size();
  }

  /** @brief 查询同时拥有指定组件集合的类型。 */
  template <typename... Components> [[nodiscard]] meta_view<Components...> view() const;

private:
  meta_registry();
  ~meta_registry();

  [[nodiscard]] detail::meta_component_storage_base*
  find_component_storage(detail::meta_component_key key);
  [[nodiscard]] const detail::meta_component_storage_base*
  find_component_storage(detail::meta_component_key key) const;
  detail::meta_component_storage_base&
  ensure_component_storage(detail::meta_component_key key,
                           std::unique_ptr<detail::meta_component_storage_base> storage);
  [[nodiscard]] vector<type_id> view_component_ids(const detail::meta_component_key* keys,
                                                   std::size_t count) const;

  meta_registry_state* state_ = nullptr;
};

/** @brief 获取进程内全局类型元数据注册表。 */
[[nodiscard]] CAORS_API meta_registry& get_meta_registry() noexcept;

namespace detail {

template <typename T>
inline constexpr bool is_supported_wrapper_v = wrapper_traits<std::remove_cv_t<T>>::is_wrapper;

template <typename T>
inline constexpr bool is_nested_type_indirection_v =
    std::is_pointer_v<std::remove_cv_t<T>> || is_supported_wrapper_v<T>;

template <typename T> [[nodiscard]] const void* raw_pointer_value(const void* storage) noexcept {
  if (storage == nullptr) {
    return nullptr;
  }

  return *static_cast<T* const*>(storage);
}

template <typename T> [[nodiscard]] void* raw_pointer_mutable_value(void* storage) noexcept {
  if (storage == nullptr) {
    return nullptr;
  }

  if constexpr (std::is_const_v<T>) {
    return nullptr;
  } else {
    return *static_cast<T**>(storage);
  }
}

template <typename T> [[nodiscard]] bool raw_pointer_has_value(const void* storage) noexcept {
  return raw_pointer_value<T>(storage) != nullptr;
}

template <typename T> bool raw_pointer_reset(void* storage) noexcept {
  if (storage == nullptr) {
    return false;
  }

  *static_cast<T**>(storage) = nullptr;
  return true;
}

template <typename T> bool raw_pointer_ensure_value(void*) { return false; }

template <typename Wrapper>
[[nodiscard]] const void* wrapper_indirect_value(const void* storage) noexcept {
  if (storage == nullptr) {
    return nullptr;
  }

  return wrapper_access<Wrapper>::value(*static_cast<const Wrapper*>(storage));
}

template <typename Wrapper>
[[nodiscard]] void* wrapper_mutable_indirect_value(void* storage) noexcept {
  if (storage == nullptr) {
    return nullptr;
  }

  return wrapper_access<Wrapper>::mutable_value(*static_cast<Wrapper*>(storage));
}

template <typename Wrapper>
[[nodiscard]] bool wrapper_has_indirect_value(const void* storage) noexcept {
  if (storage == nullptr) {
    return false;
  }

  return wrapper_access<Wrapper>::has_value(*static_cast<const Wrapper*>(storage));
}

template <typename Wrapper> bool wrapper_reset_indirect_value(void* storage) noexcept {
  if (storage == nullptr) {
    return false;
  }

  return wrapper_access<Wrapper>::reset(*static_cast<Wrapper*>(storage));
}

template <typename Wrapper> bool wrapper_ensure_indirect_value(void* storage) {
  if (storage == nullptr) {
    return false;
  }

  if constexpr (requires(Wrapper& source) { wrapper_access<Wrapper>::ensure_value(source); }) {
    return wrapper_access<Wrapper>::ensure_value(*static_cast<Wrapper*>(storage));
  } else {
    return false;
  }
}

template <typename Enum> [[nodiscard]] std::int64_t enum_to_i64(const void* storage) noexcept {
  using underlying_type = std::underlying_type_t<Enum>;
  return static_cast<std::int64_t>(
      static_cast<underlying_type>(*static_cast<const Enum*>(storage)));
}

template <typename Enum> [[nodiscard]] std::uint64_t enum_to_u64(const void* storage) noexcept {
  using underlying_type = std::underlying_type_t<Enum>;
  return static_cast<std::uint64_t>(
      static_cast<underlying_type>(*static_cast<const Enum*>(storage)));
}

template <typename Enum>
[[nodiscard]] bool enum_from_i64(void* storage, std::int64_t value) noexcept {
  using underlying_type = std::underlying_type_t<Enum>;
  if (storage == nullptr) {
    return false;
  }

  if constexpr (std::is_signed_v<underlying_type>) {
    if (value < static_cast<std::int64_t>(std::numeric_limits<underlying_type>::min()) ||
        value > static_cast<std::int64_t>(std::numeric_limits<underlying_type>::max())) {
      return false;
    }
  } else {
    if (value < 0 || static_cast<std::uint64_t>(value) >
                         static_cast<std::uint64_t>(std::numeric_limits<underlying_type>::max())) {
      return false;
    }
  }

  *static_cast<Enum*>(storage) = static_cast<Enum>(static_cast<underlying_type>(value));
  return true;
}

template <typename Enum>
[[nodiscard]] bool enum_from_u64(void* storage, std::uint64_t value) noexcept {
  using underlying_type = std::underlying_type_t<Enum>;
  if (storage == nullptr ||
      value > static_cast<std::uint64_t>(std::numeric_limits<underlying_type>::max())) {
    return false;
  }

  *static_cast<Enum*>(storage) = static_cast<Enum>(static_cast<underlying_type>(value));
  return true;
}

template <typename Wrapper>
[[nodiscard]] object* wrapper_object_ref_value(const void* storage) noexcept {
  if (storage == nullptr) {
    return nullptr;
  }

  if constexpr (requires(const Wrapper& source) {
                  wrapper_access<Wrapper>::object_value(source);
                }) {
    using result_type =
        decltype(wrapper_access<Wrapper>::object_value(std::declval<const Wrapper&>()));
    if constexpr (std::is_convertible_v<result_type, object*>) {
      return wrapper_access<Wrapper>::object_value(*static_cast<const Wrapper*>(storage));
    } else {
      return nullptr;
    }
  } else {
    return nullptr;
  }
}

template <typename Wrapper>
[[nodiscard]] bool wrapper_set_object_ref(void* storage, object* instance) noexcept {
  if (storage == nullptr) {
    return false;
  }

  if constexpr (requires(Wrapper& source, object* object_instance) {
                  wrapper_access<Wrapper>::set_object(source, object_instance);
                }) {
    using result_type = decltype(wrapper_access<Wrapper>::set_object(std::declval<Wrapper&>(),
                                                                     std::declval<object*>()));
    if constexpr (std::is_convertible_v<result_type, bool>) {
      return wrapper_access<Wrapper>::set_object(*static_cast<Wrapper*>(storage), instance);
    } else {
      return false;
    }
  } else {
    return false;
  }
}

template <typename T> [[nodiscard]] type_id semantic_type_id() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (has_type_name_v<type>) {
    const error result = get_meta_registry().register_type_for<type>(
        make_meta_type_data<type>(type_name<type>::value));
    if (result && result.value() != errc::duplicate_id) {
      return type_id::invalid();
    }
  } else {
    (void)get_meta_registry().ensure_type_for<type>();
  }

  const std::optional<meta_type> registered = get_meta_registry().find_type<type>();
  return registered.has_value() ? registered->id() : type_id::invalid();
}

template <typename T, typename = void> struct has_const_container_iterator : std::false_type {};

template <typename T>
struct has_const_container_iterator<T, std::void_t<decltype(std::declval<const T&>().begin()),
                                                   decltype(std::declval<const T&>().end())>>
    : std::true_type {};

template <typename T>
inline constexpr bool has_const_container_iterator_v = has_const_container_iterator<T>::value;

template <typename Iterator>
[[nodiscard]] void* clone_container_iterator(const void* iterator) noexcept {
  if (iterator == nullptr) {
    return nullptr;
  }

  return new (std::nothrow) Iterator(*static_cast<const Iterator*>(iterator));
}

template <typename Iterator> void destroy_container_iterator(void* iterator) noexcept {
  delete static_cast<Iterator*>(iterator);
}

template <typename Iterator> void advance_container_iterator(void* iterator) noexcept {
  if (iterator != nullptr) {
    ++(*static_cast<Iterator*>(iterator));
  }
}

template <typename Iterator>
[[nodiscard]] bool equal_container_iterator(const void* lhs, const void* rhs) noexcept {
  if (lhs == nullptr || rhs == nullptr) {
    return lhs == rhs;
  }

  return *static_cast<const Iterator*>(lhs) == *static_cast<const Iterator*>(rhs);
}

template <typename Container>
[[nodiscard]] void* begin_container_iterator(const void* storage) noexcept {
  if (storage == nullptr) {
    return nullptr;
  }

  using iterator_type = decltype(std::declval<const Container&>().begin());
  return new (std::nothrow) iterator_type(static_cast<const Container*>(storage)->begin());
}

template <typename Container>
[[nodiscard]] void* end_container_iterator(const void* storage) noexcept {
  if (storage == nullptr) {
    return nullptr;
  }

  using iterator_type = decltype(std::declval<const Container&>().end());
  return new (std::nothrow) iterator_type(static_cast<const Container*>(storage)->end());
}

template <typename Iterator>
[[nodiscard]] const void* set_iterator_value(const void* iterator) noexcept {
  if (iterator == nullptr) {
    return nullptr;
  }

  return &**static_cast<const Iterator*>(iterator);
}

template <typename Iterator>
[[nodiscard]] const void* associative_iterator_key(const void* iterator) noexcept {
  if (iterator == nullptr) {
    return nullptr;
  }

  return &(*static_cast<const Iterator*>(iterator))->first;
}

template <typename Iterator>
[[nodiscard]] const void* associative_iterator_value(const void* iterator) noexcept {
  if (iterator == nullptr) {
    return nullptr;
  }

  return &(*static_cast<const Iterator*>(iterator))->second;
}

template <typename T> [[nodiscard]] error register_associative_data_for(type_id id) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (associative_traits<type>::is_associative) {
    using key_type = typename associative_traits<type>::key_type;
    using mapped_type = typename associative_traits<type>::mapped_type;

    const type_id key = semantic_type_id<key_type>();
    const type_id value = semantic_type_id<mapped_type>();
    if (!id || !key || !value) {
      return {};
    }

    meta_associative_data data{
        key,
        value,
        [](const void* storage) noexcept -> std::size_t {
          if (storage == nullptr) {
            return 0;
          }

          return associative_traits<type>::size(*static_cast<const type*>(storage));
        },
        [](const void* storage, meta_associative_raw_visitor visitor, void* user_data) -> bool {
          if (storage == nullptr || visitor == nullptr) {
            return false;
          }

          bool keep_going = true;
          associative_traits<type>::for_each(*static_cast<const type*>(storage),
                                             [&](const key_type& key, const mapped_type& mapped) {
                                               if (!keep_going) {
                                                 return;
                                               }

                                               keep_going = visitor(&key, &mapped, user_data);
                                             });

          return keep_going;
        },
        [](void* storage) noexcept {
          if (storage != nullptr) {
            associative_traits<type>::clear(*static_cast<type*>(storage));
          }
        },
        [](void* storage, const void* key, const void* mapped) -> bool {
          if (storage == nullptr || key == nullptr || mapped == nullptr) {
            return false;
          }

          return associative_traits<type>::insert(*static_cast<type*>(storage),
                                                  *static_cast<const key_type*>(key),
                                                  *static_cast<const mapped_type*>(mapped));
        },
    };

    if constexpr (has_const_container_iterator_v<type>) {
      using iterator_type = decltype(std::declval<const type&>().begin());
      data.begin = &begin_container_iterator<type>;
      data.end = &end_container_iterator<type>;
      data.iterator_clone = &clone_container_iterator<iterator_type>;
      data.iterator_destroy = &destroy_container_iterator<iterator_type>;
      data.iterator_advance = &advance_container_iterator<iterator_type>;
      data.iterator_equal = &equal_container_iterator<iterator_type>;
      data.iterator_key = &associative_iterator_key<iterator_type>;
      data.iterator_value = &associative_iterator_value<iterator_type>;
    }

    const error result = get_meta_registry().emplace<meta_associative_data>(id, data);
    if (result && result.value() != errc::duplicate_component) {
      return result;
    }
  }

  return {};
}

template <typename T> [[nodiscard]] error register_pair_data_for(type_id id) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (pair_traits<type>::is_pair) {
    using first_type = typename pair_traits<type>::first_type;
    using second_type = typename pair_traits<type>::second_type;

    const type_id key = semantic_type_id<first_type>();
    const type_id value = semantic_type_id<second_type>();
    if (!id || !key || !value) {
      return {};
    }

    meta_pair_data data{
        key,
        value,
        [](const void* storage) noexcept -> const void* {
          if (storage == nullptr) {
            return nullptr;
          }

          return pair_traits<type>::first(*static_cast<const type*>(storage));
        },
        [](void* storage) noexcept -> void* {
          if (storage == nullptr) {
            return nullptr;
          }

          return pair_traits<type>::mutable_first(*static_cast<type*>(storage));
        },
        [](const void* storage) noexcept -> const void* {
          if (storage == nullptr) {
            return nullptr;
          }

          return pair_traits<type>::second(*static_cast<const type*>(storage));
        },
        [](void* storage) noexcept -> void* {
          if (storage == nullptr) {
            return nullptr;
          }

          return pair_traits<type>::mutable_second(*static_cast<type*>(storage));
        },
    };

    const error result = get_meta_registry().emplace<meta_pair_data>(id, data);
    if (result && result.value() != errc::duplicate_component) {
      return result;
    }
  }

  return {};
}

template <typename Tuple, std::size_t Index>
[[nodiscard]] meta_tuple_element_data make_tuple_element_data() {
  using type = std::remove_cv_t<std::remove_reference_t<Tuple>>;
  using element_type = typename tuple_traits<type>::template element_type<Index>;

  return meta_tuple_element_data{
      semantic_type_id<element_type>(),
      [](const void* storage) noexcept -> const void* {
        if (storage == nullptr) {
          return nullptr;
        }

        return tuple_traits<type>::template element<Index>(*static_cast<const type*>(storage));
      },
      [](void* storage) noexcept -> void* {
        if (storage == nullptr) {
          return nullptr;
        }

        return tuple_traits<type>::template mutable_element<Index>(*static_cast<type*>(storage));
      },
  };
}

template <typename Tuple, std::size_t... Indices>
[[nodiscard]] meta_tuple_data make_tuple_data(std::index_sequence<Indices...>) {
  meta_tuple_data data{};
  data.elements.reserve(sizeof...(Indices));
  (data.elements.push_back(make_tuple_element_data<Tuple, Indices>()), ...);
  return data;
}

template <typename T> [[nodiscard]] error register_tuple_data_for(type_id id) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (tuple_traits<type>::is_tuple) {
    if (!id) {
      return {};
    }

    meta_tuple_data data =
        make_tuple_data<type>(std::make_index_sequence<tuple_traits<type>::size>{});
    if (!data.valid()) {
      return {};
    }

    const error result = get_meta_registry().emplace<meta_tuple_data>(id, data);
    if (result && result.value() != errc::duplicate_component) {
      return result;
    }
  }

  return {};
}

template <typename Variant, std::size_t Index>
[[nodiscard]] meta_variant_alternative_data make_variant_alternative_data() {
  using type = std::remove_cv_t<std::remove_reference_t<Variant>>;
  using alternative_type = typename variant_traits<type>::template alternative_type<Index>;

  return meta_variant_alternative_data{
      semantic_type_id<alternative_type>(),
      [](const void* storage) noexcept -> const void* {
        if (storage == nullptr) {
          return nullptr;
        }

        return variant_traits<type>::template alternative<Index>(
            *static_cast<const type*>(storage));
      },
      [](void* storage) noexcept -> void* {
        if (storage == nullptr) {
          return nullptr;
        }

        return variant_traits<type>::template mutable_alternative<Index>(
            *static_cast<type*>(storage));
      },
      [](void* storage) -> bool {
        if (storage == nullptr) {
          return false;
        }

        return variant_traits<type>::template emplace<Index>(*static_cast<type*>(storage));
      },
  };
}

template <typename Variant, std::size_t... Indices>
[[nodiscard]] meta_variant_data make_variant_data(std::index_sequence<Indices...>) {
  using type = std::remove_cv_t<std::remove_reference_t<Variant>>;

  meta_variant_data data{};
  data.alternatives.reserve(sizeof...(Indices));
  (data.alternatives.push_back(make_variant_alternative_data<type, Indices>()), ...);
  data.index = [](const void* storage) noexcept -> std::size_t {
    if (storage == nullptr) {
      return static_cast<std::size_t>(-1);
    }

    return variant_traits<type>::index(*static_cast<const type*>(storage));
  };
  return data;
}

template <typename T> [[nodiscard]] error register_variant_data_for(type_id id) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (variant_traits<type>::is_variant) {
    if (!id) {
      return {};
    }

    meta_variant_data data =
        make_variant_data<type>(std::make_index_sequence<variant_traits<type>::size>{});
    if (!data.valid()) {
      return {};
    }

    const error result = get_meta_registry().emplace<meta_variant_data>(id, data);
    if (result && result.value() != errc::duplicate_component) {
      return result;
    }
  }

  return {};
}

template <typename T> [[nodiscard]] error register_sequence_data_for(type_id id) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (sequence_traits<type>::is_sequence) {
    using element_type = typename sequence_traits<type>::element_type;

    const type_id value = semantic_type_id<element_type>();
    if (!id || !value) {
      return {};
    }

    meta_sequence_data data{
        value,
        [](const void* storage) noexcept -> std::size_t {
          if (storage == nullptr) {
            return 0;
          }

          return sequence_traits<type>::size(*static_cast<const type*>(storage));
        },
        [](const void* storage, std::size_t index) noexcept -> const void* {
          if (storage == nullptr) {
            return nullptr;
          }

          return sequence_traits<type>::element(*static_cast<const type*>(storage), index);
        },
        [](void* storage, std::size_t index) noexcept -> void* {
          if (storage == nullptr) {
            return nullptr;
          }

          return sequence_traits<type>::mutable_element(*static_cast<type*>(storage), index);
        },
        [](void* storage) noexcept {
          if (storage != nullptr) {
            sequence_traits<type>::clear(*static_cast<type*>(storage));
          }
        },
        [](void* storage, std::size_t size) -> bool {
          if (storage == nullptr) {
            return false;
          }

          return sequence_traits<type>::resize(*static_cast<type*>(storage), size);
        },
        [](void* storage, const void* element) -> bool {
          if (storage == nullptr || element == nullptr) {
            return false;
          }

          return sequence_traits<type>::append(*static_cast<type*>(storage),
                                               *static_cast<const element_type*>(element));
        },
    };

    if constexpr (requires { sequence_traits<type>::read_in_place; }) {
      data.read_in_place = sequence_traits<type>::read_in_place;
    }

    const error result = get_meta_registry().emplace<meta_sequence_data>(id, data);
    if (result && result.value() != errc::duplicate_component) {
      return result;
    }
  }

  return {};
}

template <typename T> [[nodiscard]] error register_set_data_for(type_id id) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (set_traits<type>::is_set) {
    using element_type = typename set_traits<type>::element_type;

    const type_id value = semantic_type_id<element_type>();
    if (!id || !value) {
      return {};
    }

    meta_set_data data{
        value,
        [](const void* storage) noexcept -> std::size_t {
          if (storage == nullptr) {
            return 0;
          }

          return set_traits<type>::size(*static_cast<const type*>(storage));
        },
        [](const void* storage, meta_set_raw_visitor visitor, void* user_data) -> bool {
          if (storage == nullptr || visitor == nullptr) {
            return false;
          }

          bool keep_going = true;
          set_traits<type>::for_each(*static_cast<const type*>(storage),
                                     [&](const element_type& value) {
                                       if (!keep_going) {
                                         return;
                                       }

                                       keep_going = visitor(&value, user_data);
                                     });

          return keep_going;
        },
        [](void* storage) noexcept {
          if (storage != nullptr) {
            set_traits<type>::clear(*static_cast<type*>(storage));
          }
        },
        [](void* storage, const void* element) -> bool {
          if (storage == nullptr || element == nullptr) {
            return false;
          }

          return set_traits<type>::insert(*static_cast<type*>(storage),
                                          *static_cast<const element_type*>(element));
        },
    };

    if constexpr (has_const_container_iterator_v<type>) {
      using iterator_type = decltype(std::declval<const type&>().begin());
      data.begin = &begin_container_iterator<type>;
      data.end = &end_container_iterator<type>;
      data.iterator_clone = &clone_container_iterator<iterator_type>;
      data.iterator_destroy = &destroy_container_iterator<iterator_type>;
      data.iterator_advance = &advance_container_iterator<iterator_type>;
      data.iterator_equal = &equal_container_iterator<iterator_type>;
      data.iterator_value = &set_iterator_value<iterator_type>;
    }

    const error result = get_meta_registry().emplace<meta_set_data>(id, data);
    if (result && result.value() != errc::duplicate_component) {
      return result;
    }
  }

  return {};
}

template <typename T> [[nodiscard]] error register_container_data_for(type_id id) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (sequence_traits<type>::is_sequence) {
    return register_sequence_data_for<type>(id);
  } else if constexpr (set_traits<type>::is_set) {
    return register_set_data_for<type>(id);
  } else if constexpr (associative_traits<type>::is_associative) {
    return register_associative_data_for<type>(id);
  } else if constexpr (pair_traits<type>::is_pair) {
    return register_pair_data_for<type>(id);
  } else if constexpr (tuple_traits<type>::is_tuple) {
    return register_tuple_data_for<type>(id);
  } else if constexpr (variant_traits<type>::is_variant) {
    return register_variant_data_for<type>(id);
  }

  return {};
}

template <typename Type> struct type_semantics_traits {
  using type = std::remove_cv_t<std::remove_reference_t<Type>>;

  [[nodiscard]] static meta_type_semantics_data get() {
    meta_type_semantics_data info{};
    if constexpr (sequence_traits<type>::is_sequence) {
      using element_type = typename sequence_traits<type>::element_type;

      info.kind = type_kind::sequence;
      info.value_type = semantic_type_id<element_type>();
      return info;
    } else if constexpr (set_traits<type>::is_set) {
      using element_type = typename set_traits<type>::element_type;

      info.kind = type_kind::set;
      info.value_type = semantic_type_id<element_type>();
      return info;
    } else if constexpr (associative_traits<type>::is_associative) {
      using key_type = typename associative_traits<type>::key_type;
      using mapped_type = typename associative_traits<type>::mapped_type;

      info.kind = type_kind::associative;
      info.key_type = semantic_type_id<key_type>();
      info.value_type = semantic_type_id<mapped_type>();
      return info;
    } else if constexpr (pair_traits<type>::is_pair) {
      using first_type = typename pair_traits<type>::first_type;
      using second_type = typename pair_traits<type>::second_type;

      info.kind = type_kind::pair;
      info.key_type = semantic_type_id<first_type>();
      info.value_type = semantic_type_id<second_type>();
      return info;
    } else if constexpr (tuple_traits<type>::is_tuple) {
      info.kind = type_kind::tuple;
      return info;
    } else if constexpr (variant_traits<type>::is_variant) {
      info.kind = type_kind::variant;
      return info;
    } else if constexpr (wrapper_traits<type>::is_wrapper) {
      using wrapped_type = typename wrapper_traits<type>::wrapped_type;
      using value_type = std::remove_cv_t<wrapped_type>;

      static_assert(!is_nested_type_indirection_v<value_type>,
                    "CAORS meta supports at most one pointer or wrapper level");

      info.kind = type_kind::wrapper;
      info.wrapper_semantics = wrapper_traits<type>::semantics;
      info.value_type = semantic_type_id<value_type>();
      info.indirect_value = &wrapper_indirect_value<type>;
      info.mutable_indirect_value = &wrapper_mutable_indirect_value<type>;
      info.has_indirect_value = &wrapper_has_indirect_value<type>;
      info.reset_indirect_value = &wrapper_reset_indirect_value<type>;
      info.ensure_indirect_value = &wrapper_ensure_indirect_value<type>;
      if constexpr (wrapper_traits<type>::semantics == wrapper_semantics::object_ref) {
        info.get_object_ref = &wrapper_object_ref_value<type>;
        info.set_object_ref = &wrapper_set_object_ref<type>;
      }
      info.nullable = wrapper_traits<type>::nullable;
      return info;
    } else {
      if constexpr (std::is_enum_v<type>) {
        using underlying_type = std::underlying_type_t<type>;
        info.kind = type_kind::enumeration;
        info.enum_to_i64 = &enum_to_i64<type>;
        info.enum_to_u64 = &enum_to_u64<type>;
        info.enum_from_i64 = &enum_from_i64<type>;
        info.enum_from_u64 = &enum_from_u64<type>;
        info.enum_signed = std::is_signed_v<underlying_type>;
      } else {
        info.kind = type_kind::value;
      }
      return info;
    }
  }
};

template <typename T> struct type_semantics_traits<T*> {
  using value_type = std::remove_cv_t<T>;

  static_assert(!is_nested_type_indirection_v<value_type>,
                "CAORS meta supports at most one pointer or wrapper level");

  [[nodiscard]] static meta_type_semantics_data get() {
    meta_type_semantics_data info{};
    info.kind = type_kind::raw_pointer;
    info.wrapper_semantics = wrapper_semantics::inline_value;
    info.value_type = semantic_type_id<value_type>();
    info.indirect_value = &raw_pointer_value<T>;
    info.mutable_indirect_value = &raw_pointer_mutable_value<T>;
    info.has_indirect_value = &raw_pointer_has_value<T>;
    info.reset_indirect_value = &raw_pointer_reset<T>;
    info.ensure_indirect_value = &raw_pointer_ensure_value<T>;
    info.pointer_depth = 1;
    info.nullable = true;
    return info;
  }
};

template <typename T> [[nodiscard]] meta_type_semantics_data make_type_semantics_data() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  return type_semantics_traits<type>::get();
}

template <typename T> [[nodiscard]] error ensure_type_semantics_for(type_id id) {
  if (!id) {
    return {error_domain::meta, errc::invalid_type};
  }

  if (get_meta_registry().has<meta_type_semantics_data>(id)) {
    return {};
  }

  meta_type_semantics_data semantics = make_type_semantics_data<T>();
  if (!semantics.value_type && semantics.kind != type_kind::tuple &&
      semantics.kind != type_kind::variant) {
    semantics.value_type = id;
  }

  const error result = get_meta_registry().emplace<meta_type_semantics_data>(id, semantics);
  if (result && result.value() != errc::duplicate_component) {
    return result;
  }

  return {};
}

} // namespace detail

/** @brief 获取指定 C++ 类型的运行期类型 id，必要时先创建匿名类型记录。 */
template <typename T> [[nodiscard]] type_id type_id_of() {
  return get_meta_registry().ensure_type_for<T>().id();
}

/** @brief RTTR 风格的元类型注册构建器。 */
template <typename T> class meta_builder {
public:
  meta_builder() : type_(get_meta_registry().ensure_type_for<T>()) {}

  meta_builder& type(std::string_view name) {
    status_ = get_meta_registry().register_type_for<T>(make_meta_type_data<T>(name));
    refresh();
    return *this;
  }

  meta_builder& guid(type_guid guid_value) {
    if (!type_) {
      status_ = {error_domain::meta, errc::invalid_type};
      return *this;
    }

    meta_type_data data{
        type_.id(), type_.name(), guid_value, type_.size(), type_.alignment(), type_.parent_id(),
    };
    status_ = get_meta_registry().register_type_for<T>(data);
    refresh();
    return *this;
  }

  meta_builder& guid(caors::guid guid_value) { return guid(type_guid{guid_value}); }

  meta_builder& parent(type_id parent_id) {
    if (!type_) {
      status_ = {error_domain::meta, errc::invalid_type};
      return *this;
    }

    meta_type_data data{
        type_.id(), type_.name(), type_.guid(), type_.size(), type_.alignment(), parent_id,
    };
    status_ = get_meta_registry().register_type_for<T>(data);
    refresh();
    return *this;
  }

  [[nodiscard]] meta_type get() const noexcept { return type_; }
  [[nodiscard]] error result() const noexcept { return status_; }
  [[nodiscard]] bool succeeded() const noexcept {
    return status_.succeeded() || status_.value() == errc::duplicate_id;
  }

  [[nodiscard]] operator meta_type() const noexcept { return type_; }

private:
  void refresh() {
    std::optional<meta_type> refreshed = get_meta_registry().find_type<T>();
    if (refreshed.has_value()) {
      type_ = *refreshed;
    }
  }

  meta_type type_{};
  error status_{};
};

/** @brief 获取或创建指定 C++ 类型的元类型注册构建器。 */
template <typename T> [[nodiscard]] meta_builder<T> meta() { return meta_builder<T>{}; }

/** @brief 向全局元数据注册表注册基础类型。 */
template <typename T>
[[nodiscard]] error register_type(string_name name, type_id parent_id = type_id::invalid()) {
  return get_meta_registry().register_type_for<T>(make_meta_type_data<T>(name, parent_id));
}

/** @brief 向全局元数据注册表注册带稳定 GUID 的基础类型。 */
template <typename T>
[[nodiscard]] error register_type(string_name name, type_guid guid,
                                  type_id parent_id = type_id::invalid()) {
  return get_meta_registry().register_type_for<T>(make_meta_type_data<T>(name, guid, parent_id));
}

/** @brief 向全局元数据注册表注册基础类型。 */
template <typename T>
[[nodiscard]] error register_type(std::string_view name, type_id parent_id = type_id::invalid()) {
  return register_type<T>(string_name{name}, parent_id);
}

/** @brief 向全局元数据注册表注册带稳定 GUID 的基础类型。 */
template <typename T>
[[nodiscard]] error register_type(std::string_view name, type_guid guid,
                                  type_id parent_id = type_id::invalid()) {
  return register_type<T>(string_name{name}, guid, parent_id);
}

/** @brief 在全局元数据注册表中按 C++ 类型查找基础类型。 */
template <typename T> [[nodiscard]] std::optional<meta_type> find_type() {
  return get_meta_registry().find_type<T>();
}

/** @brief 在全局元数据注册表中按类型 id 查找基础类型。 */
[[nodiscard]] CAORS_API std::optional<meta_type> find_type(type_id id);

/** @brief 在全局元数据注册表中按稳定 GUID 查找基础类型。 */
[[nodiscard]] CAORS_API std::optional<meta_type> find_type(type_guid guid);

/** @brief 在全局元数据注册表中按类型名查找基础类型。 */
[[nodiscard]] CAORS_API std::optional<meta_type> find_type(std::string_view name);

/** @brief 在全局元数据注册表中按类型名查找基础类型。 */
[[nodiscard]] CAORS_API std::optional<meta_type> find_type(string_name name);

/** @brief 在全局元数据注册表中判断类型继承关系。 */
[[nodiscard]] CAORS_API bool is_a(type_id id, type_id base_id);

template <typename... Components>
template <typename Component>
[[nodiscard]] std::optional<Component> meta_view<Components...>::find(type_id id) const {
  if (registry_ == nullptr) {
    return std::nullopt;
  }

  return registry_->template find<Component>(id);
}

template <typename... Components>
[[nodiscard]] meta_view<Components...> meta_registry::view() const {
  static_assert(sizeof...(Components) > 0, "Meta view requires at least one component type");

  detail::meta_component_key keys[] = {
      detail::meta_component_key_of<std::decay_t<Components>>()...};
  return meta_view<Components...>{this, view_component_ids(keys, sizeof...(Components))};
}

} // namespace caors

#endif
