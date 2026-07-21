<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Math

`caors::math` 是 CAORS 的基础数学值类型模块。它面向游戏、编辑器、资源导入和运行期动态调用等常见场景，
提供稳定、轻量、无额外所有权语义的值类型。

当前模块只依赖 C++ 标准库，不依赖 `meta`、`serialization`、`objects` 或 `resources`。这样可以让上层模块
按需为数学类型增加注册、序列化、编辑器属性或 `meta::value` fast path，而不会让底层数学类型反向依赖运行期系统。

## 头文件

```cpp
#include <caors/math.h>
```

也可以按需包含细分头文件：

```cpp
#include <caors/math/vector.h>
#include <caors/math/quaternion.h>
#include <caors/math/matrix.h>
#include <caors/math/rect.h>
#include <caors/math/aabb.h>
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

## 当前边界

- 当前没有 SIMD 后端。
- 当前没有双精度、整型向量和模板化维度类型。
- 当前没有坐标系、角度单位、投影矩阵和变换矩阵 helper。
- 当前没有内建 meta 注册和序列化 codec。
- 当前没有接入 `meta::value` fast path。

这些能力会根据实际使用和 benchmark 数据逐步加入。
