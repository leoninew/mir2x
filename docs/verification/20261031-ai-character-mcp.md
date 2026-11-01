# 验证：通过 Python MCP 操作当前游戏角色
最后修改时间: 2026-11-01 21:19:34

## 审查状态与勾选规则

Draft。本文汇总已完成的实现与开发检查，并登记整体待验证事项；**整体 Verification 尚未开始，首版尚未整体验收通过**。T07 整体仍未启动；用户后续授权的运行管理 CLI/skill/便携制品及隔离开发检查另见[运行管理记录](20261101-runtime-launcher.md)，不会将其替代角色与钱物验收。

- `[x]` 表示该条描述的工作已完成且有对应证据；“代码接入”“自动检查通过”“真实角色验证通过”分别记录。
- `[ ]` 表示仍待完成或未取得所要求的证据，不能由其它层次的通过结果替代。
- [总计划](../plan/20261031-ai-character-mcp.md)是分配和接受状态的权威记录；task 顶部的 Accepted 只表示方案已接受。
- `.runtime/collaboration/` 的 manifest、报告快照和日志保留交付时版本。旧批次测试、旧二进制和延迟回放消息不改变当前状态；后续修改文档不回写冻结 manifest。

## 版本与证据边界

### 已接受的交付快照

下表记录 T05 delivery2 接受时的版本。用户随后已重新编译，当前 build/install EXE 见下一节；不再把表内旧哈希当作当前文件哈希。

| 项目 | 当前记录 |
| --- | --- |
| 方案 / 当前阶段 | 修订 3 / Implementation 阶段性收尾 |
| 源码基线 | `b1ddda8d8742afb1d60839f759733a2e8ee26207` + 当前未提交功能工作区 |
| 最新局部交付 | `MCP-T05-001/r3/delivery2`，替代被退回的 delivery1 |
| 主审 | `.runtime/collaboration/T05-stage-main-review.json`，2026-11-01 18:25:55，`accepted_local_implementation` |
| 交付时共同构建标识 | `VENGINEERING-20261101174437` |
| 客户端交付路径 | `build/client/src/client.exe`（当前已由用户重编替换） |
| 客户端交付 SHA256 | `3386b48d70f75232e3a2f2b62635a185a91fc3e12facee5c94ee6e707bb57dad` |
| 服务端交付路径 | `build/server/src/server.exe`（当前已由用户重编替换） |
| 服务端交付 SHA256 | `e78d9ddb999a1d06d457537b40d921b3a90a14071f12490916e16eed4e672dac` |
| 最新 manifest | `.runtime/collaboration/T05-delivery-manifest.json` |
| manifest SHA256 | `3adfccb7e84a3b58b17a886081a8c96135530486b36cb078883b143c48568335` |
| 平台 | Windows / MSYS2 UCRT64 / GCC 16.2 / Release |
| Python 开发检查 | Windows Python 3.13.11、uv 0.11.16、官方 mcp 2.3.0；依赖以 `uv.lock` 为准 |

- [x] 本次文档更新前只读重算 T05 manifest：41 项源码、17 项产物、32 项证据全部零差异；HEAD 与上述基线一致。
- [x] 读取最终构建、CTest、无待构建、Python pytest 和 ruff 日志，核对以下结果；本次没有重新执行这些检查。

### 用户重新编译后的产物

用户已确认进行了编译。下列信息由本次只读读取文件确认，没有重新运行构建、安装或 EXE；未收集用户此次构建命令的原始输出，也未取得运行效果或新产物复测结果。

| 项目 | 当前文件记录 |
| --- | --- |
| 生成配置标识 | `build/config_file/buildconfig.hpp`：`VENGINEERING-20261101192035`；客户端 EXE 也含此标识 |
| 客户端路径 | `build/client/src/client.exe` 与 `install/client/client.exe`，两者 SHA256 相同 |
| 客户端 SHA256 / 修改时间 | `8363832274265463e1a2226264e28b18c1eead4da6477cfdb08b1a1c4ec60320` / 2026-11-01 19:20:48 |
| 服务端路径 | `build/server/src/server.exe` 与 `install/server/server.exe`，两者 SHA256 相同 |
| 服务端 SHA256 / 修改时间 | `f7c390854c9412a81466016c46254b9b7b5232bb52157e980f175ff23189585b` / 2026-11-01 19:21:05 |
| 对原 T05 manifest 的核对 | 41 项源码与 32 项证据仍一致；17 项产物中 15 项一致、2 项为用户重编的 client/server |

- [x] 用户编译完成反馈已记录，新 build/install 两端 EXE 对应关系及哈希已核对；此前暂停核实的两项变化来源现已明确。
- [ ] 补充此次构建/安装命令输出、实际资源与运行路径、client/server 版本握手和查看效果记录。
- [ ] 为新二进制取得适当复验及最终 35 能力 SDK/宿主、真实角色证据；原 166 项测试和 no-work 仍仅记录交付时检查，不改标为此次产物已复测。

原 manifest、主审 JSON 和测试日志保持冻结，新产物不会反向覆盖原交付接受记录。`build/` 成功通常不代表 `install/` 自动更新；此次已通过文件哈希确认两个安装 EXE 与 build 对应，资源是否同源仍待运行核对。

## 子任务交付与集成

| 已接受实现 | 当前交付 / 记录 | 已有开发证据 | 仍待整体确认 |
| --- | --- | --- | --- |
| [x] T01 | [契约与传输](../task/20261031-ai-character-mcp/T01-contract-transport.md)；`T01-delivery-manifest.json` | 协议 13 + 安全 11；真实 loopback、Windows ACL/AccessCheck/junction | 真实使用条件与 POSIX |
| [x] T08 | [响应回调](../task/20261031-ai-character-mcp/T08-response-lifecycle.md) | 9 项回调测试，作用域、期限、迟到和重入 | 实际进程切换/退出时序 |
| [x] T02 | [控制生命周期](../task/20261031-ai-character-mcp/T02-control-lifecycle.md)；`MCP-T02-001/r3/delivery1` | 最终 74 项回归；当时二进制真实只读 stdio 冒烟 | 真实 RUN 面板、DPI、人工接管与动画 |
| [x] T03 | [观察与移动](../task/20261031-ai-character-mcp/T03-observe-move.md)；`MCP-T03-001/r3/delivery1` | 93 项回归；当时二进制 9 能力只读 stdio 冒烟 | 真实快照、移动、停止与地图生命周期 |
| [x] T04 | [NPC 与动作](../task/20261031-ai-character-mcp/T04-interaction-actions.md)；`MCP-T04-001/r3/delivery1` | 116 项回归；当时最终二进制 19 能力真实只读 stdio 冒烟 | NPC/拾取/战斗/消费实际正常与失败路径 |
| [x] T05 | [经济动作](../task/20261031-ai-character-mcp/T05-economy.md)；`MCP-T05-001/r3/delivery2` | 最终 166 项回归；41 源码/17 产物/32 证据一致 | 最终 35 能力 stdio、真实钱物变化、双角色 |
| [x] T06 | [Python MCP](../task/20261031-ai-character-mcp/T06-python-mcp.md)；`MCP-T06-001` | 76 项、ruff、官方 SDK 子进程 stdio；游戏端为 fake | 最终真实 C++ 和实际宿主联动 |
| [ ] T07 | [集成、文档与开发证据](../task/20261031-ai-character-mcp/T07-integration-docs.md) | 尚未分配；本次仅补文档和清单 | 按下文逐项收集实机证据，再交付审查 |

历史 manifest 仅约束其交付快照。后续已登记任务改过的共享 CMake、runtime、GUI 等文件不能拿旧 manifest 直接判为回归。T05 manifest 的 `pending_main_review` 是交付时状态，当前接受结论见总计划及上述主审 JSON。

## 已接入的完整能力清单

以下勾选仅证明当前源码具备实际注册和对应适配入口。后续运行管理明确版本已重新发现 35 项，见本文最新增量节；动作仍受人工授权、策略、上下文及游戏状态限制。

- [x] `game_get_session`：实例、阶段、控制状态、能力和策略摘要。
- [x] `game_get_state`：当前角色、地图、属性及动作位置。
- [x] `game_list_nearby`：真实视野内对象与地面物品、冻结分页。
- [x] `game_get_map_region`：最多 33×33 地形；视野外动态阻挡未知。
- [x] `game_get_inventory`：背包、腰带、装备及 GUI 预留，未知字段明示。
- [x] `game_get_skills`：已学技能与当前支持的 typed 施法类别。
- [x] `game_get_interaction`：真实 NPC、输入、商店、交易邀请/面板与拍卖状态。
- [x] `game_get_operation`：六态操作、结果证据及迟到结果。
- [x] `game_get_events`：1024 条事件环、分页和 gap。
- [x] `game_stop`：优先暂停并停止 AI 所有的意图。
- [x] `game_move_to`：原寻路入口、有界搜索、逐邻格原动作发送。
- [x] `game_npc_open`：当前可见 NPC 的普通交互入口。
- [x] `game_npc_choose`：当前对话的真实选项句柄。
- [x] `game_npc_input`：当前输入请求，沿原 GUI 校验及提交。
- [x] `game_npc_close`：本地关闭，不声称撤回已提交脚本。
- [x] `game_pick_up`：当前格原拾取链；无相关成功回执时保持 unknown。
- [x] `game_attack`：单次或有界 until_dead，真实身份及玩家授权复查。
- [x] `game_cast`：target/ground/self/toggle，参数服从真实服务端分支。
- [x] `game_use_item`：现有药品/技能书路径，以精确实例变化作观察证据。
- [x] `game_shop_list`：当前真实商品/详情和报价。
- [x] `game_shop_buy`：真实单次成本与数量、报价/预算/发送前校验。
- [x] `game_shop_quote_sell`：当前 INVOP 的原 queryTag 报价。
- [x] `game_shop_sell`：原出售单位和 commitTag，不虚构整堆出售。
- [x] `game_trade_request`：真实目标玩家邀请。
- [x] `game_trade_respond`：真实邀请及原弹窗接受/拒绝。
- [x] `game_trade_offer`：完整替换本方报价、物品预留及本方 ack 门槛。
- [x] `game_trade_lock`：锁定指定报价版本。
- [x] `game_trade_commit`：双版本/双锁定核对，完成消息才结束交易。
- [x] `game_trade_cancel`：正常关闭请求；未确认前不返暂存物品。
- [x] `game_auction_list`：分类缓存、异步刷新、来源及冻结分页。
- [x] `game_auction_register`：原上架盒、数量、备注与真实 scoped 回执。
- [x] `game_auction_buy`：精确条目及限价，成功后等待投递。
- [x] `game_auction_cancel`：仅自己的条目，撤回成功后等待投递。
- [x] `game_delivery_list`：仅已收到的可信系统消息中的投递。
- [x] `game_delivery_claim`：真实 record 领取，SM_OK 确认。
- [x] 六个只读资源：`mir2x://session`、`character`、`nearby`、`inventory`、`interaction`、`capabilities`，与查询工具共源。

## 已完成的检查与修复

### 自动检查与证据

| 检查组 | 最终通过数 | 能证明的层次 |
| --- | --- | --- |
| `ai_bridge_protocol` / `ai_bridge_security` | 13 / 11 | 严格契约、真实 TCP 和 Windows 权限 |
| `response_callbacks` | 9 | 实际共用回调管理器 |
| `ai_bridge_controller` / `ai_bridge_runtime` | 26 / 10 | 控制器、输入手势及 AI 意图状态机 |
| `ai_bridge_actions` | 29 | 快照、句柄、分页、原寻路 helper 和动作证据 |
| `ai_bridge_npc` / `ai_bridge_npc_gui` | 9 / 4 | NPC 模型及真实 GUI 组件 |
| `ai_bridge_economy` / `ai_bridge_economy_gui` | 12 / 7 | 经济模型、真实拍卖/投递组件 |
| `ai_bridge_shop` / `ai_bridge_shop_gui` | 11 / 8 | 商店模型、真实购买/出售组件 |
| `ai_bridge_trade_gui` | 12 | 真实邀请/交易组件和来源/提交边界 |
| `netio` / `widget` | 3 / 2 | 原网络与基础控件回归 |
| C++ 合计 | **166/166，38.81 秒** | 定向开发回归；不是整个仓库的全量 `check` |
| Python pytest | **76/76，8.99 秒** | 官方 SDK stdio + fake 游戏桥接、Windows 实例读取 |
| Python ruff check / format | 通过 / 17 文件已格式化 | Python 静态与格式检查 |

- [x] 最后必要增量 7/7，重新链接 client 及相关 GUI 测试；server 同配置已是最新。日志：`.runtime/collaboration/t05-build.log`。
- [x] 最终 CTest 与无待构建：`t05-regression-ctest.log`、`t05-lasttest.log`、`t05-up-to-date.log`；历史 156/164/165 项日志单独留存。
- [x] Python 锁定安装、pytest、ruff：`t06-sync.log`、`t06-pytest.log`、`t06-ruff-check.log`、`t06-ruff-format.log`，均位于 `.runtime/collaboration/`。
- [x] T04 最终二进制官方 SDK→Python→真实 C++ 的认证、19 能力、非 RUN 查询、events/stop、正常退出后凭据 1→0。证据：`.runtime/collaboration/T04-client-smoke-report.md`；不覆盖 T05 最终二进制。
- [x] 开发期 GUI 空纹理/音效夹具及 include 等失败已修，保留原业务断言；dummy SDL 使用实际 client_objs、字体、生成纹理和受控网络端口。
- [x] 交付时源码定向空白检查通过，未暂存、提交或回滚 Git；未改用户存档及下载包。

### 已落实的重要修复

- [x] 断开先处理，随后无条件 `checkDeadlines()`；请求后再次检查，普通预算耗尽不推迟安全超时。busy 响应计入每轮 8 请求。
- [x] controller_error / callbackFault 统一关闭桥接及本实例凭据，普通游戏继续；关闭后面板不残留“控制中”。
- [x] 同进程登出仅使会话失效，保留 instance_id、transport/controller 和冻结策略。
- [x] AI 续发代次守卫、显式意图/焦点所有权及后续人工动作/匹配服务器校正分开，停止不清新人工意图。
- [x] 观察句柄失效只移除变化成员，保留其它有效句柄；冻结旧分页不会复活已失效目标。
- [x] 4096 轮有界寻路区分搜索耗尽 unavailable 与已证无路 unreachable；服务器校正丢弃旧路线。
- [x] 有害攻击/施法对所有当前可见玩家保守授权复查，typed 目标拒绝错误坐标替代 UID；人工续打保持原路径。
- [x] 商店、交易、拍卖来源绑定真实消息的 scope/代次；旧面板不能在新 world/session 授权，同轮新面板不被首次 observe 清除。
- [x] 无编号购买回执在人工同 tuple 重购后失去资格，首次结果锁存；8192 满表仍能撤销旧 watch，未知 tuple 不伪造唯一性。
- [x] 商店迟到报价槽保留 drain；相同脚本面板重发不无故清报价，变价/换物品/过期拒绝提交。
- [x] 玩家交易仅 canonical ack 改变 confirmed；迟到未确认快照不重开 commit，关闭/完成竞态不重复返物。
- [x] 经济操作在真实发送前再次检查 context、Active、当前 operation 及 deadline；准备期间过期不得发送。
- [x] unknown 保留经济预算/暂存，迟到仅记录 late_resolution，不自动重试或续发；offer/lock 与上架售价不误计金币支出。

这些修复已有相应纯模型或真实组件回归，真实角色联动仍按下文登记。

## 意图与计划对齐结果

35 工具和 6 资源、客户端主线程桥接、原游戏协议/GUI 入口、人工授权与结果分级均已接入。没有新增登录、聊天发送、任意代码/报文、通用键鼠或管理工具。客户端共享入口及回调修改属于修订 3 范围；T02 后收到的 deadline 顺序窄修已纳入 T03，历史接受版本保留。

| 验收 | 当前结果 | 已有依据 / 尚缺内容 |
| --- | --- | --- |
| A01 当前可见角色 | 部分证据，未整体通过 | 实例/角色绑定和只读 C++ 互通已测；缺登录角色 UID/地图与画面、动作效果对照 |
| A02 角色与周边观察 | 部分证据，未整体通过 | null、视野输入、句柄和分页自动检查；缺实际角色/地图/背包/技能逐字段对照 |
| A03 首版动作 | 实现已接入，实机未验证 | 各类入口、状态机和组件已测；缺最终二进制全链路及每类游戏动作 |
| A04 停止与人工优先 | 部分证据，未整体通过 | 控制/手势/所有权/期限回归及旧版本 stop 冒烟；缺实际动画、输入顺序和经济提交中接管 |
| A05 失效与故障 | 部分证据，未整体通过 | 断开、非 RUN、过期、超时、迟到自动检查；缺实际角色死亡/换图/断线/故障进程联动 |
| A06 权限与经济边界 | 部分证据，未整体通过 | Windows ACL、策略、账本、预算、unknown 测试；缺最终实例负面验证及真实钱物核对 |
| A07 中文文档与验证 | 基础文档已补，未整体通过 | 本文、使用/协议说明与已有检查；缺按文档实际复现及 T07 汇总交付 |
| A08 各类正常及失败路径 | 自动检查部分覆盖，实机未验证 | NPC/动作/经济纯模型及组件回归；缺每类真实正常/失败、重复/中断/状态变化证据 |

## 后续运行管理增量（本轮最新）

- [x] 用户将此前暂存批次提交为 `68c4ba0`，运行管理新增工作保持未暂存。
- [x] 新 CLI/默认策略/skill/独立 EXE/安装组件与中文速查已实现；Python 96/96（含冻结包凭据认证）、runtime CTest 11/11、真实服务器正常关闭通过。
- [x] 中文/空格搬迁目录、PATH 无 Python 的 EXE 启动两端、官方 SDK stdio→真实 C++ 35 能力、未 RUN 查询/停止和退出清凭据通过。
- [ ] 真实 LLM 宿主注册与自然语言会话、登录/选角/RUN、角色动作和钱物等原 V01–V06 未勾项目仍未验证。

最新 client/server 和管理器精确哈希、保留失败、修复与复验见[运行管理检查记录](20261101-runtime-launcher.md)。本轮保持现有配置 build signature，程序文件哈希已有变化；历史 T05 manifest 和 166/166 日志不修改为新入口证据。

## 待办：实机与整体集成

所有以下未勾项均需后续明确恢复工作后执行。责任归属为 T07 集成；涉及实现修复时再按模块登记，不因本文清单自动派发。每项记录实际版本、命令/操作、预期、实际结果、operation_id/context、日志/画面及未满足条件；钱物场景增加前后差额。

### V01 环境、版本与最终 MCP 链路

- [x] V01.1a 用户已编译；只读核对当前 build/install 的 client/server EXE 各自相同，记录生成标识和哈希。
- [ ] V01.1b 补充构建/安装输出，核对实际运行 EXE、资源来源和 client/server 握手；保留版本检查，不混用下载包。
- [ ] V01.2 准备 `.runtime/mcp-e2e/` 独立服务器/数据库、7100/8100 端口和两个测试角色，不使用用户已有存档作消耗测试。
- [ ] V01.3 完成正常登录、选角、进图、退出基础冒烟，保存当前窗口角色 UID、地图和服务器端口。
- [x] V01.4 后续运行管理版本通过官方 SDK→独立 Python EXE→真实 C++ stdio：initialize、35 tools、6 resources、C++ 恰 35 capabilities。中文/空格目录，版本见[独立记录](20261101-runtime-launcher.md)，未登录角色。
- [x] V01.5 上述明确版本非 RUN（loading）state/inventory/skills/interaction 均 not_in_game，events/stop 正常，正常关闭凭据 1→0；不替代实际登录/角色验收。
- [ ] V01.6 实际 AI 宿主按配置启动，stdout 无杂音，角色/实例不自动改绑；多实例须显式选择。

### V02 观察与移动

- [ ] V02.1 当前角色 UID、map_uid、位置/动画终点、HP/MP/等级/金币与窗口及客户端状态逐项相符。
- [ ] V02.2 背包、腰带、装备、已学技能、预留数量与真实 GUI 对照；尚未收到的数据保持 null/缺失标记。
- [ ] V02.3 真实视野、地面 revision、对象消失/同 UID 重建、survivor 保留；旧分页展示不得恢复动作句柄。
- [ ] V02.4 分页冻结最近两份、过滤条件/作用域变化、地形边界及视野外动态未知符合契约。
- [ ] V02.5 移动可达/当前格/部分动画停止，以实际格位置且停止后成功，不用队列终点冒充到达。
- [ ] V02.6 无路、4096 轮耗尽、动态阻挡、3 秒 stalled、30 秒上限、服务器校正均正确终结且不续旧路。

### V03 控制、生命周期与故障

- [ ] V03.1 RUN 面板显示角色、控制态和原因；授权范围可查询；高 DPI/缩放、半开按钮边界和主窗口过滤实测。
- [ ] V03.2 仅完整按钮手势开启/恢复；只读连接、换图、重连、重登均不能静默授权，失焦/聚焦不暂停。
- [ ] V03.3 accepted 未 begin、移动/攻击/拾取/施法中分别人工接管；第一个人工事件正常到达原 GUI/游戏。
- [ ] V03.4 stop 只停 AI 所有移动/攻击/焦点/队列；之后人工续打、挖矿、拾取及匹配校正仍正常。
- [ ] V03.5 真实死亡、换图、离开 RUN、同角色重登/换角色、断线使旧 context/句柄失效；同进程 instance/冻结策略保持。
- [ ] V03.6 MCP 退出、连接丢失和宿主取消后停止后续发送，不重放；旧回调不能触达新 Process 或新角色。
- [ ] V03.7 安全期限在普通队列饱和时仍生效；stop 优先、每轮不超过 8 响应，已接收动作与旧 pending 不永久饥饿。
- [ ] V03.8 controller/审计/回执故障关闭桥接并诊断，普通游戏继续；显式退出、窗口关闭与异常退出清本实例凭据。

### V04 NPC、拾取、战斗与物品

- [ ] V04.1 NPC 打开/选择/输入/关闭正常路径，及旧选项、重复输入、远距离、目标失效和外部文本不改变权限。
- [ ] V04.2 NPC 同轮新面板、换图旧面板、面板替换及脚本输入可用性；无可构造输入时明确缺少条件。
- [ ] V04.3 拾取物品/金币、背包满/锁定/别人抢拾、重复请求；无相关成功回执时 unknown，不伪造 seq/partial 成功。
- [ ] V04.4 once 只结束自身动作，until_dead 仅同身份死亡才成功；目标消失、追击、死亡、接管和停止不续发。
- [ ] V04.5 实际支持技能的 target/ground/self/toggle、冷却、未学/不支持、错误坐标参数与有益自身目标。
- [ ] V04.6 有害范围攻击/施法与新玩家进入视野的授权检查，含半月/十方；未授权时不发动作。
- [ ] V04.7 药品/技能书正常与失败，精确 item_id/seq_id/count、奖励变化不误算消费，unknown 不重试。

### V05 商店、直接交易、拍卖与投递

- [ ] V05.1 商店真实列表/详情刷新、同轮新 INVOP、旧报价/变价/面板关闭/30 秒过期与迟到查询隔离。
- [ ] V05.2 堆叠/非堆叠购买、材料成本、余额不足、精确实例；前后钱物按当前服务端单次 costList 核对。
- [ ] V05.3 AI 首购→人工同商品重购，成功/失败双响应两种顺序；旧 watch 不认领人工结果、不错误释放预算。
- [ ] V05.4 NPC 出售一次 1 件、原堆属性/seq 重查、最低价、拒绝/迟到与钱物差额；不将原堆数量当出售量。
- [ ] V05.5 双角色邀请/接受/拒绝、报价替换、双方改价、ack/锁定/双版本确认/完整完成及重复 commit。
- [ ] V05.6 交易取消/完成竞态、对方离开/死亡/断线、提交中接管；未确认前不返物，旧代次不认领新交易。
- [ ] V05.7 拍卖实际分类、刷新/超时/切类/换图；旧分类或旧 world 回包不重开新面板。
- [ ] V05.8 上架原数量与备注边界、成功/拒绝/unknown，未知保留盒内物品，售价不是支出且无虚构 500 手续费。
- [ ] V05.9 拍卖购买/撤回、自己商品限制、已售/过期/余额不足；delivery_pending 与背包到账分开。
- [ ] V05.10 可信系统投递、普通玩家伪造标签拒绝、领取/重复领取/无效 record/超时及真实钱物到账。
- [ ] V05.11 各经济类型的重复 request_id、提交后响应丢失、准备后过期/人工接管、unknown 锁和迟到结果；不重试、不续发、不提前释放预算。
- [ ] V05.12 核对完整预算账本：offer/lock 零支出、commit 预留实际报价、上架价零支出；人工核对/重登的恢复步骤。

### V06 回归、性能、平台与文档闭环

- [ ] V06.1 桥接禁用时原移动/续打/挖矿/拾取/技能/NPC/商店/交易/拍卖/退出回归；不能只测 AI 路径。
- [ ] V06.2 最终实例默认不监听、错误令牌/旧实例/多实例、速率/帧/连接/背压边界与实际审计轮转/脱敏。
- [ ] V06.3 测量真实主线程 transport/dispatch/DTO/审计/适配观察/渲染的耗时分布，含最大列表和连续负载；记录平均、分位和最大值。
- [ ] V06.4 对 8192 条经济发布/冻结分页纯 probe 的 249060 微秒成本评估并处理卡顿；有界数量不等于满足 2ms 软目标。
- [ ] V06.5 Linux/macOS 的 POSIX 凭据、权限、链接、清理、编译和测试；未执行前仅声明 Windows 已验证。
- [ ] V06.6 按中文使用、协议、构建文档从头复现；正式启动/验证辅助脚本、无秘密证据和测试进程正常清理。
- [ ] V06.7 T07 交付开发证据与不能构造场景的条件；整体对齐复核、必要修复/复验、A01 至 A08 逐项审查及用户确认。

## 结果解释与已知限制

- 拾取协议没有相关成功 ack；地面 count/seq 未知保持 null，failedItemID 仅是不相关观察，不伪造成功/partial。
- 单次攻击/施法的 client_state 不承诺伤害或击杀；目标消失不等于死亡。
- 无 respID 的商店/交易不能提供任意乱序旧包的请求级关联保证；唯一性不足时 unknown。
- 当前堆叠购买成本按服务端单次 costList，不乘 count；默认 INVOP_TRADE 一次出售 1 件；上架没有 500 金币手续费。
- 拍卖买/撤成功只说明等待系统投递；投递列表仅 client_known，payload/金币/关联拍卖未知保持 null。
- NPC 脚本授权独立，结构化经济预算不覆盖任意脚本副作用。同 OS 用户恶意进程不在凭据隔离保证内。
- 桌面 native pipe unavailable 导致本轮未取得真实 GUI 证据。dummy SDL/受控网络与纯性能 probe 不代替实际角色/整帧结果。
- 没有本次功能的全仓库 `check`、完整游戏长时间/多人/节点系统测试结果；166 项仅是列出的定向回归。

## 文档更新与后续记录

- [x] 本文登记全部 35 工具、6 资源、8 个 task、A01 至 A08、实际检查、修复、实机/性能/平台/文档待办。
- [x] 同步[总计划](../plan/20261031-ai-character-mcp.md)、[T07](../task/20261031-ai-character-mcp/T07-integration-docs.md)、[使用说明](../../readme/development/ai-mcp.md)、[协议说明](../../readme/development/ai-bridge-protocol.md)及开发知识库入口。
- [x] 本次 20 份文档的 214 个本地链接/锚点、标题时间、代码块和行尾空白静态检查通过；清单 35 个工具逐项匹配 schema，无缺项或额外工具。
- [x] 任务文档及 T06/contracts README 已注明当前状态与历史交付的区别；原 manifest/报告快照保持不变，后续文档哈希变化不用于声称旧证据覆盖新运行代码。
- [ ] 后续执行每项验证时更新对应复选框、版本和证据；失败保留原结果，修复后追加复验，不能仅改为“通过”。

当前结论：局部实现与定向开发检查已完成，真实角色及最终集成仍有上述未勾事项。本文作为后续验证入口，不将文档整理计为整体 Verification 完成。
