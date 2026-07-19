// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：serialization_write_builds_variant_from_reflected_object_test.cpp

#include "serialization_test_support.h"

#include "caors/io/memory_file_system.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - builds variant from reflected object", "[serialization]") {
  REQUIRE(registration_result_ok(
      caors::registration::type_<dom_actor>("caors.test.serialization.dom_actor")
          .meta()
          .property("id", &dom_actor::id)
          .serialization()
          .field("id", "actor_id", false, 7)
          .commit()));

  const dom_actor actor{42};
  const caors::serialization::variant_result result = caors::to_value(actor);

  REQUIRE(result.succeeded());
  CHECK(result->kind() == caors::meta::variant_kind::object);
  REQUIRE(result->as_object().size() == 1);
  CHECK(result->as_object()[0].name == "actor_id");
  CHECK(result->as_object()[0].field_id == 7);
  CHECK(result->as_object()[0].value.kind() == caors::meta::variant_kind::signed_integer);
  CHECK(result->as_object()[0].value.as_i64() == 42);
}

// 原测试文件：serialization_write_converts_runtime_values_to_variant_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - converts runtime values to variant", "[serialization]") {
  caors::any number = caors::any::from(42);
  caors::serialization::variant_result number_result = caors::serialization::to_variant(number);
  REQUIRE(number_result.succeeded());
  CHECK(number_result->is_i64());
  REQUIRE(number_result->try_as_i64() != nullptr);
  CHECK(*number_result->try_as_i64() == 42);

  caors::string name = "Ada";
  caors::any_ref name_ref = caors::any_ref::from(name);
  caors::serialization::variant_result name_result = caors::serialization::to_variant(name_ref);
  REQUIRE(name_result.succeeded());
  CHECK(name_result->is_string());
  REQUIRE(name_result->try_as_string() != nullptr);
  CHECK(*name_result->try_as_string() == "Ada");

  const std::optional<caors::guid> id = caors::guid::parse("2f6d1f64-7a3a-4db2-9a2d-123456789abc");
  REQUIRE(id.has_value());
  caors::serialization::variant_result guid_result =
      caors::serialization::to_variant(caors::any::from(*id));
  REQUIRE(guid_result.succeeded());
  CHECK(guid_result->is_string());
  REQUIRE(guid_result->try_as_string() != nullptr);
  CHECK(*guid_result->try_as_string() == "2f6d1f64-7a3a-4db2-9a2d-123456789abc");

  caors::serialization::variant_result invalid_result =
      caors::serialization::to_variant(caors::any_ref{});
  CHECK(invalid_result.failed());
  CHECK(invalid_result.error().value() == caors::errc::invalid_argument);
}

// 原测试文件：serialization_write_generic_writer_backend_serializes_fields_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - generic writer backend serializes fields", "[serialization]") {
  REQUIRE(registration_result_ok(
      caors::registration::type_<writer_actor>("caors.test.serialization.writer_actor")
          .meta()
          .property("id", &writer_actor::id)
          .serialization()
          .field("id", {}, false, 7)
          .commit()));

  numeric_field_writer writer;
  const writer_actor actor{42};
  const caors::error result = caors::serialize(actor, writer);

  REQUIRE(result.succeeded());
  CHECK(writer.object_open);
  CHECK(writer.object_closed);
  CHECK(writer.last_field_id == 7);
  CHECK(writer.last_i64 == 42);
}

// 原测试文件：serialization_write_json_options_pretty_prints_json_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

namespace {

struct json_options_actor {
  int id = 0;
};

} // namespace

TEST_CASE("serialization - json options can pretty print json", "[serialization]") {
  REQUIRE(registration_result_ok(
      caors::registration::type_<json_options_actor>("caors.test.serialization.json_options_actor")
          .meta()
          .property("id", &json_options_actor::id)
          .serialization()
          .commit()));

  caors::serialization::json_options options{};
  options.pretty = true;
  options.indent = 2;

  const json_options_actor actor{7};
  const caors::serialization::serialization_text_result result = caors::to_string(actor, options);

  REQUIRE(result.succeeded());
  CHECK(*result == "{\n  \"id\": 7\n}");
}

// 原测试文件：serialization_write_serializes_json_to_caller_provided_buffer_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - serializes json to caller provided buffer", "[serialization]") {
  require_serializable_actor_metadata();

  const serializable_actor actor{3, 10.5F, false, "Lin"};
  caors::string output = "previous";
  const caors::error status = caors::serialization::serialize_json_to(actor, output);

  REQUIRE(status.succeeded());
  CHECK(output == R"({"id":3,"health":10.5,"active":false,"name":"Lin"})");

  output = "previous";
  const caors::error text_status = caors::serialization::serialize_text_to(actor, output);
  REQUIRE(text_status.succeeded());
  CHECK(output == R"({"id":3,"health":10.5,"active":false,"name":"Lin"})");
}

TEST_CASE("serialization - serializes json to output stream", "[serialization]") {
  require_serializable_actor_metadata();

  caors::io::memory_file_system files;
  caors::io::output_stream_result opened = files.open_write("actors/lin.json");
  REQUIRE(opened.succeeded());
  REQUIRE(opened.value != nullptr);

  const serializable_actor actor{3, 10.5F, false, "Lin"};
  REQUIRE(caors::serialization::serialize_json_to(actor, *opened.value).succeeded());
  REQUIRE(opened.value->close().succeeded());

  caors::io::byte_buffer_result bytes = files.read_all("actors/lin.json");
  REQUIRE(bytes.succeeded());
  CHECK(std::string_view{reinterpret_cast<const char*>(bytes.value.data()), bytes.value.size()} ==
        R"({"id":3,"health":10.5,"active":false,"name":"Lin"})");
}

// 原测试文件：serialization_write_serializes_reflected_struct_fields_to_json_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - serializes reflected struct fields to json", "[serialization]") {
  require_serializable_actor_metadata();

  const serializable_actor actor{7, 95.5F, true, "Ada\nLovelace"};
  const caors::serialization::serialization_text_result result = caors::to_string(actor);

  REQUIRE(result.succeeded());
  CHECK(*result == R"({"id":7,"health":95.5,"active":true,"name":"Ada\nLovelace"})");
}

// 原测试文件：serialization_write_serializes_reflected_struct_fields_to_text_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - serializes reflected struct fields to text", "[serialization]") {
  require_serializable_actor_metadata();

  const serializable_actor actor{7, 95.5F, true, "Ada"};
  const caors::serialization::serialization_text_result result =
      caors::serialization::serialize_text(actor);

  REQUIRE(result.succeeded());
  CHECK(*result == R"({"id":7,"health":95.5,"active":true,"name":"Ada"})");
}

// 原测试文件：serialization_write_variant_exposes_checked_accessors_and_helpers_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - variant exposes checked accessors and helpers", "[serialization]") {
  caors::variant items = caors::variant::array({});
  CHECK(items.is_array());
  CHECK(items.try_as_array() != nullptr);
  CHECK(items.try_as_object() == nullptr);
  CHECK(items.push_back(caors::variant::from(std::int64_t{1})));
  CHECK(items.push_back(caors::variant::from(std::int64_t{2})));
  REQUIRE(items.try_as_array() != nullptr);
  REQUIRE(items.try_as_array()->size() == 2);
  CHECK(items.try_as_array()->at(0).as_i64() == 1);

  caors::variant text = caors::variant::from(std::string_view{"Ada"});
  CHECK(text.is_string());
  REQUIRE(text.try_as_string() != nullptr);
  CHECK(*text.try_as_string() == "Ada");
  CHECK(text.try_as_i64() == nullptr);
  CHECK_FALSE(text.push_back(caors::variant::null()));

  caors::variant object = caors::variant::object({});
  CHECK(object.add_member(caors::string_name{"items"}, 7, std::move(items)));
  CHECK(object.find_member(caors::string_name{"items"}) != nullptr);
  CHECK(object.find_member(7) != nullptr);
  CHECK(object.find_member(caors::string_name{"missing"}) == nullptr);
  CHECK(object.find_member(99) == nullptr);
  CHECK(object.find_member_variant(caors::string_name{"items"}) != nullptr);
  CHECK(object.find_member_variant(7) != nullptr);
  CHECK(object.find_member_variant(caors::string_name{"missing"}) == nullptr);
  CHECK(object.find_member_variant(99) == nullptr);
  REQUIRE(object.find_member(7) != nullptr);
  CHECK(object.find_member(7)->value.is_array());
  REQUIRE(object.find_member_variant(7) != nullptr);
  CHECK(object.find_member_variant(7)->is_array());

  caors::variant* mutable_items = object.find_member_variant(caors::string_name{"items"});
  REQUIRE(mutable_items != nullptr);
  CHECK(mutable_items->push_back(caors::variant::from(std::int64_t{3})));
  CHECK(object.find_member_variant(7)->as_array().size() == 3);
}

// 原测试文件：serialization_write_writes_intermediate_arrays_and_objects_through_text_writer_test.cpp

#include "serialization_test_support.h"

using namespace caors::tests::serialization;

TEST_CASE("serialization - writes intermediate arrays and objects through text writer",
          "[serialization]") {
  caors::variant::array_type items;
  items.push_back(caors::variant::from(std::int64_t{1}));
  items.push_back(caors::variant::from(std::int64_t{2}));

  caors::variant::object_type members;
  members.push_back(caors::meta::variant_member{
      caors::string_name{"items"},
      1,
      caors::variant::array(std::move(items)),
  });

  caors::serialization::text_writer writer;
  const caors::error result =
      caors::serialization::write_variant(writer, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  CHECK(writer.text() == R"({"items":[1,2]})");
}
