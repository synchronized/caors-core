// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_PROPERTY_RECORD_H_
#define CAORS_META_DETAIL_PROPERTY_RECORD_H_

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

class property_record;

/** @brief 属性底层存储形式，用于热路径选择安全的访问方式。 */
enum class property_storage_kind {
  none,
  member,
  accessor,
  global,
};

/** @brief 属性访问函数表，负责把属性底层存储转换成运行期值和引用。 */
struct property_ops {
  using type_fn = meta::type (*)(const property_record& property) noexcept;
  using const_data_fn = const void* (*)(const property_record& property, const void* object) noexcept;
  using mutable_data_fn = void* (*)(const property_record& property, void* object) noexcept;
  using get_value_fn = any (*)(const property_record& property, const void* object);
  using get_const_ref_fn = any_ref (*)(const property_record& property, const void* object);
  using get_mutable_ref_fn = any_ref (*)(const property_record& property, void* object);
  using set_value_fn = bool (*)(const property_record& property, void* object, const any& source);

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

/**
 * @brief 底层属性反射包装器。
 *
 * 属性访问通过 `property_ops` 函数表完成。wrapper
 * 只保存属性名、成员偏移和可写标记；属性所有者和值类型 由对应 ops 模型通过 `meta::resolve<T>()`
 * 返回，避免属性数据重复缓存静态类型节点。
 */
class property_record final {
public:
  static constexpr std::size_t storage_size = sizeof(void*) * 4U;

  property_record() noexcept = default;

  property_record(string_name property_name, std::size_t property_offset, bool property_writable,
                property_storage_kind storage_kind, property_ops property_operations) noexcept
      : name_(property_name), offset_(property_offset), writable_(property_writable),
        kind_(storage_kind), ops_(property_operations) {}

  property_record(const property_record& other)
      : name_(other.name_), storage_(other.storage_), offset_(other.offset_),
        writable_(other.writable_), kind_(other.kind_), ops_(other.ops_), schema_(other.schema_) {
    if (other.metadata_ != nullptr) {
      metadata_ =
          memory::make_unique<meta::metadata_map>(&memory::library_resource(), *other.metadata_);
    }
  }

  property_record& operator=(const property_record& other) {
    if (this == &other) {
      return *this;
    }

    name_ = other.name_;
    storage_ = other.storage_;
    offset_ = other.offset_;
    writable_ = other.writable_;
    kind_ = other.kind_;
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

  property_record(property_record&&) noexcept = default;
  property_record& operator=(property_record&&) noexcept = default;
  ~property_record() = default;

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

  /** @brief 查询属性底层存储形式。 */
  [[nodiscard]] property_storage_kind storage_kind() const noexcept { return kind_; }

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

  /** @brief 使用提交期缓存类型获取只读引用，成员属性避免重复类型查询。 */
  [[nodiscard]] any_ref get_ref_unchecked(meta::type cached_type, const void* object) const {
    if (kind_ == property_storage_kind::member) {
      return member_get_ref(cached_type, object);
    }

    return ops_.get_const_ref != nullptr ? ops_.get_const_ref(*this, object) : any_ref{};
  }

  /** @brief 使用提交期缓存类型获取引用，成员属性避免重复类型查询。 */
  [[nodiscard]] any_ref get_ref_unchecked(meta::type cached_type, void* object) const {
    if (kind_ == property_storage_kind::member) {
      any_ref ref = member_get_ref(cached_type, object);
      if (ref) {
        return ref;
      }
      return member_get_ref(cached_type, static_cast<const void*>(object));
    }

    if (ops_.get_mutable_ref == nullptr) {
      return {};
    }
    any_ref ref = ops_.get_mutable_ref(*this, object);
    if (ref || ops_.get_const_ref == nullptr) {
      return ref;
    }
    return ops_.get_const_ref(*this, object);
  }

  /** @brief 从运行期值写入属性。 */
  bool try_set_value(void* object, const any& source) const {
    return valid() && ops_.set_value(*this, object, source);
  }

  /** @brief 读取只读对象属性并转换为统一运行期值。 */
  [[nodiscard]] meta::value get_runtime_value(const void* object) const;

  /** @brief 读取可写对象属性并转换为统一运行期值。 */
  [[nodiscard]] meta::value get_runtime_value(void* object) const;

  /** @brief 将统一运行期值转换并写入属性。 */
  [[nodiscard]] bool try_set_runtime_value(void* object, const meta::value& source) const;

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
  property_storage_kind kind_ = property_storage_kind::none;
  property_ops ops_{};
  meta::property_schema schema_{};
  memory::unique_ptr<meta::metadata_map> metadata_{};
};

} // namespace caors::meta::detail

#endif
