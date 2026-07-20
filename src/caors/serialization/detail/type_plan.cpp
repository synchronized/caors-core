// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "type_plan.h"

#include "caors/memory/resource.h"
#include "caors/memory/unique_ptr.h"
#include "caors/meta/registry.h"

#include <mutex>
#include <utility>

namespace caors::serialization::detail {
namespace {

struct type_plan_cache {
  type_plan_cache()
      : plans{core::hash_map<std::uint64_t, memory::unique_ptr<type_plan>>::allocator_type{
            &memory::library_resource()}} {}

  std::mutex mutex{};
  core::hash_map<std::uint64_t, memory::unique_ptr<type_plan>> plans;
};

[[nodiscard]] type_plan_cache& cache() {
  static type_plan_cache instance;
  return instance;
}

[[nodiscard]] bool plan_matches_type(const type_plan& plan,
                                     const serialization::type_schema* schema,
                                     const meta::type& type) noexcept {
  return plan.source_schema == schema && plan.property_count == type.property_count();
}

[[nodiscard]] result<type_plan> build_type_plan(const meta::type& type,
                                                const serialization::type_schema* schema) {
  if (schema == nullptr) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  if (!schema->valid()) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  type_plan plan{};
  plan.source_schema = schema;
  plan.object_schema = *schema;
  plan.property_count = type.property_count();
  plan.has_explicit_fields = !schema->fields.empty();

  if (!schema->fields.empty()) {
    plan.fields.reserve(schema->fields.size());
    for (const serialization::field_schema& field_schema : schema->fields) {
      const meta::property property = type.find_property(field_schema.field_name);
      if (!property) {
        return {{error_domain::serialization, errc::not_found}, {}};
      }

      plan.fields.push_back(planned_field{field_schema, property});
    }

    return {{}, std::move(plan)};
  }

  plan.fields.reserve(type.property_count());
  for (std::size_t index = 0; index < type.property_count(); ++index) {
    const meta::property property = type.property_at(index);
    if (!property) {
      return {{error_domain::serialization, errc::invalid_field}, {}};
    }

    plan.fields.push_back(planned_field{
        serialization::make_field(property.name().view()),
        property,
    });
  }

  return {{}, std::move(plan)};
}

} // namespace

result<const type_plan*> find_type_plan(const meta::type& type) {
  if (!type.valid()) {
    return {{error_domain::serialization, errc::invalid_type}, nullptr};
  }

  const serialization::type_schema* schema =
      meta::get_registry().find_extension_ptr<serialization::type_schema>(type.id());
  if (schema == nullptr) {
    return {{error_domain::serialization, errc::missing_type}, nullptr};
  }

  type_plan_cache& type_cache = cache();
  std::lock_guard lock{type_cache.mutex};

  const std::uint64_t key = type.id().value();
  auto found = type_cache.plans.find(key);
  if (found != type_cache.plans.end() && found->second &&
      plan_matches_type(*found->second, schema, type)) {
    return {{}, found->second.get()};
  }

  result<type_plan> built = build_type_plan(type, schema);
  if (built.failed()) {
    return {built.status, nullptr};
  }

  memory::unique_ptr<type_plan> stored =
      memory::make_unique<type_plan>(&memory::library_resource(), std::move(built.value));
  const type_plan* result = stored.get();
  type_cache.plans[key] = std::move(stored);
  return {{}, result};
}

} // namespace caors::serialization::detail
