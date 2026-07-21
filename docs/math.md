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

## 当前类型

- `caors::math::vec2`
- `caors::math::vec3`
- `caors::math::vec4`
- `caors::math::quat`
- `caors::math::mat3`
- `caors::math::mat4`
- `caors::math::rect`
- `caors::math::aabb`
- `caors::math::ray`
- `caors::math::sphere`
- `caors::math::plane`

当前第一版只提供单精度类型。后续如果需要双精度或整型数学类型，应先评估命名、序列化格式和
`meta::value` fast path 的 API 影响。

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

## 与 meta::value fast path 的关系

`meta::value` fast path 当前优先服务高频动态调用：基础数值、布尔、字符串、`string_name` 和
`meta::object*`。数学类型暂时不直接内联到 `meta::value`，原因是：

- `meta::value` 的 `kind` 是高频动态调用热路径，过早加入大量数学类型会扩大 API 和分支表。
- `mat4`、`aabb` 等类型尺寸较大，不一定适合直接作为 tagged value 内联类型。
- 是否值得内联，应基于实际脚本调用、编辑器属性访问和 benchmark 数据判断。

推荐策略：

- 先让数学类型作为普通 C++ 值类型稳定下来。
- 通过 `meta` 注册和 serialization codec 支持反射、序列化和资源配置。
- 后续只把确实高频的类型加入 fast path，优先候选是 `vec2`、`vec3`、`vec4`、`quat`。
- 较大的 `mat4`、`rect`、`aabb` 默认先走 `any` / `any_ref` 或 capability 路径。

## Meta 注册和序列化

`caors/math/registration.h` 提供 `caors::math::register_meta_types()`，用于一次注册当前内建数学值类型：

```cpp
caors::error status = caors::math::register_meta_types();
```

注册内容：

- `vec2`、`vec3`、`vec4`：注册 `x/y/z/w` 分量属性。
- `quat`：注册 `(x, y, z, w)` 分量属性，其中 `w` 是标量部分。
- `rect`：注册 `position` 和 `size` 属性。
- `aabb`：注册 `min` 和 `max` 属性。
- `mat3`、`mat4`：当前只注册类型身份；矩阵元素通过 codec 按行主序数组读写，不暴露底层
  `values` C 数组属性。

`caors/math/serialization.h` 提供内建 `serialization::codec_traits<T>` 特化；`caors/serialization.h`
会自动包含它。math codec 不要求数学类型先完成 meta 注册，编译期已知类型或通过 `any` / `any_ref`
携带 `meta::type` 时即可查询到静态 codec。

序列化格式：

- `vec2`：`[x, y]`
- `vec3`：`[x, y, z]`
- `vec4`：`[x, y, z, w]`
- `quat`：`[x, y, z, w]`
- `mat3`：行主序 9 元素数组。
- `mat4`：行主序 16 元素数组。
- `rect`：`{"position":[x,y],"size":[w,h]}`
- `aabb`：`{"min":[x,y,z],"max":[x,y,z]}`

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
- 当前没有双精度、整型向量和模板化维度类型。
- 当前没有完整相机、骨骼、碰撞或 frustum helper。
- 当前没有接入 `meta::value` fast path。
- `ray`、`sphere`、`plane` 当前暂未提供内建 meta 注册 helper 和 codec；需要时可通过普通
  `registration::type_<T>(name)` 自行注册。

这些能力会根据实际使用和 benchmark 数据逐步加入。
