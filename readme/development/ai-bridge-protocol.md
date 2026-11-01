# AI 桥接协议与实现入口
最后修改时间: 2026-11-01 19:26:21

[返回知识库](README.md) · [使用说明](ai-mcp.md) · [验证清单](../../docs/verification/20261031-ai-character-mcp.md)

本页记录当前修订 3 的实现。权威数据契约是 [v1 schema](../../contracts/ai-bridge/v1/contract.schema.json)，协议细节及生成方式见 [contracts README](../../contracts/ai-bridge/v1/README.md)。当前源码接入 35 工具；最终二进制与真实角色证据边界以验证清单为准。

## 分层和源码

| 层次 | 文件 / 职责 |
| --- | --- |
| Python MCP | `tools/mir2x-mcp/src/mir2x_mcp/`：官方 SDK stdio、固定 typed 工具/资源、安全实例发现和 IPC 客户端 |
| 跨语言契约 | `contracts/ai-bridge/v1/`：封闭 schema、fixture、策略样例与确定性生成器 |
| 传输与安全 | `client/src/aibridge/transport.*`、`protocol.*`、`instancefile.*`、`policy.*`：长度帧、认证、队列和冻结策略 |
| 控制与记录 | `controller.*`、`operation.*`、`events.*`、`audit.*`：授权、上下文、去重、预算、操作和脱敏审计 |
| 纯领域模型 | `snapshot.*`、`movement.*`、`actions.*`、`npcmodel.*`、`economy.*`、`shopmodel.*`、`tradesource.hpp` |
| 真实适配 | `gameadapter.*`、`combatadapter.*`、`npcadapter.*`、`shopadapter.*`、`tradeadapter.*`、`auctionadapter.*` |
| 主线程集成 | `runtime.*`、`runtime.intent.hpp`、Client/ProcessRun/MyHero/Hero 及原 GUI 共用 typed 入口 |
| 游戏响应 | `client/src/responsecallbacks.hpp`、`Client::sendScoped`：单调 ID、Process scope、期限及取消 |

Python 不直接访问游戏网络或数据库。游戏状态只在客户端主线程读取和改变；纯库不持有跨 Process 游戏对象。

## 实例、帧与严格校验

- 默认关闭；显式开启后只监听 `127.0.0.1:0`，实例 ID 为 128 随机位，token 为 256 随机位。
- Windows 实例/审计权限仅当前用户与 SYSTEM；拒绝重解析路径，无法保证权限时关闭桥接。POSIX 分支要求目录 0700、文件 0600，尚未在 POSIX 运行。
- 首帧 hello 带 v、instance_id 和 token；认证只授予查询，不授予动作。最多 4 个未认证、1 个已认证连接；认证期限 3 秒。
- 帧为 4 字节大端长度 + UTF-8 JSON；请求 65536 字节、响应 1048576 字节。先检查长度再分配正文。
- 深度最多 16；拒绝重复键、未知字段、无效 UTF-8、非有限数、错误整数类型和溢出。UID/金额/代次等用十进制字符串；执行 `x-decimalMaximum`、`x-maxUtf8Bytes` 扩展约束。
- 心跳间隔 2 秒、无有效活动 6 秒关闭；持续写出 6 秒未完成也关闭，即便仍有心跳。普通有效请求每秒最多 20，stop 绕过此普通限额。
- 待处理普通请求最多 64、独立 stop 槽、出站最多 2MiB；超额 busy 或关闭慢连接，不能无限堆积。

同一 OS 用户可读取凭据，本设计不声称隔离该用户的恶意进程。日志、错误、资源和 capabilities 不能返回 token、密码、原始输入或内部异常文本。

## 请求、上下文与句柄

查询包含 `v/id/method/params`；动作另有 UUID `request_id`、`context`、`timeout_ms`。`id` 仅匹配一次 IPC 调用，request_id 才是连接内动作去重键。`game_stop` 需认证，但不要求动作上下文或 request_id。

context 为 `instance_id/session_id/world_epoch/control_epoch`。入队、begin 前 validate，以及经济实际发送前再次检查；accepted 与真实提交分开。同进程登出不重建实例，仍须失效旧会话。loading 不允许绕过同次登录预算和 economic_uncertain。

观察句柄绑定 instance/session/world；单纯 control_epoch 改变不使观察数据失效，动作仍必须使用最新完整 context。目标身份含单调 incarnation，地面身份含格子及服务端更新 revision；离开视野或身份变化立即失效，不能只依赖地址/UID。

列表每页最多 100、每类最多 8192 行，保留最近两份完整冻结快照及同份缺失字段元数据。继续分页不重读当前世界；新快照淘汰、scope/过滤不匹配、伪造 cursor 等返回 stale_cursor。展示旧页不意味着其动作句柄仍有效。地图查询最多 33×33，动态阻挡视野外未知。

商店/交易/NPC/拍卖来源须由当前 scope 的真实服务端消息建立，普通 refresh 不得把保留的旧 GUI 面板授权到新世界。同轮真实新消息也不能被首次 scope 同步误清。

## 调度与控制

Client 主循环先处理游戏网络和人工输入。runtime 的桥接轮次为：

1. transport 轮询最多 16 个 handler（当前传入 1ms），整体轮次目标软预算 2ms。
2. 消费全部断开通知、清旧连接 pending，然后无条件 checkDeadlines。
3. 取得最多 8 请求，先 stop；已有动作有预算则推进，旧 pending 先服务一项，再接新请求。
4. 每轮响应最多 8，busy 也计数；额外 runtime pending 最多 8。普通工作至少有服务机会，单次可超过软预算。
5. 再次无条件 checkDeadlines；只有普通 advance 受剩余预算约束。随后各域观察、合并快照事件、刷新面板。

2ms 不是硬实时保证。DTO、审计、适配器观察和单次有界处理仍可能超时；8192 条经济探针已经出现 249060 微秒，真实全帧分布待测。

控制态为 disabled/read_only/active/paused。游戏主窗口冲突输入先暂停并撤销旧 AI 代次，再交原处理；完整控制按钮手势例外，失焦仅取消未完成手势。stopHook 只清显式 AI 意图/焦点，不能无条件清玩家队列。

## 操作、预算与证据

操作为 accepted→running→succeeded/failed/cancelled/unknown。submitted 来自实际游戏发送记录且保持 sticky；队列插入成功不是提交，提交不是完成。成功必须附非空 evidence 和 completion_scope：server_ack、observed_postcondition 或 client_state。

| 情况 | 当前处理 |
| --- | --- |
| 确定未提交的取消 | cancelled；本地准备按原 GUI 安全恢复 |
| 本地持续动作确已停止 | 可报告停止范围，已提交消耗不因停止回滚 |
| 已提交但无法确认结果 | unknown、side_effects_possible，经济预留保持 |
| 截止时刻才到达明确回执 | 已按 deadline 终结，后续仅 late_resolution |
| 目标消失 / 其它奖励变化 | 不能分别推断击杀 / 自身消费或拾取成功 |

每连接去重账本最多 4096，不驱逐；相同规范化动作返回原 operation，变化返回 duplicate_conflict。账本满仍能 stop。Python 不自动重放动作，不承诺跨连接/进程 exactly-once。

金币预算由真实 adapter 提供：提交前预留，明确失败释放，成功计入，unknown 保留并锁住后续经济动作。暂停/恢复/过图不能清同次登录额度。offer/lock 和上架售价不是支出；NPC 脚本权限不能由结构化交易预算替代。

无 respID 的商店/交易只按实际状态及受限唯一性观察，不伪造 server_ack；人工同商品重购使旧 watch 不再可关联。拾取实际没有相关成功回执，不能因纯 helper 支持相关回执输入就宣称真实协议有此能力。拍卖/领取沿 scoped 相关回执；买/撤成功后仍是 delivery_pending。

## 事件、审计与退出

事件环最近 1024 条，旧 after_seq 返回 gap；宿主应重新取快照。领域变化每绘制帧最多合并一次 snapshot 通知，当前没有后台 MCP 订阅推送。

审计位于已保护 bridgeDir 下本实例目录，5MiB×3 轮转，不记录任意 params、NPC 输入或认证秘密。审计/适配器内部异常使 controller_error；runtime 关闭桥接和本实例凭据，更新面板并诊断，普通游戏继续。

Client 显式退出/主循环/异常路径加 atexit 兜底关闭桥接。scoped callback 先检查弱生命周期，再回报原 operation；Process 销毁/期限结束取消回调，迟到/未知响应不触达旧对象。只清本实例文件，不通配删除其它实例。

## 修改与验证路径

新增能力先修订意图/计划和共同契约，再生成 schema/fixture、Python 静态模型/函数，接入真实 GUI 共用入口及注册；不得仅加名称或 stub。分别覆盖严格参数、控制/策略、真实发送、结果 scope、迟到和人工接管，再补实际游戏正常/失败证据。

当前检查入口见[开发与测试流程](workflow-and-testing.md#ai-桥接的定向回归)。完整验收及尚未执行事项见[验证清单](../../docs/verification/20261031-ai-character-mcp.md)；自动检查、组件路径、真实 stdio、真实角色四层证据分别记录。
