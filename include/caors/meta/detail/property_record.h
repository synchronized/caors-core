// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_PROPERTY_RECORD_H_
#define CAORS_META_DETAIL_PROPERTY_RECORD_H_

#include "caors/memory/unique_ptr.h"
#include "caors/meta/detail/request_payload.h"
#include "caors/meta/metadata.h"
#include "caors/meta/property_schema.h"
#include "caors/meta/type.h"
#include "caors/meta/value/any_ref.h"

#include <cstddef>
#include <cstdint>
#include <utility>

namespace caors::meta::detail {

/** @brief 属性底层存储形式，用于编译访问器选择热路径。 */
enum class property_storage_kind : std::uint8_t {
  none,
  member,
  accessor,
  global,
};

/** @brief 属性 wrapper 支持的内部请求。已发布数值只允许追加。 */
enum class property_request_kind : std::uint32_t {
  owner_type = 1,
  value_type = 2,
  storage_info = 3,
  const_data = 4,
  mutable_data = 5,
  get_any = 6,
  get_const_ref = 7,
  get_mutable_ref = 8,
  set_any = 9,
};

struct property_type_request {
  meta::type result{};
};

struct property_storage_info {
  property_storage_kind kind = property_storage_kind::none;
  std::size_t offset = 0;
  meta::type value_type{};
  bool writable = false;
};

struct property_const_data_request {
  const void* object = nullptr;
  const void* result = nullptr;
};

struct property_mutable_data_request {
  void* object = nullptr;
  void* result = nullptr;
};

struct property_get_any_request {
  const void* object = nullptr;
  any result{};
};

struct property_get_const_ref_request {
  const void* object = nullptr;
  any_ref result{};
};

struct property_get_mutable_ref_request {
  void* object = nullptr;
  any_ref result{};
};

struct property_set_any_request {
  void* object = nullptr;
  const any* source = nullptr;
  bool result = false;
};

/** @brief 拥有具体属性模型的最小类型擦除 wrapper。 */
class property_wrapper final {
public:
  using invoke_fn = bool (*)(void*, property_request_kind, request_payload);
  using destroy_fn = void (*)(void*, memory::memory_resource*) noexcept;

  constexpr property_wrapper() noexcept = default;
  constexpr property_wrapper(void* state, invoke_fn invoke, destroy_fn destroy,
                             memory::memory_resource* resource) noexcept
      : state_(state), invoke_(invoke), destroy_(destroy), resource_(resource) {}

  property_wrapper(const property_wrapper&) = delete;
  property_wrapper& operator=(const property_wrapper&) = delete;

  property_wrapper(property_wrapper&& other) noexcept
      : state_(std::exchange(other.state_, nullptr)),
        invoke_(std::exchange(other.invoke_, nullptr)),
        destroy_(std::exchange(other.destroy_, nullptr)),
        resource_(std::exchange(other.resource_, nullptr)) {}

  property_wrapper& operator=(property_wrapper&& other) noexcept {
    if (this != &other) {
      reset();
      state_ = std::exchange(other.state_, nullptr);
      invoke_ = std::exchange(other.invoke_, nullptr);
      destroy_ = std::exchange(other.destroy_, nullptr);
      resource_ = std::exchange(other.resource_, nullptr);
    }
    return *this;
  }

  ~property_wrapper() { reset(); }

  [[nodiscard]] bool valid() const noexcept { return state_ != nullptr && invoke_ != nullptr; }

  [[nodiscard]] bool invoke(property_request_kind kind, request_payload payload) const {
    return valid() && invoke_(state_, kind, payload);
  }

private:
  void reset() noexcept {
    if (state_ != nullptr && destroy_ != nullptr) {
      destroy_(state_, resource_);
    }
    state_ = nullptr;
    invoke_ = nullptr;
    destroy_ = nullptr;
    resource_ = nullptr;
  }

  void* state_ = nullptr;
  invoke_fn invoke_ = nullptr;
  destroy_fn destroy_ = nullptr;
  memory::memory_resource* resource_ = nullptr;
};

template <typename Model, typename... Args>
[[nodiscard]] property_wrapper
make_property_wrapper_with_resource(memory::memory_resource* resource, Args&&... args) {
  resource = resource != nullptr ? resource : &memory::process_resource();
  Model* state = memory::make<Model>(resource, std::forward<Args>(args)...);
  return property_wrapper{
      state,
      &Model::invoke,
      [](void* value, memory::memory_resource* owner) noexcept {
        memory::destroy(static_cast<Model*>(value), owner);
      },
      resource,
  };
}

template <typename Model, typename... Args>
[[nodiscard]] property_wrapper make_property_wrapper(Args&&... args) {
  return make_property_wrapper_with_resource<Model>(nullptr, std::forward<Args>(args)...);
}

/** @brief 注册表内部保存的属性注册数据和 opaque wrapper。 */
class property_record final {
public:
  property_record() noexcept = default;
  property_record(string_name name, property_wrapper wrapper,
                  memory::memory_resource* resource = nullptr) noexcept
      : name_(name), resource_(resource != nullptr ? resource : &memory::process_resource()),
        wrapper_(std::move(wrapper)) {}

  property_record(const property_record&) = delete;
  property_record& operator=(const property_record&) = delete;
  property_record(property_record&&) noexcept = default;
  property_record& operator=(property_record&&) noexcept = default;
  ~property_record() = default;

  [[nodiscard]] bool valid() const noexcept {
    return !name_.empty() && wrapper_.valid() && get_type().valid();
  }

  [[nodiscard]] string_name name() const noexcept { return name_; }
  [[nodiscard]] const meta::metadata_map& metadata() const noexcept {
    return metadata_ != nullptr ? *metadata_ : empty_metadata_map();
  }
  [[nodiscard]] const meta::property_schema& schema() const noexcept { return schema_; }
  [[nodiscard]] const meta::metadata_value* find_metadata(meta::metadata_key key) const noexcept {
    return metadata_ != nullptr ? meta::find_metadata(*metadata_, key) : nullptr;
  }

  void set_metadata(meta::metadata_key key, meta::metadata_value value) {
    if (metadata_ == nullptr) {
      metadata_ =
          memory::make_unique<meta::metadata_map>(resource_, meta::make_metadata_map(resource_));
    }
    meta::set_metadata(*metadata_, key, std::move(value));
  }

  void apply_schema_entry(meta::registration::property_schema_entry entry) {
    meta::registration::apply_property_schema_entry(schema_, std::move(entry));
  }

  [[nodiscard]] meta::type get_owner_type() const noexcept {
    property_type_request request{};
    return wrapper_.invoke(property_request_kind::owner_type, request_payload{request})
               ? request.result
               : meta::type{};
  }

  [[nodiscard]] meta::type get_type() const noexcept {
    property_type_request request{};
    return wrapper_.invoke(property_request_kind::value_type, request_payload{request})
               ? request.result
               : meta::type{};
  }

  [[nodiscard]] property_storage_info storage_info() const noexcept {
    property_storage_info request{};
    (void)wrapper_.invoke(property_request_kind::storage_info, request_payload{request});
    return request;
  }

  [[nodiscard]] bool writable() const noexcept { return storage_info().writable; }
  [[nodiscard]] property_storage_kind storage_kind() const noexcept { return storage_info().kind; }
  [[nodiscard]] std::size_t offset() const noexcept { return storage_info().offset; }

  [[nodiscard]] const void* data(const void* object) const noexcept {
    property_const_data_request request{object};
    return wrapper_.invoke(property_request_kind::const_data, request_payload{request})
               ? request.result
               : nullptr;
  }

  [[nodiscard]] void* mutable_data(void* object) const noexcept {
    property_mutable_data_request request{object};
    return wrapper_.invoke(property_request_kind::mutable_data, request_payload{request})
               ? request.result
               : nullptr;
  }

  [[nodiscard]] any get_value(const void* object) const {
    property_get_any_request request{object};
    (void)wrapper_.invoke(property_request_kind::get_any, request_payload{request});
    return std::move(request.result);
  }

  [[nodiscard]] any_ref get_ref(const void* object) const {
    property_get_const_ref_request request{object};
    (void)wrapper_.invoke(property_request_kind::get_const_ref, request_payload{request});
    return request.result;
  }

  [[nodiscard]] any_ref get_ref(void* object) const {
    property_get_mutable_ref_request mutable_request{object};
    if (wrapper_.invoke(property_request_kind::get_mutable_ref, request_payload{mutable_request}) &&
        mutable_request.result) {
      return mutable_request.result;
    }
    return get_ref(static_cast<const void*>(object));
  }

  [[nodiscard]] any_ref get_ref_unchecked(meta::type, const void* object) const {
    return get_ref(object);
  }

  [[nodiscard]] any_ref get_ref_unchecked(meta::type, void* object) const {
    return get_ref(object);
  }

  bool try_set_value(void* object, const any& source) const {
    property_set_any_request request{object, &source};
    return wrapper_.invoke(property_request_kind::set_any, request_payload{request}) &&
           request.result;
  }

  [[nodiscard]] meta::value get_runtime_value(const void* object) const;
  [[nodiscard]] meta::value get_runtime_value(void* object) const;
  [[nodiscard]] bool try_set_runtime_value(void* object, const meta::value& source) const;

private:
  [[nodiscard]] static const meta::metadata_map& empty_metadata_map() noexcept {
    static const meta::metadata_map empty{
        typename meta::metadata_map::allocator_type{&memory::process_resource()}};
    return empty;
  }

  string_name name_{};
  meta::property_schema schema_{};
  memory::unique_ptr<meta::metadata_map> metadata_{};
  memory::memory_resource* resource_ = &memory::process_resource();
  property_wrapper wrapper_{};
};

} // namespace caors::meta::detail

#endif
