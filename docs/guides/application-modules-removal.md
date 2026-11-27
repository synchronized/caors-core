<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 CAORS contributors -->

# 资源、资产及 IO 模块退出

CAORS 不再接管应用的资源与文件策略。resources、assets 和 io 的公共头、实现、专用构建消费已移除。
这是破坏性变更，没有兼容桥，不把旧实现复制或改名为另一个基础模块。历史代码由 Git 保留。

## 下游迁移

- 加载、资源标识、缓存、依赖协调、重载及取消由宿主选择并实现；CAORS 不提供替代资产管理器。
- 文件、网络、URI 和存储后端由宿主管理，不存在 CAORS 内建文件服务。
- JSON 接收完整内存文本；输出可使用 to_json 或写入调用者的 caors::string。
- MessagePack 接收 std::span<const std::byte>；编码输出为 serialization::byte_buffer / byte_buffer_result。
  宿主取得内存结果后自行发送或保存；原 io::byte_buffer 类型及流入口移除。
- 插件保留 platform::shared_library、协议、注册及模块寿命管理。
  宿主读取清单后调用 plugins::parse_plugin_manifest(text)，load_plugin_manifest(path) 已删除。
- 内存、反射、对象、事件、数学和基础任务调度继续保留。通用序列化 resolver 与对象身份协议
  有独立用途，不因资源模块退出而删除。

无需把路径转换为 CAORS 资源 GUID，也无需为使用底座实现 loader/provider。

## 验证与限制

本改动的实际验证由[移除记录](../records/2026-11-27-application-modules-removal.md)维护。
旧版本文档和记录只作为历史证据；不能用于描述当前工作树的可用接口。
