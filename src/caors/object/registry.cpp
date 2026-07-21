// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/object/registry.h"

#include "caors/meta/capability_traits.h"
#include "caors/registration.h"

namespace caors::objects {
namespace {

[[nodiscard]] type make_object_type(meta::type source) noexcept {
  type object_type{source};
  return object_type ? object_type : type{};
}

[[nodiscard]] object_factory_capability factory_of(const type& object_type) noexcept {
  return object_type ? object_factory_capability{object_type.info()} : object_factory_capability{};
}

} // namespace

namespace detail {

error ensure_object_base_registered(meta::registry& registry) {
  meta::context context{registry};
  const error status = registration::type_<object>(context, object::static_type_name()).commit();
  return status.value() == errc::duplicate_id ? error{} : status;
}

} // namespace detail

type find(meta::context& context, object_type_id id) {
  return make_object_type(meta::find(context, id));
}

type find(meta::context& context, std::string_view name) {
  return make_object_type(meta::find(context, name));
}

bool contains(meta::context& context, object_type_id id) {
  return static_cast<bool>(::caors::objects::find(context, id));
}

bool is_a(meta::context& context, object_type_id id, object_type_id base_id) {
  return meta::is_a(context, id, base_id);
}

std::size_t size(meta::context& context) noexcept {
  return meta::detail::capability_count(context.get_registry(),
                                        meta::capability_key_of<object_factory_ops>());
}

object* try_create(meta::context& context, object_type_id id, memory_resource* resource) noexcept {
  return factory_of(::caors::objects::find(context, id)).try_create(resource);
}

object* try_create(meta::context& context, std::string_view name,
                   memory_resource* resource) noexcept {
  return factory_of(::caors::objects::find(context, name)).try_create(resource);
}

object* create(meta::context& context, object_type_id id, memory_resource* resource) {
  return factory_of(::caors::objects::find(context, id)).create(resource);
}

object* create(meta::context& context, std::string_view name, memory_resource* resource) {
  return factory_of(::caors::objects::find(context, name)).create(resource);
}

bool destroy(meta::context& context, object_type_id id, object* instance,
             memory_resource* resource) noexcept {
  if (instance == nullptr) {
    return false;
  }

  return factory_of(::caors::objects::find(context, id)).destroy(instance, resource);
}

} // namespace caors::objects
