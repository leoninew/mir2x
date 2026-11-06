# Task / 子任务 T03：金币与物品掉落接入
最后修改时间: 2026-11-06 22:24:01

## Review status / 审查状态

Accepted，方案修订 2。用户已采纳父方案并要求拆分；这是局部方案状态；当前执行登记与证据见总计划及本文补充。

## Parent and authority / 父任务与权威记录

- 总意图：[玩法配置盘点与集中管理](../../intent/20261106-centralized-configuration-catalog.md)。
- 权威总计划：[Plan 修订 2](../../plan/20261106-centralized-configuration-catalog.md)；权威协调根 `D:/SourceCodes/opensource/mir2x`；任务 ID：T03。
- 依赖、分配编号、授权与接受结论以总计划为唯一来源；任务不得自行领取或修改执行登记。

## Goal and acceptance / 目标与验收

- 使真实金币与物品掉落路径消费已加载配置，并证明初次迁移后的有效表和原有抽样语义保持。
- 意图归属：A03、A05。完成条件如下文检查及交付要求，方案 Accepted 不等于交付已接受。

## Scope and files / 范围与文件

- 修改 server/src/dropitemconfig.cpp 及必要的 dropitemconfig.hpp、原生掉落测试和测试注册。
- 消费 T02 的 monsterDropTable，不改权威初值、Schema 或数据模型；停止从 dropitemconfig.inc 构建运行表。
- 保留旧源表的原始证据和未生效说明，维护 dropgen 的历史输入定位说明；不重建缺失外部输入工作流。
- 不改随机数接口、金币波动、倍率、物品打包/拆分、经验或刷怪。

## Inputs / 输入

引用总计划任务定义表：T02 的完整快照接口、主从初始化和已接受交付版本；继承其中绑定的 T01 有效掉落基线与共用样本。

## Technical decisions / 局部技术设计

- getMonsterDropItemConfigList(id) 保留现有公共返回合同，返回 Server 快照中的已展开分组表，不再以另一个 static 读取旧 .inc。
- getMonsterDropItemList 的抽样、group 非零命中后退出、金币 goldRate/拆分/随机波动和其他物品路径保持；只更换输入表。
- group 的排序遵循原 std::map，组内 vector 遵循原来源顺序和 repeat 展开顺序。GUI 或 JSON object 排序不能介入生产序列。
- 确定性生产例使用 probability=1、已存在物品身份和明确数量，另比较有序有效表，避免以随机抽样次数或不稳定概率阈值证明行为。

## Implementation outline / 实施安排

1. 将生产查表入口改为读取快照，清除旧 .inc 运行时接线。
2. 逐怪物比较基线和新表，包括同组、重复尝试和无掉落情况。
3. 通过真实生产掉落函数检查金币和物品例，完成受影响测试。
4. 交付 diff 与精确结果，不修改表值来消除失败。

## Checks and risks / 检查与风险

- 有效怪物的已展开分组表与 T01 基线逐字段、逐顺序一致。
- 生产函数覆盖组内停止、repeat 及金币数量/拆分的确定性路径；旧倍率与随机波动代码保持。
- 确认源码不再 include 旧掉落源表，未知新引用仍由加载器拒绝，原未生效记录仍可追溯。
- 风险：同一组的行顺序和 repeat 展开影响实际结果；不能用表面总数或重写的抽样函数代替生产路径检查。
- 预期交付：生产接线 diff、有效表对照、实际生产用例及原生构建/CTest 结果、旧生成链维护说明。
- 实际命令、工作目录和结果在执行时记录，检查只用隔离输入；当前没有运行本任务的构建、测试或产品操作。

## Handoff basis / 交接依据

本任务由主会话直接实施，编号 CONFIG-T03-01；沿用用户全部实现与最终构建验证的授权，实际状态见总计划。下列初始派发模板保留为历史。

- 协议入口：`C:/Users/leon/.codex/plugins/cache/specflow-local/specflow/0.2.1/skills/specflow/SKILL.md` 及其 `references/task-coordination.md`。
- 分配与授权：总计划 Execution registry / Assignment history；当前执行者、分配编号和产品实施授权均未登记。
- 工作区与基线：未分配。实际派发时绑定工作区、源码/差异及已接受前置版本，不把调研 HEAD 当作默认执行版本。
- 可写回报位置：`D:/SourceCodes/opensource/mir2x/docs/task/20261106-centralized-configuration-catalog/T03-loot-integration.md` 的接收/进展/交付章节；主会话读取该位置并独立审查。
- 回报渠道：执行时输出可复制摘要，默认由用户转交；此次不启动子 agent 或外部会话。

## Reception and progress / 接收与进展

已由主会话接收 CONFIG-T03-01；下列未分配记录为拆分时历史。

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

- 2026-11-06 21:49:52；主会话 CONFIG-T03-01；Plan修订2及用户连续实施授权。
- 已接受针对性交付；5402有效展开项与冻结生产表相等，实际抽样函数逐字不变；T07补生产repeat/group/gold确定性检查。
- 当前版本为工作区diff；输入基线0d1065c及T01冻结数据，未stage/commit。最终准确证据由T07与verification汇总，不把旧阶段计数相加。

## 最终交付补充

- 2026-11-06 22:24:01，主会话CONFIG-T03-01；当前工作区未提交交付已由主会话审查，已纳入T07最终实现。
- 准确版本：`build/gameplay-baseline/final-manifest.json`；逐任务结果/后继修复/原始日志/制品见[最终验证](../../verification/20261106-centralized-configuration-catalog.md)。原生129、Python64及后继affected11、Vue37分别记录，不拼计。
- 主计划accepted表示源码/自动化/发行交付；窗口目视因UI控制通道不可用保持未验证，不暗示运行服/角色操作通过。未stage/commit/install到运行服。
