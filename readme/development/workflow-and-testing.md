# 开发与测试流程
最后修改时间: 2026-11-01 19:26:21

[返回知识库](README.md)

## 一个功能的日常开发闭环

1. 写清可观察行为：谁在什么状态下做什么、成功和失败时看到什么。
2. 用 [代码地图](code-map.md) 确定入口，沿一次请求追到 Actor、持久化和客户端响应。
3. 确认是否影响协议、静态 ID、存档、Lua 重放或资源；只改 UI 不应顺带重定义业务规则。
4. 修改源码后构建目标，安装到实际运行目录。
5. 选择相关的现有测试；新增回归应验证行为或实际风险，不机械复制实现。
6. 用同一次构建的客户端和服务端完成操作验证，记录数据是否正确保存、重登后是否一致。
7. 更新对应知识文档，说明通过了哪些验证及仍未覆盖的场景。

## 修改网络功能

按以下位置逐项查找，避免“能发出请求却无人处理”：

| 环节 | 文件或符号 |
| --- | --- |
| 请求/响应号与结构 | `common/src/clientmsg.hpp`、`servermsg.hpp` |
| 消息属性注册 | `_RSVD_register_clientmsg`、`_RSVD_register_servermsg` |
| 复杂业务数据 | `serdesmsg.hpp`、`sditem.hpp` 等及其序列化函数 |
| 客户端发起 | 对应 Process、界面或 `Client::send()` 调用方 |
| 未上线请求 | `ServiceCore::operateNet()`、`servicecorenet.cpp` |
| 已上线请求 | `Player::operateNet()`、`playernet.cpp` |
| Actor 内部协作 | `actormsg.hpp` 与目标类的 `on_AM_*` / 分派入口 |
| 客户端响应分派 | `Client::onServerMessage()` |
| 当前状态处理 | `process*net.cpp` 或 RUN 内对应处理 |

改消息布局后重编两端。不要以关闭版本检查替代兼容性验证；参见 [协议边界](runtime-and-protocol.md#构建签名的实际边界)。

## 现有测试地图

测试使用 Catch2 v3 和 CTest，目标默认 `EXCLUDE_FROM_ALL`。正常完整构建不会自动编译全部测试。

| suite 前缀 / target 后缀 | 覆盖入口 | 额外要求 |
| --- | --- | --- |
| `arbvar` | 通用变量容器 | 公共库 |
| `itemquality` | 物品品质 | 公共库 |
| `servermsg` | 服务端消息属性/编码相关 | 公共库 |
| `netio` | 连接拒绝、解析失败、对端断开 | 本机网络行为；部分环境可能跳过直接 loopback 分支 |
| `widget` | 控件层级、重排、重设父对象 | 不等于所有按钮交互已覆盖 |
| `xmltypeset` | 客户端排版和几何 | 已安装字体/emoji，正常 SDL 视频后端 |
| `itemgrant` | 玩家物品发放相关 | `server_objs` |
| `questdb` | 任务数据存储 | `server_objs` |
| `luarunner` | Lua 协程和远程调用运行器 | 已安装脚本；工作目录为 `install/server` |
| `queststate` | 任务状态机 | 同上 |
| `playerquest` | 玩家侧任务行为 | 同上 |
| `gridtrigger` | 地图格子触发 | 同上 |
| `questscripts` | 实际任务脚本回归与商人/NPC 合约 | 内嵌 Lua、源码中的 Lua fixture；模拟游戏世界 |
| `ai_bridge_protocol`、`ai_bridge_security` | 共同契约、真实环回传输、实例与策略 | Windows 使用真实 ACL/AccessCheck/junction；POSIX 另验 |
| `response_callbacks` | 单调响应 ID、作用域、期限、迟到与重入 | 独立共用回调管理器 |
| `ai_bridge_controller`、`ai_bridge_runtime` | 控制/预算/去重/事件/审计、按钮手势及意图所有权 | fake 时钟/适配器及实际 SDL 事件数据；不是实际游戏输入 |
| `ai_bridge_actions`、`ai_bridge_npc`、`ai_bridge_economy`、`ai_bridge_shop` | 快照/寻路/动作/NPC/经济纯模型 | 复用产品 helper；fake 地图/网络不能替代实机 |
| `ai_bridge_npc_gui`、`ai_bridge_economy_gui`、`ai_bridge_trade_gui`、`ai_bridge_shop_gui` | NPC/拍卖/交易/商店实际 GUI 共用入口 | `client_objs`、字体/emoji、dummy SDL/software、生成纹理和受控同步网络端口 |

target 名为 `test_<名称>`。来源：[公共测试](../../common/test/unit/CMakeLists.txt)、[客户端测试](../../client/test/unit/CMakeLists.txt)、[服务端测试](../../server/test/unit/CMakeLists.txt)、[测试基础设施](../../cmake/Mir2xTest.cmake)。

当前 [client/test/integration](../../client/test/integration/CMakeLists.txt) 和 [test/integration](../../test/integration/CMakeLists.txt) 只有注册示例，尚无客户端/服务端联动用例，不能把单元测试通过视为登录进图已验证。

## 常用命令

以下在 UCRT64 环境、仓库根目录、完成过初次构建后执行。

```bash
# 构建并运行已有的两组基础测试。
cmake --build build --target test_widget test_netio --parallel 8
ctest --test-dir build -R '^(widget|netio)/' --output-on-failure

# 修改任务脚本后，更新安装脚本，再运行真实内容的回归 harness。
cmake --install build --config Release
cmake --build build --target test_questscripts --parallel 8
ctest --test-dir build -R '^questscripts/' --output-on-failure

# 修改任务框架时按实际影响选择这些 suite。
cmake --build build --target test_queststate test_playerquest test_gridtrigger test_questdb --parallel 8
ctest --test-dir build -R '^(queststate|playerquest|gridtrigger|questdb)/' --output-on-failure
```

运行所有测试：

```bash
# check 依赖测试目标，会先构建测试再运行 CTest。
cmake --build build --target check --parallel 8

# 或由构建入口负责构建、安装并测试。
python3 ./build.py --build-dir=. \
    --triplet=x64-mingw-static --host-triplet=x64-mingw-static \
    --c-compiler=gcc --cxx-compiler=g++ --parallel=8 --run-test
```

`ctest` 自身不会替你构建缺失的 test executable。修改核心 C++ 或内嵌 Lua 后，也应先重新构建并安装相关代码；上面的纯内容脚本示例不表示所有脚本修改都无需编译。

`questscripts` 使用 vcpkg 的 Lua C 库运行 Lua 文件，不要求另装系统 `lua`。`luac -p` 至多做语法检查，也不会替代嵌在字符串中、稍后远程执行的 Lua 代码测试。

## 按改动选择验证范围

| 改动 | 合适的起步验证 | 仍需人工确认 |
| --- | --- | --- |
| 注册/改密按钮配置 | 客户端编译、相关控件检查 | 鼠标提交、回车提交、关闭按钮、成功/错误提示 |
| 网络连接行为 | `netio`，必要时消息 suite | 连不上、服务端未 Launch、断线和重连 |
| 任务 FSM 框架 | `queststate`、`playerquest`、`questdb`、`gridtrigger`，按影响加 `luarunner` | 重登、重启、过时对话与在途操作 |
| 任务或商人内容 | `questscripts` | 真客户端菜单和完整业务数据变化 |
| 物品定义或发放 | `itemquality` / `itemgrant`，按实际职责选择 | 堆叠、容量、实例属性、保存重登 |
| 协议或序列化 | 相关消息测试和收发双方构建 | 正常、拒绝、重复请求和旧版本行为 |
| AI 桥接、控制权或经济适配 | 下述定向 C++ 回归、Python 检查，按影响选取 | 真实角色、人工接管、钱物对照和异常路径；见验证清单 |

不要把某个 suite 名与业务覆盖画等号。真正修改之前需要读对应测试用例，确认它是否检查了本次变化。

## AI 桥接的定向回归

当前最终交付 `MCP-T05-001/r3/delivery2` 已通过下列 15 组共 166 项 CTest（38.81 秒），Python 76 项及 ruff 也已通过；本次文档维护未重跑。版本和证据索引见[验证文档](../../docs/verification/20261031-ai-character-mcp.md)。这不是全仓库 `check`，也不是 T07 真实角色验收。

在已配置的 MSYS2 UCRT64 仓库根目录按需复跑；首次增加源文件/依赖先按 Windows 文档配置。GUI 组件需已安装字体/emoji 资源，默认读取 `install/client/res`，或设置 `MIR2X_TEST_RES_DIR`。

```bash
cmake --build build --target client server \
    test_ai_bridge_protocol test_ai_bridge_security test_response_callbacks \
    test_ai_bridge_controller test_ai_bridge_runtime test_ai_bridge_actions \
    test_ai_bridge_npc test_ai_bridge_npc_gui \
    test_ai_bridge_economy test_ai_bridge_economy_gui \
    test_ai_bridge_shop test_ai_bridge_shop_gui test_ai_bridge_trade_gui \
    test_netio test_widget --parallel 8
ctest --test-dir build -R '^(ai_bridge_[^/]+|response_callbacks|netio|widget)/' --output-on-failure
```

Windows PowerShell 的 Python 检查（先按 [Python 项目说明](../../tools/mir2x-mcp/README.md)安装锁定依赖）：

```powershell
$env:UV_CACHE_DIR = Join-Path $PWD '.runtime/uv-cache'
$env:UV_PYTHON_INSTALL_DIR = Join-Path $PWD '.runtime/uv-python'
uv run --project tools/mir2x-mcp --locked --no-sync python -m pytest tools/mir2x-mcp/tests -q
uv run --project tools/mir2x-mcp --locked --no-sync ruff check tools/mir2x-mcp
uv run --project tools/mir2x-mcp --locked --no-sync ruff format --check tools/mir2x-mcp
```

- [x] 纯模型与真实 GUI 组件、真实环回传输和 Windows 权限检查已取得最终开发证据。
- [x] Python 官方 SDK 子进程 stdio 已检查 35 工具/6 资源，游戏桥接为 fake；另有至 T04 的真实 C++ 19 能力只读冒烟。
- [ ] 最终 T05 二进制真实 35 能力 SDK/宿主、登录角色和全部正常/失败路径。
- [ ] 桥接禁用的原 GUI/动作回归、真实整帧耗时、POSIX、正式辅助脚本和文档实机复现。

经济与双角色验证必须使用独立测试库/角色，记录请求/operation/context、前后钱物和回执/画面。阶段收尾后不因文档中列出命令自动启动测试；剩余逐项条件统一在验证文档 V01 至 V06。

## 调试、日志与定位

- Windows 的 g3log 默认写到进程工作目录；启动输出会打印实际文件路径。入口见 [log.hpp](../../common/src/log.hpp)。
- 客户端入口日志名为 `mir2x-client-v0.1`，服务端为 `mir2x-server-v0.1`；不要只查构建日志。
- `--debug-click-event`、`--debug-sdl-event` 等客户端参数，及服务端 trace 参数，具体名称以两端 `*argparser.hpp` 为准。
- 断点优先从 `doPostAccount()`、`sendLogin()`、`ServiceCore::net_CM_LOGIN()`、`Player::on_AM_BINDCHANNEL()` 等业务入口设置，再向下看基础设施。
- FLTK `.fl` 文件会生成 C++ 源码，修改 UI 定义应回到 `.fl`，不要只编辑构建目录中的生成文件。

## 最小开发记录

一个可复查的变更记录至少说明：源码提交和未提交补丁、构建类型、依赖/资源版本、实际运行的 EXE 路径、验证命令与结果。涉及存档时还要记录使用的是新建库还是迁移后的已有库。

初次构建的本机记录在 `.vcpkg/build-provenance.json`。后续新增文档或修改代码后，这个旧记录仍然只对应原先那次构建，不能直接拿来宣称新工作树已验证。
