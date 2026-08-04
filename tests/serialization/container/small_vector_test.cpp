// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory.h"
#include "serialization_test_support.h"

namespace {

struct small_vector_actor {
  small_vector_actor() = default;
  explicit small_vector_actor(caors::memory_resource* resource) : values(resource) {}

  caors::small_vector<caors::string, 1> values{};
};

void require_small_vector_actor_metadata() {
  REQUIRE(caors::tests::serialization::registration_result_ok(
      caors::registration::type_<small_vector_actor>("caors.test.serialization.small_vector_actor")
          .meta()
          .property("values", &small_vector_actor::values)
          .serialization()
          .commit()));
}

} // namespace

TEST_CASE("serialization - small vector JSON roundtrip preserves container resource",
          "[serialization][small_vector]") {
  require_small_vector_actor_metadata();
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};
  small_vector_actor source{&tracking};
  source.values.emplace_back("Ada");
  source.values.emplace_back("Grace");

  const caors::serialization::serialization_text_result encoded = caors::to_json(source);
  REQUIRE(encoded.succeeded());
  CHECK(encoded.value == R"({"values":["Ada","Grace"]})");

  small_vector_actor decoded{&tracking};
  REQUIRE(caors::read_json_from(decoded, encoded.value).succeeded());
  CHECK(decoded.values.resource() == &tracking);
  REQUIRE(decoded.values.size() == 2);
  CHECK(decoded.values[0] == "Ada");
  CHECK(decoded.values[1] == "Grace");
  CHECK(decoded.values[0].get_allocator().resource() == &tracking);
  CHECK(decoded.values[1].get_allocator().resource() == &tracking);
}

TEST_CASE("serialization - small vector MessagePack roundtrip hides storage mode",
          "[serialization][messagepack][small_vector]") {
  require_small_vector_actor_metadata();
  caors::tracking_resource tracking{&caors::get_system_memory_resource()};
  small_vector_actor source{&tracking};
  source.values.emplace_back("inline");
  source.values.emplace_back("dynamic");
  REQUIRE_FALSE(source.values.uses_inline_storage());

  const caors::io::byte_buffer_result encoded = caors::to_messagepack(source);
  REQUIRE(encoded.succeeded());

  small_vector_actor decoded{&tracking};
  REQUIRE(caors::read_messagepack_from(decoded, encoded.value).succeeded());
  CHECK(decoded.values.resource() == &tracking);
  REQUIRE(decoded.values.size() == 2);
  CHECK(decoded.values[0] == "inline");
  CHECK(decoded.values[1] == "dynamic");
  CHECK(decoded.values[0].get_allocator().resource() == &tracking);
  CHECK(decoded.values[1].get_allocator().resource() == &tracking);
}
