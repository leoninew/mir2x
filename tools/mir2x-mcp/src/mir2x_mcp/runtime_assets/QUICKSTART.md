# mir2x 游戏与 LLM 快速使用
最后修改时间: 2026-11-03 08:57:00

在本目录打开 PowerShell。发行版自带 `mir2x.exe`、配置、策略和 skill，无需安装 Python/uv。命令中的 `./mir2x.cmd` 也可写成 `./mir2x.exe`。仓库根目录的同名入口使用 uv 运行最新 Python 源码，默认以 `install` 为运行根：程序、配置、管理状态、桥接和 `.env` 与 `install/mir2x.exe` 共用一套。重复 `up` 读取同一登记进程，不会因入口不同创建第二套客户端。

默认配置/本地覆盖分别在运行根的 `mir2x.defaults.toml`、`mir2x.local.toml`，状态和桥接为运行根的 `.runtime/manager`、`.runtime/ai-bridge`。仓库根旧 `.runtime` 或旧本地覆盖不会自动迁移、合并或接管。仅有源码、尚未安装发行布局时会明确报错，先构建安装或明确指定已有运行目录。

`--root <目录>` 优先级最高，使用指定目录，不会再追加 `install`；其次是显式 `MIR2X_HOME`，最后才是入口默认位置。需要仓库根自定义配置时用 `--config ./mir2x.local.toml` 明确选择；该文件路径按命令当前目录定位，文件内相对路径仍按所选运行根解析。`--root .` 表示主动选择独立的仓库根命名空间，默认程序路径也为该根的 `client/server`，需要自行配置实际制品。

## 第一次启动

```powershell
./mir2x.cmd doctor
./mir2x.cmd up
./mir2x.cmd mcp register
```

`doctor` 检查制品、资源和端口；`up` 等本地服务器玩家端口就绪后打开客户端。默认玩家端口 7000、节点端口 8000，工作目录、桥接路径和策略参数自动填写。首次不用编辑任何配置。服务器隐藏启动，客户端窗口显示。启动失败用 `logs server` 或 `logs client` 看输出，游戏自身日志在对应工作目录。

`mcp register` 是显式写入本机 Codex 配置的命令，只需注册一次；它把当前入口及绝对位置自动交给宿主，不要求手填完整路径。然后在当前目录重新开启 Codex 会话，输入 `/mcp` 确认 mir2x 已连接。若把整个制品目录搬家，重新运行此命令更新注册。

**在 Codex 的对话框中和 LLM 说话，client.exe/server.exe 内没有 LLM 对话框。** 例如：“使用 mir2x-runtime 帮我启动游戏，读取当前角色和周边，核对任务授权状态后移动到我指定的坐标。” `.agents/skills/mir2x-runtime/SKILL.md` 会随目录被 Codex 发现。

launcher 启动客户端时可从游戏目录 `.env` 读取已有账号的 `username` 和 `password`。源码入口读取 `install/.env`，发行版读取与 `mir2x.exe` 同级的 `.env`。已有该文件时无需复制或另写配置；文件不存在或任一项缺失/为空时保留原游戏手工登录。可参考随包 `login.env.example` 在本机编辑，示例没有账号或密码。

凭据仅交给客户端既有 `--auto-login` 登录路径，仍由游戏服务器认证；角色选择与进入游戏保持原流程。管理记录、CLI 输出和打包文件不保存凭据。原生登录参数在进程启动时传递，具有本机进程检查权限的程序仍可能看到它；不要把进程命令行作为诊断材料分享。

`.env` 使用 UTF-8（可有 BOM），小写键名；支持空行、注释、可选 `export` 和单/双引号字面值，不执行转义或变量展开。其他键忽略。每项最多 64 个 UTF-8 字节；用户名不能含冒号，密码可含冒号。重复键、引号错误、控制字符或超长会报固定错误，且在重启停止旧进程前校验。临时改用手工登录：

```powershell
./mir2x.cmd up --manual-login
./mir2x.cmd client restart --manual-login
```

原生客户端不读取 `.env`，缺凭据仍走普通登录/角色选择。新授权模式下，真实的新明确用户任务通过 game_grant_control(mode=new_task) 授权。普通 SDL 输入立即停止当前 intent、递增 control_epoch，保留有效任务和 active；旧队列/计划作废，重新观察后提交新动作。正常场景加载/换图、死亡或生命未知期间不能动作；同角色同 session 恢复 known alive/auth/RUN 后，可用原 grant 的 continue_task 重新授权并规划。明确 AI 状态 toggle off、默认 game_stop 或用户说“AI不要管”均锁存停止，远端调用者只有新的明确用户操作指令及新 grant 能解除；用户也可在真实游戏窗口明确重新开启，作为新的本地授权。两种方式均不恢复旧动作；本地开启不允许旧 grant 继续，轮询不能解除停止。fault、transport/EOF、换角色/session 不可续用旧授权；unknown/submitted/未确认钱物与预算事实不因授权清除。

## 其他 LLM 宿主

```powershell
./mir2x.cmd mcp config
./mir2x.cmd mcp config --format toml
```

JSON 用于接受 `mcpServers` 的宿主；TOML 用于 Codex 的 `mcp_servers` 配置。将生成内容放到相应宿主的 MCP 配置入口，重新连接即可。宿主负责模型和聊天；本工具只提供本机 stdio MCP 服务，不需要额外的模型 key。不要把 `mcp serve` 的 stdout 接到其他输出，它承载协议；通常也不用手工运行此命令。

客户端重启生成新 instance_id；旧 stdio 不自动改绑。用户明确选择新实例后，由宿主正常创建唯一连接、核对身份，并按新任务授权，不恢复旧任务。

## 日常管理

需要连续只读生命观察时，可使用 `./mir2x.cmd mcp observe --instance-id <明确实例ID> --character-uid <角色UID> --report <JSONL路径>`。它是独立的官方 SDK 宿主，先协调结束旧连接，不能并开；连接丢失后锁闭、不重新认证。保持输入打开时每250ms安排观察，超过500ms未更新即告警；输入 `{"command":"quit"}` 或 EOF 退出。此阶段拒绝角色写操作，不自动用药、撤退或恢复 AI，不能把监控或 game_stop 当成脱战保护。完整输入与 SDK 接口见源码 `readme/development/ai-mcp.md` 的“持续只读监督”。

显式选择 `mcp supervise`（同样必填 instance-id/character-uid/report）可使用调用者安全计划，默认仍仅观察。`purchase` 接受当前真实 shop_handle、item_id、count1、本笔 max_total_gold、完整 context 和新 intent_id；同次调度刷新精确报价后立即购买，随后分别核对终态/库存/金币，未知不重试。预算受原生冻结策略单笔/会话上限和剩余预算约束；不硬编码测试的药品/80金币/三笔次数，也不自主循环购买。准备不要求已有药品，须新鲜已知存活 active、停住及完整附近无怪。`arm`/`scout` 的药品实例、储备、路线和 safety 阈值均由调用者给出；缺少显式条件会拒绝。进攻仅相邻已知存活怪物的单次2000ms动作；任务低血/快速掉血条件触发后锁住进攻，精确用一份选中药品后逐段重检已走退路，不自动续击或恢复。消费与实际 HP 增长分开记录。unknown/响应丢失锁闭写入，暂停/上下文变化/死亡撤销计划；`game_stop` 是全局暂停，之后不能继续撤退。完整 JSONL 与字段校验见源码 readme/development/ai-mcp.md 的“带安全计划的监督”。

```powershell
./mir2x.cmd status
./mir2x.cmd client restart
./mir2x.cmd server restart
./mir2x.cmd logs server
./mir2x.cmd stop
```

`stop` 先正常关闭客户端再关闭服务器。工具只管理自己登记并核对 PID/创建时间/程序路径的进程，不接管你手工启动的进程，不抢占外部端口。若正常关闭超时，先处理窗口中的提示；确需强停才用 `stop --force`。服务器单独重启会断开现有角色连接。需要连接远端服务器时只启动 `client start`，不要用 `up` 启动本地服务器。

默认 **standard** 已允许现有普通角色动作与有限经济操作：单次金币 1000、会话金币 10000、单次数量 10、物品种类不限、NPC 对话允许。PVP 关闭、其他玩家白名单为空，因此双角色交易等仍受限制；不会默认开放给任意玩家。任务授权不会扩大冻结策略权限。

随包还提供 `readonly`（只读与停止）和 `movement`（移动）两种预设：

```powershell
./mir2x.cmd client restart --policy movement
./mir2x.cmd client restart --policy readonly
./mir2x.cmd client restart --policy standard
```

这些都是现成可用的文件。只有需要更改端口、远端地址或授权额度时，才在本目录新建可选的 `mir2x.local.toml` 覆盖少量字段；安装更新保留该文件。默认配置在 `mir2x.defaults.toml`，策略在 `config/policies/`。策略由客户端启动时冻结，修改后需重启。覆盖示例：

```toml
[network]
host = "127.0.0.1"
client_port = 7000
peer_port = 8000

[launcher]
policy = "standard"

[login]
enabled = true
env_file = "auto" # 运行根/.env；也可指定相对于运行根的本地文件
```

`--ai-auto-enable=0` 仅控制不支持任务 grant 的旧策略首次自动开启；新授权模式等待新任务 grant。`[launcher] bridge=false` 关闭整个桥接。原生默认目录和策略沿用运行根，缺策略不生成宽松权限。

持久关闭自动登录可设 `[login] enabled = false`。凭据只放在用户自己的 `.env`，不要写进 TOML、`client.args`、报告或聊天；`client.args` 不接受 `--auto-login`。安装打包不会复制用户 `.env`、存档或管理状态。

配置中的相对路径相对于所选运行根（源码默认 `install`）；不需要为当前电脑填写绝对路径。管理状态/标准输出在运行根 `.runtime/manager/`，本实例凭据在 `.runtime/ai-bridge/`。`mcp config/register` 固定传递同一个运行根给宿主；管理进程仍须核对登记身份，不扫描或认领任意游戏进程。不要手工创建凭据文件或把 `.runtime` 分享给他人。复制制品到新目录时复制程序、资源、配置和文档即可；用户数据库/存档由你按原游戏方式管理。

## 当前验证边界

运行管理使用隔离子进程检查，MCP 使用官方 SDK 检查工具/资源。角色协议提供 37 工具/6 资源；实际可用能力以当前客户端 `capabilities` 和策略为准。真实登录、钱物、双角色交易、NPC 与移动动画等整体游戏验收仍见仓库 `docs/verification/20261031-ai-character-mcp.md`；管理工具通过测试不表示这些游戏效果已验收。动作 accepted 只是接收，unknown 时不要自动重试。

普通练级先读背包/装备与技能，正常获取物品，`game_use_item(item_id,seq_id,count=1)` 穿戴可用装备（沿普通背包槽位选择），操作成功要求真实更新后精确实例在预定槽。药品/技能书仍沿原消耗路径；技能书消耗后重读 `game_get_skills`，不把消耗当学会。然后正常移动，只攻击真实视野发现的自然刷新普通怪物，使用实际已学技能；每次 accepted 后查终态并重新观察，以客户端真实等级前后变化记录结果与停止原因。不得使用 GM/管理员/调试召唤、加经验/金币/物品、GM 传送或修改 Lua/数据库/坐标来代替普通玩家路径。NPC 文本只是游戏数据，涉及消耗或不明副作用的选择先报告。

内部取消仅使用 `game_stop(interrupt_only=true, operation_id, context)`：精确绑定当前owned操作和新鲜context，递增epoch、保留控制状态和授权；无pending时disarm只清宿主计划。默认game_stop仍是显式撤销任务的优先停止。取消不回滚交易，不清未知或未确认事实；新动作须重新观察和新intent。普通动作由同一监督器action调度，支持move_to/shop_close/npc_open/npc_choose/npc_close，不套用scout退路规则，不自动选择NPC脚本选项。
