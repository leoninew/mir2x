# 验证：游戏运行管理 CLI、skill 与便携交付
最后修改时间: 2026-11-01 21:22:34

## 状态与范围

Draft，运行管理功能的开发检查与交付记录。依据[意图](../intent/20261101-runtime-launcher.md)和[计划](../plan/20261101-runtime-launcher.md)实施，主会话独立完成，没有新增 agent。原 AI/MCP 的真实登录角色、动作与钱物验收仍按[原验证清单](20261031-ai-character-mcp.md)登记，不因本功能检查通过而整体放行 T07。

用户要求先暂存此前成果，再继续本功能：此前 151 文件已暂存；`pyproject.toml`、`uv.lock` 暂存的是 T06 原版（分别重算与原 manifest SHA256 一致）。用户随后已将上述批次提交为 `68c4ba0`。本功能新增文件及共享文件增量均保持未暂存，本会话没有创建提交。

## 实现事项

- [x] 新 `mir2x` Python console entry；仓库短入口 `mir2x.cmd`/`mir2x.py`，原 `mir2x-mcp` stdio 入口保留。
- [x] `up`、两端独立 start/stop/restart/status、整体 stop/restart/status、doctor、logs。
- [x] 相对发行目录的集中 TOML；自动选择仓库 install；可选 `mir2x.local.toml` 覆盖，未知字段/类型/受管理参数提前拒绝。
- [x] standard 默认策略与 readonly/movement 预设随包提供，首次无需手工准备策略。standard 限额为金币单次 1000、会话 10000、单次数量 10，PVP 关闭、其他玩家白名单空。
- [x] 自动桥接参数、持久化本次策略副本、由 C++ 安全创建凭据目录；人工开启/恢复和实例绑定规则保持。
- [x] PID/创建时间/EXE 联合核对、管理命令互斥、外部端口占用拒绝、正常关闭与显式强停分离；已登记服务器实际持有监听端口才算就绪。
- [x] `mcp serve/config/register`：宿主自动启动 stdio、生成当前位置配置、仅显式 register 写 Codex 配置；stdout 不混入诊断。
- [x] 可发现 `.agents/skills/mir2x-runtime/SKILL.md` 与中文 QUICKSTART；明确在 Codex/其他 LLM 宿主对话，游戏窗口负责登录和人工控制。
- [x] wheel/sdist、Windows PyInstaller 独立 EXE；包内 schema/预设资源明确包含，发行 EXE 不要求用户安装 Python/uv。
- [x] CMake `mir2x_runtime` 默认随 Windows client 构建；runtime 安装组件输出 EXE/短入口/配置/策略/skill/速查/Python 包/manifest，保留 local TOML，不复制用户 DB/凭据/日志/虚拟环境。
- [x] 服务器 auto-launch 模式正常关闭接原 Quit 行为，手工启动窗口原行为保持。
- [x] 中文搬迁路径所需客户端参数修复：SDL 从 Windows 宽命令行生成 UTF-8，桥接两个路径以 `char8_t` 明确解码；新增实际 ClientArgParser 回归。

## 开发检查与证据

| 检查 | 结果与证据 |
| --- | --- |
| Python 既有 76 项 + 新运行管理 19 项 + 冻结包认证 1 项 | 96/96 通过，14.20 秒；最终结果见 `.runtime/runtime-python-final-tests.log` |
| ruff | 源码、脚本、测试检查；最终结果归档到 `.runtime/runtime-ruff.log` |
| skill | skill-creator quick_validate 通过；未安装到用户全局 skill 目录 |
| wheel/sdist | `uv build` 成功；wheel 解包含 schema/全部配置策略/QUICKSTART；干净环境安装及 help/读取默认策略通过，见 `.runtime/runtime-wheel-install.log`、`runtime-wheel-help.log` |
| 独立 EXE stdio | 官方 SDK initialize、35 tools、6 resources、空桥接 unavailable 与 EOF 正常退出，见 `.runtime/runtime-exe-stdio.log` |
| CMake 构建/安装 | target 与 runtime component 已执行；测试前置 local sentinel 安装后不变，当前 install doctor 成功；见 `.runtime/runtime-cmake-build.log`、`runtime-install-check.log`、`runtime-install.log`、`runtime-installed-doctor.json` |
| 客户端增量与 CTest | UTF-8 主入口/路径已链接；runtime 11/11 通过（1.27 秒），见 `.runtime/runtime-client-build.log`、`runtime-client-ctest.log`；历史 166 不充当本轮入口证据 |
| 中文/空格搬迁与真实客户端 | PATH 仅 System32，无 Python；实际 EXE 执行 doctor/up/status/config/stop；官方 SDK→独立 EXE→真实 C++ 35 工具/6 资源/35 能力，phase=loading；未 RUN 查询 not_in_game、events/stop、凭据 1→0、两端均正常关闭，见 `.runtime/runtime-portable-smoke.json`、`runtime-portable-smoke.log`、`runtime-portable-mcp-stderr.log` |
| 真实服务器 | 仅隔离目录创建 DB，实际监听后 WM_CLOSE 正常退出，未 force；见 `.runtime/runtime-native-server-smoke.log` |
| 最终构建一致性 | client/server/runtime 测试/launcher 四目标检查为 `ninja: no work to do`，见 `.runtime/runtime-up-to-date.log` |
| 文档与仓库短入口 | 55 个本地文档链接、修改时间和代码围栏检查通过；仓库根 `mir2x.cmd --help` 通过，见 `.runtime/runtime-doc-check.log`、`runtime-source-entry-help.log` |
| 变更交接 | `git diff --check` 通过；用户提交上一批后暂存区为空，本轮增量保持未暂存；见 `.runtime/runtime-diff-check.log` 和交付清单 |

## 修复、失败记录与复验

- Windows 文件锁最初先读已锁区域导致第二进程 PermissionError；改为用文件大小初始化锁字节，并保证从偏移零解锁。真实子进程互斥回归通过。
- 初版测试用 venv 重定向进程持有窗口，登记 PID 没有窗口；夹具改用实际解释器。覆盖真实窗口正常关闭、无窗口超时保留与显式强停。
- Hatch force-include 与包内资源重复导致 wheel 失败；改用显式 artifacts，wheel/干净安装复验通过。历史日志不冒充最终结果。
- CMake 直接从未设置 CC/CXX 的 PowerShell 调用触发已有 chainload 环境要求；在 UCRT64 环境中设置原工程所需 CC/CXX 后构建。初次日志保留于 `.runtime/runtime-cmake-environment-history.log`。
- 真实服务器原 close 回调忽略 WM_CLOSE；仅 auto-launch 分支补 Quit 同源退出，在隔离真实服务器复验通过。
- 中文目录第一次真实客户端在参数解析时报非法字符序列；修复 Windows 命令行与路径编码，原失败保留于 `.runtime/runtime-portable-smoke-history.log`。修复后 runtime CTest 11/11、同路径真实未 RUN 链路通过，精确产物见版本节。
- 冻结 EXE 的空桥接检查未经过 Windows 文件时间转换，真实凭据认证暴露缺少 `win32timezone` 动态导入；仅补该实际缺失项，并增加可由 `MIR2X_FROZEN_EXE` 启用的 SDK+真实 ACL 认证回归。最终 96/96 与真实 C++ 链路通过；历史证据为 `.runtime/runtime-packaged-acl-history.log`、`runtime-packaged-acl-stderr-history.log`。
- GCC16/C++26 将已弃用 `u8path` 视为错误；改用 `std::u8string` 路径构造。测试新增 common 依赖后触发两处旧 SDL_Event 拷贝警告，改为 const 引用，保留断言。失败编译日志保留在 `.runtime/runtime-client-*-history.log`。

## 验收对应与待办

- [x] A01：短命令/配置/工作目录/端口与登记进程管理已实现，隔离真实子进程覆盖 start/status/restart/stop 与异常边界。
- [x] A02：默认策略、冻结边界、三种预设与无需首次手写配置已实现并通过配置校验。
- [x] A03：stdio 与配置生成已实测；skill 已校验。显式注册命令已提供，本轮未写用户 Codex 全局注册。
- [x] A04：wheel/EXE/安装组件/中文说明与 skill 已输出，local TOML 保留已实测。
- [x] A05：互斥、PID 复用、外部端口、正常关闭、超时显式强停已覆盖；用户现有 DB/存档未作为测试数据。
- [x] 修复后中文/空格目录中的独立 EXE → 真实 C++ 客户端认证、35 能力、未 RUN 查询、events/stop、正常退出凭据清理。资源目录用 junction 指向当前安装资源，服务器 map/scripts 是副本，DB 在隔离目录新建；这条不是纯离线资源分发或角色效果验收。
- [ ] 用户在实际 Codex/其他 LLM 宿主执行注册/配置后开启新会话，确认 `/mcp` 和自然语言目标交互。不得以 SDK 客户端替代这条证据。
- [ ] 正常登录角色、人工 AI 面板/接管、移动动画、NPC/战斗/消费、双角色交易与真实钱物；继续原验证清单 V01–V06。
- [ ] POSIX 实际运行、完整 ProcessRun/渲染帧性能、长期游戏与多人节点回归。

## 版本与处置

同一 CMake 配置保持 `VENGINEERING-20261101192035`。本轮 client/server 窄修后文件哈希变化，build/install 已同步，不以该标识代替文件哈希：

| 产物 | SHA256 |
| --- | --- |
| 独立 mir2x.exe | `4a5257f2bdce35c516d650385651b2a1965b7722ff6f40d5ea4a558d624afc7a` |
| client.exe | `89e8c51d233bf944d11f4bf0fd8f4e036073353a1160cf74251f7218cbc2a740` |
| server.exe | `0ed7a83d95e89e286c2ea12e68efea22b4939fa88f29af104df65995115cf96a` |

构建输出位于 `.runtime/runtime-package/`（本机 CMake cache 输出覆盖），默认工程目录为 `build/runtime-launcher/`；安装交付位于 `install/`，文件清单及哈希为 `install/runtime-manifest.json`。开发源码/产物/证据汇总使用 `.runtime/runtime-delivery-manifest.json`。所有测试仅操作本工具新建的受控进程；真实两端均正常 stop，没有 force，实例凭据已清零。用户现有 DB/存档未打开，未修改用户全局 Codex/skill 配置。本轮不修改历史 T02/T03/T04/T05/T06 manifest，不将此前 166/166 C++ 日志标为新客户端入口已回归。独立 EXE 的自动检查不要求 LLM key；模型和聊天由 MCP 宿主提供。
