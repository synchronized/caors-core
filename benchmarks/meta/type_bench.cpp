// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "support/benchmark_support.h"

#include "caors/registration.h"

#include <ctrack.hpp>

namespace {

struct registry_lookup_primary_type {
  int value = 0;
  [[nodiscard]] int get_value() const noexcept { return value; }
};

struct registry_lookup_frozen_type {
  int value = 0;
  [[nodiscard]] int get_value() const noexcept { return value; }
};

struct registry_lookup_layer_type {
  int value = 0;
  [[nodiscard]] int get_value() const noexcept { return value; }
};

} // namespace

namespace caors::benchmarks {

void benchmark_meta_lookup(std::size_t iterations) {
  {
    CTRACK_NAME("meta.resolve");
    for (std::size_t index = 0; index < iterations; ++index) {
      const caors::meta::type type = caors::meta::resolve<benchmark_actor>();
      consume(type.valid() ? 1U : 0U);
    }
  }

  {
    CTRACK_NAME("meta.find_registered");
    for (std::size_t index = 0; index < iterations; ++index) {
      const caors::meta::type type = caors::meta::find<benchmark_actor>();
      consume(type.registered() ? 1U : 0U);
    }
  }

  caors::meta::registry primary_registry{};
  caors::meta::context primary_context{primary_registry};
  if (caors::registration::type_<registry_lookup_primary_type>(primary_context,
                                                               "caors.benchmark.registry.primary")
          .meta()
          .property("value", &registry_lookup_primary_type::value)
          .method("get_value", &registry_lookup_primary_type::get_value)
          .serialization()
          .field("value")
          .commit()
          .failed()) {
    return;
  }
  const caors::meta::type primary_type =
      caors::meta::find<registry_lookup_primary_type>(primary_context);
  const caors::type_id primary_id = primary_type.id();

  caors::meta::registry frozen_registry{caors::meta::registry_options{
      caors::meta::registry_management_policy::frozen_read_optimized}};
  caors::meta::context frozen_context{frozen_registry};
  if (caors::registration::type_<registry_lookup_frozen_type>(frozen_context,
                                                              "caors.benchmark.registry.frozen")
          .meta()
          .property("value", &registry_lookup_frozen_type::value)
          .method("get_value", &registry_lookup_frozen_type::get_value)
          .serialization()
          .field("value")
          .commit()
          .failed() ||
      frozen_registry.freeze().failed()) {
    return;
  }
  const caors::meta::type frozen_type =
      caors::meta::find<registry_lookup_frozen_type>(frozen_context);
  const caors::type_id frozen_id = frozen_type.id();
  const caors::string_name frozen_property_name{"value"};
  const caors::string_name frozen_method_name{"get_value"};

  caors::meta::registry layered_registry{};
  caors::meta::context layered_context{layered_registry};
  auto layer = layered_registry.create_layer();
  caors::meta::context layer_context{layer->get_registry()};
  if (caors::registration::type_<registry_lookup_layer_type>(layer_context,
                                                             "caors.benchmark.registry.layer")
          .meta()
          .property("value", &registry_lookup_layer_type::value)
          .method("get_value", &registry_lookup_layer_type::get_value)
          .serialization()
          .field("value")
          .commit()
          .failed() ||
      layered_registry.publish_layer(*layer).failed()) {
    return;
  }
  const caors::meta::type layer_type =
      caors::meta::find<registry_lookup_layer_type>(layered_context);
  const caors::type_id layer_id = layer_type.id();

  {
    CTRACK_NAME("meta.registry.locked_find_type_id");
    for (std::size_t index = 0; index < iterations; ++index) {
      const caors::meta::type type = primary_registry.find_type(primary_id);
      consume(type.registered() ? 1U : 0U);
    }
  }

  {
    CTRACK_NAME("meta.registry.frozen_find_type_id");
    for (std::size_t index = 0; index < iterations; ++index) {
      const caors::meta::type type = frozen_registry.find_type(frozen_id);
      consume(type.registered() ? 1U : 0U);
    }
  }

  {
    CTRACK_NAME("meta.registry.layer_find_type_id");
    for (std::size_t index = 0; index < iterations; ++index) {
      const caors::meta::type type = layered_registry.find_type(layer_id);
      consume(type.registered() ? 1U : 0U);
    }
  }

  {
    CTRACK_NAME("meta.registry.frozen_contains");
    for (std::size_t index = 0; index < iterations; ++index) {
      consume(frozen_registry.contains(frozen_id) ? 1U : 0U);
    }
  }

  {
    CTRACK_NAME("meta.registry.frozen_property_lookup");
    for (std::size_t index = 0; index < iterations; ++index) {
      const caors::meta::property property = frozen_type.find_property(frozen_property_name);
      consume(property ? 1U : 0U);
    }
  }

  {
    CTRACK_NAME("meta.registry.frozen_method_lookup");
    for (std::size_t index = 0; index < iterations; ++index) {
      const caors::meta::method_set method = frozen_type.find_method(frozen_method_name);
      consume(method ? 1U : 0U);
    }
  }

  {
    CTRACK_NAME("meta.registry.locked_schema_lookup");
    for (std::size_t index = 0; index < iterations; ++index) {
      const caors::serialization::schema_view schema =
          caors::serialization::find_schema(primary_type);
      consume(schema ? schema.field_count() : 0U);
    }
  }

  {
    CTRACK_NAME("meta.registry.frozen_schema_lookup");
    for (std::size_t index = 0; index < iterations; ++index) {
      const caors::serialization::schema_view schema =
          caors::serialization::find_schema(frozen_type);
      consume(schema ? schema.field_count() : 0U);
    }
  }

  {
    CTRACK_NAME("meta.registry.layer_schema_lookup");
    for (std::size_t index = 0; index < iterations; ++index) {
      const caors::serialization::schema_view schema =
          caors::serialization::find_schema(layer_type);
      consume(schema ? schema.field_count() : 0U);
    }
  }
}

} // namespace caors::benchmarks
