// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "meta_value_test_common.h"

#include "caors/meta/object.h"

using namespace caors::tests::meta_value;

namespace {

class runtime_value_object : public caors::meta::object_base<runtime_value_object> {
public:
  static constexpr std::string_view object_type_name = "caors.test.meta.runtime_value_object";
};

struct runtime_value_actor {
  int hp = 10;

  int add(int value) {
    hp += value;
    return hp;
  }

  int& ref() noexcept { return hp; }
};

} // namespace

TEST_CASE("meta value - stores scalar and text fast path values", "[meta]") {
  caors::meta::value empty;
  CHECK(empty.is_null());

  caors::meta::value boolean = true;
  CHECK(boolean.kind() == caors::meta::value_kind::boolean);
  CHECK(boolean.as_bool());

  caors::meta::value signed_integer = -42;
  CHECK(signed_integer.is_i64());
  CHECK(signed_integer.as_i64() == -42);

  caors::meta::value unsigned_integer = 42U;
  CHECK(unsigned_integer.is_u64());
  CHECK(unsigned_integer.as_u64() == 42U);

  caors::meta::value floating = 3.5F;
  CHECK(floating.is_f64());
  CHECK(floating.as_f64() == 3.5);

  const caors::string_name name{"caors.test.meta.value.name"};
  caors::meta::value named = name;
  CHECK(named.is_string_name());
  REQUIRE(named.try_as_string_view().has_value());
  CHECK(*named.try_as_string_view() == name.view());

  caors::meta::value text = "runtime text";
  CHECK(text.is_string());
  REQUIRE(text.try_as_string_view().has_value());
  CHECK(*text.try_as_string_view() == "runtime text");
}

TEST_CASE("meta value - stores object pointer separately from C++ any", "[meta]") {
  runtime_value_object object;

  caors::meta::value stored = caors::meta::value::from_object(&object);
  CHECK(stored.kind() == caors::meta::value_kind::object);
  CHECK(stored.as_object() == &object);

  const caors::meta::any_ref ref = stored.to_ref();
  REQUIRE(ref);
  CHECK(ref.try_get<caors::meta::object>() != nullptr);
}

TEST_CASE("meta value - can wrap any_ref and any fallback values", "[meta]") {
  require_value_test_meta();

  int number = 7;
  caors::meta::value ref_value = caors::meta::value::from_ref(number);
  CHECK(ref_value.kind() == caors::meta::value_kind::reference);
  REQUIRE(ref_value.as_ref().try_get<int>() != nullptr);
  CHECK(*ref_value.as_ref().try_get<int>() == 7);
  CHECK(ref_value.as_ref().try_set(9));
  CHECK(number == 9);

  caors::meta::value any_value = caors::meta::value::from_any(caors::any::from(number));
  CHECK(any_value.kind() == caors::meta::value_kind::any);
  caors::any copied = any_value.to_any();
  REQUIRE(copied.try_get<int>() != nullptr);
  CHECK(*copied.try_get<int>() == 9);
}

TEST_CASE("meta value - integrates with property and method runtime APIs", "[meta]") {
  REQUIRE(type_result_ok(
      caors::registration::type_<runtime_value_actor>("caors.test.meta.runtime_value_actor")
          .meta()
          .property("hp", &runtime_value_actor::hp)
          .method("add", &runtime_value_actor::add)
          .method("ref", &runtime_value_actor::ref)
          .commit()));

  const caors::meta::type reflected = caors::meta::find("caors.test.meta.runtime_value_actor");
  REQUIRE(reflected);

  runtime_value_actor actor{};
  caors::meta::instance instance = caors::meta::view(actor);

  const caors::meta::property hp = reflected.find_property("hp");
  REQUIRE(hp);

  caors::meta::value hp_value = hp.get_value(instance);
  REQUIRE(hp_value.is_any());
  caors::any hp_any = hp_value.to_any();
  REQUIRE(hp_any.try_get<int>() != nullptr);
  CHECK(*hp_any.try_get<int>() == 10);

  CHECK(hp.try_set_value(instance, caors::meta::value::from_any(12)));
  CHECK(actor.hp == 12);
  CHECK(instance.set("hp", caors::meta::value::from_any(14)));
  CHECK(actor.hp == 14);

  std::array<caors::meta::value, 1> arguments{caors::meta::value::from_any(6)};
  caors::meta::invoke_value_result add_result =
      reflected.find_method("add").invoke(instance, arguments);
  REQUIRE(add_result);
  REQUIRE(add_result.has_value());
  caors::any add_any = add_result.value().to_any();
  REQUIRE(add_any.try_get<int>() != nullptr);
  CHECK(*add_any.try_get<int>() == 20);

  caors::meta::invoke_value_result ref_result = instance.call("ref");
  REQUIRE(ref_result);
  REQUIRE(ref_result.has_value());
  caors::meta::any_ref ref = ref_result.value().as_ref();
  REQUIRE(ref);
  int* mutable_hp = ref.try_get_mutable<int>();
  REQUIRE(mutable_hp != nullptr);
  *mutable_hp = 31;
  CHECK(actor.hp == 31);
}
