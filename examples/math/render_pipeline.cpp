// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/math.h"

#include <array>
#include <iostream>

namespace {

struct render_item {
  const char* name = "";
  caors::math::aabb bounds{};
};

void print_vec3(const char* label, caors::math::vec3 value) {
  std::cout << label << ": (" << value.x << ", " << value.y << ", " << value.z << ")\n";
}

} // namespace

int main() {
  using caors::math::clip_depth;
  using caors::math::handedness;

  const caors::math::vec3 eye{0.0F, 2.0F, 6.0F};
  const caors::math::vec3 target{0.0F, 0.0F, 0.0F};
  const caors::math::vec3 up{0.0F, 1.0F, 0.0F};
  constexpr float aspect = 16.0F / 9.0F;

  const caors::math::mat4 view_projection =
      caors::math::perspective_view_projection<handedness::right, clip_depth::zero_to_one>(
          eye, target, up, caors::math::radians(60.0F), aspect, 0.1F, 100.0F);

  caors::math::mat4 inverse_view_projection{};
  if (!caors::math::try_inverse(view_projection, inverse_view_projection)) {
    std::cerr << "view-projection 矩阵不可逆\n";
    return 1;
  }

  const caors::math::frustum camera_frustum =
      caors::math::extract_frustum<clip_depth::zero_to_one>(view_projection);
  const caors::math::rect viewport{{0.0F, 0.0F}, {1920.0F, 1080.0F}};

  const std::array<render_item, 3> items{{
      {"actor", caors::math::aabb::from_center_size({0.0F, 0.0F, 0.0F}, {1.0F, 2.0F, 1.0F})},
      {"prop", caors::math::aabb::from_center_size({2.0F, 0.0F, -3.0F}, {1.0F, 1.0F, 1.0F})},
      {"far_left", caors::math::aabb::from_center_size({-60.0F, 0.0F, -5.0F}, {2.0F, 2.0F, 2.0F})},
  }};

  for (const render_item& item : items) {
    const bool visible = caors::math::intersects(camera_frustum, item.bounds);
    std::cout << item.name << ": " << (visible ? "visible" : "culled") << '\n';

    if (!visible) {
      continue;
    }

    const caors::math::vec3 world = item.bounds.center();
    const caors::math::vec3 screen =
        caors::math::project_point<clip_depth::zero_to_one>(view_projection, viewport, world);

    caors::math::vec3 unprojected{};
    if (caors::math::try_unproject_point<clip_depth::zero_to_one>(inverse_view_projection, viewport,
                                                                  screen, unprojected)) {
      print_vec3("  world", world);
      print_vec3("  screen", screen);
      print_vec3("  unprojected", unprojected);
    }
  }
}
