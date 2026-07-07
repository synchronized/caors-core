// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_object_ptr_graph_test_support.h"

using namespace caors::tests::serialization;
using namespace caors::tests::serialization::object_ptr_graph;

TEST_CASE("serialization - roundtrips object pointer graphs through runtime ids",
          "[serialization]") {
  require_object_ptr_target_class_metadata();
  require_object_ptr_graph_metadata();

  caors::objects::manager manager;
  object_ptr_target* primary = manager.create<object_ptr_target>();
  object_ptr_target* secondary = manager.create<object_ptr_target>();
  REQUIRE(primary != nullptr);
  REQUIRE(secondary != nullptr);

  const caors::object_id primary_id = primary->id();
  const caors::object_id secondary_id = secondary->id();
  const caors::serialization::reference_resolver resolver =
      caors::serialization::make_reference_resolver(manager);
  caors::serialization::context context{};
  context.references.object_resolver = &resolver;

  object_ptr_graph_actor graph{};
  graph.primary.reset(primary);
  graph.secondary.reset(secondary);
  graph.related.push_back(caors::object_ptr<object_ptr_target>{primary});
  graph.related.push_back(caors::object_ptr<object_ptr_target>{secondary});
  graph.related.push_back(caors::object_ptr<object_ptr_target>{primary});
  graph.related.push_back(caors::object_ptr<object_ptr_target>{});

  const caors::serialization::serialization_text_result json =
      caors::serialize_json(graph, context);
  REQUIRE(json.succeeded());
  const std::string expected_json = "{\"primary\":" + std::to_string(primary_id.value()) +
                                    ",\"secondary\":" + std::to_string(secondary_id.value()) +
                                    ",\"related\":[" + std::to_string(primary_id.value()) + "," +
                                    std::to_string(secondary_id.value()) + "," +
                                    std::to_string(primary_id.value()) + ",0]}";
  CHECK(std::string_view{json.value.data(), json.value.size()} == std::string_view{expected_json});

  object_ptr_graph_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, json.value, context);
  REQUIRE(result.succeeded());
  CHECK(decoded.primary.get() == primary);
  CHECK(decoded.secondary.get() == secondary);
  REQUIRE(decoded.related.size() == 4);
  CHECK(decoded.related[0].get() == primary);
  CHECK(decoded.related[1].get() == secondary);
  CHECK(decoded.related[2].get() == decoded.primary.get());
  CHECK(decoded.related[3].get() == nullptr);

  REQUIRE(manager.destroy(secondary_id));

  object_ptr_graph_actor missing{};
  const caors::error missing_result = caors::deserialize_json(missing, json.value, context);
  CHECK(missing_result.failed());
  CHECK(missing_result.value() == caors::errc::not_found);
}
