# Task / 子任务 T02：原生加载与经验接入
最后修改时间: 2026-11-06 22:24:01

## Review status / 审查状态

Accepted，方案修订 2。用户已采纳父方案并要求拆分；这是局部方案状态；当前执行登记与证据见总计划及本文补充。

## Parent and authority / 父任务与权威记录

- 总意图：[玩法配置盘点与集中管理](../../intent/20261106-centralized-configuration-catalog.md)。
- 权威总计划：[Plan 修订 2](../../plan/20261106-centralized-configuration-catalog.md)；权威协调根 `D:/SourceCodes/opensource/mir2x`；任务 ID：T02。
- 依赖、分配编号、授权与接受结论以总计划为唯一来源；任务不得自行领取或修改执行登记。

## Goal and acceptance / 目标与验收

- 交付可独立校验并构建完整快照的 C++ 配置模块，接通主从启动与怪物基础经验，保持编译期身份和经验算法。
- 意图归属：A03、A04、A07。完成条件如下文检查及交付要求，方案 Accepted 不等于交付已接受。

## Scope and files / 范围与文件

- 新增 server/src/gameplayconfig.{hpp,cpp} 及必要的类型文件；修改 server/src/main.cpp、server.{hpp,cpp}、monster.cpp 和对应原生测试。
- 在 vcpkg.json、cmake/Mir2xDependencies.cmake 引入 nlohmann-json；接线本功能构建，不升级 registry 或改端口/并发参数。
- 修改 common/src/monsterrecord.hpp、monsterrecord.inc 中已迁移经验字段，保持其余字段及顺序；同步 monstercsv2cpp 的输出和说明。
- 实现 tools/gameplayconfig 的永久身份目录导出入口；根据 C++ 记录输出 catalog，不把旧玩法数值继续作为默认来源。
- 不切换生产掉落函数或地图脚本；加载器先完整支持三类契约，供 T03/T04 消费。

## Inputs / 输入

引用总计划任务定义表：T01 的 Schema、初始数据、实际身份目录、基线导出和共用样本均已被主会话接受，派发时记录其准确版本。

## Technical decisions / 局部技术设计

- 固定 API 为 GameplayConfig::load(root)、monsterExp(id)、monsterDropTable(id)、mapSpawns(id)。构造时完成严格 JSON 解析、引用/范围校验，存储为有类型容器，不在游戏每次计算时解析 JSON。
- monsterDropTable 使用现有 std::map<int,std::vector<DropItemConfig>> 形态并完成 repeat 展开；mapSpawns 保留重复组、区域顺序和可选 cratio。未知身份拒绝，有效但未配置掉落/刷怪的身份返回空容器。
- Server::initializeGameplayConfig(root) 初始化一次，gameplayConfig() const 返回只读引用；未初始化禁止消费。配置模块采用独立 library，server_objs 链接该库，避免一份 cpp 同时被递归收集和单独编译。
- main 在日志初始化后、创建 actor 执行者前读 config/gameplay，覆盖 master/slave；失败记录文件/字段并非零退出。当前通用异常末尾 return 0 不能冒充此失败处理，不顺便改其他异常策略。
- 经验仅将 dispatchOffenderExp 中 getMR().exp 的取值换为快照基数，保持归属列表、时间、伤害比例、取整和最低值计算。删除已迁移静态数值，不增加旧值 fallback。
- 身份导出工具只读编译期记录，不启动游戏；catalog 与实际数字身份匹配，重名不合并。

## Implementation outline / 实施安排

1. 接线 JSON 依赖和独立配置库，按 T01 样本落实完整加载/校验/构表。
2. 实现 Server 快照持有和两种启动路径的初始化与失败退出。
3. 接入经验消费并清理旧经验维护入口；生成只读身份目录。
4. 验证实际经验输入、身份稳定、配置缺失/错误及完整快照，再交付接口。

## Checks and risks / 检查与风险

- Catch2/CTest 验证 T01 正反样本、无半份发布、重复初始化拒绝、主从初始化路径和已配置/空领域查询。
- 构建 client/server，比较迁移前后全部身份；核经验生产消费使用新基数，公式和行为未被重写。
- 无效配置的隔离初始化检查必须在 actor/监听前失败并返回非零；不访问用户数据库或运行服。
- 风险：新增库与 server_objs 不能重复收集源文件；公共记录清理影响客户端重编。任何 Schema 变更都先回报主会话，不能单边变更 T01。
- 预期交付：原生模块及接口 diff、准确输入版本、构建/CTest 原始结果、主从/失败退出证据、身份一致性与经验消费检查。
- 实际命令、工作目录和结果在执行时记录，检查只用隔离输入；当前没有运行本任务的构建、测试或产品操作。

## Handoff basis / 交接依据

本任务由主会话直接实施，编号 CONFIG-T02-01；沿用用户全部实现与最终构建验证的授权，实际状态见总计划。下列初始派发模板保留为历史。

- 协议入口：`C:/Users/leon/.codex/plugins/cache/specflow-local/specflow/0.2.1/skills/specflow/SKILL.md` 及其 `references/task-coordination.md`。
- 分配与授权：总计划 Execution registry / Assignment history；当前执行者、分配编号和产品实施授权均未登记。
- 工作区与基线：未分配。实际派发时绑定工作区、源码/差异及已接受前置版本，不把调研 HEAD 当作默认执行版本。
- 可写回报位置：`D:/SourceCodes/opensource/mir2x/docs/task/20261106-centralized-configuration-catalog/T02-native-loading-experience.md` 的接收/进展/交付章节；主会话读取该位置并独立审查。
- 回报渠道：执行时输出可复制摘要，默认由用户转交；此次不启动子 agent 或外部会话。

## Reception and progress / 接收与进展

主会话于 2026-11-06 20:59:47 接收 CONFIG-T02-01；使用当前工作区、T01已接受初始数据与冻结baseline。只构建必要测试探针，不在中间构建发行制品。

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

- 2026-11-06 21:49:52；主会话 CONFIG-T02-01；Plan修订2及用户连续实施授权。
- 已接受针对性交付；原生完整快照/主从启动/经验接线，3case/386assertions通过；成套build及Server初始化边界最终归T07。
- 当前版本为工作区diff；输入基线0d1065c及T01冻结数据，未stage/commit。最终准确证据由T07与verification汇总，不把旧阶段计数相加。

## 最终交付补充

- 2026-11-06 22:24:01，主会话CONFIG-T02-01；当前工作区未提交交付已由主会话审查，已纳入T07最终实现。
- 准确版本：`build/gameplay-baseline/final-manifest.json`；逐任务结果/后继修复/原始日志/制品见[最终验证](../../verification/20261106-centralized-configuration-catalog.md)。原生129、Python64及后继affected11、Vue37分别记录，不拼计。
- 主计划accepted表示源码/自动化/发行交付；窗口目视因UI控制通道不可用保持未验证，不暗示运行服/角色操作通过。未stage/commit/install到运行服。
