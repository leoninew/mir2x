# 子任务 T05：商店、玩家交易、拍卖与投递
最后修改时间: 2026-11-01 19:26:21

## 审查状态

方案审查状态：Accepted。沿用总计划修订3及用户整体Implementation授权；本任务执行分配、交付接受与依赖以总计划为准，本处不表示T07或整体Verification已完成。

- [x] `MCP-T05-001/r3/delivery2` 已于 2026-11-01 18:24:19 由主会话接受局部实现，18:25:55 主审记录为 `accepted_local_implementation`；166/166 回归及冻结版本已核对。
- [ ] 最终 35 能力 stdio、实际钱物/双角色、POSIX 和完整主线程耗时尚未验证；[验证清单](../../verification/20261031-ai-character-mcp.md)列出全部待办。

本次仅同步文档状态，不更改冻结 manifest/报告快照、产品源码或测试日志；不分配 T07，不启动整体 Verification。

## 父任务与权威记录

- [总意图](../../intent/20261031-ai-character-mcp.md)、[权威总计划](../../plan/20261031-ai-character-mcp.md)。任务 T05，协调根 `D:\SourceCodes\opensource\mir2x`。

## 目标与验收

完整实现 NPC 买卖、玩家直接交易、拍卖上架/购买/撤回和关联投递领取，覆盖 A03、A04、A05、A06、A08。GUI 和桥接共用业务逻辑，受限额度与物品预留一致；未知结果不能导致自动重试或凭空返还物品。

## 范围与文件

- 新增 `client/src/aibridge/economy.*`、报价/交易状态及适配。
- `gui/purchaseboard/`、`inventoryboard.cpp/.hpp`、`gui/directtradeboard/`、`gui/auctionboard/`、`gui/friendchatboard/`。
- `processrun.cpp/.hpp`、`processrunnet.cpp` 的标准请求和消息观察，复用 T02 scoped send。
- 新增 `test_ai_bridge_economy.cpp`；不新增游戏协议、后台数据库操作、仓储/修理/合成或聊天发送。

## 输入

直接前置 T04 已接受，包含当前 NPC/INVOP 句柄、背包状态、回执事件和交互版本。上游含控制/策略/去重/scoped callback 基础。全部工具的固定接口已在总计划确定。

## 局部技术设计

### 商店买卖

1. 购买只使用当前 SM_NPCSELL 和 CM_QUERYSELLITEMLIST 得到的真实商品/详情。quote_handle 绑定 session、NPC、item/seq、价格/材料成本、商品版本和创建时刻；30 秒有效，面板关闭、列表变化或换图立即失效。
   详情未取得时返回 refresh_pending，按总计划的有界刷新规则合并查询；只有收到真实价格/成本后才生成报价句柄。
2. buy 校验 count、商品供应、金币/材料余额、策略限额和 max_total_gold 后，通过现有 requestBuy 发送一次。非堆叠商品 SM_BUYSUCCEED 属于明确回执；堆叠商品只能依约观察物品与成本变化，不能等待一个服务端不会发送的成功回执。
3. NPC 出售要求当前 SDStartInvOp.invOp 为 INVOP_TRADE；用真实 queryTag 请求 item_id/seq_id 的报价，以匹配的 SM_INVOPCOST 生成句柄。报价请求唯一在途；不接受仅相同 NPC 但不同物品的迟到价格。
4. sell 提交真实 commitTag，校验 min_total_gold。原 UI 的提交以 item/seq 为出售单位，没有数量字段；本次显示该单位并沿用，不编造部分出售能力。
5. 报价/提交都显示对应面板和选中物品。出售最终证据为实际扣物/加钱及脚本反馈；缺乏充分证据为 unknown。仅关闭面板不等于撤销出售。

### 玩家直接交易

1. 抽取 DirectTradeBoard 的报价、物品预留/恢复、锁定与确认方法，GUI 按钮和桥接调用同一个实现。邀请从 SM_REQUESTDIRECTTRADE 建立 invitation_handle，响应处理同步原 InputStringBoard。
2. trade_handle 绑定对方、会话和本次交易代次；每次接收本方/对方报价更新各自 revision。更新请求不直接把本方 pending 状态当作服务器已接受，要等 SM_UPDATEDIRECTTRADE 的本方 ack。
3. offer 是完整替换当前本方未锁定报价；按 item_id/seq_id/count 校验同一库存可用数量，重复条目拒绝。金钱遵守原 UI INT_MAX 及策略的更严格上限。
4. lock 等待真实本方锁定 ack；commit 同时要求双方已锁定、未确认以及 local_revision/peer_revision 与调用时完全一致。对方变化使确认参数失效。完整 SM_COMPLETEDIRECTTRADE 后才确认交易完成并使用权威金币/背包更新；单方确认只算运行中。
5. cancel/拒绝/对方离线沿用正常关闭消息；若 commit 已提交且结果未明，不直接恢复暂存物品。人工接管优先停止 AI 后续步骤，用户仍能通过原交易面板查看和处理当前会话。

### 拍卖与投递

1. 使用现有分类查询和 AuctionBoard 列表生成 auction_handle，绑定 auctionID、卖家、商品、价格及列表版本，有效 30 秒。刷新按总计划返回缓存与 refresh_pending，不阻塞主循环；购买/撤回验证条目及金额约束，服务器对已售/过期商品的错误原样映射。
2. 上架复用 AuctionRegisterBoard/AuctionRegisterBox 的完整原数量语义和物品暂存，price 为正数且不超过 INT64_MAX，note 按 SYS_AUCTIONNOTESIZE 的 UTF-8 字节限制校验。不自造协议没有的拆分数量输入。
3. buy/register/unregister 使用 scoped response callback。明确 SM_OK 才结束提交；超时释放 UI 的等待指示，但显示“结果未知”，冻结相关 AI 经济动作与物品预留，不假装退款或返物。
4. 拍卖购买/撤回完成返回 delivery_pending；投递不是背包到账。接收和缓存可信系统聊天对端 `SYS_CHATDBID_SYSTEM` 的消息时提取 SYS_DELIVERY 的 record，生成不可猜的 delivery_handle；普通玩家消息即使含相同 XML 标签也不能生成可领取句柄。
5. delivery_list 只列当前客户端已收到的系统投递，包含来源、关联拍卖（可知时）、领取观察状态及 coverage。没有全量历史信箱查询能力时如实注明，不声称“无列出即无待领”。
6. claim 使用句柄中真实 record 并校验 SYS_DELIVERYRECORDSIZE；只接受服务器 SM_OK 作为领取确认，记录物品/金币更新证据。重复/无效领取映射 CDERR_INVALID_RECORD/CDERR_INVALID_CLAIM，不再次创建奖励。

### 权限、相关性与恢复

- 每种方法先检查固定策略、会话、角色、物品预留与预算。预留按一次经济操作原子登记；明确失败释放，成功计入本 session 预算，unknown 保留。
- NPC 脚本对话的独立权限及无法通用报价的边界沿用总计划；不能把交易限额写成对所有脚本副作用的硬保证。
- 无 respID 的消息仅作为状态证据，不能靠时间接近强行认定因果。并发人工变化已触发接管时，以最后可确认的结果和 unknown 说明处理。
- 未提交时 stop 可恢复本地预留；已提交须根据明确回执或后续完整状态核对。无核对接口时要求人工核对/重登，不新增数据库查询或隐蔽同步协议。

## 实施安排

1. 建立统一 quote/offer/auction/delivery 状态模型、版本和经济预算接口。
2. 抽取原 GUI 的物品预留/报价/提交逻辑，实现商店买卖并验证 UI 一致性。
3. 实现玩家交易各步骤，区分锁定/单方确认/完成。
4. 实现拍卖与系统投递，所有有 respID 的调用使用 T02 的 scoped send。
5. 加入 unknown 经济锁、迟到结果、重复请求和恢复说明，完成假网络与 GUI 适配测试。

## 检查与风险

- 构建 `client test_ai_bridge_economy test_ai_bridge_controller test_ai_bridge_actions test_widget`；运行对应 CTest 前缀。
- 商店：堆叠/非堆叠购买、材料成本、资金不足、变价/过期、不可售物品、匹配/迟到报价、出售单位、失败后库存和额度。
- 玩家交易：接受/拒绝、编辑双方报价、锁定前/后改价、重复 commit、对方离开/死亡/断线、提交期间人工接管；金币物品在本地和服务端结果中一致。
- 拍卖：上架成功/失败/超时、自己商品购买拒绝、余额不足、条目被抢购、撤回、备注字节边界、64 位 ID/金额、物品等待投递而未入包。
- 投递：可信系统来源、伪造普通聊天标签拒绝、合法领取、重复/无效领取、延迟物品更新和超时后回执。
- 所有经济工具均覆盖“服务端已执行但桥接响应丢失”，断言不自动重试、不错误释放预算/物品。检查 GUI 拖放及原按钮仍正常。

## 交接依据

- 已分配MCP-T05-001；A经济模型/拍卖投递、B玩家交易/共享游戏、主会话商店与交互聚合，独占范围见总计划；前置MCP-T04-001/r3/delivery1已接受。SpecFlow 入口 `C:\Users\leon\.codex\plugins\cache\specflow-local\specflow\0.2.1\skills\specflow\SKILL.md` 及 `references/task-coordination.md`。
- 当前已授权 Implementation；授权、编号、基线和可用 T04 版本由总计划派发。回报写本文接收与交付节，主会话读取；跨会话由用户转交摘要。

## 接收与进展

负责人A已于2026-11-01 10:17:07接收MCP-T05-001，沿用修订3。HEAD b1ddda8d8742afb1d60839f759733a2e8ee26207；前置T04 MCP-T04-001/r3/delivery1已接受，manifest SHA256 e9ebee39da726234498abc425cecc5f0e0dfa86bdd28955a95788bf12cafa776已核对。读取总意图/计划、T05准备、T04交接和三方接口提案。A独占纯经济模型、拍卖/投递实际适配与共用GUI、必要预算窄修复、测试/CMake/task/manifest及统一构建。B交易/共享游戏与主会话商店/NPC并行修改已登记，属于预期变更。无Git写操作、不增加agent、不改用户DB/下载包/版本检查。

纯接口已先落盘economy.hpp，商店直接使用QuoteStore/ShopQuote/quoteJson；拍卖投递入口auctionadapter.hpp。主会话商店接收/进展写.runtime/collaboration/T05-shop-report.md，不并发写负责人主记录。

| 时间 | 分配编号 / 执行者 | 接收、进展或阻塞 | 证据 / 回报位置 |
| --- | --- | --- | --- |
| 2026-10-31 | 无 / 无 | 仅完成局部计划 | 本文 |
| 2026-11-01 10:17:07 | MCP-T05-001 / A | 已接收修订3，确认T04基线及三方独占范围；先落纯economy/auction自包含接口 | .runtime/collaboration/T05-a-api.txt |
| 2026-11-01 17:57:08 | MCP-T05-001 / 三方 | 开发候选client/server链接，164/164回归通过38.06秒；最后发送guard/两处cpp晚于部分对象，继续增量，不以此作为最终manifest | t05-build-164-history.log、t05-regression-164-history.log |
| 2026-11-01 18:01:09 | MCP-T05-001 / B及A | 玩家交易最终共享发送guard与真实GUI准备后期限回归已落盘，B13产品重新冻结；A统一当前配置增量，商店12产品已冻结 | T05-trade-report.md、T05-shop-report.md、T05-auction-report.md |
| 2026-11-01 18:23:39 | MCP-T05-001 / A及主会话 | 主会话18:10:50容量P1窄修与shop_GUI8已由A增量7/7、166/166回归38.81秒及no-work覆盖；delivery2替代退回的delivery1，停止写入等待审查 | T05-shop-capacity-ready.txt、T05-delivery-manifest.json |

## 交付与审查

开发交付与阶段收尾已完成，版本 `MCP-T05-001/r3/delivery2`，**局部实现已 accepted，整体首版尚未验收**。交付时的 pending 状态保留在冻结 manifest，随后主审见 `.runtime/collaboration/T05-stage-main-review.json` 与总计划。本版本替代因商店8192容量P1边界退回的delivery1；旧165项证据不覆盖18:10:50的shop-candidate3。用户阶段收尾要求继续有效：不启动新游戏/server/SDK冒烟、不派T07，用户稍后自行编译和查看效果。产品保持冻结，无Git写入。

### 实际变更

- 新增16项真实注册：商店4、玩家交易6、拍卖/投递6，连同前置19项预期恰35。没有经济stub，没有修改v1/Python契约。最终binary未重新运行能力发现，预期数量与运行观察分开记录。
- 纯economy提供有限报价/期限句柄、冻结分页、收据阶段分类及共同发送前context/Active/operation/deadline守卫。所有实际有respID经济提交使用scoped callback；回调仅持纯收据，未知保留预算与物品，不自动重发或返还。
- DirectTradeBoard/InputStringBoard/ProcessRun共用真实交易与邀请入口，报价匹配本方ack更新版本，offer/lock零金币支出，commit预留一次真实本方报价；Complete才完整完成，Close前不返物。真实来源scope/UID/generation阻止旧GUI在新world/session恢复句柄。
- PurchaseBoard/InventoryBoard共用真实查询/购买/出售，10秒无respID槽保留迟到drain，30秒报价重查身份/面板/版本/成本。标准脚本相同STARTINVOP重发保留报价；真实来源资格保留同轮新面板。人工同商品重购撤销旧AI回执资格，已认领结果锁存不覆盖。
- AuctionBoard/AuctionRegisterBoard共用实际列表/选中/暂存/提交入口，查询按分类限时与scope绑定；上架全原数量，售价非支出。明确SM_OK才server_ack，买/撤为delivery_pending。Unknown解除等待但保留盒内物品/预算，禁用重复提交和返物。
- FriendChatBoard仅真实SYS_CHATDBID_SYSTEM已收消息形成领取句柄；普通玩家伪造XML不生效。一次领取只提交真实record，SM_OK才确认，payload/金币/关联auction未知保持null与缺失标记；列表不代表完整信箱。
- 三方独占接口、41个精确产品文件及实际diff范围见 `.runtime/collaboration/T05-api-handoff.md`、三个专属报告与 `T05-delivery-manifest.json`。原T04输入manifest与接受记录保持不变。

### 修复与验证

- 最终 `t05-build.log` **7/7** 必要增量完成，重新编译PurchaseBoard和shop GUI测试并链接client及4个实际client_objs GUI目标；server同配置产物已是最新。`t05-regression-ctest.log` **166/166通过，38.81秒**；18:22:45的 `t05-up-to-date.log` **ninja: no work to do**。没有重新配置，覆盖B18:01:09发送guard、A最后两cpp及主会话18:10:50容量窄修。
- 分组：actions29/controller26/runtime10/protocol13/security11/response_callbacks9/netio3/widget2/NPC9/NPC_GUI4/economy12/economy_GUI7/trade_GUI12/shop11/shop_GUI8。
- 41产品重新散列与三方清单0差异，行末空白0，定向git diff --check退出0（只有既有CRLF提示）。产物17、证据32项精确散列均入manifest。最终client SHA256 `3386B48D70F75232E3A2F2B62635A185A91FC3E12FACEE5C94EE6E707BB57DAD`，server `E78D9DDB999A1D06D457537B40D921B3A90A14071F12490916E16EED4E672DAC`，共同build `VENGINEERING-20261101174437`。
- 开发期156项3个商店GUI AV、交易/拍卖夹具缺纹理/音效等失败日志保留；夹具按实际SDL资源需求修复，不放宽业务断言。B独立商店审查两项实际缺陷及原P1的8192容量残留已修并由对应纯/GUI回归覆盖：已有tuple满表仍饱和计数2，仅新增键受限，未记录键返回不可关联哨兵。新增实际GUI测试覆盖8191→8192→同tuple重购成功/错误均不认领、第三次饱和及8193哨兵。历史164/165及delivery1均不作为本版本通过证据。
- 经济模型测试验证unknown保留预算、去重、迟到仅追加且不重放；实际client_objs GUI测试验证交易ack门槛/暂存/取消完成竞态、准备后deadline、邀请来源、上架成功/明确拒绝/unknown不返物、拍卖归属/分类超时、商店旧槽/原单位/人工重购。
- delivery2 manifest SHA256 `3ADFCCB7E84A3B58B17A886081A8C96135530486B36CB078883B143C48568335`；41产品/17产物/32证据共90项散列已独立重算，0差异。三方专属报告按本次交付重新冻结快照，peer-review以当前散列记录；历史165日志及delivery1单独存档。主会话按18:20指令直接读取落盘结果，不依赖已失败的消息通道。

### 真实语义、限制与后续边界

- 当前服务端packable商店购买costList不乘count，本次按实际单次请求成本校验和记预算；INVOP_TRADE脚本uidRemove默认一次1件，出售quote.unit_count=1。当前上架服务端没有旧GUI曾提示的500手续费，售价不是支出。本次未静默修改服务端经济规则。
- 最大8192经济句柄发布加冻结分页纯probe实测 **249060微秒**，不是完整GUI/ProcessRun测量；极大查询可显著超过软2ms。已限制数量，未证明真实全帧延迟分布或硬实时。
- **readonly_smoke=not_run_final_binary**。遵照阶段收尾不再启动最终binary的新SDK冒烟；旧T04的19能力smoke不能替代此35注册候选。真实登录角色买卖、拍卖/领取、双角色交易、人工接管钱物效果、POSIX仍未验证。dummy SDL/假网络证明组件路径，不是实际游戏窗口证据。
- 无respID商店/交易不能提供任意乱序旧包的请求级归属保证；仅沿实际有序状态和受限唯一性，歧义保持unknown。响应scope销毁/过期后被丢弃的回包不声称能再解析为成功。
- codex-cli向主会话投递probe证据返回delivery_failed/UNSUPPORTED，已停止该失败通道并落 `.runtime/collaboration/T05-coordination-failure.json`，不把queued或失败当作已送达。完整交付已落盘可审查；等待用户/主会话后续，不自开T07或宣布整体Verification完成。
