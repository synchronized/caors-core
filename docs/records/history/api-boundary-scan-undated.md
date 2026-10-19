<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 公开 API 边界历史扫描摘要

来源：整理前 `docs/api_stability.md` 的既有结论。原文没有记录扫描日期、被验证提交或命令输出，
因此只能用于历史追溯，不作为当前扫描通过的证据；整理日期为 2026-10-19。
当前检查方法见[Beta 验收清单](../../checklists/beta-readiness.md)。

本轮扫描结论：

- `examples/` 没有 raw ops 或 `detail` 调用，示例路径保持公开 API 优先。
- `docs/meta.md` 仍会描述 `get_raw_ops<T>()` 这类高级扩展入口；这些入口不属于普通用户主路径。
- `tests/meta/static_type_record/`、`tests/meta/global/` 和 `tests/registration/init/` 仍有少量 `detail` 调用，属于注册表和静态类型数据
  内部边界测试，不作为普通测试写法。
- `include/caors/**/detail` 仍作为内部实现头存在，当前不要求删除；重点是避免进入用户示例和高层文档的默认路径。
- 根命名空间别名规则已经写入 `docs/development.md`，后续新增别名前应先确认是否属于高频稳定入口。
