// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_polymorphic_value_factory_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::polymorphic_value_factory;

TEST_CASE("serialization - polymorphic value factory uses memory resource", "[serialization]") {
  require_polymorphic_factory_metadata();

  polymorphic_factory_holder holder{};
  auto shared = std::make_shared<polymorphic_factory_player>(1);
  shared->level = 41;
  shared->name = "Rin";
  holder.shared = std::move(shared);

  const caors::serialization::serialization_text_result json = caors::serialize_json(holder);
  REQUIRE(json.succeeded());

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::serialization::context context{};
  context.memory_resource = &tracking;

  polymorphic_factory_holder decoded{};
  const caors::error status = caors::deserialize_json(decoded, json.value, context);
  REQUIRE(status.succeeded());

  auto* decoded_shared = dynamic_cast<polymorphic_factory_player*>(decoded.shared.get());
  REQUIRE(decoded_shared != nullptr);
  CHECK(decoded_shared->level == 41);
  CHECK(decoded_shared->name == "Rin");
  CHECK(tracking.statistics().allocation_count == 2);
  CHECK(tracking.statistics().bytes_in_use >= sizeof(polymorphic_factory_player));

  decoded.shared.reset();
  CHECK(tracking.statistics().deallocation_count == 2);
  CHECK(tracking.statistics().bytes_in_use == 0);
}
