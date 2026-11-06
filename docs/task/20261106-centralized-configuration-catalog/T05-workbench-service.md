# Task / 子任务 T05：Python 配置管理服务
最后修改时间: 2026-11-06 22:24:01

## Review status / 审查状态

Accepted，方案修订 2。用户已采纳父方案并要求拆分；这是局部方案状态；当前执行登记与证据见总计划及本文补充。

## Parent and authority / 父任务与权威记录

- 总意图：[玩法配置盘点与集中管理](../../intent/20261106-centralized-configuration-catalog.md)。
- 权威总计划：[Plan 修订 2](../../plan/20261106-centralized-configuration-catalog.md)；权威协调根 `D:/SourceCodes/opensource/mir2x`；任务 ID：T05。
- 依赖、分配编号、授权与接受结论以总计划为唯一来源；任务不得自行领取或修改执行登记。

## Goal and acceptance / 目标与验收

- 交付基于模板的可测试管理应用工程和实际配置读取、校验、预览、保存 API，独立于游戏进程运行。
- 意图归属：A06、A07。完成条件如下文检查及交付要求，方案 Accepted 不等于交付已接受。

## Scope and files / 范围与文件

- 引入模板必要源码、工程配置与锁文件到 tools/gameplay-workbench，统一为 mir2x_workbench；不写模板仓库，不复制 .venv/node_modules/build/dist/logs 或外部开发环境。
- 拥有 Python 应用层、models、workspace/config service、routers、desktop 目录选择桥接、Python 测试和 API/开发说明。
- 前端只引入模板原始骨架供后续使用，不实现业务页面；不修改原生源码、T01 权威数据和 Schema。
- 保留模板自身 YAML/Dynaconf，玩法使用 JSON + 严格 Pydantic；配置依赖与游戏运行进程分开。

## Inputs / 输入

引用总计划任务定义表：T01 的结构、Schema、catalog、初始文件和共享样本已接受；使用既定模板实际文件和父计划固定 API，不等待 T02-T04 产品实现。

## Technical decisions / 局部技术设计

- WorkspaceService 绑定用户选择的 source/server 或 installed/server 目录；DesktopApi 提供窄目录选择方法并与 FastAPI 共用该 service。浏览器开发/测试通过应用设置绑定隔离根。
- 固定接口沿父计划 workspace/catalog/config/validate/save 合同；workspace_id、catalog_revision、SHA-256 文件 revision 防止跨目录或旧版本保存。未选择目录时显示明确状态，不扫描私人路径。
- 读取时重新读文件；拒绝重复对象键、NaN、错误数值类型和字段/引用错误。Pydantic 不进行 bool/string 到整数的宽松转换。
- validate 返回带字段路径的业务 issues；PUT 完整复核 data、catalog 和读取版本，错误沿模板 code/error/requestId；目录切换/版本冲突为 409，非法数据为 422。
- 服务内部串行保存，先留备份、同目录临时写再 replace；单文件成功才更新 revision。冲突不覆盖原文件；备份/日志不进入权威配置索引。任意外部写者之间不承诺通用 CAS。
- 按固定 domain/数字 ID 映射文件；说明和未生效记录只读，绝不将它们作为可启用条目或暴露通用 shell/任意文件写。

## Implementation outline / 实施安排

1. 引入并重命名模板工程，保持前后端/桌面启动和现有错误契约。
2. 实现模型、域校验、共享样本检查及目录绑定。
3. 实现实际读/校验/保存 API、版本检查和备份。
4. 用 HTTP 集成检查可访问的临时配置副本，交付给界面任务。

## Checks and risks / 检查与风险

- 沿模板运行 Python 类型、lint、格式与 pytest；验证共享合法/非法样本与 Schema 约束。
- 真实 API 读→编辑→validate→save→read，再检查实际磁盘字节和数组顺序；覆盖外部修改、workspace 切换、catalog 变化、写失败原文件保留。
- 确认没有业务读写经 shell 执行，GET 不使用旧启动缓存，未生效记录只读；不连接游戏或读取角色数据库。
- 风险：模板本身不是配置编辑器；不能仅通过 mock API 宣称文件保存已实现。模板锁文件在实际重命名/依赖变更后按工具更新，不手工伪造。
- 预期交付：管理工程及 Python 服务 diff、固定 API/目录选择说明、实际 HTTP 与磁盘检查、准确依赖及测试输入版本。
- 实际命令、工作目录和结果在执行时记录，检查只用隔离输入；当前没有运行本任务的构建、测试或产品操作。

## Handoff basis / 交接依据

本任务由主会话直接实施，编号 CONFIG-T05-01；沿用用户全部实现与最终构建验证的授权，实际状态见总计划。下列初始派发模板保留为历史。

- 协议入口：`C:/Users/leon/.codex/plugins/cache/specflow-local/specflow/0.2.1/skills/specflow/SKILL.md` 及其 `references/task-coordination.md`。
- 分配与授权：总计划 Execution registry / Assignment history；当前执行者、分配编号和产品实施授权均未登记。
- 工作区与基线：未分配。实际派发时绑定工作区、源码/差异及已接受前置版本，不把调研 HEAD 当作默认执行版本。
- 可写回报位置：`D:/SourceCodes/opensource/mir2x/docs/task/20261106-centralized-configuration-catalog/T05-workbench-service.md` 的接收/进展/交付章节；主会话读取该位置并独立审查。
- 回报渠道：执行时输出可复制摘要，默认由用户转交；此次不启动子 agent 或外部会话。

## Reception and progress / 接收与进展

已由主会话接收 CONFIG-T05-01；下列未分配记录为拆分时历史。

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

- 2026-11-06 21:49:52；主会话 CONFIG-T05-01；Plan修订2及用户连续实施授权。
- 交付并接受：独立WorkspaceService/GameplayConfigService、严格模型和API、目录选择、备份/原子替换/外部冲突。针对性20pass，Mypy27文件及Ruff通过；项目/包/入口/uv.lock完成命名，依赖已锁定实际同步。最终wheel/EXE及与native的闭环留T07。
- 当前版本为工作区diff；输入基线0d1065c及T01冻结数据，未stage/commit。最终准确证据由T07与verification汇总，不把旧阶段计数相加。

## 最终交付补充

- 2026-11-06 22:24:01，主会话CONFIG-T05-01；当前工作区未提交交付已由主会话审查，已纳入T07最终实现。
- 准确版本：`build/gameplay-baseline/final-manifest.json`；逐任务结果/后继修复/原始日志/制品见[最终验证](../../verification/20261106-centralized-configuration-catalog.md)。原生129、Python64及后继affected11、Vue37分别记录，不拼计。
- 主计划accepted表示源码/自动化/发行交付；窗口目视因UI控制通道不可用保持未验证，不暗示运行服/角色操作通过。未stage/commit/install到运行服。
