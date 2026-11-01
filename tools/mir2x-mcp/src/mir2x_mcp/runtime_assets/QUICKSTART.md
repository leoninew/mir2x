# mir2x 游戏与 LLM 快速使用
最后修改时间: 2026-11-01 20:50:05

在本目录打开 PowerShell。发行版自带 `mir2x.exe`、配置、策略和 skill，无需安装 Python/uv。命令中的 `./mir2x.cmd` 也可写成 `./mir2x.exe`。仓库根目录的同名入口使用 uv 运行最新 Python 源码，并自动使用 `install/client`、`install/server`。

## 第一次启动

```powershell
./mir2x.cmd doctor
./mir2x.cmd up
./mir2x.cmd mcp register
```

`doctor` 检查制品、资源和端口；`up` 等本地服务器玩家端口就绪后打开客户端。默认玩家端口 7000、节点端口 8000，工作目录、桥接路径和策略参数自动填写。首次不用编辑任何配置。服务器隐藏启动，客户端窗口显示。启动失败用 `logs server` 或 `logs client` 看输出，游戏自身日志在对应工作目录。

`mcp register` 是显式写入本机 Codex 配置的命令，只需注册一次；它把当前入口及绝对位置自动交给宿主，不要求手填完整路径。然后在当前目录重新开启 Codex 会话，输入 `/mcp` 确认 mir2x 已连接。若把整个制品目录搬家，重新运行此命令更新注册。

**在 Codex 的对话框中和 LLM 说话，client.exe/server.exe 内没有 LLM 对话框。** 例如：“使用 mir2x-runtime 帮我启动游戏，读取当前角色和周边，等我开启 AI 控制后移动到我指定的坐标。” `.agents/skills/mir2x-runtime/SKILL.md` 会随目录被 Codex 发现。

游戏账号和登录仍使用原游戏界面；本工具不创建账号、存储密码或代为选择角色。正常进入角色后，在游戏 AI 面板人工点击开启控制，再在 Codex 中下达目标。LLM 先读 `game_get_session`，通过 MCP 调用 `game_*` 工具操作当前角色。人工接管/暂停后，也必须在游戏窗口恢复。

## 其他 LLM 宿主

```powershell
./mir2x.cmd mcp config
./mir2x.cmd mcp config --format toml
```

JSON 用于接受 `mcpServers` 的宿主；TOML 用于 Codex 的 `mcp_servers` 配置。将生成内容放到相应宿主的 MCP 配置入口，重新连接即可。宿主负责模型和聊天；本工具只提供本机 stdio MCP 服务，不需要额外的模型 key。不要把 `mcp serve` 的 stdout 接到其他输出，它承载协议；通常也不用手工运行此命令。

## 日常管理

```powershell
./mir2x.cmd status
./mir2x.cmd client restart
./mir2x.cmd server restart
./mir2x.cmd logs server
./mir2x.cmd stop
```

`stop` 先正常关闭客户端再关闭服务器。工具只管理自己登记并核对 PID/创建时间/程序路径的进程，不接管你手工启动的进程，不抢占外部端口。若正常关闭超时，先处理窗口中的提示；确需强停才用 `stop --force`。服务器单独重启会断开现有角色连接。需要连接远端服务器时只启动 `client start`，不要用 `up` 启动本地服务器。

默认 **standard** 已允许现有普通角色动作与有限经济操作：单次金币 1000、会话金币 10000、单次数量 10、物品种类不限、NPC 对话允许。PVP 关闭、其他玩家白名单为空，因此双角色交易等仍受限制；不会默认开放给任意玩家。默认策略不等于绕过游戏中的人工开启。

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
```

所有相对路径相对于本目录；不需要为当前电脑填写绝对路径。管理状态/标准输出在 `.runtime/manager/`，本实例凭据在 `.runtime/ai-bridge/`。不要手工创建凭据文件或把 `.runtime` 分享给他人。复制制品到新目录时复制程序、资源、配置和文档即可；用户数据库/存档由你按原游戏方式管理。

## 当前验证边界

运行管理使用隔离子进程检查，MCP 使用官方 SDK 检查工具/资源。角色协议提供 35 工具/6 资源；实际可用能力以当前客户端 `capabilities` 和策略为准。真实登录、钱物、双角色交易、NPC 与移动动画等整体游戏验收仍见仓库 `docs/verification/20261031-ai-character-mcp.md`；管理工具通过测试不表示这些游戏效果已验收。动作 accepted 只是接收，unknown 时不要自动重试。
