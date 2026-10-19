<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# Beta 验收执行清单

本页维护进入 beta 的检查步骤和通过条件，不声明项目已经进入 beta。
候选与实验性范围只在[API 稳定性政策](../reference/api-stability.md)维护；各模块行为见[文档中心](../README.md)。

## 功能与文档门槛

- [ ] 文档、示例、测试和公开头使用一致的推荐主入口，不展示已删除的兼容 API。
- [ ] 公开头明确所有权、内存资源、借用有效期、线程、回调线程、错误和失败行为。
- [ ] detail/raw ops 不进入普通用户示例或高层主路径，内部边界测试明确其用途。
- [ ] Meta、对象、资源和序列化最小闭环均有可运行示例与对应测试。
- [ ] `examples/beta/` 覆盖 project config、VFS/provider、同步/异步加载、preload、resolver、reload/save 和 typed options。
- [ ] 相关平台全量构建、全量测试、示例和 Release benchmark 冒烟通过，正式性能基线具备可复现环境。
- [ ] 路线图和计划中的未来能力没有被描述成现有稳定 API，未验证项及其影响已记录。

## 边界扫描

在仓库根目录执行，逐项区分普通用户主路径、高级扩展与内部测试；命中本身不直接判为失败：

```sh
rg -n 'get_raw_ops|deprecated|CAORS_.*DEPRECATED' include/caors docs examples tests
rg -n '#include.*caors/.*/detail/' include/caors docs examples tests
rg -n 'meta::detail|serialization::detail|resources::detail|objects::detail|caors::detail' docs examples tests
rg -n 'namespace caors::|using .*::' include/caors
```

## 构建与验证入口

构建和安装命令统一见[构建指南](../guides/build.md)，跨平台步骤见[Linux 清单](linux-validation.md)，
基准构建及用例见[Benchmark 指南](../guides/benchmarking.md)，示例顺序见
[Beta 示例](../../examples/beta/README.md)。

每轮检查记录被验证提交、环境、实际命令、通过/失败、未验证项及延期影响，结果写入
[独立记录](../records/README.md)，不在本清单追加无日期的“本轮扫描通过”。
