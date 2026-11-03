# 20261103 自动 AI 启动与右下角控件

本记录属于独立启动/UI 实现单元；装备由协作者 B 实现，统一构建由 A 单写。真实安装、角色操作和普通练级由审查方负责，见[真实记录](20261103-mcp-leveling.md)。本记录不替代真实游戏验收。

2026-11-03 10:12 阶段更新：下文原 190 项与 `8CF706...` 是 candidate1 的开发检查。该版本实际登录时把初始生命未知当死亡，自动开启实机失败；已由 `candidate2-life-known` 窄修替换。新客户端 SHA256 为 `F7CA96693BF95CE385F979F3058150303747B569665A03842B08D7D1D2869DE3`，新增 5 项后全相关 **195/195、43.71 秒**，随后 no-work。新代码仍待协调方实机验证，不以开发通过覆盖旧实机失败。准确 delta、red/green 和当前组合源码见 `.runtime/collaboration/20261103-ai-life-implementation-report.md` 与 `20261103-ai-life-review.json`。

## 使用与权限

原生 `client.exe` 和标准 launcher 默认启用桥接。原生默认路径从 EXE 所在 `client/` 的父目录解析为 `.runtime/ai-bridge` 与 `config/policies/standard.json`，launcher 继续传入同一所选运行根的绝对路径。缺少策略时桥接初始化失败，普通游戏继续，不自动创建宽松策略。

角色首次有效 RUN、存活且认证宿主绑定均就绪时，控制器自动开启一次 AI，不再需要额外开启点击。登录/选角仍用原游戏流程；`.env` 只由 launcher 可选读取，不是原生客户端的配置或启动前提。stdio 仍由真正的宿主启动，不启动无宿主后台 stdio。宿主先读取 `game_get_session`，核对唯一实例、角色、context、capabilities、状态和冻结策略。

右下工具区上方显示 AI 状态及“关闭 AI”“开启 / 恢复 AI”按钮，原底部 154 逻辑像素的 HUD 保持可用。位置跟随 renderer 的逻辑尺寸，游戏事件先按原 SDL 路径转换一次；按下/释放必须匹配窗口、context 和布局位置，缩放或改图不会转移旧手势的授权。

显式关闭立即停止/撤销旧 AI 意图并递增控制代次；已发送动作仍可能有副作用，unknown 不重发。认证只读会话、事件、状态和停止查询保留。重复观察、普通 poll、同连接查询和 `game_stop` 不覆盖关闭选择。显式窗口恢复沿同一控制器，保持进程身份和会话，不重建账本。

人工输入先暂停 AI，再按原路径送给游戏。`game_stop`、人工接管、死亡、地图/角色/世界失效、离开 RUN 和传输丢失都会耗尽首次启动资格，不能靠重连或观察自动恢复。内部故障继续 fail-closed，须重启客户端，按钮不能开启故障控制器。

## 退出默认行为

- 仅关闭首次自动开启：原生 `--ai-auto-enable=0`，或 launcher `[client] args=["--ai-auto-enable=0"]`。认证只读及窗口开启仍可用。
- 关闭整个桥接：原生 `--enable-ai-bridge=0`，或 launcher `[launcher] bridge=false`。角色 MCP 不可用，launcher 显式传入关闭参数，避免被原生默认覆盖。
- 临时手工登录：launcher `--manual-login`；它只影响可选登录输入，不改变 AI 策略或控制状态。

PVP 默认关闭、peer 白名单、物品/金币预算和固定实例绑定保持原策略。用户授权自动启动不代表自动恢复已停任务，也不授权 GM、数据库、Lua 或测试怪注入。

## 开发验证结果

A 产品冻结于 2026-11-03 09:38:09 +08:00；B 装备 candidate4 冻结于 09:42:06。最终补齐开发证据仅更新本文与审查包，不改变已经构建的产品代码。构建沿现有 MSYS2 UCRT64 配置，由 A 单写，不重编未变的 server。

- C++ 全相关回归 190/190、42.55 秒，日志 `.runtime/collaboration/20261103-auto-ai-final2-ctest.log`；覆盖 controller/runtime、35 方法契约、安全、actions、NPC、经济/商店/拍卖/交易、网络与 widget。其中实际控件 GUI 3/3、装备 GUI 10/10、actions 31/31。
- 最后必要增量只重编 actions 的只读测试夹具，见 `20261103-auto-ai-final2-build.log`；相同目标随后 `ninja: no work to do`，见 `20261103-auto-ai-no-work.log`。
- Python 和冻结 launcher 144/144、36.16 秒，见 `20261103-auto-ai-final-python-pytest.log`；包括官方 SDK 的 35 工具/6 资源、固定实例保护、源码与发行入口一致性、可选登录和明确 bridge=false。SDK 使用隔离夹具，不是当前角色实测。
- Ruff 通过、29 个 Python 文件格式检查无修改，skill 校验通过。11 个 stage 文件逐项散列一致，wheel/sdist 中模块、schema、示例与当前源码一致，不含用户 `.env`、运行状态或数据库；见 `20261103-auto-ai-artifact-check.json`。
- `test_ai_bridge_control_gui` 直接链接真实 `client_objs`，用真实字体与 SDL dummy/software 检查按钮、身份保留、逻辑缩放、resize 手势失效和故障不能恢复。截图 `.runtime/test-ai-bridge/control-gui/ai-control-panel.png` 是受控 GUI 产物，不是登录角色窗口证明。

历史失败保留：dev1 旧成员引用、dev2 装备测试 structured binding 编译诊断、首次 Python 工具生成文件过期，以及首次全回归 189/190。最后一项是旧查询错误夹具仍假设默认只读；B 显式设置 `autoEnable=false`，保留错误码与状态断言，最终 190/190 覆盖该修复。旧日志不代替最终结果。

最终 client 为 `build/client/src/client.exe`，SHA256 `8CF70627836FC81FC29417CB695CAB7992F10059D82413DB5596A28132CE209D`；launcher 为 `.runtime/runtime-package/mir2x.exe`，SHA256 `64ED2CB2D5B5920B0BB2E1F6471D30D686F67D26EB6444DB863249D3B858ED14`。共同游戏构建签名仍为 `VENGINEERING-20261101192035`，因此必须同时核对二进制散列，不能仅靠签名区分候选。server 未改、未重编。

A 未安装或重启真实客户端/服务器，未读真实 `.env`，未做角色动作；审查方的安装与实测另记，不从开发通过推定。

入口一致性 `.env` 单元已单独审查并提交 `e30f54d84dcb85f1f99fb0896b42349a5a73902c`，其 55 项原始快照完整保留于 `.runtime/collaboration/20261103-env-login-candidate2-snapshot`。本轮打包输出不能冒充该旧候选。NPC 错误隔离独立提交为 `ab41bdf4a06afa403d8fa840d6d689431e46a401`。

## 真实验收待办

- [ ] 审查方安装准确候选、正常进入角色，维持同一宿主认证连接，确认首次 active 无需额外点击。
- [ ] 真实窗口核对右下角、DPI/resize、原 HUD 与按钮命中；关闭后停止旧意图且多次查询保持 disabled，恢复保留身份。
- [ ] 人工接管、stop、换图、死亡与断线之后不自动恢复；真实内部故障不在角色上主动制造。
- [ ] 普通获取、穿戴/替换装备须核精确实例与真实槽位。技能书使用后重读已学技能；不把消费成功当学习证明。
- [ ] 只发现和攻击当前视野中的自然刷新普通怪物，accepted 后查终态并重新观察，记录客户端真实 level 前后增长及停止原因。

不得使用 GM/管理员/调试召唤、加经验/金币/物品、GM 传送或直接改 DB/Lua/坐标。静态刷新表仅为探索提示，不能当当前怪物存在证据。开发测试不能宣称真实练级完成。
