// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/math/serialization.h"
#include "caors/math/registration.h"
#include "caors/registration.h"
#include "caors/serialization.h"

#include <iostream>

namespace {

struct camera_config {
  caors::math::vec3 position{};
  caors::math::quat rotation{};
  caors::math::rect viewport{};
  caors::math::mat4 projection{};
};

[[nodiscard]] bool check(caors::error status, const char* step) {
  if (!status) {
    return true;
  }

  std::cerr << step << " failed: " << static_cast<int>(status.value()) << "\n";
  return false;
}

[[nodiscard]] bool register_types() {
  if (!check(caors::math::register_meta_types(), "register math types")) {
    return false;
  }

  return check(caors::registration::type_<camera_config>("examples.math.camera_config")
                   .meta()
                   .property("position", &camera_config::position)
                   .property("rotation", &camera_config::rotation)
                   .property("viewport", &camera_config::viewport)
                   .property("projection", &camera_config::projection)
                   .serialization()
                   .commit(),
               "register camera_config");
}

} // namespace

int main() {
  if (!register_types()) {
    return 1;
  }

  camera_config config{};
  config.position = {0.0F, 2.0F, 5.0F};
  config.rotation = caors::math::quat::identity();
  config.viewport = {{0.0F, 0.0F}, {1280.0F, 720.0F}};
  config.projection =
      caors::math::perspective_rh_zo(caors::math::radians(60.0F), 16.0F / 9.0F, 0.1F, 100.0F);

  const caors::serialization::serialization_text_result text =
      caors::serialization::to_text(config);
  if (!check(text.status, "serialize camera_config")) {
    return 1;
  }

  std::cout << text.value << "\n";

  camera_config decoded{};
  if (!check(caors::read_json_from(decoded, text.value), "deserialize camera_config")) {
    return 1;
  }

  std::cout << "decoded_position=(" << decoded.position.x << ", " << decoded.position.y << ", "
            << decoded.position.z << ")\n";
  return 0;
}
