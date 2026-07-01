// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_VALUE_ANY_H_
#define CAORS_VALUE_ANY_H_

#include "caors/meta.h"
#include "caors/reflection/container_traits.h"

#include <cstddef>
#include <new>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors {

class any_ref;
class sequence_view;
class set_view;
class associative_view;

namespace detail {

using value_clone_fn = void* (*)(const void*);
using value_destroy_fn = void (*)(void*) noexcept;
using value_assign_fn = bool (*)(void*, const void*);
using set_raw_visitor = bool (*)(const void*, void*);
using sequence_size_fn = std::size_t (*)(const void*) noexcept;
using sequence_const_element_fn = const void* (*)(const void*, std::size_t) noexcept;
using sequence_mutable_element_fn = void* (*)(void*, std::size_t) noexcept;
using sequence_clear_fn = void (*)(void*) noexcept;
using sequence_resize_fn = bool (*)(void*, std::size_t);
using sequence_append_fn = bool (*)(void*, const void*);
using set_size_fn = std::size_t (*)(const void*) noexcept;
using set_visit_fn = bool (*)(const void*, set_raw_visitor, void*);
using set_clear_fn = void (*)(void*) noexcept;
using set_insert_fn = bool (*)(void*, const void*);
using associative_raw_visitor = bool (*)(const void*, const void*, void*);
using associative_size_fn = std::size_t (*)(const void*) noexcept;
using associative_visit_fn = bool (*)(const void*, associative_raw_visitor, void*);
using associative_clear_fn = void (*)(void*) noexcept;
using associative_insert_fn = bool (*)(void*, const void*, const void*);

template <typename T> void* clone_value(const void* source) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_copy_constructible_v<type>) {
    return new type(*static_cast<const type*>(source));
  } else {
    return nullptr;
  }
}

template <typename T> void destroy_value(void* value) noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  delete static_cast<type*>(value);
}

template <typename T> bool assign_value(void* target, const void* source) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_assignable_v<type&, const type&>) {
    *static_cast<type*>(target) = *static_cast<const type*>(source);
    return true;
  } else {
    return false;
  }
}

template <typename T> [[nodiscard]] value_clone_fn value_clone_fn_of() noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_copy_constructible_v<type>) {
    return &clone_value<type>;
  } else {
    return nullptr;
  }
}

template <typename T> [[nodiscard]] value_destroy_fn value_destroy_fn_of() noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_copy_constructible_v<type>) {
    return &destroy_value<type>;
  } else {
    return nullptr;
  }
}

template <typename T> [[nodiscard]] value_assign_fn value_assign_fn_of() noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_assignable_v<type&, const type&>) {
    return &assign_value<type>;
  } else {
    return nullptr;
  }
}

struct associative_type_data {
  type_id key_type{};
  string_name key_type_name{};
  cpp_type_key key_cpp_type = nullptr;
  std::size_t key_size = 0;
  std::size_t key_alignment = 0;
  value_clone_fn clone_key = nullptr;
  value_destroy_fn destroy_key = nullptr;
  value_assign_fn assign_key = nullptr;
  type_id mapped_type{};
  string_name mapped_type_name{};
  cpp_type_key mapped_cpp_type = nullptr;
  std::size_t mapped_size = 0;
  std::size_t mapped_alignment = 0;
  value_clone_fn clone_mapped = nullptr;
  value_destroy_fn destroy_mapped = nullptr;
  value_assign_fn assign_mapped = nullptr;
  associative_size_fn size = nullptr;
  associative_visit_fn visit = nullptr;
  associative_clear_fn clear = nullptr;
  associative_insert_fn insert = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return key_type && !key_type_name.empty() && key_cpp_type != nullptr && key_size > 0 &&
           key_alignment > 0 && mapped_type && !mapped_type_name.empty() &&
           mapped_cpp_type != nullptr && mapped_size > 0 && mapped_alignment > 0 &&
           size != nullptr && visit != nullptr;
  }
};

struct sequence_type_data {
  type_id element_type{};
  string_name element_type_name{};
  cpp_type_key element_cpp_type = nullptr;
  std::size_t element_size = 0;
  std::size_t element_alignment = 0;
  value_clone_fn clone_element = nullptr;
  value_destroy_fn destroy_element = nullptr;
  value_assign_fn assign_element = nullptr;
  sequence_size_fn size = nullptr;
  sequence_const_element_fn element = nullptr;
  sequence_mutable_element_fn mutable_element = nullptr;
  sequence_clear_fn clear = nullptr;
  sequence_resize_fn resize = nullptr;
  sequence_append_fn append = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return element_type && !element_type_name.empty() && element_cpp_type != nullptr &&
           element_size > 0 && element_alignment > 0 && size != nullptr && element != nullptr;
  }
};

struct set_type_data {
  type_id element_type{};
  string_name element_type_name{};
  cpp_type_key element_cpp_type = nullptr;
  std::size_t element_size = 0;
  std::size_t element_alignment = 0;
  value_clone_fn clone_element = nullptr;
  value_destroy_fn destroy_element = nullptr;
  value_assign_fn assign_element = nullptr;
  set_size_fn size = nullptr;
  set_visit_fn visit = nullptr;
  set_clear_fn clear = nullptr;
  set_insert_fn insert = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return element_type && !element_type_name.empty() && element_cpp_type != nullptr &&
           element_size > 0 && element_alignment > 0 && size != nullptr && visit != nullptr;
  }
};

template <typename T> [[nodiscard]] std::optional<meta_type> ensure_value_type_registered() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (has_type_name_v<type>) {
    const error result = caors::register_type<type>(type_name<type>::value);
    if (result && result.value() != errc::duplicate_id) {
      return std::nullopt;
    }
  }

  return caors::find_type<type>();
}

template <typename T> [[nodiscard]] error register_associative_type_for(type_id id) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (associative_traits<type>::is_associative) {
    using key_type = typename associative_traits<type>::key_type;
    using mapped_type = typename associative_traits<type>::mapped_type;

    const std::optional<meta_type> key = ensure_value_type_registered<key_type>();
    const std::optional<meta_type> mapped = ensure_value_type_registered<mapped_type>();
    if (!id || !key.has_value() || !mapped.has_value()) {
      return {};
    }

    associative_type_data data{
        key->id(),
        key->name(),
        cpp_type_key_of<key_type>(),
        sizeof(key_type),
        alignof(key_type),
        value_clone_fn_of<key_type>(),
        value_destroy_fn_of<key_type>(),
        value_assign_fn_of<key_type>(),
        mapped->id(),
        mapped->name(),
        cpp_type_key_of<mapped_type>(),
        sizeof(mapped_type),
        alignof(mapped_type),
        value_clone_fn_of<mapped_type>(),
        value_destroy_fn_of<mapped_type>(),
        value_assign_fn_of<mapped_type>(),
        [](const void* storage) noexcept -> std::size_t {
          if (storage == nullptr) {
            return 0;
          }

          return associative_traits<type>::size(*static_cast<const type*>(storage));
        },
        [](const void* storage, associative_raw_visitor visitor, void* user_data) -> bool {
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

    const error result = get_meta_registry().emplace<associative_type_data>(id, data);
    if (result && result.value() != errc::duplicate_component) {
      return result;
    }
  }

  return {};
}

template <typename T> [[nodiscard]] error register_sequence_type_for(type_id id) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (sequence_traits<type>::is_sequence) {
    using element_type = typename sequence_traits<type>::element_type;

    const std::optional<meta_type> element = ensure_value_type_registered<element_type>();
    if (!id || !element.has_value()) {
      return {};
    }

    sequence_type_data data{
        element->id(),
        element->name(),
        cpp_type_key_of<element_type>(),
        sizeof(element_type),
        alignof(element_type),
        value_clone_fn_of<element_type>(),
        value_destroy_fn_of<element_type>(),
        value_assign_fn_of<element_type>(),
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

    const error result = get_meta_registry().emplace<sequence_type_data>(id, data);
    if (result && result.value() != errc::duplicate_component) {
      return result;
    }
  }

  return {};
}

template <typename T> [[nodiscard]] error register_set_type_for(type_id id) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (set_traits<type>::is_set) {
    using element_type = typename set_traits<type>::element_type;

    const std::optional<meta_type> element = ensure_value_type_registered<element_type>();
    if (!id || !element.has_value()) {
      return {};
    }

    set_type_data data{
        element->id(),
        element->name(),
        cpp_type_key_of<element_type>(),
        sizeof(element_type),
        alignof(element_type),
        value_clone_fn_of<element_type>(),
        value_destroy_fn_of<element_type>(),
        value_assign_fn_of<element_type>(),
        [](const void* storage) noexcept -> std::size_t {
          if (storage == nullptr) {
            return 0;
          }

          return set_traits<type>::size(*static_cast<const type*>(storage));
        },
        [](const void* storage, set_raw_visitor visitor, void* user_data) -> bool {
          if (storage == nullptr || visitor == nullptr) {
            return false;
          }

          bool keep_going = true;
          set_traits<type>::for_each(*static_cast<const type*>(storage),
                                     [&](const element_type& element) {
                                       if (!keep_going) {
                                         return;
                                       }

                                       keep_going = visitor(&element, user_data);
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

    const error result = get_meta_registry().emplace<set_type_data>(id, data);
    if (result && result.value() != errc::duplicate_component) {
      return result;
    }
  }

  return {};
}

template <typename T> [[nodiscard]] error register_container_types_for(type_id id) {
  if (const error result = register_sequence_type_for<T>(id); result) {
    return result;
  }
  if (const error result = register_set_type_for<T>(id); result) {
    return result;
  }
  return register_associative_type_for<T>(id);
}

} // namespace detail

using set_element_visitor = bool (*)(any_ref element, void* user_data);
using associative_entry_visitor = bool (*)(any_ref key, any_ref mapped, void* user_data);

/**
 * @brief 运行期 C++ 类型值。
 *
 * `any` 持有一份对象拷贝，并保存对应的运行期类型信息。它不引用原对象，因此可以安全地
 * 脱离字段所属对象生命周期使用。第一版只支持可拷贝类型；不可拷贝对象会产生无效 `any`。
 */
class any final {
public:
  any() noexcept = default;
  ~any() { reset(); }

  any(const any& other) { copy_from(other); }

  any(any&& other) noexcept { move_from(other); }

  any& operator=(const any& other) {
    if (this == &other) {
      return *this;
    }

    any copy{other};
    swap(copy);
    return *this;
  }

  any& operator=(any&& other) noexcept {
    if (this == &other) {
      return *this;
    }

    reset();
    move_from(other);
    return *this;
  }

  /**
   * @brief 从 C++ 值创建运行期值。
   *
   * 没有 `type_name<T>` 默认映射的类型需要使用显式类型名重载。
   */
  template <typename T> [[nodiscard]] static any from(const T& source) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    static_assert(detail::has_type_name_v<type>,
                  "any::from<T>() requires type_name<T> or an explicit type name");
    return from(source, caors::type_name<type>::value);
  }

  /** @brief 从 C++ 值和显式类型名创建运行期值。 */
  template <typename T> [[nodiscard]] static any from(const T& source, std::string_view name) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    static_assert(std::is_copy_constructible_v<type>, "any requires copy constructible types");

    const error result = caors::register_type<type>(name);
    if (result && result.value() != errc::duplicate_id) {
      return {};
    }

    const std::optional<meta_type> registered = caors::find_type<type>();
    if (!registered.has_value() || registered->name().empty()) {
      return {};
    }

    (void)detail::register_container_types_for<type>(registered->id());

    return copy_from_raw(registered->id(), registered->name(), detail::cpp_type_key_of<type>(),
                         &source, sizeof(type), alignof(type), detail::value_clone_fn_of<type>(),
                         detail::value_destroy_fn_of<type>(), detail::value_assign_fn_of<type>());
  }

  [[nodiscard]] bool valid() const noexcept {
    return static_cast<bool>(type_) && !type_name_.empty() && cpp_type_ != nullptr &&
           data_ != nullptr && size_ > 0 && alignment_ > 0 && clone_ != nullptr &&
           destroy_ != nullptr;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] type_id type() const noexcept { return type_; }
  [[nodiscard]] string_name type_name() const noexcept { return type_name_; }
  [[nodiscard]] detail::cpp_type_key cpp_type() const noexcept { return cpp_type_; }
  [[nodiscard]] const void* data() const noexcept { return valid() ? data_ : nullptr; }
  [[nodiscard]] void* mutable_data() noexcept { return valid() ? data_ : nullptr; }
  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] std::size_t alignment() const noexcept { return alignment_; }
  [[nodiscard]] sequence_view create_sequence_view();
  [[nodiscard]] sequence_view create_sequence_view() const;
  [[nodiscard]] set_view create_set_view();
  [[nodiscard]] set_view create_set_view() const;
  [[nodiscard]] associative_view create_associative_view();
  [[nodiscard]] associative_view create_associative_view() const;

  /** @brief 清空当前值。 */
  void reset() noexcept {
    if (data_ != nullptr && destroy_ != nullptr) {
      destroy_(data_);
    }

    type_ = {};
    type_name_ = {};
    cpp_type_ = nullptr;
    data_ = nullptr;
    size_ = 0;
    alignment_ = 0;
    clone_ = nullptr;
    destroy_ = nullptr;
    assign_ = nullptr;
  }

  /** @brief 交换两个运行期值。 */
  void swap(any& other) noexcept {
    using std::swap;
    swap(type_, other.type_);
    swap(type_name_, other.type_name_);
    swap(cpp_type_, other.cpp_type_);
    swap(data_, other.data_);
    swap(size_, other.size_);
    swap(alignment_, other.alignment_);
    swap(clone_, other.clone_);
    swap(destroy_, other.destroy_);
    swap(assign_, other.assign_);
  }

  /** @brief 判断当前值是否匹配指定 C++ 类型。 */
  template <typename T> [[nodiscard]] bool matches_type() const noexcept {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    return valid() && cpp_type_ == detail::cpp_type_key_of<type>() && size_ == sizeof(type) &&
           alignment_ == alignof(type);
  }

  /** @brief 判断两个运行期值是否具有相同 C++ 类型。 */
  [[nodiscard]] bool matches_type(const any& other) const noexcept {
    return valid() && other.valid() && cpp_type_ == other.cpp_type_ && size_ == other.size_ &&
           alignment_ == other.alignment_;
  }

  /** @brief 按指定类型获取只读指针。 */
  template <typename T>
  [[nodiscard]] const std::remove_cv_t<std::remove_reference_t<T>>* try_get() const noexcept {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    if (!matches_type<type>()) {
      return nullptr;
    }

    return static_cast<const type*>(data_);
  }

  /** @brief 按指定类型获取可写指针。 */
  template <typename T>
  [[nodiscard]] std::remove_cv_t<std::remove_reference_t<T>>* try_get_mutable() noexcept {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    if (!matches_type<type>()) {
      return nullptr;
    }

    return static_cast<type*>(data_);
  }

  /** @brief 从同类型 C++ 值写入当前值。 */
  template <typename T> bool try_set(const T& source) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    if (!matches_type<type>() || assign_ == nullptr) {
      return false;
    }

    return assign_(data_, &source);
  }

  /** @brief 从同类型运行期值写入当前值。 */
  bool try_set(const any& source) {
    if (!matches_type(source) || assign_ == nullptr) {
      return false;
    }

    return assign_(data_, source.data_);
  }

private:
  friend class any_ref;
  friend struct reflection_field_data;

  [[nodiscard]] static any copy_from_raw(type_id type, string_name type_name,
                                         detail::cpp_type_key cpp_type, const void* source,
                                         std::size_t size, std::size_t alignment,
                                         detail::value_clone_fn clone,
                                         detail::value_destroy_fn destroy,
                                         detail::value_assign_fn assign) {
    if (!type || type_name.empty() || cpp_type == nullptr || source == nullptr || size == 0 ||
        alignment == 0 || clone == nullptr || destroy == nullptr) {
      return {};
    }

    void* copy = clone(source);
    if (copy == nullptr) {
      return {};
    }

    any result;
    result.type_ = type;
    result.type_name_ = type_name;
    result.cpp_type_ = cpp_type;
    result.data_ = copy;
    result.size_ = size;
    result.alignment_ = alignment;
    result.clone_ = clone;
    result.destroy_ = destroy;
    result.assign_ = assign;
    return result;
  }

  void copy_from(const any& other) {
    if (!other.valid()) {
      return;
    }

    *this = copy_from_raw(other.type_, other.type_name_, other.cpp_type_, other.data_, other.size_,
                          other.alignment_, other.clone_, other.destroy_, other.assign_);
  }

  void move_from(any& other) noexcept {
    type_ = other.type_;
    type_name_ = other.type_name_;
    cpp_type_ = other.cpp_type_;
    data_ = other.data_;
    size_ = other.size_;
    alignment_ = other.alignment_;
    clone_ = other.clone_;
    destroy_ = other.destroy_;
    assign_ = other.assign_;

    other.type_ = {};
    other.type_name_ = {};
    other.cpp_type_ = nullptr;
    other.data_ = nullptr;
    other.size_ = 0;
    other.alignment_ = 0;
    other.clone_ = nullptr;
    other.destroy_ = nullptr;
    other.assign_ = nullptr;
  }

  type_id type_{};
  string_name type_name_{};
  detail::cpp_type_key cpp_type_ = nullptr;
  void* data_ = nullptr;
  std::size_t size_ = 0;
  std::size_t alignment_ = 0;
  detail::value_clone_fn clone_ = nullptr;
  detail::value_destroy_fn destroy_ = nullptr;
  detail::value_assign_fn assign_ = nullptr;
};

inline void swap(any& lhs, any& rhs) noexcept { lhs.swap(rhs); }

/**
 * @brief 运行期 C++ 类型引用。
 *
 * `any_ref` 不拥有对象，只保存外部对象地址和运行期类型信息。调用方必须保证被引用对象在
 * `any_ref` 使用期间仍然存活；可写引用只能从非 const 对象或可写字段创建。
 */
class any_ref final {
public:
  any_ref() noexcept = default;

  /** @brief 从可写 C++ 对象创建引用。 */
  template <typename T> [[nodiscard]] static any_ref from(T& source) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    static_assert(detail::has_type_name_v<type>,
                  "any_ref::from<T>() requires type_name<T> or an explicit type name");
    return from(source, caors::type_name<type>::value);
  }

  /** @brief 从可写 C++ 对象和显式类型名创建引用。 */
  template <typename T> [[nodiscard]] static any_ref from(T& source, std::string_view name) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    const error result = caors::register_type<type>(name);
    if (result && result.value() != errc::duplicate_id) {
      return {};
    }

    const std::optional<meta_type> registered = caors::find_type<type>();
    if (!registered.has_value() || registered->name().empty()) {
      return {};
    }

    (void)detail::register_container_types_for<type>(registered->id());

    return from_raw(registered->id(), registered->name(), detail::cpp_type_key_of<type>(), &source,
                    sizeof(type), alignof(type), detail::value_clone_fn_of<type>(),
                    detail::value_destroy_fn_of<type>(), detail::value_assign_fn_of<type>(),
                    !std::is_const_v<T>);
  }

  /** @brief 从只读 C++ 对象创建引用。 */
  template <typename T> [[nodiscard]] static any_ref from_const(const T& source) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    static_assert(detail::has_type_name_v<type>,
                  "any_ref::from_const<T>() requires type_name<T> or an explicit type name");
    return from_const(source, caors::type_name<type>::value);
  }

  /** @brief 从只读 C++ 对象和显式类型名创建引用。 */
  template <typename T>
  [[nodiscard]] static any_ref from_const(const T& source, std::string_view name) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    const error result = caors::register_type<type>(name);
    if (result && result.value() != errc::duplicate_id) {
      return {};
    }

    const std::optional<meta_type> registered = caors::find_type<type>();
    if (!registered.has_value() || registered->name().empty()) {
      return {};
    }

    (void)detail::register_container_types_for<type>(registered->id());

    return from_raw(registered->id(), registered->name(), detail::cpp_type_key_of<type>(), &source,
                    sizeof(type), alignof(type), detail::value_clone_fn_of<type>(),
                    detail::value_destroy_fn_of<type>(), detail::value_assign_fn_of<type>(), false);
  }

  [[nodiscard]] bool valid() const noexcept {
    return static_cast<bool>(type_) && !type_name_.empty() && cpp_type_ != nullptr &&
           data_ != nullptr && size_ > 0 && alignment_ > 0;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] type_id type() const noexcept { return type_; }
  [[nodiscard]] string_name type_name() const noexcept { return type_name_; }
  [[nodiscard]] detail::cpp_type_key cpp_type() const noexcept { return cpp_type_; }
  [[nodiscard]] const void* data() const noexcept { return valid() ? data_ : nullptr; }
  [[nodiscard]] void* mutable_data() const noexcept {
    return valid() && writable_ ? const_cast<void*>(data_) : nullptr;
  }
  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] std::size_t alignment() const noexcept { return alignment_; }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }
  [[nodiscard]] sequence_view create_sequence_view() const;
  [[nodiscard]] set_view create_set_view() const;
  [[nodiscard]] associative_view create_associative_view() const;

  /** @brief 判断当前引用是否匹配指定 C++ 类型。 */
  template <typename T> [[nodiscard]] bool matches_type() const noexcept {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    return valid() && cpp_type_ == detail::cpp_type_key_of<type>() && size_ == sizeof(type) &&
           alignment_ == alignof(type);
  }

  /** @brief 判断两个运行期值是否具有相同 C++ 类型。 */
  [[nodiscard]] bool matches_type(const any& other) const noexcept {
    return valid() && other.valid() && cpp_type_ == other.cpp_type() && size_ == other.size() &&
           alignment_ == other.alignment();
  }

  /** @brief 判断两个运行期引用是否具有相同 C++ 类型。 */
  [[nodiscard]] bool matches_type(const any_ref& other) const noexcept {
    return valid() && other.valid() && cpp_type_ == other.cpp_type_ && size_ == other.size_ &&
           alignment_ == other.alignment_;
  }

  /** @brief 按指定类型获取只读指针。 */
  template <typename T>
  [[nodiscard]] const std::remove_cv_t<std::remove_reference_t<T>>* try_get() const noexcept {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    if (!matches_type<type>()) {
      return nullptr;
    }

    return static_cast<const type*>(data_);
  }

  /** @brief 按指定类型获取可写指针。 */
  template <typename T>
  [[nodiscard]] std::remove_cv_t<std::remove_reference_t<T>>* try_get_mutable() const noexcept {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    if (!matches_type<type>() || !writable_) {
      return nullptr;
    }

    return static_cast<type*>(const_cast<void*>(data_));
  }

  /** @brief 从同类型 C++ 值写入当前引用指向的对象。 */
  template <typename T> bool try_set(const T& source) const {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    if (!matches_type<type>() || !writable_ || assign_ == nullptr) {
      return false;
    }

    return assign_(const_cast<void*>(data_), &source);
  }

  /** @brief 从同类型运行期值写入当前引用指向的对象。 */
  bool try_set(const any& source) const {
    if (!matches_type(source) || !writable_ || assign_ == nullptr) {
      return false;
    }

    return assign_(const_cast<void*>(data_), source.data());
  }

  /** @brief 从同类型运行期引用写入当前引用指向的对象。 */
  bool try_set(const any_ref& source) const {
    if (!matches_type(source) || !writable_ || assign_ == nullptr) {
      return false;
    }

    return assign_(const_cast<void*>(data_), source.data_);
  }

  /** @brief 尝试复制引用指向的对象。不可拷贝对象会返回无效 `any`。 */
  [[nodiscard]] any to_any() const {
    return any::copy_from_raw(type_, type_name_, cpp_type_, data_, size_, alignment_, clone_,
                              destroy_, assign_);
  }

private:
  friend class sequence_view;
  friend class set_view;
  friend class associative_view;
  friend struct reflection_field_data;

  [[nodiscard]] static any_ref from_raw(type_id type, string_name type_name,
                                        detail::cpp_type_key cpp_type, const void* data,
                                        std::size_t size, std::size_t alignment,
                                        detail::value_clone_fn clone,
                                        detail::value_destroy_fn destroy,
                                        detail::value_assign_fn assign, bool writable) noexcept {
    if (!type || type_name.empty() || cpp_type == nullptr || data == nullptr || size == 0 ||
        alignment == 0) {
      return {};
    }

    any_ref result;
    result.type_ = type;
    result.type_name_ = type_name;
    result.cpp_type_ = cpp_type;
    result.data_ = data;
    result.size_ = size;
    result.alignment_ = alignment;
    result.clone_ = clone;
    result.destroy_ = destroy;
    result.assign_ = assign;
    result.writable_ = writable;
    return result;
  }

  type_id type_{};
  string_name type_name_{};
  detail::cpp_type_key cpp_type_ = nullptr;
  const void* data_ = nullptr;
  std::size_t size_ = 0;
  std::size_t alignment_ = 0;
  detail::value_clone_fn clone_ = nullptr;
  detail::value_destroy_fn destroy_ = nullptr;
  detail::value_assign_fn assign_ = nullptr;
  bool writable_ = false;
};

/**
 * @brief 顺序容器运行期视图。
 *
 * `sequence_view` 不拥有容器，只引用 `any` 或 `any_ref`
 * 指向的顺序容器对象。只读视图允许查询和访问元素； 可写视图才允许清空、调整大小和追加元素。
 */
class sequence_view final {
public:
  sequence_view() noexcept = default;

  [[nodiscard]] bool valid() const noexcept {
    return data_ != nullptr && data_->valid() && storage_ != nullptr;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }

  [[nodiscard]] type_id element_type() const noexcept {
    return valid() ? data_->element_type : type_id{};
  }

  [[nodiscard]] string_name element_type_name() const noexcept {
    return valid() ? data_->element_type_name : string_name{};
  }

  /** @brief 获取顺序容器元素数量。 */
  [[nodiscard]] std::optional<std::size_t> size() const {
    if (!valid() || data_->size == nullptr) {
      return std::nullopt;
    }

    return data_->size(storage_);
  }

  /** @brief 获取指定元素引用。 */
  [[nodiscard]] any_ref element_ref(std::size_t index) const {
    if (!valid() || data_->element == nullptr) {
      return {};
    }

    const void* element = nullptr;
    bool element_writable = false;
    if (writable() && data_->mutable_element != nullptr) {
      element = data_->mutable_element(const_cast<void*>(storage_), index);
      element_writable = element != nullptr;
    } else {
      element = data_->element(storage_, index);
    }

    if (element == nullptr) {
      return {};
    }

    return any_ref::from_raw(data_->element_type, data_->element_type_name, data_->element_cpp_type,
                             element, data_->element_size, data_->element_alignment,
                             data_->clone_element, data_->destroy_element, data_->assign_element,
                             element_writable);
  }

  /** @brief 清空顺序容器。 */
  bool clear() const {
    if (!writable() || data_->clear == nullptr) {
      return false;
    }

    data_->clear(const_cast<void*>(storage_));
    return true;
  }

  /** @brief 调整顺序容器大小。 */
  bool resize(std::size_t size) const {
    if (!writable() || data_->resize == nullptr) {
      return false;
    }

    return data_->resize(const_cast<void*>(storage_), size);
  }

  /** @brief 追加一个元素。 */
  bool append_value(const any& value) const {
    if (!writable() || data_->append == nullptr || !value.valid() ||
        value.cpp_type() != data_->element_cpp_type || value.size() != data_->element_size ||
        value.alignment() != data_->element_alignment) {
      return false;
    }

    return data_->append(const_cast<void*>(storage_), value.data());
  }

private:
  friend class any;
  friend class any_ref;

  sequence_view(const detail::sequence_type_data* data, const void* storage, bool writable) noexcept
      : data_(data), storage_(storage), writable_(writable) {}

  const detail::sequence_type_data* data_ = nullptr;
  const void* storage_ = nullptr;
  bool writable_ = false;
};

/**
 * @brief 集合容器运行期视图。
 *
 * `set_view` 不拥有容器，只引用 `any` 或 `any_ref` 指向的集合容器对象。集合元素按 key
 * 语义暴露为只读引用； 可写视图允许清空和插入元素。
 */
class set_view final {
public:
  set_view() noexcept = default;

  [[nodiscard]] bool valid() const noexcept {
    return data_ != nullptr && data_->valid() && storage_ != nullptr;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }

  [[nodiscard]] type_id element_type() const noexcept {
    return valid() ? data_->element_type : type_id{};
  }

  [[nodiscard]] string_name element_type_name() const noexcept {
    return valid() ? data_->element_type_name : string_name{};
  }

  /** @brief 获取集合元素数量。 */
  [[nodiscard]] std::optional<std::size_t> size() const {
    if (!valid() || data_->size == nullptr) {
      return std::nullopt;
    }

    return data_->size(storage_);
  }

  /** @brief 遍历集合元素只读引用。 */
  bool for_each_ref(set_element_visitor visitor, void* user_data = nullptr) const {
    if (!valid() || data_->visit == nullptr || visitor == nullptr) {
      return false;
    }

    struct visit_context {
      const detail::set_type_data* data = nullptr;
      set_element_visitor visitor = nullptr;
      void* user_data = nullptr;
    };

    visit_context context{data_, visitor, user_data};
    return data_->visit(
        storage_,
        [](const void* element, void* raw_context) -> bool {
          auto* context = static_cast<visit_context*>(raw_context);
          const detail::set_type_data* data = context->data;
          any_ref element_ref = any_ref::from_raw(
              data->element_type, data->element_type_name, data->element_cpp_type, element,
              data->element_size, data->element_alignment, data->clone_element,
              data->destroy_element, data->assign_element, false);

          return context->visitor(element_ref, context->user_data);
        },
        &context);
  }

  /** @brief 清空集合。 */
  bool clear() const {
    if (!writable() || data_->clear == nullptr) {
      return false;
    }

    data_->clear(const_cast<void*>(storage_));
    return true;
  }

  /** @brief 插入一个元素。 */
  bool insert_value(const any& value) const {
    if (!writable() || data_->insert == nullptr || !value.valid() ||
        value.cpp_type() != data_->element_cpp_type || value.size() != data_->element_size ||
        value.alignment() != data_->element_alignment) {
      return false;
    }

    return data_->insert(const_cast<void*>(storage_), value.data());
  }

private:
  friend class any;
  friend class any_ref;

  set_view(const detail::set_type_data* data, const void* storage, bool writable) noexcept
      : data_(data), storage_(storage), writable_(writable) {}

  const detail::set_type_data* data_ = nullptr;
  const void* storage_ = nullptr;
  bool writable_ = false;
};

/**
 * @brief 关联容器运行期视图。
 *
 * `associative_view` 不拥有容器，只引用 `any` 或 `any_ref`
 * 指向的关联容器对象。只读视图允许查询和遍历； 可写视图才允许清空和插入。
 */
class associative_view final {
public:
  associative_view() noexcept = default;

  [[nodiscard]] bool valid() const noexcept {
    return data_ != nullptr && data_->valid() && storage_ != nullptr;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }

  [[nodiscard]] type_id key_type() const noexcept { return valid() ? data_->key_type : type_id{}; }

  [[nodiscard]] string_name key_type_name() const noexcept {
    return valid() ? data_->key_type_name : string_name{};
  }

  [[nodiscard]] type_id mapped_type() const noexcept {
    return valid() ? data_->mapped_type : type_id{};
  }

  [[nodiscard]] string_name mapped_type_name() const noexcept {
    return valid() ? data_->mapped_type_name : string_name{};
  }

  /** @brief 获取关联容器元素数量。 */
  [[nodiscard]] std::optional<std::size_t> size() const {
    if (!valid() || data_->size == nullptr) {
      return std::nullopt;
    }

    return data_->size(storage_);
  }

  /** @brief 遍历关联容器 key/value 只读引用。 */
  bool for_each_ref(associative_entry_visitor visitor, void* user_data = nullptr) const {
    if (!valid() || data_->visit == nullptr || visitor == nullptr) {
      return false;
    }

    struct visit_context {
      const detail::associative_type_data* data = nullptr;
      associative_entry_visitor visitor = nullptr;
      void* user_data = nullptr;
    };

    visit_context context{data_, visitor, user_data};
    return data_->visit(
        storage_,
        [](const void* key, const void* mapped, void* raw_context) -> bool {
          auto* context = static_cast<visit_context*>(raw_context);
          const detail::associative_type_data* data = context->data;
          any_ref key_ref = any_ref::from_raw(
              data->key_type, data->key_type_name, data->key_cpp_type, key, data->key_size,
              data->key_alignment, data->clone_key, data->destroy_key, data->assign_key, false);
          any_ref mapped_ref = any_ref::from_raw(data->mapped_type, data->mapped_type_name,
                                                 data->mapped_cpp_type, mapped, data->mapped_size,
                                                 data->mapped_alignment, data->clone_mapped,
                                                 data->destroy_mapped, data->assign_mapped, false);

          return context->visitor(key_ref, mapped_ref, context->user_data);
        },
        &context);
  }

  /** @brief 清空关联容器。 */
  bool clear() const {
    if (!writable() || data_->clear == nullptr) {
      return false;
    }

    data_->clear(const_cast<void*>(storage_));
    return true;
  }

  /** @brief 插入一组 key/value。 */
  bool insert_value(const any& key, const any& mapped) const {
    if (!writable() || data_->insert == nullptr || !key.valid() || !mapped.valid() ||
        key.cpp_type() != data_->key_cpp_type || key.size() != data_->key_size ||
        key.alignment() != data_->key_alignment || mapped.cpp_type() != data_->mapped_cpp_type ||
        mapped.size() != data_->mapped_size || mapped.alignment() != data_->mapped_alignment) {
      return false;
    }

    return data_->insert(const_cast<void*>(storage_), key.data(), mapped.data());
  }

private:
  friend class any;
  friend class any_ref;

  associative_view(const detail::associative_type_data* data, const void* storage,
                   bool writable) noexcept
      : data_(data), storage_(storage), writable_(writable) {}

  const detail::associative_type_data* data_ = nullptr;
  const void* storage_ = nullptr;
  bool writable_ = false;
};

inline associative_view any::create_associative_view() {
  if (!valid()) {
    return {};
  }

  const detail::associative_type_data* data =
      get_meta_registry().find_ptr<detail::associative_type_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return associative_view{data, mutable_data(), true};
}

inline associative_view any::create_associative_view() const {
  if (!valid()) {
    return {};
  }

  const detail::associative_type_data* data =
      get_meta_registry().find_ptr<detail::associative_type_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return associative_view{data, data_, false};
}

inline associative_view any_ref::create_associative_view() const {
  if (!valid()) {
    return {};
  }

  const detail::associative_type_data* data =
      get_meta_registry().find_ptr<detail::associative_type_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return associative_view{data, data_, writable_};
}

inline sequence_view any::create_sequence_view() {
  if (!valid()) {
    return {};
  }

  const detail::sequence_type_data* data =
      get_meta_registry().find_ptr<detail::sequence_type_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return sequence_view{data, mutable_data(), true};
}

inline sequence_view any::create_sequence_view() const {
  if (!valid()) {
    return {};
  }

  const detail::sequence_type_data* data =
      get_meta_registry().find_ptr<detail::sequence_type_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return sequence_view{data, data_, false};
}

inline sequence_view any_ref::create_sequence_view() const {
  if (!valid()) {
    return {};
  }

  const detail::sequence_type_data* data =
      get_meta_registry().find_ptr<detail::sequence_type_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return sequence_view{data, data_, writable_};
}

inline set_view any::create_set_view() {
  if (!valid()) {
    return {};
  }

  const detail::set_type_data* data = get_meta_registry().find_ptr<detail::set_type_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return set_view{data, mutable_data(), true};
}

inline set_view any::create_set_view() const {
  if (!valid()) {
    return {};
  }

  const detail::set_type_data* data = get_meta_registry().find_ptr<detail::set_type_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return set_view{data, data_, false};
}

inline set_view any_ref::create_set_view() const {
  if (!valid()) {
    return {};
  }

  const detail::set_type_data* data = get_meta_registry().find_ptr<detail::set_type_data>(type_);
  if (data == nullptr) {
    return {};
  }

  return set_view{data, data_, writable_};
}

} // namespace caors

#endif
