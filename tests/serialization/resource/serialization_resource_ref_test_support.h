// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "serialization_test_support.h"

#include "caors/resources.h"
#include "caors/resources/serialization.h"

namespace caors::tests::serialization::resource_ref_codec {

constexpr caors::resource_guid texture_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x6c, 0xd2, 0xda, 0x3d, 0xb5, 0x49, 0x47, 0x56, 0x8b, 0x8b, 0x0c, 0x84,
                            0x29, 0x3a, 0xf4, 0x10})};

class serialized_texture_resource : public caors::resource {
public:
  using caors::resource::resource;
};

struct resource_ref_actor {
  caors::resource_ref<serialized_texture_resource> texture{};
};

inline void require_resource_ref_actor_metadata() {
  REQUIRE(type_result_ok(caors::meta::class_<serialized_texture_resource>(
                             "caors.test.serialization.serialized_texture_resource")
                             .result()));
  REQUIRE(
      type_result_ok(caors::meta::class_<caors::resource_ref<serialized_texture_resource>>(
                         "caors.resource_ref<caors.test.serialization.serialized_texture_resource>")
                         .result()));

  REQUIRE(reflection_result_ok(
      ((void)caors::meta::class_<resource_ref_actor>("caors.test.serialization.resource_ref_actor"),
       caors::meta::class_<resource_ref_actor>())
          .property("texture", &resource_ref_actor::texture)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::class_<resource_ref_actor>().commit()));
}

} // namespace caors::tests::serialization::resource_ref_codec
