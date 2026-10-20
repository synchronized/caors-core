// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "plugin_identity_test_support.h"
#include "shared_capability_adapter.h"

#include "caors/core/string_name.h"
#include "caors/meta/type_id.h"
#include "caors/registration/type_builder.h"

CAORS_TEST_PLUGIN_EXPORT std::uint64_t caors_test_plugin_a_type_id() noexcept {
  return caors::meta::detail::type_id_of_cpp_type<
             caors::tests::plugin_identity::shared_plugin_type>()
      .value();
}

CAORS_TEST_PLUGIN_EXPORT int caors_test_plugin_a_register_type() noexcept {
  const caors::error status =
      caors::registration::type_<caors::tests::plugin_identity::shared_plugin_type>(
          "caors.test.plugin_identity.shared")
          .commit();
  return static_cast<int>(status.value());
}

CAORS_TEST_PLUGIN_EXPORT int caors_test_plugin_a_register_consistent_type() noexcept {
  const caors::error status =
      caors::registration::type_<caors::tests::plugin_identity::consistent_plugin_type>(
          "caors.test.plugin_identity.consistent")
          .commit();
  return static_cast<int>(status.value());
}

CAORS_TEST_PLUGIN_EXPORT std::uint64_t caors_test_plugin_a_capability_key() noexcept {
  return caors::meta::capability_key_of<caors::tests::plugin_identity::shared_static_capability>()
      .value();
}

CAORS_TEST_PLUGIN_EXPORT void caors_test_plugin_a_name(caors::string_name* result) {
  *result = caors::string_name{"caors.identity.shared.name"};
}

CAORS_TEST_PLUGIN_EXPORT void* caors_test_plugin_a_create() {
  return new caors::tests::plugin_identity::shared_plugin_type{42};
}
CAORS_TEST_PLUGIN_EXPORT int caors_test_plugin_a_destroy(void* object) noexcept {
  auto* value = static_cast<caors::tests::plugin_identity::shared_plugin_type*>(object);
  const int result = value->value;
  delete value;
  return result;
}
