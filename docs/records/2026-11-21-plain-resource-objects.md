<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 普通资源对象的所有权与异步交付

## 范围

[宿主驱动资源计划](../plans/host-driven-resource-loading.md)P1 的首个批次，基于 cc8ad192，
分支 feature/resource-source-boundary。解除现有 handle、weak_handle、make_handle、adopt_handle
及 loader/sync_loader_adapter 对 resource 基类的静态限制，不新增第二套句柄或加载协议。
guid() 仅对旧 resource 派生对象提供，旧缓存及引用行为保持原契约。

## 验证与限制

新增测试覆盖普通对象强弱引用、非虚析构基类转换后的具体对象销毁、分配器回收、
采用拥有式对象的原析构策略，以及宿主保存完成凭证后延迟交付普通对象。
测试没有让普通对象伪装成 GUID 资源，也没有增加路径到 GUID 的转换。
Windows Clang Debug 动态库本机验证通过：handle、cache、async 三组回归，
无 IO 的资源边界编译探针，插件 resource_lifetime 专项及分块示例。
格式与文档链接检查通过。本批未重跑完整平台矩阵，合并前须按计划补充相关平台验证；
旧批次平台结果不作为本批证据。

当前 cache、manager、ref、database 和 async_load_context 仍基于 GUID，完整请求管线
尚不能用于任意普通对象或路径键。本批不代表宿主键迁移完成，也未删除 IO。
后续先验证统一 Key 参数化与类型分派，再迁移缓存、引用、序列化及加载上下文，
不把普通对象支持包装成新的平行资源框架。
