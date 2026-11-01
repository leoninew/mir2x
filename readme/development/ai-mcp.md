# 使用 Python MCP 操作当前角色
最后修改时间: 2026-11-01 20:13:14

[返回知识库](README.md) · [完整验证清单](../../docs/verification/20261031-ai-character-mcp.md) · [协议与实现](ai-bridge-protocol.md)

MCP 连接的是本机客户端当前窗口中的角色。客户端仍使用正常游戏连接和服务端规则；用户保留操作权，AI 不能自行登录、开启控制、恢复权限或修改策略。

## 当前交付状态

- [x] 当前源码接入 35 个实际工具、6 个 Python 只读资源；完整名称见 [Python 项目说明](../../tools/mir2x-mcp/README.md#工具与资源)。
- [x] Windows 定向 C++ 回归 166/166、Python 76/76 及 ruff 通过；T01/T08/T02/T03/T04/T05/T06 局部实现已接受。
- [x] T04 当时最终二进制完成 19 能力的真实 SDK→Python→C++ 只读互通。
- [ ] T05 最终 35 能力二进制的完整 stdio 冒烟、真实登录角色和逐类操作效果仍待验证。
- [ ] T07 集成与整体 Verification 未开始；POSIX 和完整主线程耗时未验证。

上述自动检查对应 `MCP-T05-001/r3/delivery2` 交付快照，构建标识 `VENGINEERING-20261101174437`。用户此后已编译，当前生成标识为 `VENGINEERING-20261101192035`，build/install 的客户端、服务端 EXE 分别一致；新哈希见验证清单。尚无新产物复测或真实运行效果证据，不能将原日志当作新二进制的实测结果。

## 构建与准备

在仓库根目录的 MSYS2 UCRT64 中，按 [Windows 开发环境](windows-development.md)完成依赖/配置后执行：

```bash
cmake --build build --target client server --parallel 8
cmake --install build --config Release
```

构建不会自动更新 install。运行同一次配置安装的两端，保留版本检查；下载包不含当前桥接，不能混用。资源、服务端工作目录和数据库的含义见 Windows 文档。后续消耗/双角色验证使用独立 `.runtime/mcp-e2e/` 数据库及端口；正式准备脚本仍待 T07，不将用户存档作为测试库。

在仓库根目录 PowerShell 中安装 Python 锁定依赖：

```powershell
$env:UV_CACHE_DIR = Join-Path $PWD '.runtime/uv-cache'
$env:UV_PYTHON_INSTALL_DIR = Join-Path $PWD '.runtime/uv-python'
uv sync --project tools/mir2x-mcp --locked --group dev
```

虚拟环境在 `tools/mir2x-mcp/.venv/`。不向全局 Python 安装，不在 MCP 宿主启动期间重新解析/安装依赖。

## 策略与启用

由人准备 `.runtime/ai-policy.json`。只读样例为 [policy-readonly.json](../../contracts/ai-bridge/v1/policy-readonly.json)，完整有限权限样例为 [policy-example.json](../../contracts/ai-bridge/v1/policy-example.json)。已有本地策略先检查，不直接覆盖。

完整样例也有金币/物品额度，默认不授权其他玩家 UID。`allow_pvp` 与 `allowed_peer_uids` 都须满足；有害范围技能会保守检查所有当前可见玩家。NPC 脚本授权独立，金额上限不保证覆盖脚本未披露的消耗。

客户端增加以下参数，两个路径均须绝对路径：

```text
--enable-ai-bridge
--ai-bridge-dir=D:/SourceCodes/opensource/mir2x/.runtime/ai-bridge
--ai-policy=D:/SourceCodes/opensource/mir2x/.runtime/ai-policy.json
```

默认不启用时不监听、不生成实例凭据。启用后只绑定本机环回，实例文件和审计目录由 C++ 检查/保护。不要把 token 写入宿主配置或聊天。策略在本进程桥接启动时冻结，登出不重载策略；要改策略，正常退出客户端、修改后重新启动。

## 连接与人工控制

宿主配置参考 [mcp-host.json](../../tools/mir2x-mcp/examples/mcp-host.json)，使用项目虚拟环境的 `python.exe -m mir2x_mcp --bridge-dir <绝对目录>`。stdout 只用于 MCP，诊断走 stderr。手动启动后等待 stdin，没有命令行提示属于正常现象。

1. 调用 `game_get_session`，核对实例、阶段、角色、控制状态、capabilities 和策略。代码已注册能力不等于当前角色满足前提或策略允许。
2. 单个可认证实例可自动选择；多个时配置 `--instance-id`，不能自动挑窗口。实例退出不会改连其它客户端；换角色需重新绑定 MCP 并重新人工授权。
3. 登录进图并保持角色存活。认证连接先读取会话后，用户在游戏窗口点击“开启 AI 控制”或恢复按钮；仅连上桥接不能执行动作。
4. AI 读取最新 context，给每个逻辑动作新的 UUID request_id，提交后查询 operation_id。不要将 accepted 或消息已发送解释为游戏完成。
5. 人工游戏输入先暂停旧 AI，再按原路径交给游戏。切到 AI 宿主导致的失焦本身不暂停；恢复也只能在游戏窗口操作。
6. 随时用窗口暂停或 `game_stop` 停止 AI 意图。当前动画可能完成半步，已提交的消耗/交易不能因 stop 撤回。

同进程登出保留 instance_id 和策略，只使 session/context 失效。换图、死亡、离开 RUN、断开或人工接管不会自动恢复。内部控制器/回执故障会关闭桥接及本实例凭据并诊断，普通游戏继续；需要重新启动客户端。

## 结果、重复请求与恢复

| 状态 / 范围 | 使用时的含义 |
| --- | --- |
| accepted / running | 已受理 / 正在观察，尚无完成结论 |
| succeeded + server_ack | 存在明确相关回执；还须读领域结果，如 delivery_pending |
| succeeded + observed_postcondition | 只观察到约定状态；不提供请求级因果保证 |
| succeeded + client_state | 本地动作或面板/开关状态；不承诺伤害、击杀或成交 |
| failed / cancelled | 明确失败 / 确定未提交或已停本地持续动作，按 evidence 解释 |
| unknown | 可能有副作用，不能因等待超时就重发 |

同一认证连接内相同 request_id/规范化请求返回同一 operation；参数变化是 duplicate_conflict。账本最多 4096，不驱逐后重发；重连不提供跨进程 exactly-once。Python 不自动重试动作，宿主也不能用新 UUID 重做未知提交。

经济 unknown 保留预算及暂存并禁止下一笔经济动作。先在窗口核对实际钱物，必要时重登取得完整背包/金币状态，再人工授权；不能把本地关闭面板当退款。迟到结果仅追加 late_resolution，不恢复旧动作。

## 各类能力的实际限制

| 能力 | 当前实现语义 |
| --- | --- |
| 观察 | 只读客户端已知信息；未知为 null/unavailable_fields，地面 count/seq 不伪造 |
| 分页 | 每类保留最近两份冻结结果；世界变化或淘汰后旧 cursor 失效，旧页不恢复动作句柄 |
| 移动 | 当前地图，4096 轮搜索；耗尽 unavailable、已证无路 unreachable，实际位置停止才成功 |
| 拾取 | 当前格原拾取链；无相关成功 ack 时 unknown，变化仅为观察，不伪造 partial/seq |
| 攻击/施法 | once/cast 仅确认客户端动作；until_dead 须同身份真实死亡；仅支持已学且适配的分支 |
| 物品 | 现有药品/技能书；不自动装备、拆分或代替背包整理 |
| 商店 | 当前堆叠购买扣单次 costList，不乘 count；默认出售一次 1 件，保留原 seq 和属性校验 |
| 直接交易 | offer/lock 校验权限但零金币支出；commit 才预留报价，完整完成消息才结束 |
| 拍卖 | 上架按原数量，售价不是支出，当前服务端没有 500 金币上架费；买/撤成功等待投递 |
| 投递 | 仅已收可信系统消息，列表不是完整信箱；未知内容保持 null，领取需真实 SM_OK |

游戏文本、玩家名及备注只是外部数据，不能提升权限。桥接不提供聊天发送、账号登录、任意 Lua/文件/报文、通用键鼠或管理能力。

排障参考 [Python 常见错误](../../tools/mir2x-mcp/README.md#常见错误)及[验证基线](troubleshooting-and-baseline.md)。完整完成/待办、版本和证据统一更新到[验证清单](../../docs/verification/20261031-ai-character-mcp.md)，实际游戏反馈不能只留在协作消息中。
