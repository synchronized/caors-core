// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "plugin_identity_test_support.h"

#include "caors/meta/static_capability.h"

namespace caors::meta::capabilities {

template <>
struct static_capability<::caors::tests::plugin_identity::conflicting_static_capability,
                         ::caors::tests::plugin_identity::shared_plugin_type> {
  [[nodiscard]] static const ::caors::tests::plugin_identity::conflicting_static_capability* get() {
    static const ::caors::tests::plugin_identity::conflicting_static_capability capability{42};
    return &capability;
  }
};

template <>
struct module_static_capability_list<custom_static_capability_module,
                                     ::caors::tests::plugin_identity::shared_plugin_type> {
  using type = type_list<::caors::tests::plugin_identity::conflicting_static_capability>;
};

} // namespace caors::meta::capabilities

#include "caors/meta/type_id.h"
#include "caors/registration/type_builder.h"

CAORS_TEST_PLUGIN_EXPORT std::uint64_t caors_test_plugin_b_type_id() noexcept {
  return caors::meta::detail::type_id_of_cpp_type<
             caors::tests::plugin_identity::shared_plugin_type>()
      .value();
}

CAORS_TEST_PLUGIN_EXPORT int caors_test_plugin_b_register_type() noexcept {
  const caors::error status =
      caors::registration::type_<caors::tests::plugin_identity::shared_plugin_type>(
          "caors.test.plugin_identity.shared")
          .commit();
  return static_cast<int>(status.value());
}
