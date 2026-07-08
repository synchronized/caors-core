// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_VALUE_ANY_H_
#define CAORS_VALUE_ANY_H_

#include "caors/memory/resource.h"
#include "caors/meta.h"
#include "caors/meta/detail/type_descriptor.h"
#include "caors/value/fwd.h"

#include <cstddef>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::reflection::detail {
struct field_data;
} // namespace caors::reflection::detail

namespace caors::value {

namespace detail {

[[nodiscard]] const meta::detail::type_descriptor* descriptor_of(const any& value) noexcept;
[[nodiscard]] const meta::detail::type_descriptor* descriptor_of(const any_ref& value) noexcept;
[[nodiscard]] any default_construct(type_id type, const meta::detail::type_descriptor* descriptor);
[[nodiscard]] any default_construct(type_id type, const meta::detail::type_descriptor* descriptor,
                                    memory::memory_resource* resource);

} // namespace detail

/**
 * @brief 运行期 C++ 类型值。
 *
 * `any` 持有一份对象拷贝，并保存对应的运行期类型信息。它不引用原对象，因此可以安全地
 * 脱离字段所属对象生命周期使用。第一版只支持可拷贝类型；不可拷贝对象会产生无效 `any`。
 */
class any final {
public:
  any() noexcept = default;
  ~any();

  any(const any& other);
  any(any&& other) noexcept;
  any& operator=(const any& other);
  any& operator=(any&& other) noexcept;

  /**
   * @brief 从 C++ 值创建运行期值。
   *
   * 无显式类型名时，值会使用 C++ 类型 descriptor 创建；该类型不要求已经注册到
   * 全局 meta 注册表。
   */
  template <typename T> [[nodiscard]] static any from(const T& source) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    static_assert(std::is_copy_constructible_v<type>, "any requires copy constructible types");
    return copy_from_raw(meta::detail::descriptor_type_id<type>(),
                         meta::detail::descriptor_ptr_of<type>(), &source);
  }

  /** @brief 从 C++ 值和显式类型名创建运行期值；类型名必须已经注册并匹配该 C++ 类型。 */
  template <typename T> [[nodiscard]] static any from(const T& source, std::string_view name) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    static_assert(std::is_copy_constructible_v<type>, "any requires copy constructible types");

    const meta::type registered = caors::meta::find<type>();
    if (!registered.registered() || registered.name().empty() || registered.name() != name) {
      return {};
    }

    return copy_from_raw(registered.id(), meta::detail::descriptor_ptr_of<type>(), &source);
  }

  /** @brief 按运行期类型 id 默认构造一个值。不可默认构造类型会返回无效 `any`。 */
  [[nodiscard]] static any default_construct(type_id type);

  /**
   * @brief 按运行期类型 id 用指定内存资源默认构造一个值。
   *
   * 返回的 `any`
   * 会记住对应资源，并在析构时用匹配的资源销毁函数释放对象。若类型没有资源感知构造函数表，
   * 会回退到普通默认构造。
   */
  [[nodiscard]] static any default_construct(type_id type, memory::memory_resource* resource);

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] type_id type() const noexcept;
  [[nodiscard]] string_name type_name() const noexcept;
  [[nodiscard]] const void* data() const noexcept;
  [[nodiscard]] void* mutable_data() noexcept;
  [[nodiscard]] any_ref ref();
  [[nodiscard]] any_ref ref() const;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] std::size_t alignment() const noexcept;
  [[nodiscard]] sequence_view create_sequence_view();
  [[nodiscard]] sequence_view create_sequence_view() const;
  [[nodiscard]] set_view create_set_view();
  [[nodiscard]] set_view create_set_view() const;
  [[nodiscard]] associative_view create_associative_view();
  [[nodiscard]] associative_view create_associative_view() const;
  [[nodiscard]] pair_view create_pair_view();
  [[nodiscard]] pair_view create_pair_view() const;
  [[nodiscard]] tuple_view create_tuple_view();
  [[nodiscard]] tuple_view create_tuple_view() const;
  [[nodiscard]] variant_view create_variant_view();
  [[nodiscard]] variant_view create_variant_view() const;
  [[nodiscard]] indirect_view create_indirect_view();
  [[nodiscard]] indirect_view create_indirect_view() const;

  /** @brief 清空当前值。 */
  void reset() noexcept;

  /** @brief 交换两个运行期值。 */
  void swap(any& other) noexcept;

  /** @brief 判断当前值是否匹配指定 C++ 类型。 */
  template <typename T> [[nodiscard]] bool matches_type() const noexcept {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    return valid() && get_type_id() == meta::detail::type_id_of_cpp_type<type>() &&
           size() == sizeof(type) && alignment() == alignof(type);
  }

  /** @brief 判断两个运行期值是否具有相同 C++ 类型。 */
  [[nodiscard]] bool matches_type(const any& other) const noexcept;

  /** @brief 按指定类型获取只读指针。 */
  template <typename T>
  [[nodiscard]] const std::remove_cv_t<std::remove_reference_t<T>>* try_get() const noexcept {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    if (!matches_type<type>()) {
      return nullptr;
    }

    return static_cast<const type*>(data());
  }

  /** @brief 按指定类型获取可写指针。 */
  template <typename T>
  [[nodiscard]] std::remove_cv_t<std::remove_reference_t<T>>* try_get_mutable() noexcept {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    if (!matches_type<type>()) {
      return nullptr;
    }

    return static_cast<type*>(mutable_data());
  }

  /** @brief 从同类型 C++ 值写入当前值。 */
  template <typename T> bool try_set(const T& source) {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    const auto assign = descriptor_ != nullptr ? descriptor_->ops.assign : nullptr;
    if (!matches_type<type>() || assign == nullptr) {
      return false;
    }

    return assign(data_, &source);
  }

  /** @brief 从同类型运行期值写入当前值。 */
  bool try_set(const any& source);

private:
  friend class any_ref;
  friend struct reflection::detail::field_data;
  friend const meta::detail::type_descriptor* detail::descriptor_of(const any& value) noexcept;
  friend any detail::default_construct(type_id type,
                                       const meta::detail::type_descriptor* descriptor);
  friend any detail::default_construct(type_id type,
                                       const meta::detail::type_descriptor* descriptor,
                                       memory::memory_resource* resource);

  [[nodiscard]] static any
  copy_from_raw(type_id type, const meta::detail::type_descriptor* descriptor, const void* source);

  enum class storage_mode : unsigned char {
    empty,
    inline_value,
    heap_value,
    resource_value,
  };

  static constexpr std::size_t inline_storage_size = sizeof(void*) * 3;
  static constexpr std::size_t inline_storage_alignment = alignof(std::max_align_t);

  [[nodiscard]] static bool
  can_store_inline(const meta::detail::type_descriptor* descriptor) noexcept;
  [[nodiscard]] static bool
  can_default_construct_inline(const meta::detail::type_descriptor* descriptor) noexcept;

  void copy_from(const any& other);
  void move_from(any& other) noexcept;
  [[nodiscard]] void* inline_data() noexcept;
  [[nodiscard]] const void* inline_data() const noexcept;
  [[nodiscard]] bool stores_inline() const noexcept { return mode_ == storage_mode::inline_value; }
  [[nodiscard]] type_id get_type_id() const noexcept;

  const meta::detail::type_descriptor* descriptor_ = nullptr;
  void* data_ = nullptr;
  memory::memory_resource* resource_ = nullptr;
  meta::detail::value_resource_destroy_fn resource_destroy_ = nullptr;
  storage_mode mode_ = storage_mode::empty;
  alignas(std::max_align_t) unsigned char inline_storage_[inline_storage_size]{};
};

void swap(any& lhs, any& rhs) noexcept;

} // namespace caors::value

namespace caors {

using value::any;

} // namespace caors

#endif
