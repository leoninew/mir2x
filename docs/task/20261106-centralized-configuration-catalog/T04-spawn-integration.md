# Task / 子任务 T04：地图刷怪数据接入
最后修改时间: 2026-11-06 22:24:01

## Review status / 审查状态

Accepted，方案修订 2。用户已采纳父方案并要求拆分；这是局部方案状态；当前执行登记与证据见总计划及本文补充。

## Parent and authority / 父任务与权威记录

- 总意图：[玩法配置盘点与集中管理](../../intent/20261106-centralized-configuration-catalog.md)。
- 权威总计划：[Plan 修订 2](../../plan/20261106-centralized-configuration-catalog.md)；权威协调根 `D:/SourceCodes/opensource/mir2x`；任务 ID：T04。
- 依赖、分配编号、授权与接受结论以总计划为唯一来源；任务不得自行领取或修改执行登记。

## Goal and acceptance / 目标与验收

- 将地图刷怪字面量参数切换为运行快照的独立 Lua table，保持全部地图原有逻辑、区域语义和实例隔离。
- 意图归属：A01、A03、A05。完成条件如下文检查及交付要求，方案 Accepted 不等于交付已接受。

## Scope and files / 范围与文件

- 修改 server/src/servermap.cpp 的 LuaThreadRunner 绑定，必要的局部类型接线及原生/Lua 测试。
- 对 T01 确认的 251 份地图脚本只替换 addmon.monGener 的数据实参；helper/default 和无该调用的两份文件保持。
- 不改 server/script/map/addmonster.lua 算法、地图守卫放置、main、协程、其他脚本和配置初值。
- 同步旧 monstergen 的历史维护说明，不让其输出被当作当前权威数据。

## Inputs / 输入

引用总计划任务定义表：T03 已接受原生基线，其中包含 T02 快照与 mapSpawns；使用已绑定 T01 地图数据、原表和未生效记录。

## Technical decisions / 局部技术设计

- getMapSpawnConfig() 无前端任意路径参数，以当前地图 ID 查询；每次创建新 Lua table，将 monster_id 对应到现有名称，恢复 name/loc 及原字段。
- 保持同一怪物的多个组和重复区域，不按名称合并。cratio 仅保留传值和只读说明，不让它参与生成计算。
- 每个地图实例持有自己的可变输入表，uidList、lastUpdateTime、gridCount 和原过滤只作用于该副本，不污染配置快照和其他地图。
- 加载使用启动时快照；运行中保存文件不会使后加载地图混用新磁盘值。原 count=0、有效格密度和时间判断语义保持。

## Implementation outline / 实施安排

1. 增加当前地图专用取表绑定，并测试返回结构和副本隔离。
2. 依据精确源片段批量替换纯数据实参，逐文件检查其余代码字节。
3. 将每份新 table 与旧有效数据逐组逐区域对比。
4. 隔离运行原 monGener，核周期/数量与两个实例状态，交付源差异和覆盖清单。

## Checks and risks / 检查与风险

- 每份脚本的守卫、main 和协程不变；调用次数和全部迁移表与 T01 对账。
- 真实原 monGener 在受控时间/地图函数下运行；核周期、目标数限制、重复组和实例间状态不共享。
- 确认持久配置不含 uidList/lastUpdateTime/gridCount 等运行态；未生效说明及 24 个 cratio 可追溯。
- 风险：Lua 会修改/删改输入表，浅拷贝不足；不能用 JSON 深拷贝结果或自写计数公式替代实际 Lua 消费测试。
- 预期交付：251 份文件的变更/未变片段核对清单、取表绑定 diff、表对照、Lua/原生检查原始结果。
- 实际命令、工作目录和结果在执行时记录，检查只用隔离输入；当前没有运行本任务的构建、测试或产品操作。

## Handoff basis / 交接依据

本任务由主会话直接实施，编号 CONFIG-T04-01；沿用用户全部实现与最终构建验证的授权，实际状态见总计划。下列初始派发模板保留为历史。

- 协议入口：`C:/Users/leon/.codex/plugins/cache/specflow-local/specflow/0.2.1/skills/specflow/SKILL.md` 及其 `references/task-coordination.md`。
- 分配与授权：总计划 Execution registry / Assignment history；当前执行者、分配编号和产品实施授权均未登记。
- 工作区与基线：未分配。实际派发时绑定工作区、源码/差异及已接受前置版本，不把调研 HEAD 当作默认执行版本。
- 可写回报位置：`D:/SourceCodes/opensource/mir2x/docs/task/20261106-centralized-configuration-catalog/T04-spawn-integration.md` 的接收/进展/交付章节；主会话读取该位置并独立审查。
- 回报渠道：执行时输出可复制摘要，默认由用户转交；此次不启动子 agent 或外部会话。

## Reception and progress / 接收与进展

已由主会话接收 CONFIG-T04-01；下列未分配记录为拆分时历史。

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

- 2026-11-06 21:49:52；主会话 CONFIG-T04-01；Plan修订2及用户连续实施授权。
- 已接受针对性交付；251脚本只换数据实参，真实Lua定向2case/34308assertions与迁移3项通过，addmonster.lua未改。
- 当前版本为工作区diff；输入基线0d1065c及T01冻结数据，未stage/commit。最终准确证据由T07与verification汇总，不把旧阶段计数相加。

## 最终交付补充

- 2026-11-06 22:24:01，主会话CONFIG-T04-01；当前工作区未提交交付已由主会话审查，已纳入T07最终实现。
- 准确版本：`build/gameplay-baseline/final-manifest.json`；逐任务结果/后继修复/原始日志/制品见[最终验证](../../verification/20261106-centralized-configuration-catalog.md)。原生129、Python64及后继affected11、Vue37分别记录，不拼计。
- 主计划accepted表示源码/自动化/发行交付；窗口目视因UI控制通道不可用保持未验证，不暗示运行服/角色操作通过。未stage/commit/install到运行服。
