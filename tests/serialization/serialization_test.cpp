// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization.h"

#include <catch2/catch_all.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace {

struct serializable_actor {
  int id = 0;
  float health = 0.0F;
  bool active = false;
  std::string name{};
};

struct configured_actor {
  int id = 0;
  float health = 0.0F;
  bool debug = false;
};

struct writer_actor {
  int id = 0;
};

struct dom_actor {
  int id = 0;
};

struct numeric_writer_actor {
  int id = 0;
};

struct reader_actor {
  int id = 0;
  float health = 0.0F;
  bool active = false;
  std::string name{};
};

struct reader_field_id_actor {
  int id = 0;
};

struct reader_missing_field_actor {
  int id = 0;
};

struct serialization_first_actor {
  int id = 0;
};

struct reflected_only_actor {
  int id = 0;
};

struct missing_field_actor {
  int id = 0;
};

struct unsupported_field_value {
  int id = 0;
};

struct unsupported_field_actor {
  unsupported_field_value value{};
};

[[nodiscard]] bool reflection_result_ok(caors::reflection_register_result result) noexcept {
  return result == caors::reflection_register_result::success ||
         result == caors::reflection_register_result::duplicate_reflection;
}

[[nodiscard]] bool serialization_result_ok(caors::error result) noexcept {
  return result.succeeded() || result.value() == caors::errc::duplicate_component;
}

class numeric_field_writer final : public caors::serialization_writer {
public:
  [[nodiscard]] caors::serialization_format_features features() const noexcept override {
    return caors::serialization_format_features::numeric_fields;
  }

  [[nodiscard]] caors::error begin_object(const caors::serialization_type_data&) override {
    object_open = true;
    return {};
  }

  [[nodiscard]] caors::error begin_field(const caors::serialization_field_data& field) override {
    if (field.field_id == 0) {
      return {caors::error_domain::serialization, caors::errc::invalid_field};
    }

    last_field_id = field.field_id;
    return {};
  }

  [[nodiscard]] caors::error begin_array(std::size_t) override { return {}; }
  [[nodiscard]] caors::error begin_element(std::size_t) override { return {}; }
  [[nodiscard]] caors::error write_bool(bool) override { return {}; }
  [[nodiscard]] caors::error write_i64(std::int64_t value) override {
    last_i64 = value;
    return {};
  }
  [[nodiscard]] caors::error write_u64(std::uint64_t) override { return {}; }
  [[nodiscard]] caors::error write_f64(double) override { return {}; }
  [[nodiscard]] caors::error write_string(std::string_view) override { return {}; }
  [[nodiscard]] caors::error end_element() override { return {}; }
  [[nodiscard]] caors::error end_array() override { return {}; }
  [[nodiscard]] caors::error end_field() override { return {}; }
  [[nodiscard]] caors::error end_object() override {
    object_closed = true;
    return {};
  }

  bool object_open = false;
  bool object_closed = false;
  std::uint32_t last_field_id = 0;
  std::int64_t last_i64 = 0;
};

} // namespace

namespace caors {

template <> struct type_name<unsupported_field_value> {
  static constexpr std::string_view value = "caors.test.serialization.unsupported_value";
};

} // namespace caors

TEST_CASE("serialization - registers type metadata component", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<serializable_actor>("caors.test.serialization.actor")
          .field("id", &serializable_actor::id)
          .field("health", &serializable_actor::health)
          .field("active", &serializable_actor::active)
          .field("name", &serializable_actor::name)
          .commit()));

  const caors::error status = caors::register_serialization<serializable_actor>();

  CHECK(serialization_result_ok(status));
  CHECK(caors::get_serialization_registry().contains(caors::find_type<serializable_actor>()->id()));
  CHECK(caors::get_serialization_registry().size() >= 1);

  const caors::error duplicate = caors::register_serialization<serializable_actor>();
  CHECK(duplicate.failed());
  CHECK(duplicate.domain() == caors::error_domain::serialization);
  CHECK(duplicate.value() == caors::errc::duplicate_component);
}

TEST_CASE("serialization - serializes reflected struct fields to text", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<serializable_actor>("caors.test.serialization.actor")
          .field("id", &serializable_actor::id)
          .field("health", &serializable_actor::health)
          .field("active", &serializable_actor::active)
          .field("name", &serializable_actor::name)
          .commit()));

  const caors::error register_result = caors::register_serialization<serializable_actor>();
  REQUIRE(serialization_result_ok(register_result));

  const serializable_actor actor{7, 95.5F, true, "Ada"};
  const caors::serialization_text_result result = caors::serialize_text(actor);

  REQUIRE(result.succeeded());
  CHECK(result.text == R"({"id":7,"health":95.5,"active":true,"name":"Ada"})");
}

TEST_CASE("serialization - field metadata renames and skips fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<configured_actor>("caors.test.serialization.configured_actor")
          .field("id", &configured_actor::id)
          .field("health", &configured_actor::health)
          .field("debug", &configured_actor::debug)
          .commit()));

  REQUIRE(caors::register_serialization<configured_actor>(
              {
                  caors::make_serialization_field("health", "hp"),
                  caors::make_serialization_field("debug", {}, true),
                  caors::make_serialization_field("id"),
              })
              .succeeded());

  const configured_actor actor{7, 42.5F, true};
  const caors::serialization_text_result result = caors::serialize_text(actor);

  REQUIRE(result.succeeded());
  CHECK(result.text == R"({"hp":42.5,"id":7})");
}

TEST_CASE("serialization - generic writer backend serializes fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<writer_actor>("caors.test.serialization.writer_actor")
          .field("id", &writer_actor::id)
          .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<writer_actor>({
      caors::make_serialization_field("id", {}, false, 7),
  })));

  numeric_field_writer writer;
  const writer_actor actor{42};
  const caors::error result = caors::serialize(actor, writer);

  REQUIRE(result.succeeded());
  CHECK(writer.object_open);
  CHECK(writer.object_closed);
  CHECK(writer.last_field_id == 7);
  CHECK(writer.last_i64 == 42);
}

TEST_CASE("serialization - builds intermediate value from reflected object", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<dom_actor>("caors.test.serialization.dom_actor")
          .field("id", &dom_actor::id)
          .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<dom_actor>({
      caors::make_serialization_field("id", "actor_id", false, 7),
  })));

  const dom_actor actor{42};
  const caors::serialization_value_result result = caors::to_serialization_value(actor);

  REQUIRE(result.succeeded());
  CHECK(result.value.kind() == caors::serialization_value_kind::object);
  REQUIRE(result.value.as_object().size() == 1);
  CHECK(result.value.as_object()[0].name == "actor_id");
  CHECK(result.value.as_object()[0].field_id == 7);
  CHECK(result.value.as_object()[0].value.kind() ==
        caors::serialization_value_kind::signed_integer);
  CHECK(result.value.as_object()[0].value.as_i64() == 42);
}

TEST_CASE("serialization - writes intermediate arrays and objects through text writer",
          "[serialization]") {
  caors::serialization_value::array_type items;
  items.push_back(caors::serialization_value::from(std::int64_t{1}));
  items.push_back(caors::serialization_value::from(std::int64_t{2}));

  caors::serialization_value::object_type members;
  members.push_back(caors::serialization_member{
      caors::string_name{"items"},
      1,
      caors::serialization_value::array(std::move(items)),
  });

  caors::text_serialization_writer writer;
  const caors::error result =
      caors::write(writer, caors::serialization_value::object(std::move(members)));

  REQUIRE(result.succeeded());
  CHECK(writer.text() == R"({"items":[1,2]})");
}

TEST_CASE("serialization - value reader deserializes reflected struct fields", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<reader_actor>("caors.test.serialization.reader_actor")
          .field("id", &reader_actor::id)
          .field("health", &reader_actor::health)
          .field("active", &reader_actor::active)
          .field("name", &reader_actor::name)
          .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<reader_actor>()));

  caors::serialization_value::object_type members;
  members.push_back(caors::serialization_member{
      caors::string_name{"id"},
      0,
      caors::serialization_value::from(std::int64_t{7}),
  });
  members.push_back(caors::serialization_member{
      caors::string_name{"health"},
      0,
      caors::serialization_value::from(95.5),
  });
  members.push_back(caors::serialization_member{
      caors::string_name{"active"},
      0,
      caors::serialization_value::from(true),
  });
  members.push_back(caors::serialization_member{
      caors::string_name{"name"},
      0,
      caors::serialization_value::from(std::string_view{"Ada"}),
  });

  reader_actor actor{};
  const caors::error result =
      caors::deserialize(actor, caors::serialization_value::object(std::move(members)));

  REQUIRE(result.succeeded());
  CHECK(actor.id == 7);
  CHECK(actor.health == Catch::Approx(95.5));
  CHECK(actor.active);
  CHECK(actor.name == "Ada");
}

TEST_CASE("serialization - value reader matches fields by stable field id", "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<reader_field_id_actor>(
                                   "caors.test.serialization.reader_field_id_actor")
                                   .field("id", &reader_field_id_actor::id)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<reader_field_id_actor>({
      caors::make_serialization_field("id", "actor_id", false, 7),
  })));

  caors::serialization_value::object_type members;
  members.push_back(caors::serialization_member{
      caors::string_name{"legacy_id"},
      7,
      caors::serialization_value::from(std::int64_t{42}),
  });

  reader_field_id_actor actor{};
  const caors::error result =
      caors::deserialize(actor, caors::serialization_value::object(std::move(members)));

  REQUIRE(result.succeeded());
  CHECK(actor.id == 42);
}

TEST_CASE("serialization - value reader reports missing fields", "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<reader_missing_field_actor>(
                                   "caors.test.serialization.reader_missing_field_actor")
                                   .field("id", &reader_missing_field_actor::id)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<reader_missing_field_actor>()));

  reader_missing_field_actor actor{};
  const caors::error result = caors::deserialize(actor, caors::serialization_value::object({}));

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::not_found);
}

TEST_CASE("serialization - writer features validate required field identifiers",
          "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<numeric_writer_actor>(
                                   "caors.test.serialization.numeric_writer_actor")
                                   .field("id", &numeric_writer_actor::id)
                                   .commit()));
  REQUIRE(
      caors::register_serialization<numeric_writer_actor>({
                                                              caors::make_serialization_field("id"),
                                                          })
          .succeeded());

  numeric_field_writer writer;
  const numeric_writer_actor actor{5};
  const caors::error result = caors::serialize(actor, writer);

  CHECK(result.failed());
  CHECK(result.domain() == caors::error_domain::serialization);
  CHECK(result.value() == caors::errc::invalid_field);
}

TEST_CASE("serialization - registration can happen before reflection fields", "[serialization]") {
  REQUIRE(caors::register_serialization<serialization_first_actor>(
              {
                  caors::make_serialization_field("id", "actor_id"),
              })
              .succeeded());

  REQUIRE(reflection_result_ok(caors::reflection::class_<serialization_first_actor>(
                                   "caors.test.serialization.serialization_first_actor")
                                   .field("id", &serialization_first_actor::id)
                                   .commit()));

  const serialization_first_actor actor{9};
  const caors::serialization_text_result result = caors::serialize_text(actor);

  REQUIRE(result.succeeded());
  CHECK(result.text == R"({"actor_id":9})");
}

TEST_CASE("serialization - reports missing serialization metadata", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<reflected_only_actor>("caors.test.serialization.reflected_only")
          .field("id", &reflected_only_actor::id)
          .commit()));

  const reflected_only_actor actor{3};
  const caors::serialization_text_result result = caors::serialize_text(actor);

  CHECK(result.failed());
  CHECK(result.status.domain() == caors::error_domain::serialization);
  CHECK(result.status.value() == caors::errc::missing_type);
  CHECK(result.text.empty());
}

TEST_CASE("serialization - reports missing configured reflection field", "[serialization]") {
  REQUIRE(reflection_result_ok(
      caors::reflection::class_<missing_field_actor>("caors.test.serialization.missing_field")
          .field("id", &missing_field_actor::id)
          .commit()));
  REQUIRE(caors::register_serialization<missing_field_actor>(
              {
                  caors::make_serialization_field("missing"),
              })
              .succeeded());

  const missing_field_actor actor{3};
  const caors::serialization_text_result result = caors::serialize_text(actor);

  CHECK(result.failed());
  CHECK(result.status.domain() == caors::error_domain::serialization);
  CHECK(result.status.value() == caors::errc::not_found);
}

TEST_CASE("serialization - reports unsupported reflected field kind", "[serialization]") {
  REQUIRE(reflection_result_ok(caors::reflection::class_<unsupported_field_actor>(
                                   "caors.test.serialization.unsupported_field")
                                   .field("value", &unsupported_field_actor::value)
                                   .commit()));
  REQUIRE(serialization_result_ok(caors::register_serialization<unsupported_field_actor>()));

  const unsupported_field_actor actor{{1}};
  const caors::serialization_text_result result = caors::serialize_text(actor);

  CHECK(result.failed());
  CHECK(result.status.domain() == caors::error_domain::serialization);
  CHECK(result.status.value() == caors::errc::unsupported);
}
