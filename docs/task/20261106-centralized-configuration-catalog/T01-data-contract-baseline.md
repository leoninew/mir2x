# Task / 子任务 T01：数据契约与迁移基线
最后修改时间: 2026-11-06 22:24:01

## Review status / 审查状态

Accepted，方案修订 2。用户已采纳父方案并要求拆分；这是局部方案状态，主会话已接收 CONFIG-T01-01 实施，无产品交付。

## Parent and authority / 父任务与权威记录

- 总意图：[玩法配置盘点与集中管理](../../intent/20261106-centralized-configuration-catalog.md)。
- 权威总计划：[Plan 修订 2](../../plan/20261106-centralized-configuration-catalog.md)；权威协调根 `D:/SourceCodes/opensource/mir2x`；任务 ID：T01。
- 依赖、分配编号、授权与接受结论以总计划为唯一来源；任务不得自行领取或修改执行登记。

## Goal and acceptance / 目标与验收

- 交付完整的版本化数据契约、实际身份目录、初始 JSON 和逐项迁移对账，为两条实现链提供同一份可核对输入；本任务不切换游戏消费。
- 意图归属：A01-A05、A07。完成条件如下文检查及交付要求，方案 Accepted 不等于交付已接受。

## Scope and files / 范围与文件

- 新增 server/config/gameplay/ 下的 schema、manifest、monsters、drops、spawns、catalog 与 migration-notes；维护数据字段说明和迁移核对记录。
- 迁移用脚本/一次性隔离导出源码归入 tools/gameplayconfig 的 migration 子目录；临时构建文件和原始运行证据不作为产品输入。
- 只读取现有 common 记录、生产掉落构表和地图 Lua；不删除旧 .exp/.inc/字面量，不改算法或运行路径。永久原生导出工具及 nlohmann 依赖接线由 T02 负责。
- 初始权威数据只由本任务写入；后续消费者不得为让测试通过自行改这些数值。相邻玩法字段只留候选说明。

## Inputs / 输入

无前置任务。使用实际派发时冻结的原始记录、dropitemconfig.cpp 和地图脚本版本，保留已有文档/工作区改动；词法统计仅供定位，不是有效身份和行为证明。

## Technical decisions / 局部技术设计

- 严格采用父计划“跨任务固定接口”的六种文档形状。schema_version 为 1，schema 文件命名为 manifest/monsters/drops/spawns/catalog/migration-notes.schema.json，位于 server/config/gameplay/schema/。
- 一次性 C++ 导出在隔离目录链接当前 common/生产掉落构表，取得真实 DBCOM ID、默认经验和过滤后的有序结构；Python 标准库用于规范化结果。临时导出程序不启动窗口、游戏监听、actor、角色或用户数据库。
- 经验按每个有效 monster_id 导出，包括未显式写 exp 的默认 0；同名“未知”记录分别保存，比较所有 ID/名称和记录顺序。
- 掉落同时保存原始行和旧函数实际有效结果；386 行名称未匹配只是待核线索，只有真实解析支持后才归入 migration-notes。raw 保留原字段和来源，不猜名字或启用条目。
- 刷怪用受限字面量解析，按原组和区域顺序输出；验证 251 份脚本覆盖以及重复组，不执行脚本取得配置。六个疑似无效组另核真实名称解析；cratio 原值保留且标注未消费。
- 共用契约样本放在 server/test/data/gameplay/contract/：覆盖正常核心结构，以及类型/范围、重复 ID、未知引用、重复对象键、文件索引缺项等真实失败边界。样本与 expected-results.json 被 T02/T05 共同使用，新增失败类型先同步两端。

## Implementation outline / 实施安排

1. 记录输入版本与散列，生成源声明清单和可核对的原始导出。
2. 通过真实身份/生产构表验证有效部分，逐条记录未生效原因；发现与预研不同的结果先查清。
3. 按固定结构输出初始文件、Schema、catalog、migration-notes 和共享样本。
4. 对原始/有效/未生效条目计数及字段、ID、顺序进行对账，提交数据交付记录。

## Checks and risks / 检查与风险

- 运行迁移脚本的只读校验/重新导出比较，确认相同输入得到相同输出；实际生产有效表与初始 JSON 可逐条还原。
- Schema 校验、引用检查、经验全覆盖与默认值、重复组/顺序检查；有效数与未生效数之和对应全部来源。
- 确认没有玩法值调优、身份重排和产品消费切换；不能将纯正则匹配当作最终通过依据。
- 风险：名称差异必须经过真实解析；无法确认的条目不擅自归档。基线/Schema 未接受前，T02 与 T05 都不能开工。
- 预期交付：初始数据、Schema 与共用样本的实际 diff/散列；原始源码版本、实际导出命令、有效/未生效核对结果和全部差异说明。
- 实际命令、工作目录和结果在执行时记录，检查只用隔离输入；当前没有运行本任务的构建、测试或产品操作。

## Handoff basis / 交接依据

本任务由主会话直接实施，编号 CONFIG-T01-01；沿用用户全部实现与最终构建验证的授权，实际状态见总计划。下列初始派发模板保留为历史。

- 协议入口：`C:/Users/leon/.codex/plugins/cache/specflow-local/specflow/0.2.1/skills/specflow/SKILL.md` 及其 `references/task-coordination.md`。
- 分配与授权：总计划 Execution registry / Assignment history；当前执行者、分配编号和产品实施授权均未登记。
- 工作区与基线：未分配。实际派发时绑定工作区、源码/差异及已接受前置版本，不把调研 HEAD 当作默认执行版本。
- 可写回报位置：`D:/SourceCodes/opensource/mir2x/docs/task/20261106-centralized-configuration-catalog/T01-data-contract-baseline.md` 的接收/进展/交付章节；主会话读取该位置并独立审查。
- 回报渠道：执行时输出可复制摘要，默认由用户转交；此次不启动子 agent 或外部会话。

## Reception and progress / 接收与进展

已接收 CONFIG-T01-01；执行者为主会话，2026-11-06 20:51:12 核对已接受方案、当前 HEAD 与既有工作区文档。原生迁移探针为针对性测试，制品构建留到全功能完成。

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

### 实际交付 2026-11-06 20:59:47

- CONFIG-T01-01 / 主会话，方案修订2；输入 HEAD 97d7fd0 与 baseline.json 的逐源hash。
- 已生成六种Schema、初始JSON、只读身份目录、392条未生效来源和14个共用契约样本。数值、身份和顺序保持，未切换生产消费。
- 原生产构表隔离探针编译/运行成功，5402项逐项还原；命令和原输出位于 build/gameplay-baseline。
- 针对性检查 `python tools/gameplayconfig/migration/test_baseline.py`，3通过。未启动游戏/角色/数据库，未构建发行制品。
- 可核对交付：当前工作区 server/config/gameplay、server/test/data/gameplay/contract、tools/gameplayconfig/migration；后续原生与Python共用同一契约，尚未完成联合原生/Schema验证。
- 主会话已按以上实际diff与对账接受，允许T02使用；后续消费不得修改初值。

## 当前实施补充

- 2026-11-06 21:49:52；主会话 CONFIG-T01-01；Plan修订2及用户连续实施授权。
- 已接受；一次性迁移基线和全部数据/Schema/共用契约样本已交付，原生产探针与对账见总计划。
- 当前版本为工作区diff；输入基线0d1065c及T01冻结数据，未stage/commit。最终准确证据由T07与verification汇总，不把旧阶段计数相加。

## 最终交付补充

- 2026-11-06 22:24:01，主会话CONFIG-T01-01；当前工作区未提交交付已由主会话审查，已纳入T07最终实现。
- 准确版本：`build/gameplay-baseline/final-manifest.json`；逐任务结果/后继修复/原始日志/制品见[最终验证](../../verification/20261106-centralized-configuration-catalog.md)。原生129、Python64及后继affected11、Vue37分别记录，不拼计。
- 主计划accepted表示源码/自动化/发行交付；窗口目视因UI控制通道不可用保持未验证，不暗示运行服/角色操作通过。未stage/commit/install到运行服。
