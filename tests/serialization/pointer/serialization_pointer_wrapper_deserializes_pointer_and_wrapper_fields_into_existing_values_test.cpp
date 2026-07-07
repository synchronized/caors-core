// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "serialization_test_support.h"

#include <memory>

using namespace caors::tests::serialization;

TEST_CASE("serialization - deserializes pointer and wrapper fields into existing values",
          "[serialization]") {
  require_pointer_actor_metadata();

  caors::variant::object_type stats_members;
  stats_members.push_back(caors::value::variant_member{
      caors::string_name{"level"},
      0,
      caors::variant::from(std::int64_t{4}),
  });
  stats_members.push_back(caors::value::variant_member{
      caors::string_name{"score"},
      0,
      caors::variant::from(5.5),
  });

  caors::variant::object_type members;
  members.push_back(caors::value::variant_member{
      caors::string_name{"raw"},
      0,
      caors::variant::from(std::int64_t{70}),
  });
  members.push_back(caors::value::variant_member{
      caors::string_name{"shared"},
      0,
      caors::variant::from(std::int64_t{80}),
  });
  members.push_back(caors::value::variant_member{
      caors::string_name{"unique"},
      0,
      caors::variant::from(std::int64_t{90}),
  });
  members.push_back(caors::value::variant_member{
      caors::string_name{"optional"},
      0,
      caors::variant::from(std::int64_t{100}),
  });
  members.push_back(caors::value::variant_member{
      caors::string_name{"stats"},
      0,
      caors::variant::object(std::move(stats_members)),
  });

  int raw = 0;
  nested_stats stats{};
  pointer_actor actor{};
  actor.raw = &raw;
  actor.shared = std::make_shared<int>(0);
  actor.unique = std::make_unique<int>(0);
  actor.optional = 0;
  actor.stats = &stats;

  const int* raw_address = actor.raw;
  const int* shared_address = actor.shared.get();
  const int* unique_address = actor.unique.get();
  nested_stats* stats_address = actor.stats;

  const caors::error result = caors::deserialize(actor, caors::variant::object(std::move(members)));

  REQUIRE(result.succeeded());
  CHECK(actor.raw == raw_address);
  CHECK(actor.shared.get() == shared_address);
  CHECK(actor.unique.get() == unique_address);
  CHECK(actor.stats == stats_address);
  CHECK(raw == 70);
  CHECK(*actor.shared == 80);
  CHECK(*actor.unique == 90);
  REQUIRE(actor.optional.has_value());
  CHECK(*actor.optional == 100);
  CHECK(stats.level == 4);
  CHECK(stats.score == Catch::Approx(5.5));
}
