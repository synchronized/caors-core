// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/math.h"

#include <iostream>

namespace {

void print_clip(const char* label, caors::math::vec4 value) {
  std::cout << label << "=(" << value.x << ", " << value.y << ", " << value.z << ", " << value.w
            << ")\n";
}

} // namespace

int main() {
  const caors::math::vec3 eye{0.0F, 2.0F, 5.0F};
  const caors::math::vec3 target{0.0F, 0.0F, 0.0F};
  const caors::math::vec3 up{0.0F, 1.0F, 0.0F};

  const caors::math::mat4 view = caors::math::look_at_rh(eye, target, up);
  const caors::math::mat4 perspective =
      caors::math::perspective_rh_zo(caors::math::radians(60.0F), 16.0F / 9.0F, 0.1F, 100.0F);
  const caors::math::mat4 orthographic =
      caors::math::orthographic_rh_zo(-8.0F, 8.0F, -4.5F, 4.5F, 0.1F, 100.0F);

  const caors::math::vec4 world_origin{0.0F, 0.0F, 0.0F, 1.0F};
  print_clip("perspective_clip", perspective * (view * world_origin));
  print_clip("orthographic_clip", orthographic * (view * world_origin));
  return 0;
}
