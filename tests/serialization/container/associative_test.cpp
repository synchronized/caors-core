// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：serialization_associative_deserializes_hash_map_fields_from_dictionaries_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - deserializes hash map fields from dictionaries", "[serialization]") {
  require_serializable_property_type<map_actor>("caors.test.serialization.map_actor", "labels",
                                                &map_actor::labels);

  caors::variant::dictionary_type entries;
  entries.push_back(caors::meta::variant_entry{
      caors::variant::from(std::int64_t{1}),
      caors::variant::from(std::string_view{"one"}),
  });
  entries.push_back(caors::meta::variant_entry{
      caors::variant::from(std::int64_t{2}),
      caors::variant::from(std::string_view{"two"}),
  });

  caors::variant::object_type members;
  members.push_back(caors::meta::variant_member{
      caors::string_name{"labels"},
      0,
      caors::variant::dictionary(std::move(entries)),
  });

  map_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  REQUIRE(actor.labels.size() == 2);
  CHECK(actor.labels.at(1) == "one");
  CHECK(actor.labels.at(2) == "two");
}

// 原测试文件：serialization_associative_deserializes_hash_map_fields_from_pair_arrays_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - deserializes hash map fields from pair arrays", "[serialization]") {
  require_serializable_property_type<map_actor>("caors.test.serialization.map_actor", "labels",
                                                &map_actor::labels);

  const caors::serialization::variant_result parsed =
      caors::serialization::parse_json(R"({"labels":[[3,"three"]]})");
  REQUIRE(parsed.succeeded());

  map_actor actor{};
  const caors::error result = caors::deserialize(actor, parsed.value);

  REQUIRE(result.succeeded());
  REQUIRE(actor.labels.size() == 1);
  CHECK(actor.labels.at(3) == "three");
}

// 原测试文件：serialization_associative_deserializes_hash_set_fields_from_arrays_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - deserializes hash set fields from arrays", "[serialization]") {
  require_serializable_property_type<set_actor>("caors.test.serialization.set_actor", "ids",
                                                &set_actor::ids);

  caors::variant::array_type ids;
  ids.push_back(caors::variant::from(std::int64_t{1}));
  ids.push_back(caors::variant::from(std::int64_t{2}));

  caors::variant::object_type members;
  members.push_back(caors::meta::variant_member{
      caors::string_name{"ids"},
      0,
      caors::variant::array(std::move(ids)),
  });

  set_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  REQUIRE(actor.ids.size() == 2);
  CHECK(actor.ids.contains(1));
  CHECK(actor.ids.contains(2));
}

// 原测试文件：serialization_associative_generic_associative_traits_work_without_type_registration_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - generic associative traits work without type registration",
          "[serialization]") {
  require_serializable_property_type<custom_map_actor>("caors.test.serialization.custom_map_actor",
                                                       "labels", &custom_map_actor::labels);

  custom_map_actor actor{};
  actor.labels.values.emplace(6, caors::string{"six"});

  const caors::serialization::serialization_text_result text =
      caors::serialization::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"labels":[[6,"six"]]})");

  caors::variant::dictionary_type entries;
  entries.push_back(caors::meta::variant_entry{
      caors::variant::from(std::int64_t{10}),
      caors::variant::from(std::string_view{"ten"}),
  });

  caors::variant::object_type members;
  members.push_back(caors::meta::variant_member{
      caors::string_name{"labels"},
      0,
      caors::variant::dictionary(std::move(entries)),
  });

  custom_map_actor decoded{};
  const caors::error result =
      caors::deserialize(decoded, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  REQUIRE(decoded.labels.values.size() == 1);
  CHECK(decoded.labels.values.at(10) == "ten");
}

// 原测试文件：serialization_associative_hash_set_reader_rejects_duplicate_elements_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - hash set reader rejects duplicate elements", "[serialization]") {
  require_serializable_property_type<set_actor>("caors.test.serialization.set_actor", "ids",
                                                &set_actor::ids);

  caors::variant::array_type ids;
  ids.push_back(caors::variant::from(std::int64_t{1}));
  ids.push_back(caors::variant::from(std::int64_t{1}));

  caors::variant::object_type members;
  members.push_back(caors::meta::variant_member{
      caors::string_name{"ids"},
      0,
      caors::variant::array(std::move(ids)),
  });

  set_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::variant::object(std::move(members)));

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::already_exists);
}

// 原测试文件：serialization_associative_roundtrips_std_map_fields_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - roundtrips std map fields", "[serialization]") {
  require_serializable_property_type<ordered_map_actor>(
      "caors.test.serialization.ordered_map_actor", "labels", &ordered_map_actor::labels);

  ordered_map_actor actor{};
  actor.labels.emplace(2, caors::string{"two"});
  actor.labels.emplace(1, caors::string{"one"});

  const caors::serialization::serialization_text_result text =
      caors::serialization::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"labels":[[1,"one"],[2,"two"]]})");

  ordered_map_actor decoded{};
  const caors::error result = caors::deserialize(decoded, R"({"labels":[[3,"three"],[4,"four"]]})");

  REQUIRE(result.succeeded());
  REQUIRE(decoded.labels.size() == 2);
  CHECK(decoded.labels.at(3) == "three");
  CHECK(decoded.labels.at(4) == "four");
}

// 原测试文件：serialization_associative_roundtrips_std_set_fields_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - roundtrips std set fields", "[serialization]") {
  require_serializable_property_type<ordered_set_actor>(
      "caors.test.serialization.ordered_set_actor", "ids", &ordered_set_actor::ids);

  ordered_set_actor actor{};
  actor.ids.emplace(3);
  actor.ids.emplace(1);

  const caors::serialization::serialization_text_result text =
      caors::serialization::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"ids":[1,3]})");

  ordered_set_actor decoded{};
  const caors::error result = caors::deserialize(decoded, R"({"ids":[2,4]})");

  REQUIRE(result.succeeded());
  REQUIRE(decoded.ids.size() == 2);
  CHECK(decoded.ids.contains(2));
  CHECK(decoded.ids.contains(4));
}

// 原测试文件：serialization_associative_serializes_hash_map_fields_as_dictionaries_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - serializes hash map fields as dictionaries", "[serialization]") {
  require_serializable_property_type<map_actor>("caors.test.serialization.map_actor", "labels",
                                                &map_actor::labels);

  map_actor actor{};
  actor.labels.emplace(7, caors::string{"seven"});

  const caors::serialization::serialization_text_result text =
      caors::serialization::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"labels":[[7,"seven"]]})");

  const caors::serialization::variant_result value = caors::to_value(actor);
  REQUIRE(value.succeeded());
  const caors::meta::variant_member* labels = value.value.find_member(caors::string_name{"labels"});
  REQUIRE(labels != nullptr);
  REQUIRE(labels->value.is_dictionary());
  REQUIRE(labels->value.as_dictionary().size() == 1);
  CHECK(labels->value.as_dictionary()[0].key.as_i64() == 7);
  CHECK(labels->value.as_dictionary()[0].value.as_string() == "seven");
}

// 原测试文件：serialization_associative_serializes_hash_set_fields_as_arrays_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - serializes hash set fields as arrays", "[serialization]") {
  require_serializable_property_type<set_actor>("caors.test.serialization.set_actor", "ids",
                                                &set_actor::ids);

  set_actor actor{};
  actor.ids.emplace(7);

  const caors::serialization::serialization_text_result text =
      caors::serialization::serialize_text(actor);
  REQUIRE(text.succeeded());
  CHECK(text.value == R"({"ids":[7]})");

  const caors::serialization::variant_result value = caors::to_value(actor);
  REQUIRE(value.succeeded());
  const caors::meta::variant_member* ids = value.value.find_member(caors::string_name{"ids"});
  REQUIRE(ids != nullptr);
  REQUIRE(ids->value.is_array());
  REQUIRE(ids->value.as_array().size() == 1);
  CHECK(ids->value.as_array()[0].as_i64() == 7);
}
