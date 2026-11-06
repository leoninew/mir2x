# Task / 子任务 T07：联合集成与发行检查
最后修改时间: 2026-11-06 22:24:01

## Review status / 审查状态

Accepted，方案修订 2。用户已采纳父方案并要求拆分；这是局部方案状态；当前执行登记与证据见总计划及本文补充。

## Parent and authority / 父任务与权威记录

- 总意图：[玩法配置盘点与集中管理](../../intent/20261106-centralized-configuration-catalog.md)。
- 权威总计划：[Plan 修订 2](../../plan/20261106-centralized-configuration-catalog.md)；权威协调根 `D:/SourceCodes/opensource/mir2x`；任务 ID：T07。
- 依赖、分配编号、授权与接受结论以总计划为唯一来源；任务不得自行领取或修改执行登记。

## Goal and acceptance / 目标与验收

- 合并两条已接受实现链，交付成套配置/管理应用并验证 GUI 保存到原生读取的完整闭环。
- 意图归属：A01-A08。完成条件如下文检查及交付要求，方案 Accepted 不等于交付已接受。

## Scope and files / 范围与文件

- 唯一处理最终共享集成、server 数据安装、只读 catalog 产物、管理应用打包资源及使用文档；必要修复限定在已接受方案内。
- 修改 server/src/CMakeLists.txt 等安装接线、tools/gameplay-workbench 的打包与资源配置及联合检查；不增加网络协议、运行参数或新的玩法范围。
- 完成 docs/verification/20261106-centralized-configuration-catalog.md 的整体结果；不将历史阶段数字相加冒充当前版本结果。
- 无默认 Git 提交、推送、运行服安装或角色操作；本地临时发行/安装目录属于受控检查。

## Inputs / 输入

引用总计划任务定义表：T04 与 T06 交付均经主会话接受，实际代码/差异和准确产物可访问；它们分别包含 T01-T03 与 T05 的绑定版本。不能读取任意未审工作字节代替这些输入。

## Technical decisions / 局部技术设计

- 将两链合到一个可核对版本，先看累积 diff 与单链差异；共享契约或 Schema 变化先修订计划并复核下游，不在集成时改数据取舍。
- 新安装交付完整 gameplay 布局；已存在目录整体保护，不覆盖用户值、不静默补混其他版本文件。不完整目录报告明确处理要求。
- 原生 catalog 导出结果需与本版本编译期记录相符，管理应用从所选 server 根读取同份数据；目录包含静态前端、必要 Python 资源和既有发行入口。
- 使用临时配置副本：正式管理服务/界面保存选定经验、掉落或刷怪值，再由原生生产加载模块读取同目录，核对实际输入值及语义，GUI 退出不影响读取。
- 保持原 ID、公式、掉落顺序和 Lua 实例状态；测试覆盖源数据与准确发行产物，失败修复后记录后继版本和受影响检查。

## Implementation outline / 实施安排

1. 核前置版本、集成源码和完整差异，处理共享文件并形成准确候选。
2. 完成数据安装和已有目录保护，构建 client/server 与管理应用目录包。
3. 运行相关原生、Python、Vue 检查及跨语言共用样本核对。
4. 完成正式保存→原生加载闭环和发行目录检查，记录剩余风险与未运行项，提交整体审查。

## Checks and risks / 检查与风险

- 按影响范围运行 native check 和模板 make check/test/binary，原始结果绑定准确源码/依赖/产物。
- 验证新安装完整、旧目录调整值保留；检查打包后的前端和配置读取资源，不只看开发环境通过。
- 对核心三个领域完成正式保存/原生加载，并复核 ID、有效表、刷怪纯数据和未生效记录。
- 整体对照 A01-A08 与各任务验收；有失败或未验证必须显式列出，不能靠重跑碰绿或旧包结果替代。
- 风险：共享集成版本必须准确，不能覆盖用户配置或混用旧数据；本地隔离成功不等于运行服/角色验收。需要扩展范围时回到主会话裁决。
- 预期交付：准确集成 diff/版本、发行目录与清单、安装保护及跨语言闭环证据、整体 verification 文档和未完成项。
- 实际命令、工作目录和结果在执行时记录，检查只用隔离输入；当前没有运行本任务的构建、测试或产品操作。

## Handoff basis / 交接依据

本任务由主会话直接实施，编号 CONFIG-T07-01；沿用用户全部实现与最终构建验证的授权，实际状态见总计划。下列初始派发模板保留为历史。

- 协议入口：`C:/Users/leon/.codex/plugins/cache/specflow-local/specflow/0.2.1/skills/specflow/SKILL.md` 及其 `references/task-coordination.md`。
- 分配与授权：总计划 Execution registry / Assignment history；当前执行者、分配编号和产品实施授权均未登记。
- 工作区与基线：未分配。实际派发时绑定工作区、源码/差异及已接受前置版本，不把调研 HEAD 当作默认执行版本。
- 可写回报位置：`D:/SourceCodes/opensource/mir2x/docs/task/20261106-centralized-configuration-catalog/T07-integration-release.md` 的接收/进展/交付章节；主会话读取该位置并独立审查。
- 回报渠道：执行时输出可复制摘要，默认由用户转交；此次不启动子 agent 或外部会话。

## Reception and progress / 接收与进展

已由主会话接收 CONFIG-T07-01；下列未分配记录为拆分时历史。

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

- 2026-11-06 21:49:52；主会话 CONFIG-T07-01；Plan修订2及用户连续实施授权。
- 已接收running：新增gameplay-config校验/导出/inspect工具、整目录安装保护、生产drop及API→native/实际install组件检查。功能实现已收口，尚未完成最终构建/发行验收。
- 当前版本为工作区diff；输入基线0d1065c及T01冻结数据，未stage/commit。最终准确证据由T07与verification汇总，不把旧阶段计数相加。

## 最终交付补充

- 2026-11-06 22:24:01，主会话CONFIG-T07-01；当前工作区未提交交付已由主会话审查，已纳入T07最终实现。
- 准确版本：`build/gameplay-baseline/final-manifest.json`；逐任务结果/后继修复/原始日志/制品见[最终验证](../../verification/20261106-centralized-configuration-catalog.md)。原生129、Python64及后继affected11、Vue37分别记录，不拼计。
- 主计划accepted表示源码/自动化/发行交付；窗口目视因UI控制通道不可用保持未验证，不暗示运行服/角色操作通过。未stage/commit/install到运行服。
