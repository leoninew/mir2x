# 20261103 本地 .env 登录入口验证

最后修改时间: 2026-11-03 08:59:44 +08:00

本批为独立运行管理增量 `20261103-env-login/candidate2`，补充修复仓库/发行入口根分叉，替代候选1，待审查方安装与真实账号验证。候选1的53条源码/产物/证据已完整快照，原报告/清单保留历史。它不等于整体练级或游戏效果验收；既有 NPC 修复候选、旧任务交付记录及用户暂存区保持不变。

## 行为与使用

- 源码 `./mir2x.cmd up`、`client start/restart` 默认读取 `install/.env`；发行版读取游戏根目录 `.env`。不搜索当前工作目录或父目录中的其他账号文件。
- 仓库短入口默认将整个运行根选为 `install`，与发行入口共用配置、管理记录、bridge、策略和 `.env`。`--root` 使用明确指定目录且不追加 install，优先于 `MIR2X_HOME` 和入口默认；无安装布局报清晰错误。仓库根旧状态/配置不自动迁移或认领，可用明确 root/config 选择，所有权校验保留。
- 只读取小写 `username/password`，配置缺失或任一非空值缺失时保留手工登录；已提供的字段仍须满足格式限制。UTF-8/BOM、注释、可选 `export`、单/双引号字面值受支持，无变量展开或转义，每项最多 64 个 UTF-8 字节；重复键/格式/长度错误只给固定诊断。
- 凭据仅在客户端创建时通过原 `--auto-login` 参数传给客户端；服务器认证、角色选择、游戏窗口 AI 人工开启/恢复不变。凭据不进入 Settings、管理 JSON、管理日志或发行包。现有原生参数仍对有进程检查权限的本机程序可见，不应分享带命令行的转储。
- `--manual-login` 临时禁用，`[login] enabled=false` 持久禁用，`env_file="auto"` 默认；自定义相对路径以所选运行根解析，源码默认 install。`client.args` 拒绝直接配置 `--auto-login`。
- `.env` 校验在 `restart` 停止原进程前完成，配置错误保留原进程。已运行客户端的 `start/up` 不会重新登录或改变账号。

## 已执行的开发检查

PowerShell，工作目录 `D:/SourceCodes/opensource/mir2x`，锁定 Python 项目 `tools/mir2x-mcp`。缓存/临时数据全部位于仓库 `.runtime`。最终准确散列以 `.runtime/collaboration/20261103-env-login-root2-review.json` 为准。

| 检查 | 证据与范围 |
| --- | --- |
| Python 全量 | `uv run --project tools/mir2x-mcp --locked --no-sync pytest tools/mir2x-mcp/tests -q`，设置 `MIR2X_FROZEN_EXE` 为新包，143/143、36.83秒；包含首次认证固定实例保护 |
| 两个短入口 | 实际仓库.cmd+uv、发行.cmd+新EXE，共用真实隔离登记进程：重复up同PID且already_running，status相同，实际客户端bridge参数相同，mcp config/拦截外部写入的register同根；有/无.env均通过。显式root、环境覆盖、缺/空install、源根旧配置不误用均覆盖 |
| 现有环境只读 | 09:01:00源短入口与原install EXE的status均报告server32664/client59116 running，mcp config均绑定同一install根；只执行status/config，未start/stop/restart/install/auth，也未读取.env |
| 登录入口 | source/frozen 两入口各覆盖默认登录、手工跳过、文件缺失、配置禁用；真实隔离子进程校验收到的参数，管理状态/输出不含合成凭据 |
| 拒绝与保留 | 格式/Unicode/字节边界、引号/重复键、关闭前预检、spawn 错误脱敏；无效配置不停止原隔离进程 |
| Ruff | `ruff check tools/mir2x-mcp`、`ruff format --check tools/mir2x-mcp` 通过 |
| Skill | `skill-creator/scripts/quick_validate.py` 通过；项目环境缺少其 PyYAML 依赖，改用已有系统 Python 执行，未安装新依赖 |
| 制品构建 | 现有 `mir2x_runtime` 目标生成 wheel/sdist/独立 EXE/stage；首次 shell 缺 CC/CXX 的重生成失败保留日志，按现有 gcc/g++ 环境修复后通过，无 C++/server 编译 |
| 发行文件 | 11条 stage 文件逐项散列一致，wheel/sdist 含新模块与空白示例，不含 `.env`/`.runtime`；构建后运行管理目标 no-work |
| 官方 SDK | 使用 stage EXE，由 SDK 作为 stdio 宿主启动，35工具/6资源；空隔离桥接返回 unavailable，EOF正常。另有 fake 游戏桥接认证测试，均不等于真实游戏登录 |

候选2日志位于 `.runtime/collaboration/20261103-root2-*`；候选1 `.runtime/collaboration/20261103-login-*` 保留历史。最终包与对应日志由新的独立审查清单绑定，不用旧 C++169项或候选1 Python138项冒充本批验证。

## 安装后的真实验证清单

由审查方正常关闭/替换/启动客户端，本实现会话未安装制品、未读用户 `.env`、未操作真实角色或全局注册。

- [ ] 用已有 `.env` 通过新短入口启动，确认原服务器认证成功并进入正常角色选择流程；不记录账号密码或命令行。
- [ ] 缺少凭据或显式 `--manual-login` 时确认原手工登录仍可用。
- [ ] 客户端重启后，结束宿主旧 MCP 连接，由宿主重新启动 stdio（或重新开启 Codex 会话），调用 `game_get_session` 核对新 instance/context/角色。显式实例 ID 需用户明确重新绑定；保留 `test_pinned_instance_never_switches` 保护，不静默换角色。
- [ ] 新实例由用户在游戏窗口人工开启 AI；注册成功、进程存在或旧实例 active 均不能代替新实例授权与实际工具调用。
- [ ] 普通地图探索/自然刷新怪物/正常战斗，accepted 后查询终态并重新观察，记录真实 level 前后变化及停止原因。GM/管理员/调试召唤、加经验/物品/金币、GM传送、Lua/DB/坐标修改均禁用；不访问 GM 召唤商人重测。

本批未扩展 experience、NPC 菜单或 v1 契约，未改变 PVP/经济策略。真实登录、练级闭环、钱物/双角色/完整游戏 GUI、POSIX 均未由本批开发检查证明。
