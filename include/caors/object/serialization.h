// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_SERIALIZATION_H_
#define CAORS_OBJECT_SERIALIZATION_H_

#include "caors/object/handle.h"
#include "caors/object/manager.h"
#include "caors/object/ptr.h"
#include "caors/serialization/codec.h"
#include "caors/serialization/types.h"

#include <optional>
#include <string>
#include <string_view>

namespace caors::objects {

using object_ref_resolver_ops = serialization::external_ref_resolver_ops;

/** @brief 对象引用在通用外部引用系统中的 domain。 */
[[nodiscard]] inline string_name object_ref_domain() { return string_name{"object"}; }

/** @brief 构造运行期对象 id 形式的外部引用。 */
[[nodiscard]] inline serialization::external_ref make_object_external_ref(object_id id,
                                                                          object_type_id type) {
  return serialization::external_ref{
      object_ref_domain(), {}, type, id.value(), serialization::external_ref_identity::integer,
  };
}

/** @brief 构造持久对象 GUID 形式的外部引用。 */
[[nodiscard]] inline serialization::external_ref make_object_external_ref(object_guid guid,
                                                                          object_type_id type) {
  return serialization::external_ref{
      object_ref_domain(), guid, type, 0, serialization::external_ref_identity::guid,
  };
}

namespace detail {

[[nodiscard]] inline bool manager_object_to_runtime_ref(const void* instance, type_id type,
                                                        serialization::external_ref& ref,
                                                        void* user_data) noexcept {
  ref = objects::make_object_external_ref(object_id{}, type);
  if (instance == nullptr) {
    return true;
  }

  auto* manager = static_cast<objects::manager*>(user_data);
  auto* object = static_cast<const objects::object*>(instance);
  if (manager == nullptr || manager->find(object->id()) != object) {
    return false;
  }

  ref = objects::make_object_external_ref(object->id(), type);
  return true;
}

[[nodiscard]] inline bool manager_object_to_persistent_ref(const void* instance, type_id type,
                                                           serialization::external_ref& ref,
                                                           void* user_data) noexcept {
  ref = objects::make_object_external_ref(object_guid{}, type);
  if (instance == nullptr) {
    return true;
  }

  auto* manager = static_cast<objects::manager*>(user_data);
  auto* object = static_cast<const objects::object*>(instance);
  if (manager == nullptr || manager->find(object->id()) != object) {
    return false;
  }

  const object_guid guid = manager->guid_of(object->id());
  if (!guid) {
    return false;
  }

  ref = objects::make_object_external_ref(guid, type);
  return true;
}

[[nodiscard]] inline void* manager_object_ref_resolve(const serialization::external_ref& ref,
                                                      void* user_data) noexcept {
  if (ref.domain != objects::object_ref_domain()) {
    return nullptr;
  }

  auto* manager = static_cast<objects::manager*>(user_data);
  if (manager == nullptr) {
    return nullptr;
  }

  if (ref.identity == serialization::external_ref_identity::integer) {
    return manager->find(object_id{ref.integer_id});
  }

  return manager->find(object_guid{ref.id});
}

[[nodiscard]] inline void* manager_object_create(type_id type, memory::memory_resource*,
                                                 void* user_data) {
  auto* manager = static_cast<objects::manager*>(user_data);
  return manager != nullptr ? manager->create(type) : nullptr;
}

inline void manager_object_destroy(void* instance, void* user_data) noexcept {
  auto* manager = static_cast<objects::manager*>(user_data);
  if (manager != nullptr && instance != nullptr) {
    (void)manager->destroy(static_cast<objects::object*>(instance));
  }
}

[[nodiscard]] inline const serialization::external_ref_resolver_ops*
active_object_ref_resolver(const serialization::context& context) noexcept {
  return context.effective_external_ref_resolver();
}

[[nodiscard]] inline serialization::external_ref read_runtime_object_ref(std::uint64_t encoded_id,
                                                                         type_id type) noexcept {
  return objects::make_object_external_ref(object_id{encoded_id}, type);
}

[[nodiscard]] inline serialization::external_ref
read_persistent_object_ref(std::string_view encoded_guid, type_id type, error& status) {
  const std::optional<object_guid> parsed = object_guid::parse(encoded_guid);
  if (!parsed.has_value()) {
    status = {error_domain::serialization, errc::invalid_argument};
    return {};
  }

  status = {};
  return objects::make_object_external_ref(*parsed, type);
}

[[nodiscard]] inline serialization::type_schema object_handle_type_schema() {
  return serialization::make_type_schema({
      serialization::make_field("id", {}, false, 1),
      serialization::make_field("type", {}, false, 2),
  });
}

[[nodiscard]] inline serialization::variant_result make_object_handle_value(objects::handle value) {
  variant::object_type members;
  members.reserve(2);
  members.push_back(meta::variant_member{
      core::string_name{"id"},
      1,
      variant::from(value.id().value()),
  });
  members.push_back(meta::variant_member{
      core::string_name{"type"},
      2,
      variant::from(value.type().value()),
  });

  return {{}, variant::object(std::move(members))};
}

[[nodiscard]] inline error write_object_handle(serialization::writer& writer,
                                               objects::handle value) {
  const serialization::type_schema type = object_handle_type_schema();
  error status = writer.begin_object(type);
  if (status) {
    return status;
  }

  status = writer.begin_field(type.fields[0]);
  if (status) {
    return status;
  }
  status = writer.write_u64(value.id().value());
  if (status) {
    return status;
  }
  status = writer.end_field();
  if (status) {
    return status;
  }

  status = writer.begin_field(type.fields[1]);
  if (status) {
    return status;
  }
  status = writer.write_u64(value.type().value());
  if (status) {
    return status;
  }
  status = writer.end_field();
  if (status) {
    return status;
  }

  return writer.end_object();
}

[[nodiscard]] inline error read_object_handle(serialization::reader& reader,
                                              objects::handle& value) {
  const serialization::type_schema type = object_handle_type_schema();
  error status = reader.begin_object(type);
  if (status) {
    return status;
  }

  std::uint64_t id = 0;
  status = reader.begin_field(type.fields[0]);
  if (status) {
    return status;
  }
  status = reader.read_u64(id);
  if (status) {
    return status;
  }
  status = reader.end_field();
  if (status) {
    return status;
  }

  std::uint64_t object_type = 0;
  status = reader.begin_field(type.fields[1]);
  if (status) {
    return status;
  }
  status = reader.read_u64(object_type);
  if (status) {
    return status;
  }
  status = reader.end_field();
  if (status) {
    return status;
  }

  status = reader.end_object();
  if (status) {
    return status;
  }

  value = objects::handle{object_id{id}, object_type_id{object_type}};
  return {};
}

template <typename T>
[[nodiscard]] serialization::variant_result
make_object_ptr_value(const serialization::context& context, const objects::ptr<T>& value) {
  const serialization::external_ref_resolver_ops* resolver = active_object_ref_resolver(context);
  if (resolver == nullptr || !resolver->can_encode()) {
    return {{error_domain::serialization, errc::invalid_argument}, {}};
  }

  serialization::external_ref ref{};
  if (!resolver->encode_ref(value.get(), meta::id_of<T>(), ref)) {
    return {{error_domain::serialization, errc::not_found}, {}};
  }

  if (ref.identity == serialization::external_ref_identity::guid) {
    if (!ref.id) {
      return {{}, variant::null()};
    }

    const std::string text = ref.id.string();
    return {{}, variant::from(std::string_view{text.data(), text.size()})};
  }

  return {{}, variant::from(ref.integer_id)};
}

template <typename T>
[[nodiscard]] error write_object_ptr(const serialization::context& context,
                                     serialization::writer& writer, const objects::ptr<T>& value) {
  const serialization::external_ref_resolver_ops* resolver = active_object_ref_resolver(context);
  if (resolver == nullptr || !resolver->can_encode()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  serialization::external_ref ref{};
  if (!resolver->encode_ref(value.get(), meta::id_of<T>(), ref)) {
    return {error_domain::serialization, errc::not_found};
  }

  if (ref.identity == serialization::external_ref_identity::guid) {
    if (!ref.id) {
      return writer.write_null();
    }

    const std::string text = ref.id.string();
    return writer.write_string(std::string_view{text.data(), text.size()});
  }

  return writer.write_u64(ref.integer_id);
}

template <typename T>
[[nodiscard]] error read_object_ptr(const serialization::context& context,
                                    serialization::reader& reader, objects::ptr<T>& value) {
  const serialization::external_ref_resolver_ops* resolver = active_object_ref_resolver(context);
  if (resolver == nullptr || !resolver->can_resolve()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  serialization::external_ref ref{};
  if (resolver->identity == serialization::external_ref_identity::guid) {
    if (reader.is_null()) {
      value.reset();
      return {};
    }

    string encoded_guid{};
    error status = reader.read_string(encoded_guid);
    if (status) {
      return status;
    }

    ref = read_persistent_object_ref(std::string_view{encoded_guid.data(), encoded_guid.size()},
                                     meta::id_of<T>(), status);
    if (status) {
      return status;
    }

    if (!ref.id) {
      value.reset();
      return {};
    }
  } else {
    if (reader.is_null()) {
      value.reset();
      return {};
    }

    std::uint64_t encoded_id = 0;
    error status = reader.read_u64(encoded_id);
    if (status) {
      return status;
    }

    ref = read_runtime_object_ref(encoded_id, meta::id_of<T>());
    if (encoded_id == 0) {
      value.reset();
      return {};
    }
  }

  void* resolved = (*resolver)(ref);
  auto* object = static_cast<objects::object*>(resolved);
  if (object == nullptr) {
    return {error_domain::serialization, errc::not_found};
  }

  auto* typed = dynamic_cast<T*>(object);
  if (typed == nullptr) {
    return {error_domain::serialization, errc::invalid_type};
  }

  value.reset(typed);
  return {};
}

} // namespace detail

/** @brief 使用对象生命周期管理器创建运行期 id 形式的对象引用 resolver。 */
[[nodiscard]] inline object_ref_resolver_ops make_object_ref_resolver(objects::manager& manager) {
  return object_ref_resolver_ops{
      &detail::manager_object_ref_resolve,
      &manager,
      serialization::external_ref_identity::integer,
      &detail::manager_object_to_runtime_ref,
  };
}

/** @brief 使用对象生命周期管理器和对象索引创建持久 GUID 形式的对象引用 resolver。 */
[[nodiscard]] inline object_ref_resolver_ops
make_persistent_object_ref_resolver(objects::manager& manager) {
  return object_ref_resolver_ops{
      &detail::manager_object_ref_resolve,
      &manager,
      serialization::external_ref_identity::guid,
      &detail::manager_object_to_persistent_ref,
  };
}

/** @brief 使用对象生命周期管理器创建通用运行期类型工厂。 */
[[nodiscard]] inline serialization::polymorphic_factory_ops
make_object_factory(objects::manager& manager) noexcept {
  return serialization::polymorphic_factory_ops{&detail::manager_object_create,
                                                &detail::manager_object_destroy, &manager};
}

} // namespace caors::objects

namespace caors::serialization {

/** @brief `objects::ptr<T>` 按对象身份序列化，不展开对象本体。 */
template <typename T> struct codec_traits<objects::ptr<T>> {
  static constexpr bool can_write = true;
  static constexpr bool can_read = true;
  static constexpr bool can_make_value = true;

  [[nodiscard]] static error write(const serialization::context& context, writer& writer,
                                   const objects::ptr<T>& value) {
    return objects::detail::write_object_ptr(context, writer, value);
  }

  [[nodiscard]] static error read(const serialization::context& context, reader& reader,
                                  objects::ptr<T>& value) {
    return objects::detail::read_object_ptr(context, reader, value);
  }

  [[nodiscard]] static variant_result make_value(const serialization::context& context,
                                                 const objects::ptr<T>& value) {
    return objects::detail::make_object_ptr_value(context, value);
  }
};

/** @brief `objects::handle` 和 `objects::typed_handle<T>` 按 `{id,type}` 身份对象序列化。 */
template <typename T> struct codec_traits<objects::detail::basic_handle<T>> {
  static constexpr bool can_write = true;
  static constexpr bool can_read = true;
  static constexpr bool can_make_value = true;

  using handle_type = objects::detail::basic_handle<T>;

  [[nodiscard]] static error write(writer& writer, const handle_type& value) {
    return objects::detail::write_object_handle(writer, objects::detail::to_untyped_handle(value));
  }

  [[nodiscard]] static error read(reader& reader, handle_type& value) {
    objects::handle handle{};
    const error status = objects::detail::read_object_handle(reader, handle);
    if (status) {
      return status;
    }

    value.reset(handle);
    return {};
  }

  [[nodiscard]] static variant_result make_value(const handle_type& value) {
    return objects::detail::make_object_handle_value(objects::detail::to_untyped_handle(value));
  }
};

} // namespace caors::serialization

#endif
