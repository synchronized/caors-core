// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "support/benchmark_support.h"

#include "caors/math.h"
#include "caors/math/registration.h"
#include "caors/registration.h"

#include <ctrack.hpp>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>

namespace caors::benchmarks {
namespace {

[[nodiscard]] std::uintptr_t hash_float(float value) noexcept {
  const float positive = value < 0.0F ? -value : value;
  return static_cast<std::uintptr_t>(positive * 1000000.0F);
}

[[nodiscard]] std::uintptr_t hash_double(double value) noexcept {
  const double positive = value < 0.0 ? -value : value;
  return static_cast<std::uintptr_t>(positive * 1000000.0);
}

[[nodiscard]] std::uintptr_t hash_integer(std::int32_t value) noexcept {
  return static_cast<std::uintptr_t>(value < 0 ? -value : value);
}

void consume_vec3(caors::math::vec3 value) noexcept {
  consume(hash_float(value.x) ^ (hash_float(value.y) << 1U) ^ (hash_float(value.z) << 2U));
}

void consume_dvec3(caors::math::dvec3 value) noexcept {
  consume(hash_double(value.x) ^ (hash_double(value.y) << 1U) ^ (hash_double(value.z) << 2U));
}

void consume_ivec3(caors::math::ivec3 value) noexcept {
  consume(hash_integer(value.x) ^ (hash_integer(value.y) << 1U) ^ (hash_integer(value.z) << 2U));
}

void consume_vec4(caors::math::vec4 value) noexcept {
  consume(hash_float(value.x) ^ (hash_float(value.y) << 1U) ^ (hash_float(value.z) << 2U) ^
          (hash_float(value.w) << 3U));
}

void consume_mat4(const caors::math::mat4& value) noexcept {
  consume(hash_float(value(0, 0)) ^ (hash_float(value(1, 1)) << 1U) ^
          (hash_float(value(2, 2)) << 2U) ^ (hash_float(value(3, 3)) << 3U));
}

void consume_dmat4(const caors::math::dmat4& value) noexcept {
  consume(hash_double(value(0, 0)) ^ (hash_double(value(1, 1)) << 1U) ^
          (hash_double(value(2, 2)) << 2U) ^ (hash_double(value(3, 3)) << 3U));
}

void consume_frustum(const caors::math::frustum& value) noexcept {
  consume(hash_float(value.left.distance) ^ (hash_float(value.right.distance) << 1U) ^
          (hash_float(value.near_plane.distance) << 2U) ^
          (hash_float(value.far_plane.distance) << 3U));
}

[[nodiscard]] const caors::math::vec3* try_get_vec3(const caors::meta::value& value) noexcept {
  if (const caors::meta::any_ref* ref = value.try_as_ref(); ref != nullptr) {
    return ref->try_get<caors::math::vec3>();
  }
  if (const caors::meta::any* any = value.try_as_any(); any != nullptr) {
    return any->try_get<caors::math::vec3>();
  }
  return nullptr;
}

struct math_benchmark_actor {
  caors::math::vec3 position{1.0F, 2.0F, 3.0F};
  caors::math::quat rotation = caors::math::quat::identity();
  caors::math::mat4 transform = caors::math::mat4::identity();

  [[nodiscard]] caors::math::vec3 offset(caors::math::vec3 delta) const noexcept {
    return position + delta;
  }

  [[nodiscard]] caors::math::vec3 transform_point(caors::math::vec3 point) const noexcept {
    return caors::math::transform_point(transform, point);
  }

  void set_transform(caors::math::mat4 value) noexcept { transform = value; }
};

void ensure_math_benchmark_types_registered() {
  static bool registered = false;
  if (registered) {
    return;
  }

  if (const caors::error status = caors::math::register_meta_types(); status.failed()) {
    std::cerr << "注册 math meta 类型失败\n";
    std::exit(EXIT_FAILURE);
  }

  const caors::error status =
      caors::registration::type_<math_benchmark_actor>("caors.benchmark.math_actor")
          .meta()
          .property("position", &math_benchmark_actor::position)
          .property("rotation", &math_benchmark_actor::rotation)
          .property("transform", &math_benchmark_actor::transform)
          .method("offset", &math_benchmark_actor::offset)
          .method("transform_point", &math_benchmark_actor::transform_point)
          .method("set_transform", &math_benchmark_actor::set_transform)
          .commit();
  if (status.failed()) {
    std::cerr << "注册 math benchmark 类型失败\n";
    std::exit(EXIT_FAILURE);
  }

  registered = true;
}

} // namespace

void benchmark_math(std::size_t iterations) {
  ensure_math_benchmark_types_registered();

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
    CTRACK_NAME("math.dmat4.multiply");
    caors::math::dmat4 transform =
        caors::math::trs(caors::math::dvec3{10.0, 20.0, 30.0},
                         caors::math::dquat::from_axis_angle(caors::math::dvec3{0.0, 1.0, 0.0},
                                                             caors::math::radians(45.0)),
                         caors::math::dvec3{2.0, 2.0, 2.0});
    const caors::math::dmat4 projection =
        caors::math::perspective<caors::math::handedness::right,
                                 caors::math::clip_depth::zero_to_one>(caors::math::radians(60.0),
                                                                       16.0 / 9.0, 0.1, 100.0);
    caors::math::dmat4 accumulator = caors::math::dmat4::identity();
    for (std::size_t index = 0; index < iterations; ++index) {
      transform(0, 3) = static_cast<double>(index % 31U);
      accumulator = projection * transform;
      consume_dmat4(accumulator);
    }
  }

  {
    CTRACK_NAME("math.scalar_aliases");
    caors::math::dvec3 precise_accumulator{};
    caors::math::ivec3 integer_accumulator{};
    for (std::size_t index = 0; index < iterations; ++index) {
      precise_accumulator +=
          caors::math::dvec3{1.0, 2.0, 3.0} * (static_cast<double>(index % 17U) * 0.001);
      integer_accumulator += caors::math::ivec3{1, 2, 3};
      consume_dvec3(precise_accumulator);
      consume_ivec3(integer_accumulator);
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

  {
    CTRACK_NAME("math.geometry");
    caors::math::sphere accumulator{{0.0F, 0.0F, 0.0F}, 1.0F};
    const caors::math::plane ground = caors::math::plane::from_point_normal(
        caors::math::vec3::zero(), caors::math::vec3{0.0F, 1.0F, 0.0F});
    for (std::size_t index = 0; index < iterations; ++index) {
      const float step = static_cast<float>(index % 29U) * 0.01F;
      accumulator.center = {step, step * 0.5F, step * 0.25F};
      accumulator.radius = 1.0F + step;
      consume(static_cast<std::uintptr_t>(caors::math::intersects(ground, accumulator)));
    }
  }

  {
    CTRACK_NAME("math.frustum_cull");
    const caors::math::mat4 view_projection =
        caors::math::perspective_view_projection<caors::math::handedness::right,
                                                 caors::math::clip_depth::zero_to_one>(
            caors::math::vec3{0.0F, 0.0F, 5.0F}, caors::math::vec3::zero(),
            caors::math::vec3{0.0F, 1.0F, 0.0F}, caors::math::radians(60.0F), 16.0F / 9.0F, 0.1F,
            100.0F);
    const caors::math::frustum frustum =
        caors::math::extract_frustum<caors::math::clip_depth::zero_to_one>(view_projection);
    for (std::size_t index = 0; index < iterations; ++index) {
      const float step = static_cast<float>(index % 97U) * 0.1F;
      const caors::math::aabb bounds{{-1.0F + step, -1.0F, -1.0F}, {1.0F + step, 1.0F, 1.0F}};
      consume(static_cast<std::uintptr_t>(caors::math::intersects(frustum, bounds)));
      consume_frustum(frustum);
    }
  }

  {
    CTRACK_NAME("math.project_unproject");
    const caors::math::mat4 view_projection =
        caors::math::perspective_view_projection<caors::math::handedness::right,
                                                 caors::math::clip_depth::zero_to_one>(
            caors::math::vec3::zero(), caors::math::vec3{0.0F, 0.0F, -1.0F},
            caors::math::vec3{0.0F, 1.0F, 0.0F}, caors::math::radians(60.0F), 16.0F / 9.0F, 0.1F,
            100.0F);
    caors::math::mat4 inverse_view_projection{};
    if (!caors::math::try_inverse(view_projection, inverse_view_projection)) {
      return;
    }

    const caors::math::rect viewport{{0.0F, 0.0F}, {1920.0F, 1080.0F}};
    caors::math::vec3 accumulator{};
    for (std::size_t index = 0; index < iterations; ++index) {
      const float step = static_cast<float>(index % 41U) * 0.01F;
      const caors::math::vec3 world{step, step * 0.5F, -2.0F - step};
      const caors::math::vec3 screen =
          caors::math::project_point<caors::math::clip_depth::zero_to_one>(view_projection,
                                                                           viewport, world);
      if (caors::math::try_unproject_point<caors::math::clip_depth::zero_to_one>(
              inverse_view_projection, viewport, screen, accumulator)) {
        consume_vec3(accumulator);
      }
    }
  }

  {
    CTRACK_NAME("math.meta.property_vec3_typed");
    math_benchmark_actor actor{};
    const caors::meta::instance instance = caors::meta::view(actor);
    const caors::meta::property position =
        caors::meta::find<math_benchmark_actor>().find_property("position");
    if (!position) {
      std::cerr << "math benchmark position 属性未注册\n";
      std::exit(EXIT_FAILURE);
    }

    for (std::size_t index = 0; index < iterations; ++index) {
      const float step = static_cast<float>(index % 31U) * 0.01F;
      if (!position.try_set(instance, caors::math::vec3{step, step * 2.0F, step * 3.0F})) {
        std::cerr << "写入 math vec3 typed 属性失败\n";
        std::exit(EXIT_FAILURE);
      }
      const std::optional<caors::math::vec3> value = position.try_get<caors::math::vec3>(instance);
      consume_vec3(value.value_or(caors::math::vec3{}));
    }
  }

  {
    CTRACK_NAME("math.meta.property_vec3_value");
    math_benchmark_actor actor{};
    const caors::meta::instance instance = caors::meta::view(actor);
    const caors::meta::property position =
        caors::meta::find<math_benchmark_actor>().find_property("position");
    for (std::size_t index = 0; index < iterations; ++index) {
      const float step = static_cast<float>(index % 31U) * 0.01F;
      caors::math::vec3 source{step, step * 2.0F, step * 3.0F};
      if (!position.try_set_value(instance, caors::meta::value::from_ref(source))) {
        std::cerr << "写入 math vec3 value 属性失败\n";
        std::exit(EXIT_FAILURE);
      }
      const caors::meta::value current = position.get_value(instance);
      const caors::math::vec3* current_value = try_get_vec3(current);
      if (current_value == nullptr) {
        std::cerr << "读取 math vec3 value 属性失败\n";
        std::exit(EXIT_FAILURE);
      }
      consume_vec3(*current_value);
    }
  }

  {
    CTRACK_NAME("math.meta.property_vec3_any");
    math_benchmark_actor actor{};
    const caors::meta::instance instance = caors::meta::view(actor);
    const caors::meta::property position =
        caors::meta::find<math_benchmark_actor>().find_property("position");
    for (std::size_t index = 0; index < iterations; ++index) {
      const float step = static_cast<float>(index % 31U) * 0.01F;
      const caors::any source = caors::any::from(caors::math::vec3{step, step * 2.0F, step * 3.0F});
      if (!position.try_set_any(instance, source)) {
        std::cerr << "写入 math vec3 any 属性失败\n";
        std::exit(EXIT_FAILURE);
      }
      const caors::any current = position.get_any(instance);
      consume_vec3(*current.try_get<caors::math::vec3>());
    }
  }

  {
    CTRACK_NAME("math.meta.property_mat4_typed");
    math_benchmark_actor actor{};
    const caors::meta::instance instance = caors::meta::view(actor);
    const caors::meta::property transform =
        caors::meta::find<math_benchmark_actor>().find_property("transform");
    for (std::size_t index = 0; index < iterations; ++index) {
      const float step = static_cast<float>(index % 17U) * 0.01F;
      const caors::math::mat4 source =
          caors::math::translation(caors::math::vec3{step, step, step});
      if (!transform.try_set(instance, source)) {
        std::cerr << "写入 math mat4 typed 属性失败\n";
        std::exit(EXIT_FAILURE);
      }
      const std::optional<caors::math::mat4> value = transform.try_get<caors::math::mat4>(instance);
      consume_mat4(value.value_or(caors::math::mat4{}));
    }
  }

  {
    CTRACK_NAME("math.meta.method_vec3_direct");
    const math_benchmark_actor actor{};
    for (std::size_t index = 0; index < iterations; ++index) {
      const float step = static_cast<float>(index % 31U) * 0.01F;
      consume_vec3(actor.offset({step, step, step}));
    }
  }

  {
    CTRACK_NAME("math.meta.method_vec3_value");
    math_benchmark_actor actor{};
    const caors::meta::instance instance = caors::meta::view(actor);
    const caors::meta::method offset =
        caors::meta::find<math_benchmark_actor>().find_method("offset").single();
    if (!offset) {
      std::cerr << "math benchmark offset 方法未注册\n";
      std::exit(EXIT_FAILURE);
    }

    for (std::size_t index = 0; index < iterations; ++index) {
      const float step = static_cast<float>(index % 31U) * 0.01F;
      caors::math::vec3 delta{step, step, step};
      const std::array<caors::meta::value, 1> arguments{
          caors::meta::value::from_ref(delta),
      };
      const caors::meta::invoke_value_result result = offset.invoke(instance, arguments);
      const caors::math::vec3* result_value = result ? try_get_vec3(result.value()) : nullptr;
      if (result_value == nullptr) {
        std::cerr << "调用 math vec3 value 方法失败\n";
        std::exit(EXIT_FAILURE);
      }
      consume_vec3(*result_value);
    }
  }

  {
    CTRACK_NAME("math.meta.method_vec3_any");
    math_benchmark_actor actor{};
    const caors::meta::instance instance = caors::meta::view(actor);
    const caors::meta::method offset =
        caors::meta::find<math_benchmark_actor>().find_method("offset").single();
    for (std::size_t index = 0; index < iterations; ++index) {
      const float step = static_cast<float>(index % 31U) * 0.01F;
      caors::math::vec3 delta{step, step, step};
      const std::array<caors::argument, 1> arguments{caors::arg(delta)};
      const caors::meta::invoke_result result = offset.invoke_result_of(instance, arguments);
      consume_vec3(*result.try_get<caors::math::vec3>());
    }
  }

  {
    CTRACK_NAME("math.meta.any_ref_vec3");
    caors::math::vec3 value{};
    for (std::size_t index = 0; index < iterations; ++index) {
      value.x = static_cast<float>(index % 31U);
      caors::any_ref ref = caors::any_ref::from(value);
      consume_vec3(*ref.try_get<caors::math::vec3>());
    }
  }

  {
    CTRACK_NAME("math.meta.any_vec3");
    for (std::size_t index = 0; index < iterations; ++index) {
      const float step = static_cast<float>(index % 31U) * 0.01F;
      caors::any value = caors::any::from(caors::math::vec3{step, step, step});
      consume_vec3(*value.try_get<caors::math::vec3>());
    }
  }

  {
    CTRACK_NAME("math.meta.any_quat");
    for (std::size_t index = 0; index < iterations; ++index) {
      const float step = static_cast<float>(index % 360U);
      caors::any value = caors::any::from(caors::math::quat::from_axis_angle(
          caors::math::vec3{0.0F, 1.0F, 0.0F}, caors::math::radians(step)));
      consume_value(value);
    }
  }
}

} // namespace caors::benchmarks
