// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "plugin_identity_test_support.h"

#include "caors/meta/type_id.h"

CAORS_TEST_PLUGIN_EXPORT std::uint64_t caors_test_plugin_b_type_id() noexcept {
  return caors::meta::detail::type_id_of_cpp_type<
             caors::tests::plugin_identity::shared_plugin_type>()
      .value();
}
