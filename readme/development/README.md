# mir2x 开发知识库
最后整理日期: 2026-11-05

这组文档用于在当前仓库上持续开发客户端、服务端和游戏内容。先按实际代码解释现有行为，再说明修改入口、验证方式和仍未确认的部分。

初次整理：2026-10-31。源码基线为 `bb6a22287abcd9f248d2aba755074c254c68787a`，另有本地注册、改密按钮修复。后续代码变化时，这个提交号只表示初始调研版本，不表示文档永远与 HEAD 一致。

2026-11-05 从 `develop`（`b9cf64255cfdb727283f0d7b9121c070e4c3c1a2`）迁回通用开发知识，并按 `main` 的原生构建与代码入口调整。旧 AI/MCP 实现和阶段验收记录仍保存在 `develop`；当前指南中的命令与路径面向 `main`。

## 阅读顺序

| 文档 | 解决的问题 |
| --- | --- |
| [Windows 开发环境](windows-development.md) | 在 mir2x 根目录构建、安装、启动和保留缓存 |
| [配置系统](configuration.md) | 构建参数、服务端设置、角色运行时配置与主从同步的边界 |
| [代码地图](code-map.md) | 一个功能应该从哪个目录、类和处理函数开始读 |
| [启动、登录与协议](runtime-and-protocol.md) | 从启动到注册、登录、查角色、进入地图的完整链路 |
| [业务模型与数据](domain-and-data.md) | 账号、角色、职业、UID、物品、交易、任务如何表示和保存 |
| [内容与 Lua 脚本](content-and-scripting.md) | 地图、刷怪、NPC、商店、任务和资源的修改方式 |
| [开发与测试流程](workflow-and-testing.md) | 日常改动、针对性测试、数据库与协议兼容性检查 |
| [排障与验证基线](troubleshooting-and-baseline.md) | 本次问题、已通过的检查、日志和后续验证缺口 |
| [玩法参考](../gameplay/README.md) | 练级地图、装备、技能与经验曲线的源码规则 |
| [集中配置管理评估](../../docs/research/20261106-centralized-configuration-management.md) | 现有配置的统一界面、保存方式和首期范围建议 |
| [玩法配置管理研究](../../docs/research/20261104-runtime-gameplay-config.md) | 已有配置来源、可视化管理与后续研究 |

## 按开发目标查找

| 目标 | 第一入口 | 接着阅读 |
| --- | --- | --- |
| 修改注册、登录界面 | `client/src/processcreateaccount.cpp`、`processlogin.cpp` | [登录流程](runtime-and-protocol.md#登录和进入地图) |
| 调整控件或按钮 | `client/src/widget.hpp`、`buttonbase.cpp`、`tritexbutton.cpp` | [控件故障案例](troubleshooting-and-baseline.md#注册按钮鼠标无响应) |
| 新增客户端功能及服务端请求 | `common/src/clientmsg.hpp`、`servermsg.hpp` | [协议修改流程](workflow-and-testing.md#修改网络功能) |
| 修改道具、技能或怪物定义 | `common/src/*record.inc` | [静态定义与实例](domain-and-data.md#静态定义与实例) |
| 新增商人或对话 | `server/script/npc/` | [NPC 与商人](content-and-scripting.md#npc-与商人) |
| 新增任务 | `server/script/quest/` | [任务状态与奖励](content-and-scripting.md#任务状态与奖励) |
| 调整刷怪 | `server/script/map/` | [地图与刷怪](content-and-scripting.md#地图与刷怪) |
| 修改背包、金币或交易 | `server/src/playernet.cpp`、`inventorydb.cpp`、`golddb.cpp` | [经济与持久化](domain-and-data.md#经济与持久化) |
| 排查移动、传送或场景同步 | `server/src/charobject.cpp`、`servermapop.cpp` | [运行时分工](code-map.md#运行时分工) |
| 新增设置或配置项 | 按 owner 选择构建参数、ServerConfig、`SDRuntimeConfig` 或静态数据 | [配置分层](configuration.md#配置分层)与[持久化兼容](configuration.md#持久化兼容) |

## 如何使用这份知识库

- 文档中的“代码确认”表示已经定位到实现；“本机验证”表示确实执行过；“待验证”不能当成验收结果。
- 相对路径和命令默认以仓库根目录为起点；代码链接均指向当前工作树。
- 本文档描述当前实现。原版传奇的设定、脚本移植资料与本项目已经实现的能力要分别核对。
- 新增业务规则时，记录触发条件、状态变化、数据影响、失败行为及验证位置，避免只记一个函数名。
- 修改协议、表结构、静态 ID、任务持久化规则时，同时更新相应文档。修改内容脚本时，优先维护其已有专门说明。

## 已有资料

- [项目 README](../../README.md)：上游介绍与跨平台构建入口。
- [测试说明](../../test/README.md)：Catch2、CTest 和资源要求。
- [任务框架规则](../../server/script/quest/README.md)：状态重放、状态切换、奖励及回调约束。
- [商人转换规则](../../server/script/npc/include/merchant/README.md)：商人类型、原始资料保真及交易约束。
- [TODO](../../TODO.md)：包括尚未落地的能力调研，不能当成已实现功能清单。

## 维护记录的最小内容

每次新增知识，尽量补齐以下信息：

1. 玩家或开发者看到的行为，以及具体触发条件。
2. 对应的源码路径、类或函数、协议和数据表。
3. 可重复的修改、构建或复现步骤。
4. 已执行的验证及其范围；尚未执行的 GUI 或多人场景验证。
5. 适用的提交或日期；如果推翻旧结论，直接修正文档中的原结论。
