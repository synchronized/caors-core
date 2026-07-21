// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "support/benchmark_support.h"

#include "caors/math.h"

#include <ctrack.hpp>

#include <cstdint>

namespace caors::benchmarks {
namespace {

[[nodiscard]] std::uintptr_t hash_float(float value) noexcept {
  const float positive = value < 0.0F ? -value : value;
  return static_cast<std::uintptr_t>(positive * 1000000.0F);
}

void consume_vec3(caors::math::vec3 value) noexcept {
  consume(hash_float(value.x) ^ (hash_float(value.y) << 1U) ^ (hash_float(value.z) << 2U));
}

void consume_vec4(caors::math::vec4 value) noexcept {
  consume(hash_float(value.x) ^ (hash_float(value.y) << 1U) ^ (hash_float(value.z) << 2U) ^
          (hash_float(value.w) << 3U));
}

void consume_mat4(const caors::math::mat4& value) noexcept {
  consume(hash_float(value(0, 0)) ^ (hash_float(value(1, 1)) << 1U) ^
          (hash_float(value(2, 2)) << 2U) ^ (hash_float(value(3, 3)) << 3U));
}

} // namespace

void benchmark_math(std::size_t iterations) {
  const caors::math::vec3 left{1.0F, 2.0F, 3.0F};
  const caors::math::vec3 right{4.0F, 5.0F, 6.0F};

  {
    CTRACK_NAME("math.vec3.dot_cross_normalize");
    caors::math::vec3 accumulator = caors::math::vec3::zero();
    for (std::size_t index = 0; index < iterations; ++index) {
      const float step = static_cast<float>(index % 17U) * 0.001F;
      accumulator += caors::math::cross(caors::math::normalized(left + right * step), right);
      consume_vec3(accumulator);
    }
  }

  {
    CTRACK_NAME("math.mat4.multiply");
    caors::math::mat4 transform =
        caors::math::trs(caors::math::vec3{10.0F, 20.0F, 30.0F},
                         caors::math::quat::from_axis_angle(caors::math::vec3{0.0F, 1.0F, 0.0F},
                                                            caors::math::radians(45.0F)),
                         caors::math::vec3{2.0F, 2.0F, 2.0F});
    const caors::math::mat4 projection =
        caors::math::perspective_rh_zo(caors::math::radians(60.0F), 16.0F / 9.0F, 0.1F, 100.0F);
    caors::math::mat4 accumulator = caors::math::mat4::identity();
    for (std::size_t index = 0; index < iterations; ++index) {
      transform(0, 3) = static_cast<float>(index % 31U);
      accumulator = projection * transform;
      consume_mat4(accumulator);
    }
  }

  {
    CTRACK_NAME("math.mat4.transform_vec4");
    const caors::math::mat4 transform =
        caors::math::trs(caors::math::vec3{1.0F, 2.0F, 3.0F},
                         caors::math::quat::from_axis_angle(caors::math::vec3{0.0F, 0.0F, 1.0F},
                                                            caors::math::radians(15.0F)),
                         caors::math::vec3{1.0F, 2.0F, 3.0F});
    caors::math::vec4 accumulator{1.0F, 2.0F, 3.0F, 1.0F};
    for (std::size_t index = 0; index < iterations; ++index) {
      const float step = static_cast<float>(index % 23U) * 0.01F;
      accumulator = transform * caors::math::vec4{1.0F + step, 2.0F, 3.0F, 1.0F};
      consume_vec4(accumulator);
    }
  }

  {
    CTRACK_NAME("math.quat.rotate");
    const caors::math::quat rotation = caors::math::quat::from_axis_angle(
        caors::math::vec3{0.0F, 1.0F, 0.0F}, caors::math::radians(30.0F));
    caors::math::vec3 accumulator{1.0F, 0.0F, 0.0F};
    for (std::size_t index = 0; index < iterations; ++index) {
      accumulator = caors::math::rotate(rotation, accumulator);
      consume_vec3(accumulator);
    }
  }

  {
    CTRACK_NAME("math.trs");
    caors::math::mat4 accumulator = caors::math::mat4::identity();
    for (std::size_t index = 0; index < iterations; ++index) {
      const float step = static_cast<float>(index % 360U);
      accumulator =
          caors::math::trs(caors::math::vec3{step, step * 0.5F, step * 0.25F},
                           caors::math::quat::from_axis_angle(caors::math::vec3{0.0F, 1.0F, 0.0F},
                                                              caors::math::radians(step)),
                           caors::math::vec3{1.0F, 2.0F, 3.0F});
      consume_mat4(accumulator);
    }
  }

  {
    CTRACK_NAME("math.perspective_rh_zo");
    caors::math::mat4 accumulator = caors::math::mat4::identity();
    for (std::size_t index = 0; index < iterations; ++index) {
      const float aspect = 1.0F + static_cast<float>(index % 16U) * 0.01F;
      accumulator =
          caors::math::perspective_rh_zo(caors::math::radians(60.0F), aspect, 0.1F, 100.0F);
      consume_mat4(accumulator);
    }
  }
}

} // namespace caors::benchmarks
