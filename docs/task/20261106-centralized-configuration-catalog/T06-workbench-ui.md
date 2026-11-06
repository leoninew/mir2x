# Task / 子任务 T06：Vue 与桌面管理界面
最后修改时间: 2026-11-06 22:24:01

## Review status / 审查状态

Accepted，方案修订 2。用户已采纳父方案并要求拆分；这是局部方案状态；当前执行登记与证据见总计划及本文补充。

## Parent and authority / 父任务与权威记录

- 总意图：[玩法配置盘点与集中管理](../../intent/20261106-centralized-configuration-catalog.md)。
- 权威总计划：[Plan 修订 2](../../plan/20261106-centralized-configuration-catalog.md)；权威协调根 `D:/SourceCodes/opensource/mir2x`；任务 ID：T06。
- 依赖、分配编号、授权与接受结论以总计划为唯一来源；任务不得自行领取或修改执行登记。

## Goal and acceptance / 目标与验收

- 交付可在桌面中完成三类玩法配置管理的 Vue 视图，含明确校验、差异、保存与冲突反馈。
- 意图归属：A06、A07。完成条件如下文检查及交付要求，方案 Accepted 不等于交付已接受。

## Scope and files / 范围与文件

- 修改 tools/gameplay-workbench/web/src 的业务 domains、services、types、router、必要共享组件及对应 Vitest；维护页面使用说明。
- 仅做怪物经验、按怪物掉落和按地图刷怪三个视图及只读未生效记录；沿用模板建立的 Vue/Reka/Tailwind 风格。
- 消费 T05 正式 API 与目录桥接，不改 Python 契约、原生源码、数据格式或初值；若发现缺口回报主会话。
- 不加入挂机、MCP 对话、运行管理、游戏重启、热更新或新怪物/物品/地图身份功能。

## Inputs / 输入

引用总计划任务定义表：T05 已接受的实际 API、目录绑定桥接、模型和工程基线；包含其已绑定 T01 catalog/元数据与文件样本。

## Technical decisions / 局部技术设计

- 用有类型的 API service 隔离页面请求，保留 workspace/catalog/file revision；目录切换不将旧响应或草稿误写到新目录。
- 表单按实际字段显示单位/约束；整数输入错误显示在具体控件附近，服务端 issues 映射字段，保存前调用正式 validate。
- 经验按 ID 区分重名；掉落显示分组、概率分母、repeat/count 和金币语义，搜索排序仅作用于视图，不改 items 实际顺序。
- 刷怪显示地图、怪物和区域列表，保持同怪多组；cratio 明确只读未消费，不能标成生效概率。
- 显示选中目录、差异和脏状态；409 保留草稿并提供重新读取/人工比对，保存失败不显示成功。成功显示“已保存，下次启动生效”。
- 只读未生效记录可按来源/原因查看。使用 T05 的目录选择桥接；不提供原始 shell 输入或把保存操作变成游戏控制。

## Implementation outline / 实施安排

1. 建立业务导航、目录状态、API service 和数据类型。
2. 实现三个视图的读取、编辑、字段校验与差异预览。
3. 接入保存/冲突/草稿保护和未生效记录。
4. 完成主要业务用例组件检查及桌面临时副本演示，交付最终界面版本。

## Checks and risks / 检查与风险

- 运行 corepack yarn lint:fix、corepack yarn typecheck、模板 format 与 Vitest；构建实际 Vue 静态资源。
- 验证重名选择、数组顺序、字段错误、冲突保留草稿、目录切换及保存反馈；不要仅断言样式类名。
- 在桌面/浏览器检查主要视图和窄窗口可用性，读写临时副本；真实 API 至少完成一次保存再读，不碰运行服。
- 风险：视图排序不能重排游戏有序数据；长表应保证当前对象清楚且错误可定位，不以 toast 代替字段反馈。
- 预期交付：Vue/桌面业务 diff、准确 API 输入版本、lint/typecheck/Vitest/build 结果及临时副本演示记录。
- 实际命令、工作目录和结果在执行时记录，检查只用隔离输入；当前没有运行本任务的构建、测试或产品操作。

## Handoff basis / 交接依据

本任务由主会话直接实施，编号 CONFIG-T06-01；沿用用户全部实现与最终构建验证的授权，实际状态见总计划。下列初始派发模板保留为历史。

- 协议入口：`C:/Users/leon/.codex/plugins/cache/specflow-local/specflow/0.2.1/skills/specflow/SKILL.md` 及其 `references/task-coordination.md`。
- 分配与授权：总计划 Execution registry / Assignment history；当前执行者、分配编号和产品实施授权均未登记。
- 工作区与基线：未分配。实际派发时绑定工作区、源码/差异及已接受前置版本，不把调研 HEAD 当作默认执行版本。
- 可写回报位置：`D:/SourceCodes/opensource/mir2x/docs/task/20261106-centralized-configuration-catalog/T06-workbench-ui.md` 的接收/进展/交付章节；主会话读取该位置并独立审查。
- 回报渠道：执行时输出可复制摘要，默认由用户转交；此次不启动子 agent 或外部会话。

## Reception and progress / 接收与进展

已由主会话接收 CONFIG-T06-01；下列未分配记录为拆分时历史。

| 时间 | 分配编号 / 执行者 | 接收、进展或阻塞 | 证据 / 回报位置 |
| --- | --- | --- | --- |
| 2026-11-06 20:28:00 | 无 / 无 | 建立已接受局部方案，尚未分配实施 | 总计划修订 2 |

## Delivery and review / 交付与审查

初始交付模板（实际交付见文末补充）：

- 交付时间、执行者、有效编号和方案修订。
- 实际变更、范围偏差、逐项验收及原始证据。
- 可访问的实际版本/diff、工作区及前置版本。
- 检查命令、发现与修复、复验、未运行项及原因。
- 未完成项、风险及下游影响。

接受结论仅由主会话写入总计划；没有检查通过及审查证据不得放行下游。

## 当前实施补充

- 2026-11-06 21:49:52；主会话 CONFIG-T06-01；Plan修订2及用户连续实施授权。
- 交付并接受：三类编辑视图、只读身份/未生效数据、字段错误、差异与冲突草稿、过期异步结果隔离。Vue9pass、lint/typecheck/format通过；未声称真实窗口验收，T07最终处理。
- 当前版本为工作区diff；输入基线0d1065c及T01冻结数据，未stage/commit。最终准确证据由T07与verification汇总，不把旧阶段计数相加。

## 最终交付补充

- 2026-11-06 22:24:01，主会话CONFIG-T06-01；当前工作区未提交交付已由主会话审查，已纳入T07最终实现。
- 准确版本：`build/gameplay-baseline/final-manifest.json`；逐任务结果/后继修复/原始日志/制品见[最终验证](../../verification/20261106-centralized-configuration-catalog.md)。原生129、Python64及后继affected11、Vue37分别记录，不拼计。
- 主计划accepted表示源码/自动化/发行交付；窗口目视因UI控制通道不可用保持未验证，不暗示运行服/角色操作通过。未stage/commit/install到运行服。
