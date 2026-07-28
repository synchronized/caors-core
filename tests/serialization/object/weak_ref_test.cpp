// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

// 原测试文件：serialization_object_weak_ref_test.cpp

#include "serialization_test_support.h"

#include "caors/object.h"

#include <string_view>

using namespace caors::tests::serialization;

namespace {

class weak_object_ref_target : public caors::object_base<weak_object_ref_target>,
                               public caors::objects::ref_counted {
public:
  static constexpr std::string_view object_type_name =
      "caors.test.serialization.weak_object_ref_target";

  int value = 0;
};

class other_weak_object_ref_target : public caors::object_base<other_weak_object_ref_target>,
                                     public caors::objects::ref_counted {
public:
  static constexpr std::string_view object_type_name =
      "caors.test.serialization.other_weak_object_ref_target";
};

struct object_weak_ref_actor {
  caors::object_weak_ref<weak_object_ref_target> observer{};
};

void require_weak_object_ref_target_metadata() {
  REQUIRE(registration_result_ok(caors::registration::type_<weak_object_ref_target>()
                                     .meta()
                                     .property("value", &weak_object_ref_target::value)
                                     .serialization()
                                     .commit()));
}

void require_object_weak_ref_actor_metadata() {
  require_weak_object_ref_target_metadata();

  require_serializable_property_type<object_weak_ref_actor>(
      "caors.test.serialization.object_weak_ref_actor", "observer",
      &object_weak_ref_actor::observer);
}

} // namespace

TEST_CASE("serialization - object weak refs use external reference identity", "[serialization]") {
  require_object_weak_ref_actor_metadata();

  const caors::objects::ref<weak_object_ref_target> owner =
      caors::objects::make_ref<weak_object_ref_target>();
  object_weak_ref_actor actor{};
  actor.observer = owner;

  object_ptr_resolver_data resolver_data{caors::object_id{72}, owner.get()};
  const caors::objects::object_ref_resolver resolver = make_test_object_ref_resolver(resolver_data);
  caors::serialization::context context;
  context.resolvers.external_refs.resolver = &resolver;

  const caors::serialization::serialization_text_result json = caors::to_json(context, actor);
  REQUIRE(json.succeeded());
  CHECK(json.value == R"({"observer":72})");

  object_weak_ref_actor decoded{};
  REQUIRE(caors::read_json_from(context, decoded, R"({"observer":72})").succeeded());
  const caors::objects::ref<weak_object_ref_target> decoded_owner = decoded.observer.lock();
  REQUIRE(decoded_owner);
  CHECK(decoded_owner.get() == owner.get());
}

TEST_CASE("serialization - object weak refs require an external reference resolver",
          "[serialization]") {
  require_object_weak_ref_actor_metadata();

  const caors::objects::ref<weak_object_ref_target> owner =
      caors::objects::make_ref<weak_object_ref_target>();
  const object_weak_ref_actor actor{owner};
  const caors::serialization::serialization_text_result json = caors::to_json(actor);

  CHECK(json.failed());
  CHECK(json.status.domain() == caors::error_domain::serialization);
  CHECK(json.status.value() == caors::errc::invalid_argument);

  object_weak_ref_actor decoded{};
  const caors::error read = caors::read_json_from(decoded, R"({"observer":72})");
  CHECK(read == caors::error{caors::error_domain::serialization, caors::errc::unsupported});
}

TEST_CASE("serialization - expired object weak refs serialize as null", "[serialization]") {
  require_object_weak_ref_actor_metadata();

  object_weak_ref_actor actor{};
  object_ptr_resolver_data resolver_data{caors::object_id{72}, nullptr};
  const caors::objects::object_ref_resolver resolver = make_test_object_ref_resolver(resolver_data);
  caors::serialization::context context;
  context.resolvers.external_refs.resolver = &resolver;

  const caors::serialization::serialization_text_result json = caors::to_json(context, actor);
  REQUIRE(json.succeeded());
  CHECK(json.value == R"({"observer":0})");

  object_weak_ref_actor decoded{};
  REQUIRE(caors::read_json_from(context, decoded, R"({"observer":0})").succeeded());
  CHECK(decoded.observer.expired());
}

TEST_CASE("serialization - object weak ref resolver failures preserve the existing reference",
          "[serialization]") {
  require_object_weak_ref_actor_metadata();

  const caors::objects::ref<weak_object_ref_target> owner =
      caors::objects::make_ref<weak_object_ref_target>();
  object_weak_ref_actor actor{owner};

  weak_object_ref_target unmanaged;
  object_ptr_resolver_data unmanaged_data{caors::object_id{73}, &unmanaged};
  const caors::objects::object_ref_resolver unmanaged_resolver =
      make_test_object_ref_resolver(unmanaged_data);
  caors::serialization::context unmanaged_context;
  unmanaged_context.resolvers.external_refs.resolver = &unmanaged_resolver;

  const caors::error unmanaged_result =
      caors::read_json_from(unmanaged_context, actor, R"({"observer":73})");
  CHECK(unmanaged_result ==
        caors::error{caors::error_domain::serialization, caors::errc::invalid_type});
  REQUIRE(actor.observer.lock());
  CHECK(actor.observer.lock().get() == owner.get());

  const caors::objects::ref<other_weak_object_ref_target> other =
      caors::objects::make_ref<other_weak_object_ref_target>();
  object_ptr_resolver_data wrong_type_data{caors::object_id{74}, other.get()};
  const caors::objects::object_ref_resolver wrong_type_resolver =
      make_test_object_ref_resolver(wrong_type_data);
  caors::serialization::context wrong_type_context;
  wrong_type_context.resolvers.external_refs.resolver = &wrong_type_resolver;

  const caors::error wrong_type_result =
      caors::read_json_from(wrong_type_context, actor, R"({"observer":74})");
  CHECK(wrong_type_result ==
        caors::error{caors::error_domain::serialization, caors::errc::invalid_type});
  REQUIRE(actor.observer.lock());
  CHECK(actor.observer.lock().get() == owner.get());

  object_ptr_resolver_data missing_data{caors::object_id{75}, nullptr};
  const caors::objects::object_ref_resolver missing_resolver =
      make_test_object_ref_resolver(missing_data);
  caors::serialization::context missing_context;
  missing_context.resolvers.external_refs.resolver = &missing_resolver;

  const caors::error missing_result =
      caors::read_json_from(missing_context, actor, R"({"observer":75})");
  CHECK(missing_result == caors::error{caors::error_domain::serialization, caors::errc::not_found});
  REQUIRE(actor.observer.lock());
  CHECK(actor.observer.lock().get() == owner.get());
}
