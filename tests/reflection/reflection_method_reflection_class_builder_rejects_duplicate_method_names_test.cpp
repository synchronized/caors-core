// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "reflection_method_test_support.h"

using namespace test_reflection;
using namespace caors::tests::reflection_method;
TEST_CASE("reflection - class builder rejects duplicate method names", "[reflection]") {
  CHECK(caors::reflection::class_<duplicate_method_actor>(
            "caors.test.reflection.duplicate_method_actor")
            .method("update", &duplicate_method_actor::first)
            .method("update", &duplicate_method_actor::second)
            .commit() == reflection_error(caors::errc::duplicate_method));
}
