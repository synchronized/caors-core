// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REFLECTION_DETAIL_FIELD_WRAPPER_H_
#define CAORS_REFLECTION_DETAIL_FIELD_WRAPPER_H_

#include "caors/meta.h"
#include "caors/meta/value/any_ref.h"
#include "caors/traits/container.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>

namespace caors::reflection::detail {

class field_wrapper;

/** @brief 字段访问函数表，负责把字段底层存储转换成运行期值和引用。 */
struct field_ops {
  using type_fn = meta::type (*)(const field_wrapper& field) noexcept;
  using const_data_fn = const void* (*)(const field_wrapper& field, const void* object) noexcept;
  using mutable_data_fn = void* (*)(const field_wrapper& field, void* object) noexcept;
  using get_value_fn = any (*)(const field_wrapper& field, const void* object);
  using get_const_ref_fn = any_ref (*)(const field_wrapper& field, const void* object);
  using get_mutable_ref_fn = any_ref (*)(const field_wrapper& field, void* object);
  using set_value_fn = bool (*)(const field_wrapper& field, void* object, const any& source);

  type_fn get_owner_type = nullptr;
  type_fn get_type = nullptr;
  const_data_fn data = nullptr;
  mutable_data_fn mutable_data = nullptr;
  get_value_fn get_value = nullptr;
  get_const_ref_fn get_const_ref = nullptr;
  get_mutable_ref_fn get_mutable_ref = nullptr;
  set_value_fn set_value = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return get_owner_type != nullptr && get_type != nullptr && data != nullptr &&
           mutable_data != nullptr && get_value != nullptr && get_const_ref != nullptr &&
           get_mutable_ref != nullptr && set_value != nullptr;
  }
};

template <typename T> [[nodiscard]] bool field_type_matches_name(std::string_view name) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  const meta::type registered = caors::meta::find<type>();
  return registered && !registered.name().empty() && registered.name() == name;
}

/**
 * @brief 底层字段反射包装器。
 *
 * 字段访问通过 `field_ops` 函数表完成。wrapper 只保存字段名、成员偏移和可写标记；字段所有者和值类型
 * 由对应 ops 模型通过 `meta::resolve<T>()` 返回，避免字段数据重复缓存 descriptor。
 */
class field_wrapper final {
public:
  field_wrapper() noexcept = default;

  field_wrapper(string_name field_name, std::size_t field_offset, bool field_writable,
                field_ops field_operations) noexcept
      : name_(field_name), offset_(field_offset), writable_(field_writable),
        ops_(field_operations) {}

  [[nodiscard]] bool valid() const noexcept {
    const meta::type type = get_type();
    return !name_.empty() && type.valid() && ops_.valid();
  }

  /** @brief 查询字段名称。 */
  [[nodiscard]] string_name name() const noexcept { return name_; }

  /** @brief 查询字段所有者类型视图。 */
  [[nodiscard]] meta::type get_owner_type() const noexcept {
    return ops_.get_owner_type != nullptr ? ops_.get_owner_type(*this) : meta::type{};
  }

  /** @brief 查询字段值类型。 */
  [[nodiscard]] meta::type get_type() const noexcept {
    return ops_.get_type != nullptr ? ops_.get_type(*this) : meta::type{};
  }

  /** @brief 查询字段是否可写。 */
  [[nodiscard]] bool writable() const noexcept { return writable_; }

  /** @brief 查询成员字段偏移。非成员字段 wrapper 返回注册时保存的偏移值。 */
  [[nodiscard]] std::size_t offset() const noexcept { return offset_; }

  /** @brief 判断字段是否匹配指定 C++ 类型。 */
  template <typename Field> [[nodiscard]] bool matches_type() const noexcept {
    using field_type = std::remove_cv_t<std::remove_reference_t<Field>>;
    const meta::type actual = get_type();
    const meta::type expected = caors::meta::resolve<field_type>();
    const meta::value_storage_ops* actual_storage = actual.get_raw_ops<meta::value_storage_ops>();
    return actual && expected && actual.id() == expected.id() && actual_storage != nullptr &&
           actual_storage->layout.size == sizeof(field_type) &&
           actual_storage->layout.alignment == alignof(field_type);
  }

  /** @brief 获取字段在对象内的只读地址。 */
  [[nodiscard]] const void* member_data(const void* object) const noexcept {
    if (object == nullptr) {
      return nullptr;
    }

    const auto* bytes = static_cast<const unsigned char*>(object);
    return bytes + offset_;
  }

  /** @brief 获取成员字段在对象内的可写地址。 */
  [[nodiscard]] void* member_mutable_data(void* object) const noexcept {
    if (object == nullptr || !writable_) {
      return nullptr;
    }

    auto* bytes = static_cast<unsigned char*>(object);
    return bytes + offset_;
  }

  /** @brief 获取成员字段值的运行期拷贝。 */
  [[nodiscard]] any member_get_value(meta::type type, const void* object) const {
    const void* field_value = member_data(object);
    if (field_value == nullptr) {
      return {};
    }

    return any::copy_from_raw(type, field_value);
  }

  /** @brief 获取成员字段值的只读运行期引用。 */
  [[nodiscard]] any_ref member_get_ref(meta::type type, const void* object) const {
    const void* field_value = member_data(object);
    if (field_value == nullptr) {
      return {};
    }

    return any_ref::from_raw(type, field_value, false);
  }

  /** @brief 获取成员字段值的可写运行期引用。 */
  [[nodiscard]] any_ref member_get_ref(meta::type type, void* object) const {
    void* field_value = member_mutable_data(object);
    if (field_value == nullptr) {
      return {};
    }

    return any_ref::from_raw(type, field_value, true);
  }

  /** @brief 从运行期值写入成员字段。 */
  bool member_try_set_value(void* object, const any& source) const {
    const meta::type expected = get_type();
    const meta::value_storage_ops* expected_storage =
        expected.get_raw_ops<meta::value_storage_ops>();
    if (!source.valid() || source.get_type().id() != expected.id() || expected_storage == nullptr ||
        source.size() != expected_storage->layout.size ||
        source.alignment() != expected_storage->layout.alignment ||
        expected_storage->ops.assign == nullptr) {
      return false;
    }

    void* storage = member_mutable_data(object);
    if (storage == nullptr) {
      return false;
    }

    return expected_storage->ops.assign(storage, source.data());
  }

  /** @brief 获取字段在对象内的只读地址。 */
  [[nodiscard]] const void* data(const void* object) const noexcept {
    return valid() ? ops_.data(*this, object) : nullptr;
  }

  /** @brief 获取字段在对象内的可写地址。 */
  [[nodiscard]] void* mutable_data(void* object) const noexcept {
    return valid() ? ops_.mutable_data(*this, object) : nullptr;
  }

  /** @brief 获取字段值的运行期拷贝。 */
  [[nodiscard]] any get_value(const void* object) const {
    return valid() ? ops_.get_value(*this, object) : any{};
  }

  /** @brief 获取字段值的只读运行期引用。 */
  [[nodiscard]] any_ref get_ref(const void* object) const {
    return valid() ? ops_.get_const_ref(*this, object) : any_ref{};
  }

  /** @brief 获取字段值的可写运行期引用。 */
  [[nodiscard]] any_ref get_ref(void* object) const {
    return valid() ? ops_.get_mutable_ref(*this, object) : any_ref{};
  }

  /** @brief 从运行期值写入字段。 */
  bool try_set_value(void* object, const any& source) const {
    return valid() && ops_.set_value(*this, object, source);
  }

  /** @brief 按指定类型读取字段值。 */
  template <typename Field>
  [[nodiscard]] std::optional<std::remove_cv_t<std::remove_reference_t<Field>>>
  try_get(const void* object) const {
    using field_type = std::remove_cv_t<std::remove_reference_t<Field>>;
    static_assert(std::is_copy_constructible_v<field_type>,
                  "Reflected field values must be copy constructible");

    if (!matches_type<field_type>()) {
      return std::nullopt;
    }

    const void* field_value = data(object);
    if (field_value == nullptr) {
      return std::nullopt;
    }

    return *static_cast<const field_type*>(field_value);
  }

  /** @brief 按指定类型写入字段值。 */
  template <typename Field> bool try_set(void* object, const Field& value) const {
    using field_type = std::remove_cv_t<std::remove_reference_t<Field>>;
    if (!matches_type<field_type>()) {
      return false;
    }

    void* storage = mutable_data(object);
    if (storage == nullptr) {
      return false;
    }

    auto* target = static_cast<field_type*>(storage);
    if constexpr (std::is_assignable_v<field_type&, const Field&>) {
      *target = value;
      return true;
    } else {
      return false;
    }
  }

private:
  string_name name_{};
  std::size_t offset_ = 0;
  bool writable_ = true;
  field_ops ops_{};
};

template <typename Class>
[[nodiscard]] meta::type member_field_owner_type(const field_wrapper&) noexcept {
  return meta::resolve<Class>();
}

template <typename Field>
[[nodiscard]] meta::type member_field_type(const field_wrapper&) noexcept {
  using field_type = std::remove_cv_t<Field>;
  return meta::resolve<field_type>();
}

[[nodiscard]] inline const void* member_field_data(const field_wrapper& field,
                                                   const void* object) noexcept {
  return field.member_data(object);
}

[[nodiscard]] inline void* member_field_mutable_data(const field_wrapper& field,
                                                     void* object) noexcept {
  return field.member_mutable_data(object);
}

template <typename Field>
[[nodiscard]] any member_field_get_value(const field_wrapper& field, const void* object) {
  using field_type = std::remove_cv_t<Field>;
  return field.member_get_value(meta::resolve<field_type>(), object);
}

template <typename Field>
[[nodiscard]] any_ref member_field_get_const_ref(const field_wrapper& field, const void* object) {
  using field_type = std::remove_cv_t<Field>;
  return field.member_get_ref(meta::resolve<field_type>(), object);
}

template <typename Field>
[[nodiscard]] any_ref member_field_get_mutable_ref(const field_wrapper& field, void* object) {
  using field_type = std::remove_cv_t<Field>;
  return field.member_get_ref(meta::resolve<field_type>(), object);
}

template <typename Field>
bool member_field_set_value(const field_wrapper& field, void* object, const any& source) {
  return field.member_try_set_value(object, source);
}

template <typename Class, typename Field> [[nodiscard]] field_ops member_field_ops() noexcept {
  return field_ops{
      &member_field_owner_type<Class>,
      &member_field_type<Field>,
      &member_field_data,
      &member_field_mutable_data,
      &member_field_get_value<Field>,
      &member_field_get_const_ref<Field>,
      &member_field_get_mutable_ref<Field>,
      &member_field_set_value<Field>,
  };
}

/** @brief 创建字段反射元信息。 */
template <typename Class, typename Field>
[[nodiscard]] field_wrapper make_field_wrapper(std::string_view name, std::string_view type_name,
                                               std::size_t offset) {
  using field_type = std::remove_cv_t<Field>;
  if (!meta::resolve<field_type>()) {
    return {};
  }

  if (!type_name.empty() && !detail::field_type_matches_name<field_type>(type_name)) {
    return {};
  }

  field_wrapper field{
      string_name{name},
      offset,
      !std::is_const_v<Field>,
      member_field_ops<Class, Field>(),
  };

  return field;
}

template <typename Class, typename Field>
[[nodiscard]] std::size_t member_offset(Field Class::* member) noexcept {
  const auto* object = reinterpret_cast<const volatile Class*>(0x1000);
  const auto* field = &(object->*member);
  return reinterpret_cast<std::uintptr_t>(field) - reinterpret_cast<std::uintptr_t>(object);
}

} // namespace caors::reflection::detail

#endif
