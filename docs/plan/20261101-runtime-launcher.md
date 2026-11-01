# 计划：游戏运行 CLI、skill 与便携交付
最后修改时间: 2026-11-01 21:19:34

## 审查状态与执行

Accepted，修订 1。按用户本轮要求直接实施，主会话独立完成，不新增 agent、不创建 Git 提交、不启动用户现有存档的游戏进程。用户追加要求后已暂存此前 151 文件；用户随后将上一批提交为 68c4ba0，当前本功能与混合文件增量保持未暂存。与原 T07 的启动/文档子集关联，但不自动启动原整体游戏验收。

## 方案

1. 复用 `tools/mir2x-mcp` 的 Python 3.13/Hatchling/uv 项目，增加 `mir2x` console entry 与配置、进程管理模块；进程身份用 psutil 的 PID/创建时间/EXE 联合核对。
2. 默认 TOML 和 standard/readonly/movement 策略作为包资源，安装时同时输出可读文件；本地覆盖使用独立 local TOML，不被安装覆盖。所有相对路径相对于发行目录，仓库自动选择 install。
3. `up/server/client/status/stop/restart/doctor/mcp` 提供短命令，启动锁防并发；只管理已登记进程。Windows 服务器隐藏启动、客户端显示，停止先发窗口关闭，强停须显式参数。
4. 默认开启桥接、使用 finite standard 策略，CLI 自动给 C++ 绝对路径；凭据目录交给 C++ 安全创建。MCP 默认由宿主自动启动，不把诊断打印到 stdio stdout。
5. 同项目产出 wheel/sdist 和 Windows PyInstaller 独立 EXE。CMake 的 runtime target 接入客户端构建，安装输出管理入口、配置/策略、中文说明、仓库级 skill；不复制数据库、token、venv 或运行日志。
6. 真实服务器原窗口 close 回调忽略关闭消息；仅 auto-launch 模式补同 Quit 菜单的正常退出路径，手工窗口模式保留。只在隔离目录创建测试数据库验证。
7. 中文路径实际冒烟发现 narrow argv/路径转换问题；client 主入口通过 SDL_RunApp 获取 UTF-8，两个桥接参数显式转文件系统路径，并增加 ClientArgParser 回归。
8. 宿主配置由 CLI 按当前位置生成，`mcp register` 仅在用户显式执行时写 Codex 注册；不在本次实现中自动修改全局配置。

## 检查与验收

- 配置集中校验、空格/中文/搬迁路径、策略开箱可用、并发锁和 PID 复用。
- 受控子进程真实 start/status/stop/restart、占端口拒绝；测试只用独立临时目录和假服务，不触碰用户 DB。
- 原 Python 76 项回归和 ruff；wheel 干净环境安装/help；独立 EXE help/doctor 和官方 SDK stdio initialize/list_tools/list_resources/正常关闭。
- 单独安装 runtime component；支持运行管理所需的 client/server 窄修 EXE 增量同步到 install，保留资源、存档和 local 配置。记录产物、检查范围及真实游戏未验证项到对应验证文档。

## 当前交付与检查

- [x] CLI/配置/策略/skill/便携 EXE/wheel/sdist/CMake 安装全部落盘；源码增量保持未暂存。
- [x] Python 96/96（原 76 + 运行管理 19 + 冻结包认证 1）、ruff、skill 校验；C++ runtime 11/11。
- [x] 干净 wheel 安装与资源读取；真实服务器隔离正常关闭；中文/空格目录和无 Python PATH 的两端启动、SDK→独立 EXE→真实 C++ 35 能力与退出清凭据通过。
- [x] 窄修处理 server auto-launch close、client Windows UTF-8 参数/路径、PyInstaller 实际缺失 win32timezone。历史失败和对应最终证据见[独立记录](../verification/20261101-runtime-launcher.md)。
- [ ] 用户实际 LLM 宿主注册/新会话自然语言、真实登录角色与钱物等原整体验收、POSIX/整帧性能仍未验证。

不拆分 task，协调不适用。本轮仅开发交付及必要检查，不宣布原 T07 或整体 AI/MCP 验收完成。
