<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Math

`caors::math` 是 CAORS 的基础数学值类型模块。它面向游戏、编辑器、资源导入和运行期动态调用等常见场景，
提供稳定、轻量、无额外所有权语义的值类型。

核心数学值类型头只依赖 C++ 标准库，不依赖 `meta`、`serialization`、`objects` 或 `resources`。这样可以让
上层模块按需为数学类型增加注册、序列化、编辑器属性或 `meta::value` fast path，而不会让底层数学类型
反向依赖运行期系统。需要这些能力时包含对应适配头。

## 头文件

```cpp
#include <caors/math.h>
```

也可以按需包含细分头文件：

```cpp
#include <caors/math/vector.h>
#include <caors/math/functions.h>
#include <caors/math/quaternion.h>
#include <caors/math/matrix.h>
#include <caors/math/transform.h>
#include <caors/math/projection.h>
#include <caors/math/rect.h>
#include <caors/math/aabb.h>
#include <caors/math/geometry.h>
```

反射注册和序列化 codec 是独立适配头：

```cpp
#include <caors/math/registration.h>
#include <caors/math/serialization.h>
```

## 当前类型和模板别名

基础类型使用模板实现，并提供常用别名：

- `vec2_t<T>` / `vec3_t<T>` / `vec4_t<T>`
- `quat_t<T>`
- `mat3_t<T>` / `mat4_t<T>`
- `rect_t<T>`
- `aabb_t<T>`
- `ray_t<T>` / `sphere_t<T>` / `plane_t<T>` / `frustum_t<T>`

默认短名仍表示单精度版本：

- `caors::math::vec2` / `vec3` / `vec4`，等价于 `vec2f` / `vec3f` / `vec4f`
- `caors::math::quat`，等价于 `quatf`
- `caors::math::mat3` / `mat4`，等价于 `mat3f` / `mat4f`
- `caors::math::rect`，等价于 `rectf`
- `caors::math::aabb`，等价于 `aabbf`

当前还提供这些常用标量别名：

- 双精度：`dvec2`、`dvec3`、`dvec4`、`dquat`、`dmat3`、`dmat4`、`drect`、`daabb`、
  `dray`、`dsphere`、`dplane`
- 32 位整型：`ivec2`、`ivec3`、`ivec4`、`irect`、`iaabb`
- 32 位无符号整型向量：`uvec2`、`uvec3`、`uvec4`

`irect` 适合 UI 布局、像素区域、贴图 atlas、窗口/裁剪矩形和网格单元区域。它的 `center()` 走整数除法，
需要亚像素或精确中心时应使用 `rect` / `drect`。

几何辅助类型默认短名也是单精度：

- `caors::math::ray`
- `caors::math::sphere`
- `caors::math::plane`
- `caors::math::frustum`

它们分别等价于 `rayf`、`spheref`、`planef`、`frustumf`。需要双精度时使用 `dray`、`dsphere`、`dplane`、
`dfrustum`。

## 使用示例

```cpp
caors::math::vec3 position{1.0F, 2.0F, 3.0F};
caors::math::vec3 velocity{0.0F, 1.0F, 0.0F};

position += velocity * 0.016F;

float distance = caors::math::length(position);
caors::math::vec3 direction = caors::math::normalized(position);
```

矩阵使用行主序存储，`operator()(row, column)` 不做边界检查，调用方需要保证下标合法。

```cpp
caors::math::mat4 transform = caors::math::mat4::identity();
caors::math::vec4 point = transform * caors::math::vec4{1.0F, 2.0F, 3.0F, 1.0F};
```

四元数使用 `(x, y, z, w)` 存储，`w` 是标量部分。

```cpp
caors::math::quat rotation =
    caors::math::quat::from_axis_angle(caors::math::vec3{0.0F, 0.0F, 1.0F}, radians);
caors::math::vec3 rotated = caors::math::rotate(rotation, caors::math::vec3{1.0F, 0.0F, 0.0F});
```

## 变换和投影

矩阵按行主序存储，但使用列向量语义，即 `matrix * vector`。因此 TRS 组合顺序是：

```cpp
caors::math::mat4 transform =
    caors::math::trs(position, rotation, scale);
```

这等价于：

```cpp
caors::math::translation(position) *
    caors::math::rotation(rotation) *
    caors::math::scaling(scale);
```

点、向量和方向使用不同 helper，避免把平移错误地应用到方向上：

```cpp
caors::math::vec3 world_point = caors::math::transform_point(transform, local_point);
caors::math::vec3 world_vector = caors::math::transform_vector(transform, local_vector);
caors::math::vec3 world_direction = caors::math::transform_direction(transform, local_direction);
```

投影矩阵用模板参数表达手系和裁剪空间深度范围：

```cpp
using caors::math::clip_depth;
using caors::math::handedness;

caors::math::mat4 projection =
    caors::math::perspective<handedness::right, clip_depth::zero_to_one>(
        caors::math::radians(60.0F), aspect, near_z, far_z);
```

同时提供便捷函数，适合普通调用和文档示例：

```cpp
caors::math::mat4 view =
    caors::math::look_at_rh(eye, target, up);

caors::math::mat4 perspective =
    caors::math::perspective_rh_zo(caors::math::radians(60.0F), aspect, near_z, far_z);

caors::math::mat4 ortho =
    caors::math::orthographic_rh_zo(left, right, bottom, top, near_z, far_z);
```

命名规则：

- `rh` / `lh` 表示右手系 / 左手系。
- `zo` 表示裁剪空间深度范围 `[0, 1]`，常用于 Vulkan / D3D 风格后端。
- `no` 表示裁剪空间深度范围 `[-1, 1]`，常用于 OpenGL 风格后端。

`caors::math` 只表达通用数学约定，不直接依赖 Vulkan、D3D 或 OpenGL。后续 `caors-vulkan`
可以在自己的模块内把这些矩阵和 `VkViewport`、`VkRect2D`、Y 翻转或后端特定 clip 修正规则对接起来。

视锥体和屏幕坐标 helper 只处理通用矩阵数学，不隐式翻转 Y 轴或绑定某个渲染后端：

```cpp
caors::math::mat4 vp =
    caors::math::perspective_view_projection<handedness::right, clip_depth::zero_to_one>(
        eye, target, up, caors::math::radians(60.0F), aspect, near_z, far_z);

caors::math::frustum frustum =
    caors::math::extract_frustum<clip_depth::zero_to_one>(vp);

caors::math::vec3 screen =
    caors::math::project_point<clip_depth::zero_to_one>(vp, viewport, world);
```

如果反投影在热点循环中使用，推荐先对 view-projection 求逆并复用结果：

```cpp
caors::math::mat4 inverse_vp{};
if (caors::math::try_inverse(vp, inverse_vp)) {
  caors::math::vec3 world{};
  caors::math::try_unproject_point<clip_depth::zero_to_one>(inverse_vp, viewport, screen, world);
}
```

`examples/math/render_pipeline.cpp` 展示了更完整的组合方式：构造 camera view-projection，提取
frustum，裁剪一组 AABB，再把可见对象中心点投影到屏幕并反投影回世界坐标。

## 与 meta::value fast path 的关系

数学类型当前不直接内联到 `meta::value`，通过 `any` / `any_ref` 或 capability 承载。
较大结构直接内联会增加 tagged value 的大小与分支成本；后续候选与启动条件由
[路线图](../roadmap.md#05-math-基础数学类型)维护，测量方法见[Benchmark 指南](../guides/benchmarking.md#math-fast-path-判断)。

## Meta 注册和序列化

`caors/math/registration.h` 提供 `caors::math::register_meta_types()`，用于一次注册当前内建数学值类型：

```cpp
caors::error status = caors::math::register_meta_types();
```

默认批量注册只注册 float 短名类型，避免把所有标量组合自动塞进全局 registry。需要 double/int
别名时，显式调用对应模板注册函数：

```cpp
caors::math::register_vec3<double>();        // caors.math.dvec3
caors::math::register_vec2<std::int32_t>();  // caors.math.ivec2
```

float 默认注册内容：

- `vec2`、`vec3`、`vec4`：注册 `x/y/z/w` 分量属性。
- `quat`：注册 `(x, y, z, w)` 分量属性，其中 `w` 是标量部分。
- `rect`：注册 `position` 和 `size` 属性。
- `aabb`：注册 `min` 和 `max` 属性。
- `ray`：注册 `origin` 和 `direction` 属性。
- `sphere`：注册 `center` 和 `radius` 属性。
- `plane`：注册 `normal` 和 `distance` 属性。
- `frustum`：注册 `left/right/bottom/top/near/far` 六个裁剪平面属性。
- `mat3`、`mat4`：当前只注册类型身份；矩阵元素通过 codec 按行主序数组读写，不暴露底层
  `values` C 数组属性。

`caors/math/serialization.h` 提供内建 `serialization::codec_traits<T>` 特化；`caors/serialization.h`
会自动包含它。math codec 不要求数学类型先完成 meta 注册，编译期已知类型或通过 `any` / `any_ref`
携带 `meta::type` 时即可查询到静态 codec。

codec 覆盖 `vec*_t<T>`、`quat_t<T>`、`mat*_t<T>`、`rect_t<T>`、`aabb_t<T>`、`ray_t<T>`、
`sphere_t<T>`、`plane_t<T>` 和 `frustum_t<T>`。`T` 当前应为算术标量；浮点写出为 JSON number，整型写出为整数
number。

序列化格式：

- `vec2`：`[x, y]`
- `vec3`：`[x, y, z]`
- `vec4`：`[x, y, z, w]`
- `quat`：`[x, y, z, w]`
- `mat3`：行主序 9 元素数组。
- `mat4`：行主序 16 元素数组。
- `rect`：`{"position":[x,y],"size":[w,h]}`
- `aabb`：`{"min":[x,y,z],"max":[x,y,z]}`
- `ray`：`{"origin":[x,y,z],"direction":[x,y,z]}`
- `sphere`：`{"center":[x,y,z],"radius":r}`
- `plane`：`{"normal":[x,y,z],"distance":d}`
- `frustum`：`{"left":plane,"right":plane,"bottom":plane,"top":plane,"near":plane,"far":plane}`

math 类型作为已注册对象字段时会优先走 codec，因此配置结构可以直接暴露数学字段：

```cpp
struct camera_config {
  caors::math::vec3 position;
  caors::math::quat rotation;
  caors::math::rect viewport;
};

caors::math::register_meta_types();
caors::registration::type_<camera_config>("game.camera_config")
    .meta()
    .property("position", &camera_config::position)
    .property("rotation", &camera_config::rotation)
    .property("viewport", &camera_config::viewport)
    .serialization()
    .commit();
```

## 当前边界

- 当前没有 SIMD 后端。
- 当前只做标量模板化，没有做“维度作为模板参数”的统一 `vec<T, N>` / `mat<T, R, C>`。
- 当前没有完整相机对象、骨骼或完整碰撞系统；frustum 只提供基础平面提取和粗略裁剪 helper。
- 当前没有接入 `meta::value` fast path。
- 当前 benchmark 已覆盖 math 纯计算和 `meta::property` / `meta::method` / `meta::value` /
  `any` / `any_ref` 动态路径；是否把 `vec2`、`vec3`、`vec4`、`quat` 加入 fast path 后续以这些数据为依据。

这些能力会根据实际使用和 benchmark 数据逐步加入。

## Traits 和第三方适配

`caors/math/traits.h` 提供 CAORS 数学类型的轻量 traits：

- `vector_traits<T>`：向量标量类型、分量数量和按下标访问。
- `matrix_traits<T>`：矩阵标量类型、行列数量和按行列访问。
- `quaternion_traits<T>`、`rect_traits<T>`、`aabb_traits<T>`、`ray_traits<T>`、
  `sphere_traits<T>`、`plane_traits<T>`、`frustum_traits<T>`：对应类型的命名成员访问。

这层只描述数学类型的结构，不引入 GLM、Eigen 等第三方依赖。后续如果需要接入外部数学库，应优先在
`caors/adapters` 中为外部类型提供类似 traits，而不是让 `caors::math` 直接依赖第三方库。
