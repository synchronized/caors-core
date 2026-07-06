// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include <string>
#include <string_view>

using namespace caors::tests::serialization;

namespace {

struct object_ptr_graph_actor {
  caors::object_ptr<object_ptr_target> primary{};
  caors::object_ptr<object_ptr_target> secondary{};
  caors::vector<caors::object_ptr<object_ptr_target>> related{};
};

void require_object_ptr_target_class_metadata() {
  const caors::error object_status =
      caors::objects::class_<object_ptr_target>("caors.test.serialization.object_ptr_target")
          .commit();
  REQUIRE((object_status == caors::error{} || object_status.value() == caors::errc::duplicate_id));
}

void require_object_ptr_graph_metadata() {
  require_registered_type<caors::object_ptr<object_ptr_target>>(
      "caors.object_ptr<caors.test.serialization.object_ptr_target>");
  require_registered_type<caors::vector<caors::object_ptr<object_ptr_target>>>(
      "caors.vector<caors.object_ptr<caors.test.serialization.object_ptr_target>>");
  REQUIRE(reflection_result_ok(caors::reflection::class_<object_ptr_graph_actor>(
                                   "caors.test.serialization.object_ptr_graph_actor")
                                   .field("primary", &object_ptr_graph_actor::primary)
                                   .field("secondary", &object_ptr_graph_actor::secondary)
                                   .field("related", &object_ptr_graph_actor::related)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<object_ptr_graph_actor>()));
}

} // namespace

TEST_CASE("serialization - serializes object pointers through resolver", "[serialization]") {
  REQUIRE(type_result_ok(caors::meta::type_<caors::object_ptr<object_ptr_target>>(
                             "caors.object_ptr<caors.test.serialization.object_ptr_target>")
                             .result()));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<object_ptr_actor>("caors.test.serialization.object_ptr_actor")
          .field("target", &object_ptr_actor::target)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<object_ptr_actor>()));

  object_ptr_target target;
  object_ptr_resolver_data resolver_data{caors::object_id{77}, &target};
  const caors::serialization::reference_resolver resolver{
      &resolve_object_id,
      &resolve_object,
      &resolver_data,
  };
  const caors::serialization::context context{0, &resolver};
  const object_ptr_actor actor{caors::object_ptr<object_ptr_target>{&target}};

  const caors::serialization::serialization_text_result text =
      caors::serialize_text(actor, context);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"target":77})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor, context);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* target_member =
      value.value.find_member(caors::string_name{"target"});
  REQUIRE(target_member != nullptr);
  CHECK(target_member->value.is_u64());
  CHECK(target_member->value.as_u64() == 77);

  const caors::serialization::serialization_text_result json =
      caors::serialize_json(actor, context);
  REQUIRE(json.succeeded());
  CHECK(json.value == R"({"target":77})");
}

TEST_CASE("serialization - deserializes object pointers through resolver", "[serialization]") {
  REQUIRE(type_result_ok(caors::meta::type_<caors::object_ptr<object_ptr_target>>(
                             "caors.object_ptr<caors.test.serialization.object_ptr_target>")
                             .result()));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<object_ptr_actor>("caors.test.serialization.object_ptr_actor")
          .field("target", &object_ptr_actor::target)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<object_ptr_actor>()));

  object_ptr_target target;
  object_ptr_resolver_data resolver_data{caors::object_id{88}, &target};
  const caors::serialization::reference_resolver resolver{
      &resolve_object_id,
      &resolve_object,
      &resolver_data,
  };

  caors::variant::object_type members;
  members.push_back(caors::value::variant_member{
      caors::string_name{"target"},
      0,
      caors::variant::from(std::uint64_t{88}),
  });

  object_ptr_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object(std::move(members)),
                                                 caors::serialization::context{0, &resolver});

  REQUIRE(result.succeeded());
  CHECK(actor.target.get() == &target);

  object_ptr_actor from_json{};
  const caors::error json_result = caors::deserialize_json(
      from_json, R"({"target":88})", caors::serialization::context{0, &resolver});
  REQUIRE(json_result.succeeded());
  CHECK(from_json.target.get() == &target);
}

TEST_CASE("serialization - serializes object pointers inside vectors", "[serialization]") {
  require_object_ptr_vector_actor_metadata();

  object_ptr_target target;
  object_ptr_resolver_data resolver_data{caors::object_id{77}, &target};
  const caors::serialization::reference_resolver resolver{
      &resolve_object_id,
      &resolve_object,
      &resolver_data,
  };
  const caors::serialization::context context{0, &resolver};

  object_ptr_vector_actor actor{};
  actor.targets.push_back(caors::object_ptr<object_ptr_target>{&target});
  actor.targets.push_back(caors::object_ptr<object_ptr_target>{});

  const caors::serialization::serialization_text_result text =
      caors::serialize_json(actor, context);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"targets":[77,0]})");

  const caors::serialization::variant_result value = caors::to_serialization_value(actor, context);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* targets =
      value.value.find_member(caors::string_name{"targets"});
  REQUIRE(targets != nullptr);
  REQUIRE(targets->value.is_array());
  REQUIRE(targets->value.as_array().size() == 2);
  CHECK(targets->value.as_array()[0].as_u64() == 77);
  CHECK(targets->value.as_array()[1].as_u64() == 0);
}

TEST_CASE("serialization - deserializes object pointers inside vectors", "[serialization]") {
  require_object_ptr_vector_actor_metadata();

  object_ptr_target target;
  object_ptr_resolver_data resolver_data{caors::object_id{88}, &target};
  const caors::serialization::reference_resolver resolver{
      &resolve_object_id,
      &resolve_object,
      &resolver_data,
  };

  object_ptr_vector_actor actor{};
  const caors::error result = caors::deserialize_json(actor, R"({"targets":[88,0]})",
                                                      caors::serialization::context{0, &resolver});

  REQUIRE(result.succeeded());
  REQUIRE(actor.targets.size() == 2);
  CHECK(actor.targets[0].get() == &target);
  CHECK(actor.targets[1].get() == nullptr);
}

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
  const caors::serialization::context context{0, &resolver};

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

TEST_CASE("serialization - roundtrips object pointer graphs through persistent guids",
          "[serialization]") {
  require_object_ptr_target_class_metadata();
  require_object_ptr_graph_metadata();

  const caors::object_guid primary_guid{caors::guid::from_bytes(
      caors::guid::byte_array{0x1c, 0x04, 0x1d, 0x36, 0x9e, 0x5e, 0x45, 0xe8, 0x93, 0x5c, 0x27,
                              0x28, 0x81, 0x41, 0x59, 0x44})};
  const caors::object_guid secondary_guid{caors::guid::from_bytes(
      caors::guid::byte_array{0x94, 0xa3, 0xf6, 0x77, 0xa9, 0x83, 0x4b, 0x1d, 0x81, 0x1f, 0x76,
                              0x8f, 0x5f, 0x73, 0x58, 0x0e})};
  const std::string primary_guid_text = primary_guid.string();
  const std::string secondary_guid_text = secondary_guid.string();

  caors::objects::object_index index;
  caors::objects::manager manager(index);
  object_ptr_target* primary = manager.create_with_guid<object_ptr_target>(primary_guid);
  object_ptr_target* secondary = manager.create_with_guid<object_ptr_target>(secondary_guid);
  REQUIRE(primary != nullptr);
  REQUIRE(secondary != nullptr);

  const caors::serialization::reference_resolver resolver =
      caors::serialization::make_persistent_reference_resolver(manager);
  const caors::serialization::context context{0, &resolver};

  object_ptr_graph_actor graph{};
  graph.primary.reset(primary);
  graph.secondary.reset(secondary);
  graph.related.push_back(caors::object_ptr<object_ptr_target>{secondary});
  graph.related.push_back(caors::object_ptr<object_ptr_target>{});
  graph.related.push_back(caors::object_ptr<object_ptr_target>{primary});

  const caors::serialization::serialization_text_result json =
      caors::serialize_json(graph, context);
  REQUIRE(json.succeeded());
  const std::string expected_json = "{\"primary\":\"" + primary_guid_text + "\",\"secondary\":\"" +
                                    secondary_guid_text + "\",\"related\":[\"" +
                                    secondary_guid_text + "\",null,\"" + primary_guid_text + "\"]}";
  CHECK(std::string_view{json.value.data(), json.value.size()} == std::string_view{expected_json});

  object_ptr_graph_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, json.value, context);
  REQUIRE(result.succeeded());
  CHECK(decoded.primary.get() == primary);
  CHECK(decoded.secondary.get() == secondary);
  REQUIRE(decoded.related.size() == 3);
  CHECK(decoded.related[0].get() == secondary);
  CHECK(decoded.related[1].get() == nullptr);
  CHECK(decoded.related[2].get() == decoded.primary.get());

  REQUIRE(manager.destroy(secondary->id()));

  object_ptr_graph_actor missing{};
  const caors::error missing_result = caors::deserialize_json(missing, json.value, context);
  CHECK(missing_result.failed());
  CHECK(missing_result.value() == caors::errc::not_found);
}

TEST_CASE("serialization - object pointer resolver failures are reported", "[serialization]") {
  REQUIRE(type_result_ok(caors::meta::type_<caors::object_ptr<object_ptr_target>>(
                             "caors.object_ptr<caors.test.serialization.object_ptr_target>")
                             .result()));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<object_ptr_actor>("caors.test.serialization.object_ptr_actor")
          .field("target", &object_ptr_actor::target)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<object_ptr_actor>()));

  object_ptr_target target;
  const object_ptr_actor actor{caors::object_ptr<object_ptr_target>{&target}};
  const caors::serialization::serialization_text_result missing_resolver =
      caors::serialize_text(actor);
  CHECK(missing_resolver.failed());
  CHECK(missing_resolver.status.value() == caors::errc::invalid_argument);

  object_ptr_resolver_data resolver_data{caors::object_id{99}, nullptr};
  const caors::serialization::reference_resolver resolver{
      &resolve_object_id,
      &resolve_object,
      &resolver_data,
  };

  caors::variant::object_type members;
  members.push_back(caors::value::variant_member{
      caors::string_name{"target"},
      0,
      caors::variant::from(std::uint64_t{99}),
  });

  object_ptr_actor decoded{};
  const caors::error missing_object =
      caors::deserialize(decoded, caors::variant::object(std::move(members)),
                         caors::serialization::context{0, &resolver});
  CHECK(missing_object.failed());
  CHECK(missing_object.value() == caors::errc::not_found);

  other_object_ptr_target other;
  object_ptr_resolver_data wrong_type_data{caors::object_id{100}, &other};
  const caors::serialization::reference_resolver wrong_type_resolver{
      &resolve_object_id,
      &resolve_object,
      &wrong_type_data,
  };

  caors::variant::object_type wrong_type_members;
  wrong_type_members.push_back(caors::value::variant_member{
      caors::string_name{"target"},
      0,
      caors::variant::from(std::uint64_t{100}),
  });

  const caors::error wrong_type =
      caors::deserialize(decoded, caors::variant::object(std::move(wrong_type_members)),
                         caors::serialization::context{0, &wrong_type_resolver});
  CHECK(wrong_type.failed());
  CHECK(wrong_type.value() == caors::errc::invalid_type);
}

TEST_CASE("serialization - object manager can resolve object pointers", "[serialization]") {
  const caors::error object_status =
      caors::objects::class_<object_ptr_target>("caors.test.serialization.object_ptr_target")
          .commit();
  REQUIRE((object_status == caors::error{} || object_status.value() == caors::errc::duplicate_id));
  REQUIRE(type_result_ok(caors::meta::type_<caors::object_ptr<object_ptr_target>>(
                             "caors.object_ptr<caors.test.serialization.object_ptr_target>")
                             .result()));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<object_ptr_actor>("caors.test.serialization.object_ptr_actor")
          .field("target", &object_ptr_actor::target)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<object_ptr_actor>()));

  caors::objects::manager manager;
  object_ptr_target* target = manager.create<object_ptr_target>();
  REQUIRE(target != nullptr);

  const caors::object_id target_id = target->id();
  const caors::serialization::reference_resolver resolver =
      caors::serialization::make_reference_resolver(manager);
  const caors::serialization::context context{0, &resolver};
  const object_ptr_actor actor{caors::object_ptr<object_ptr_target>{target}};

  const caors::serialization::serialization_text_result json =
      caors::serialize_json(actor, context);
  REQUIRE(json.succeeded());
  const std::string expected_json = "{\"target\":" + std::to_string(target_id.value()) + "}";
  CHECK(std::string_view{json.value.data(), json.value.size()} == std::string_view{expected_json});

  object_ptr_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, json.value, context);
  REQUIRE(result.succeeded());
  CHECK(decoded.target.get() == target);

  REQUIRE(manager.destroy(target_id));

  object_ptr_actor missing{};
  const caors::error missing_result = caors::deserialize_json(missing, json.value, context);
  CHECK(missing_result.failed());
  CHECK(missing_result.value() == caors::errc::not_found);
}

TEST_CASE("serialization - object manager can resolve persistent object guid references",
          "[serialization]") {
  const caors::error object_status =
      caors::objects::class_<object_ptr_target>("caors.test.serialization.object_ptr_target")
          .commit();
  REQUIRE((object_status == caors::error{} || object_status.value() == caors::errc::duplicate_id));
  REQUIRE(type_result_ok(caors::meta::type_<caors::object_ptr<object_ptr_target>>(
                             "caors.object_ptr<caors.test.serialization.object_ptr_target>")
                             .result()));
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<object_ptr_actor>("caors.test.serialization.object_ptr_actor")
          .field("target", &object_ptr_actor::target)
          .commit()));
  REQUIRE(serialization_result_ok(caors::serialization::register_type<object_ptr_actor>()));

  const caors::object_guid target_guid{caors::guid::from_bytes(
      caors::guid::byte_array{0x60, 0x13, 0x54, 0x9d, 0x91, 0xf7, 0x40, 0x9b, 0xb9, 0x2e, 0x10,
                              0x3d, 0xf4, 0x2c, 0x0c, 0x6a})};
  const std::string target_guid_text = target_guid.string();

  caors::objects::object_index index;
  caors::objects::manager manager(index);
  object_ptr_target* target = manager.create_with_guid<object_ptr_target>(target_guid);
  REQUIRE(target != nullptr);

  const caors::serialization::reference_resolver resolver =
      caors::serialization::make_persistent_reference_resolver(manager);
  const caors::serialization::context context{0, &resolver};
  const object_ptr_actor actor{caors::object_ptr<object_ptr_target>{target}};

  const caors::serialization::serialization_text_result json =
      caors::serialize_json(actor, context);
  REQUIRE(json.succeeded());
  const std::string expected_json = "{\"target\":\"" + target_guid_text + "\"}";
  CHECK(std::string_view{json.value.data(), json.value.size()} == std::string_view{expected_json});

  const caors::serialization::variant_result value = caors::to_serialization_value(actor, context);
  REQUIRE(value.succeeded());
  const caors::value::variant_member* target_member =
      value.value.find_member(caors::string_name{"target"});
  REQUIRE(target_member != nullptr);
  CHECK(target_member->value.is_string());
  CHECK(std::string_view{target_member->value.as_string().data(),
                         target_member->value.as_string().size()} ==
        std::string_view{target_guid_text});

  object_ptr_actor decoded{};
  const caors::error result = caors::deserialize_json(decoded, json.value, context);
  REQUIRE(result.succeeded());
  CHECK(decoded.target.get() == target);

  object_ptr_actor cleared{caors::object_ptr<object_ptr_target>{target}};
  const caors::error null_result = caors::deserialize_json(cleared, R"({"target":null})", context);
  REQUIRE(null_result.succeeded());
  CHECK(cleared.target.get() == nullptr);

  REQUIRE(manager.destroy(target->id()));

  object_ptr_actor missing{};
  const caors::error missing_result = caors::deserialize_json(missing, json.value, context);
  CHECK(missing_result.failed());
  CHECK(missing_result.value() == caors::errc::not_found);
}
