// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_PROPERTY_DATA_H_
#define CAORS_META_DETAIL_PROPERTY_DATA_H_

#include "caors/memory/unique_ptr.h"
#include "caors/meta/metadata.h"
#include "caors/meta/property_schema.h"
#include "caors/meta/type.h"
#include "caors/meta/value/any_ref.h"
#include "caors/meta/value_ops.h"
#include "caors/traits/container.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::meta::detail {

class property_data;

/** @brief 属性访问函数表，负责把属性底层存储转换成运行期值和引用。 */
struct property_ops {
  using type_fn = meta::type (*)(const property_data& property) noexcept;
  using const_data_fn = const void* (*)(const property_data& property, const void* object) noexcept;
  using mutable_data_fn = void* (*)(const property_data& property, void* object) noexcept;
  using get_value_fn = any (*)(const property_data& property, const void* object);
  using get_const_ref_fn = any_ref (*)(const property_data& property, const void* object);
  using get_mutable_ref_fn = any_ref (*)(const property_data& property, void* object);
  using set_value_fn = bool (*)(const property_data& property, void* object, const any& source);

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

template <typename T> [[nodiscard]] bool property_type_matches_name(std::string_view name) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  const meta::type registered = caors::meta::find<type>();
  return registered && !registered.name().empty() && registered.name() == name;
}

/**
 * @brief 底层属性反射包装器。
 *
 * 属性访问通过 `property_ops` 函数表完成。wrapper
 * 只保存属性名、成员偏移和可写标记；属性所有者和值类型 由对应 ops 模型通过 `meta::resolve<T>()`
 * 返回，避免属性数据重复缓存 descriptor。
 */
class property_data final {
public:
  static constexpr std::size_t storage_size = sizeof(void*) * 4U;

  property_data() noexcept = default;

  property_data(string_name property_name, std::size_t property_offset, bool property_writable,
                property_ops property_operations) noexcept
      : name_(property_name), offset_(property_offset), writable_(property_writable),
        ops_(property_operations) {}

  property_data(const property_data& other)
      : name_(other.name_), storage_(other.storage_), offset_(other.offset_),
        writable_(other.writable_), ops_(other.ops_), schema_(other.schema_) {
    if (other.metadata_ != nullptr) {
      metadata_ =
          memory::make_unique<meta::metadata_map>(&memory::library_resource(), *other.metadata_);
    }
  }

  property_data& operator=(const property_data& other) {
    if (this == &other) {
      return *this;
    }

    name_ = other.name_;
    storage_ = other.storage_;
    offset_ = other.offset_;
    writable_ = other.writable_;
    ops_ = other.ops_;
    schema_ = other.schema_;
    if (other.metadata_ != nullptr) {
      metadata_ =
          memory::make_unique<meta::metadata_map>(&memory::library_resource(), *other.metadata_);
    } else {
      metadata_.reset();
    }
    return *this;
  }

  property_data(property_data&&) noexcept = default;
  property_data& operator=(property_data&&) noexcept = default;
  ~property_data() = default;

  [[nodiscard]] bool valid() const noexcept {
    const meta::type type = get_type();
    return !name_.empty() && type.valid() && ops_.valid();
  }

  /** @brief 查询属性名称。 */
  [[nodiscard]] string_name name() const noexcept { return name_; }

  /** @brief 查询属性 metadata。 */
  [[nodiscard]] const meta::metadata_map& metadata() const noexcept {
    return metadata_ != nullptr ? *metadata_ : empty_metadata_map();
  }

  /** @brief 查询属性通用 schema。 */
  [[nodiscard]] const meta::property_schema& schema() const noexcept { return schema_; }

  /** @brief 查询属性 metadata 值。 */
  [[nodiscard]] const meta::metadata_value* find_metadata(meta::metadata_key key) const noexcept {
    return metadata_ != nullptr ? meta::find_metadata(*metadata_, key) : nullptr;
  }

  /** @brief 写入或覆盖属性 metadata。 */
  void set_metadata(meta::metadata_key key, meta::metadata_value value) {
    if (metadata_ == nullptr) {
      metadata_ = memory::make_unique<meta::metadata_map>(
          &memory::library_resource(), meta::make_metadata_map(&memory::library_resource()));
    }
    meta::set_metadata(*metadata_, key, std::move(value));
  }

  /** @brief 写入属性 schema 项。 */
  void apply_schema_entry(meta::registration::property_schema_entry entry) {
    meta::registration::apply_property_schema_entry(schema_, std::move(entry));
  }

  /** @brief 查询属性所有者类型视图。 */
  [[nodiscard]] meta::type get_owner_type() const noexcept {
    return ops_.get_owner_type != nullptr ? ops_.get_owner_type(*this) : meta::type{};
  }

  /** @brief 查询属性值类型。 */
  [[nodiscard]] meta::type get_type() const noexcept {
    return ops_.get_type != nullptr ? ops_.get_type(*this) : meta::type{};
  }

  /** @brief 查询属性是否可写。 */
  [[nodiscard]] bool writable() const noexcept { return writable_; }

  /** @brief 查询成员属性偏移。非成员属性 wrapper 返回注册时保存的偏移值。 */
  [[nodiscard]] std::size_t offset() const noexcept { return offset_; }

  /** @brief 判断属性是否匹配指定 C++ 类型。 */
  template <typename Property> [[nodiscard]] bool matches_type() const noexcept {
    using property_type = std::remove_cv_t<std::remove_reference_t<Property>>;
    const meta::type actual = get_type();
    const meta::type expected = caors::meta::resolve<property_type>();
    const meta::value_storage_ops* actual_storage = actual.get_raw_ops<meta::value_storage_ops>();
    return actual && expected && actual.id() == expected.id() && actual_storage != nullptr &&
           actual_storage->layout.size == sizeof(property_type) &&
           actual_storage->layout.alignment == alignof(property_type);
  }

  /** @brief 获取属性在对象内的只读地址。 */
  [[nodiscard]] const void* member_data(const void* object) const noexcept {
    if (object == nullptr) {
      return nullptr;
    }

    const auto* bytes = static_cast<const unsigned char*>(object);
    return bytes + offset_;
  }

  /** @brief 获取成员属性在对象内的可写地址。 */
  [[nodiscard]] void* member_mutable_data(void* object) const noexcept {
    if (object == nullptr || !writable_) {
      return nullptr;
    }

    auto* bytes = static_cast<unsigned char*>(object);
    return bytes + offset_;
  }

  /** @brief 获取成员属性值的运行期拷贝。 */
  [[nodiscard]] any member_get_value(meta::type type, const void* object) const {
    const void* property_value = member_data(object);
    if (property_value == nullptr) {
      return {};
    }

    return any::copy_from_raw(type, property_value);
  }

  /** @brief 获取成员属性值的只读运行期引用。 */
  [[nodiscard]] any_ref member_get_ref(meta::type type, const void* object) const {
    const void* property_value = member_data(object);
    if (property_value == nullptr) {
      return {};
    }

    return any_ref::from_raw(type, property_value, false);
  }

  /** @brief 获取成员属性值的可写运行期引用。 */
  [[nodiscard]] any_ref member_get_ref(meta::type type, void* object) const {
    void* property_value = member_mutable_data(object);
    if (property_value == nullptr) {
      return {};
    }

    return any_ref::from_raw(type, property_value, true);
  }

  /** @brief 从运行期值写入成员属性。 */
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

  /** @brief 获取属性在对象内的只读地址。 */
  [[nodiscard]] const void* data(const void* object) const noexcept {
    return valid() ? ops_.data(*this, object) : nullptr;
  }

  /** @brief 获取属性在对象内的可写地址。 */
  [[nodiscard]] void* mutable_data(void* object) const noexcept {
    return valid() ? ops_.mutable_data(*this, object) : nullptr;
  }

  /** @brief 获取属性值的运行期拷贝。 */
  [[nodiscard]] any get_value(const void* object) const {
    return valid() ? ops_.get_value(*this, object) : any{};
  }

  /** @brief 获取属性值的只读运行期引用。 */
  [[nodiscard]] any_ref get_ref(const void* object) const {
    return valid() ? ops_.get_const_ref(*this, object) : any_ref{};
  }

  /** @brief 获取属性值引用；优先返回可写引用，无法写入时回退为只读引用。 */
  [[nodiscard]] any_ref get_ref(void* object) const {
    if (!valid()) {
      return {};
    }

    any_ref ref = ops_.get_mutable_ref(*this, object);
    return ref ? ref : ops_.get_const_ref(*this, object);
  }

  /** @brief 从运行期值写入属性。 */
  bool try_set_value(void* object, const any& source) const {
    return valid() && ops_.set_value(*this, object, source);
  }

  /** @brief 按指定类型读取属性值。 */
  template <typename Property>
  [[nodiscard]] std::optional<std::remove_cv_t<std::remove_reference_t<Property>>>
  try_get(const void* object) const {
    using property_type = std::remove_cv_t<std::remove_reference_t<Property>>;
    static_assert(std::is_copy_constructible_v<property_type>,
                  "Reflected property values must be copy constructible");

    if (!matches_type<property_type>()) {
      return std::nullopt;
    }

    const void* property_value = data(object);
    if (property_value == nullptr) {
      return std::nullopt;
    }

    return *static_cast<const property_type*>(property_value);
  }

  /** @brief 按指定类型写入属性值。 */
  template <typename Property> bool try_set(void* object, const Property& value) const {
    using property_type = std::remove_cv_t<std::remove_reference_t<Property>>;
    if (!matches_type<property_type>()) {
      return false;
    }

    void* storage = mutable_data(object);
    if (storage == nullptr) {
      return false;
    }

    auto* target = static_cast<property_type*>(storage);
    if constexpr (std::is_assignable_v<property_type&, const Property&>) {
      *target = value;
      return true;
    } else {
      return false;
    }
  }

  template <typename Pointer>
  [[nodiscard]] Pointer load_pointer(std::size_t offset) const noexcept {
    Pointer pointer{};
    std::memcpy(&pointer, storage_.data() + offset, sizeof(Pointer));
    return pointer;
  }

  template <typename Pointer> void store_pointer(Pointer pointer, std::size_t offset) noexcept {
    static_assert(std::is_trivially_copyable_v<Pointer>,
                  "Property accessors must be trivially copyable");
    static_assert(sizeof(Pointer) <= storage_size,
                  "Property accessor is too large for meta property storage");
    std::memcpy(storage_.data() + offset, &pointer, sizeof(Pointer));
  }

  template <typename Getter, typename Setter>
  void store_accessor_pointers(Getter getter, Setter setter) noexcept {
    static_assert(sizeof(Getter) + sizeof(Setter) <= storage_size,
                  "Property getter/setter pair is too large for meta property storage");
    store_pointer(getter, 0);
    store_pointer(setter, sizeof(Getter));
  }

  template <typename Getter> void store_getter_pointer(Getter getter) noexcept {
    store_pointer(getter, 0);
  }

private:
  [[nodiscard]] static const meta::metadata_map& empty_metadata_map() noexcept {
    static const meta::metadata_map empty{
        typename meta::metadata_map::allocator_type{&memory::library_resource()}};
    return empty;
  }

  string_name name_{};
  alignas(std::max_align_t) std::array<std::byte, storage_size> storage_{};
  std::size_t offset_ = 0;
  bool writable_ = true;
  property_ops ops_{};
  meta::property_schema schema_{};
  memory::unique_ptr<meta::metadata_map> metadata_{};
};

template <typename Class>
[[nodiscard]] meta::type member_property_owner_type(const property_data&) noexcept {
  return meta::resolve<Class>();
}

template <typename Property>
[[nodiscard]] meta::type member_property_type(const property_data&) noexcept {
  using property_type = std::remove_cv_t<Property>;
  return meta::resolve<property_type>();
}

[[nodiscard]] inline const void* member_property_data(const property_data& property,
                                                      const void* object) noexcept {
  return property.member_data(object);
}

[[nodiscard]] inline void* member_property_mutable_data(const property_data& property,
                                                        void* object) noexcept {
  return property.member_mutable_data(object);
}

template <typename Property>
[[nodiscard]] any member_property_get_value(const property_data& property, const void* object) {
  using property_type = std::remove_cv_t<Property>;
  return property.member_get_value(meta::resolve<property_type>(), object);
}

template <typename Property>
[[nodiscard]] any_ref member_property_get_const_ref(const property_data& property,
                                                    const void* object) {
  using property_type = std::remove_cv_t<Property>;
  return property.member_get_ref(meta::resolve<property_type>(), object);
}

template <typename Property>
[[nodiscard]] any_ref member_property_get_mutable_ref(const property_data& property, void* object) {
  using property_type = std::remove_cv_t<Property>;
  return property.member_get_ref(meta::resolve<property_type>(), object);
}

template <typename Property>
bool member_property_set_value(const property_data& property, void* object, const any& source) {
  return property.member_try_set_value(object, source);
}

template <typename Class, typename Property>
[[nodiscard]] property_ops member_property_ops() noexcept {
  return property_ops{
      &member_property_owner_type<Class>,
      &member_property_type<Property>,
      &member_property_data,
      &member_property_mutable_data,
      &member_property_get_value<Property>,
      &member_property_get_const_ref<Property>,
      &member_property_get_mutable_ref<Property>,
      &member_property_set_value<Property>,
  };
}

template <typename Getter> struct const_member_getter_traits;

template <typename Class, typename Return>
struct const_member_getter_traits<Return (Class::*)() const> {
  using class_type = Class;
  using return_type = Return;
  using value_type = std::remove_cv_t<std::remove_reference_t<Return>>;
  using pointer_type = Return (Class::*)() const;
};

template <typename Class, typename Return>
struct const_member_getter_traits<Return (Class::*)() const noexcept> {
  using class_type = Class;
  using return_type = Return;
  using value_type = std::remove_cv_t<std::remove_reference_t<Return>>;
  using pointer_type = Return (Class::*)() const noexcept;
};

template <typename Setter> struct member_setter_traits;

template <typename Class, typename Return, typename Argument>
struct member_setter_traits<Return (Class::*)(Argument)> {
  using class_type = Class;
  using return_type = Return;
  using argument_type = Argument;
  using value_type = std::remove_cv_t<std::remove_reference_t<Argument>>;
  using pointer_type = Return (Class::*)(Argument);
};

template <typename Class, typename Return, typename Argument>
struct member_setter_traits<Return (Class::*)(Argument) noexcept> {
  using class_type = Class;
  using return_type = Return;
  using argument_type = Argument;
  using value_type = std::remove_cv_t<std::remove_reference_t<Argument>>;
  using pointer_type = Return (Class::*)(Argument) noexcept;
};

template <typename Class>
[[nodiscard]] meta::type accessor_property_owner_type(const property_data&) noexcept {
  return meta::resolve<Class>();
}

template <typename Getter>
[[nodiscard]] meta::type accessor_property_type(const property_data&) noexcept {
  using traits_type = const_member_getter_traits<Getter>;
  return meta::resolve<typename traits_type::value_type>();
}

template <typename Getter>
decltype(auto) invoke_property_getter(const property_data& property, const void* object) {
  using traits_type = const_member_getter_traits<Getter>;
  auto getter = property.template load_pointer<typename traits_type::pointer_type>(0);
  return std::invoke(getter, *static_cast<const typename traits_type::class_type*>(object));
}

template <typename Getter>
[[nodiscard]] const void* accessor_property_data(const property_data& property,
                                                 const void* object) noexcept {
  using traits_type = const_member_getter_traits<Getter>;
  using return_type = typename traits_type::return_type;
  if (object == nullptr) {
    return nullptr;
  }

  if constexpr (std::is_lvalue_reference_v<return_type>) {
    decltype(auto) value = invoke_property_getter<Getter>(property, object);
    return std::addressof(value);
  } else {
    return nullptr;
  }
}

template <typename Getter>
[[nodiscard]] void* accessor_property_mutable_data(const property_data& property,
                                                   void* object) noexcept {
  using traits_type = const_member_getter_traits<Getter>;
  using return_type = typename traits_type::return_type;
  if (object == nullptr) {
    return nullptr;
  }

  if constexpr (std::is_lvalue_reference_v<return_type> &&
                !std::is_const_v<std::remove_reference_t<return_type>>) {
    decltype(auto) value = invoke_property_getter<Getter>(property, object);
    return std::addressof(value);
  } else {
    return nullptr;
  }
}

template <typename Getter>
[[nodiscard]] any accessor_property_get_value(const property_data& property, const void* object) {
  using traits_type = const_member_getter_traits<Getter>;
  using value_type = typename traits_type::value_type;
  static_assert(std::is_copy_constructible_v<value_type>,
                "Reflected property getter values must be copy constructible");
  if (object == nullptr) {
    return {};
  }

  decltype(auto) value = invoke_property_getter<Getter>(property, object);
  return any::from(value);
}

template <typename Getter>
[[nodiscard]] any_ref accessor_property_get_const_ref(const property_data& property,
                                                      const void* object) {
  using traits_type = const_member_getter_traits<Getter>;
  using return_type = typename traits_type::return_type;
  using value_type = typename traits_type::value_type;
  if (object == nullptr) {
    return {};
  }

  if constexpr (std::is_lvalue_reference_v<return_type>) {
    decltype(auto) value = invoke_property_getter<Getter>(property, object);
    return any_ref::from_raw(meta::resolve<value_type>(), std::addressof(value), false);
  } else {
    return {};
  }
}

template <typename Getter>
[[nodiscard]] any_ref accessor_property_get_mutable_ref(const property_data& property,
                                                        void* object) {
  using traits_type = const_member_getter_traits<Getter>;
  using return_type = typename traits_type::return_type;
  using value_type = typename traits_type::value_type;
  if (object == nullptr) {
    return {};
  }

  if constexpr (std::is_lvalue_reference_v<return_type> &&
                !std::is_const_v<std::remove_reference_t<return_type>>) {
    decltype(auto) value = invoke_property_getter<Getter>(property, object);
    return any_ref::from_raw(meta::resolve<value_type>(), std::addressof(value), true);
  } else {
    return {};
  }
}

template <typename Getter>
bool readonly_accessor_property_set_value(const property_data&, void*, const any&) {
  return false;
}

template <typename Getter, typename Setter>
bool accessor_property_set_value(const property_data& property, void* object, const any& source) {
  using setter_traits = member_setter_traits<Setter>;
  using return_type = typename setter_traits::return_type;
  using argument_type = typename setter_traits::argument_type;
  using value_type = typename setter_traits::value_type;
  static_assert(!std::is_rvalue_reference_v<argument_type>,
                "Reflected property setters do not support rvalue reference arguments");
  static_assert(!std::is_lvalue_reference_v<argument_type> ||
                    std::is_const_v<std::remove_reference_t<argument_type>>,
                "Reflected property setters do not support non-const lvalue reference arguments");
  static_assert(std::is_void_v<return_type> || std::is_same_v<return_type, bool>,
                "Reflected property setters must return void or bool");
  if (object == nullptr || !source.template matches_type<value_type>()) {
    return false;
  }

  const value_type* value = source.template try_get<value_type>();
  if (value == nullptr) {
    return false;
  }

  auto setter =
      property.template load_pointer<typename setter_traits::pointer_type>(sizeof(Getter));
  if constexpr (std::is_void_v<return_type>) {
    std::invoke(setter, *static_cast<typename setter_traits::class_type*>(object), *value);
    return true;
  } else {
    return std::invoke(setter, *static_cast<typename setter_traits::class_type*>(object), *value);
  }
}

template <typename Class, typename Getter>
[[nodiscard]] property_ops readonly_accessor_property_ops() noexcept {
  return property_ops{
      &accessor_property_owner_type<Class>,
      &accessor_property_type<Getter>,
      &accessor_property_data<Getter>,
      &accessor_property_mutable_data<Getter>,
      &accessor_property_get_value<Getter>,
      &accessor_property_get_const_ref<Getter>,
      &accessor_property_get_mutable_ref<Getter>,
      &readonly_accessor_property_set_value<Getter>,
  };
}

template <typename Class, typename Getter, typename Setter>
[[nodiscard]] property_ops accessor_property_ops() noexcept {
  return property_ops{
      &accessor_property_owner_type<Class>,
      &accessor_property_type<Getter>,
      &accessor_property_data<Getter>,
      &accessor_property_mutable_data<Getter>,
      &accessor_property_get_value<Getter>,
      &accessor_property_get_const_ref<Getter>,
      &accessor_property_get_mutable_ref<Getter>,
      &accessor_property_set_value<Getter, Setter>,
  };
}

/** @brief 创建属性反射元信息。 */
template <typename Class, typename Property>
[[nodiscard]] property_data make_property_data(std::string_view name, std::string_view type_name,
                                               std::size_t offset) {
  using property_type = std::remove_cv_t<Property>;
  if (!meta::resolve<property_type>()) {
    return {};
  }

  if (!type_name.empty() && !property_type_matches_name<property_type>(type_name)) {
    return {};
  }

  property_data property{
      string_name{name},
      offset,
      !std::is_const_v<Property>,
      member_property_ops<Class, Property>(),
  };

  return property;
}

template <typename Class, typename Getter>
[[nodiscard]] property_data make_readonly_property_data(std::string_view name, Getter getter) {
  using getter_traits = const_member_getter_traits<Getter>;
  static_assert(std::is_same_v<typename getter_traits::class_type, Class>,
                "Property getter owner must match builder type");
  using value_type = typename getter_traits::value_type;
  if (!meta::resolve<value_type>()) {
    return {};
  }

  property_data property{
      string_name{name},
      0,
      false,
      readonly_accessor_property_ops<Class, Getter>(),
  };
  property.store_getter_pointer(getter);
  return property;
}

template <typename Class, typename Getter, typename Setter>
[[nodiscard]] property_data make_accessor_property_data(std::string_view name, Getter getter,
                                                        Setter setter) {
  using getter_traits = const_member_getter_traits<Getter>;
  using setter_traits = member_setter_traits<Setter>;
  static_assert(std::is_same_v<typename getter_traits::class_type, Class>,
                "Property getter owner must match builder type");
  static_assert(std::is_same_v<typename setter_traits::class_type, Class>,
                "Property setter owner must match builder type");
  static_assert(
      std::is_same_v<typename getter_traits::value_type, typename setter_traits::value_type>,
      "Property getter and setter value types must match");
  using value_type = typename getter_traits::value_type;
  if (!meta::resolve<value_type>()) {
    return {};
  }

  property_data property{
      string_name{name},
      0,
      true,
      accessor_property_ops<Class, Getter, Setter>(),
  };
  property.store_accessor_pointers(getter, setter);
  return property;
}

template <typename Class, typename Property>
[[nodiscard]] std::size_t member_offset(Property Class::* member) noexcept {
  const auto* object = reinterpret_cast<const volatile Class*>(0x1000);
  const auto* property = &(object->*member);
  return reinterpret_cast<std::uintptr_t>(property) - reinterpret_cast<std::uintptr_t>(object);
}

} // namespace caors::meta::detail

#endif
