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

struct object_weak_ref_actor {
  caors::object_weak_ref<weak_object_ref_target> observer{};
};

void require_weak_object_ref_target_metadata() {
  REQUIRE(registration_result_ok(caors::registration::type_<weak_object_ref_target>()
                                     .meta()
                                     .property("value", &weak_object_ref_target::value)
                                     .serialization()
                                     .object()
                                     .commit()));
}

void require_object_weak_ref_actor_metadata() {
  require_weak_object_ref_target_metadata();

  require_serializable_property_type<object_weak_ref_actor>(
      "caors.test.serialization.object_weak_ref_actor", "observer",
      &object_weak_ref_actor::observer);
}

} // namespace

TEST_CASE("serialization - rejects object weak refs", "[serialization]") {
  require_object_weak_ref_actor_metadata();

  const caors::objects::ref<weak_object_ref_target> owner =
      caors::objects::make_ref<weak_object_ref_target>();
  object_weak_ref_actor actor{};
  actor.observer = owner;

  const caors::serialization::serialization_text_result json = caors::to_string(actor);

  CHECK(json.failed());
  CHECK(json.status.domain() == caors::error_domain::serialization);
  CHECK(json.status.value() == caors::errc::unsupported);
}
