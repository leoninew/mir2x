# 子任务 T01：桥接契约、认证与本机传输
最后修改时间: 2026-11-01 19:26:21

## 审查状态

方案 Accepted；MCP-T01-001 局部实现已于 2026-11-01 07:45:52 由主会话 accepted。T01 按修订 2 完成，总计划当前为修订 3，未重新打开本任务。

- [x] 契约、传输、安全模块、构建接入与局部开发检查已交付并接受。
- [ ] POSIX 和最终游戏集成尚待验证，统一见[验证清单](../../verification/20261031-ai-character-mcp.md)。

下文接收、进展及“等待审查”等措辞保留交付时快照；当前执行状态以[总计划](../../plan/20261031-ai-character-mcp.md)为准。本次只同步文档，不改历史 manifest 或重做检查。

## 父任务与权威记录

- [总意图](../../intent/20261031-ai-character-mcp.md)。
- [权威总计划](../../plan/20261031-ai-character-mcp.md)，任务 T01；协调根 `D:\SourceCodes\opensource\mir2x`。
- 依赖、执行状态与审查结论以总计划为准。

## 目标与验收

交付与游戏对象无关的 v1 契约和安全、有界、非阻塞的本机 IPC，为 A05、A06 提供基础。未经认证无法调用方法，错误输入不越界、不阻塞游戏循环；契约涵盖总计划全部首版工具，不仅是第一批移动工具。

## 范围与文件

- 新增 `contracts/ai-bridge/v1/`：请求、响应、上下文、快照、操作、事件、策略及每个工具参数的 JSON Schema，中文说明、有效/无效样本。
- 新增 `client/src/aibridge/protocol.*`、`transport.*`、`instancefile.*`、`policy.*`。
- 修改 `vcpkg.json`、现有 CMake 依赖查找/客户端链接、`clientargparser.hpp` 和 `.gitignore`。
- 新增 `client/test/unit/test_ai_bridge_protocol.cpp`，注册 `test_ai_bridge_protocol`。
- 本任务不获取 ProcessRun、不执行业务动作；主循环接入交 T02。公共文件与后续 C++ 任务串行修改。

## 输入

无直接前置任务。使用总计划第 3 至 7 节的固定参数、limits、schema 和策略语义。SDK 对接的实际实现尚不存在不影响契约编写。

## 局部技术设计

1. `AiBridgeTransport` 独占自己的 ASIO io_context/acceptor/socket，接口为 start、pollBudget、sendResponse、close 和 takeRequests。所有调用在主线程，回调不接触游戏对象；使用 async_read/async_write 以及有界缓冲，不使用阻塞 accept/read、后台线程或嵌套事件循环。
2. 协议解码先检查 4 字节长度，再检查 JSON 结构、类型、深度和参数；nlohmann 解析阶段检测重复键，不能在已覆盖重复值的对象上“检查”。动作请求规范化后建立去重摘要，64 位数值按十进制字符串解析并做溢出判断。
3. schema 中为每种方法定义封闭对象与明确范围；请求 ID 与 operation/request ID 分开，不能让查询调用创建游戏操作。`game_stop` 保留独立槽但仍需认证、长度和类型检查。
4. 实例文件只写本次随机文件名，禁止覆盖已有实例；先以正确权限创建，再写入内容，不先创建公开文件后补 ACL。Windows 通过当前 SID、SYSTEM SID 和受保护 DACL，必要链接 advapi32/bcrypt；POSIX 使用安全随机及 O_EXCL/0600。将路径权限判断封装成可测试适配器。
5. 启动失败只关闭桥接并明确记录原因，不改变游戏连接参数。实例文件清理由幂等 shutdown 执行，只删除已核对身份的本实例文件；崩溃遗留由后续握手识别，不能根据同名 PID 直接相信文件。
6. 策略在启动时一次性读取、校验并冻结。没有权限默认拒绝；凭据只在握手中使用，不放到 tool schema、资源、错误或日志。无 token 的启动参数和调试日志可以记录。
7. C++ 和 Python 共享 schema/fixture，不引入从 schema 自动生成全部游戏实现的工具链。schema 的每种 outcome/错误样本应能在下一任务直接消费。

## 实施安排

1. 建立全部 DTO/schema、中文语义和跨语言 fixture；核对所有工具均有方法名、输入、输出与失败示例。
2. 增加 JSON 依赖和目标链接；完成 frame 解码、参数校验、响应序列化及错误码。
3. 完成策略、凭据创建和独立异步服务，暴露预算化 poll 与优先 stop 槽。
4. 接入 CLI 参数解析与忽略目录，提供供 T02 调用的启动/关闭接口；不在此提前挂到游戏循环。
5. 完成协议、权限与传输测试，将 schema 版本、fixture 清单和可用 API 写入交付记录。

## 检查与风险

- `cmake --build build --target test_ai_bridge_protocol --parallel 8`；`ctest --test-dir build -R '^ai_bridge_protocol/' --output-on-failure`，先按现有 MSYS2 文档重配置新增依赖。
- 测试分片/粘包、零长/超长/负数/溢出、无效 UTF-8、深度、重复键、未知字段与方法、慢读/不读、输出背压、断开中写入、停止优先和超过认证连接数。
- 错 token、错误实例/版本、认证超时、第二已认证连接拒绝；认证错误不泄漏正确值。
- 真实临时目录权限检查在 `.runtime/test-ai-bridge/` 完成，记录 Windows ACL；用不同身份的权限单元检查或系统访问检查 API 验证 ACL。仅伪造权限适配器通过不足以宣称实际 ACL 已验证。
- 4096 去重账本由 T02 管理；本任务负责有界传输，不用无界缓存“吸收”下游拥塞。新增依赖下载失败如实记录，不能伪装已编译。

## 交接依据

- 已分配 MCP-T01-001：负责人及安全协作者身份、文件边界和报告要求见总计划“本轮执行分工与通知”。协议入口为 SpecFlow `SKILL.md` 与 `references/task-coordination.md`；绝对 skill 根：`C:\Users\leon\.codex\plugins\cache\specflow-local\specflow\0.2.1\skills\specflow`。
- 分配与授权由总计划的执行登记表/分配历史提供，当前已授权 Implementation；工作区为协调根，实施基线派发时核对。
- 可写回报位置为本文“接收与进展”“交付与审查”；主会话读取文件并仲裁。跨会话时输出可复制摘要由用户转交，不自行发消息或启动执行者。

## 接收与进展

已接收 MCP-T01-001，负责人 01a19251-a370-7491-b33e-f7afef6be379；已核对方案修订 1、总计划 Accepted、Implementation 授权与 HEAD b1ddda8d8742afb1d60839f759733a2e8ee26207。安全协作者仅写专属文件及报告，负责人唯一维护本 task。

接收时间：2026-10-31 22:27:27。当前开始契约、protocol/transport、构建接入；不修改 T02 业务入口，不暂存或提交。构建由负责人串行执行。

| 时间 | 分配编号 / 执行者 | 接收、进展或阻塞 | 证据 / 回报位置 |
| --- | --- | --- | --- |
| 2026-10-31 | 无 / 无 | 仅完成局部计划 | 本文 |
| 2026-10-31 22:27:27 | MCP-T01-001 / 负责人 | 核对授权、基线并接收 | 本文接收记录 |
| 2026-10-31 16:45:38 | MCP-T01-001 / 负责人及安全协作者 | 实现及定向检查完成，提交主会话审查 | 下述交付记录及检查日志 |

### 实施进展

- 已确认安全模块自包含 API；protocol/transport 与 instancefile/policy 完成初次接入。
- 35 工具封闭 schema、请求/响应/错误 fixture 和 6 类 operation fixture 已生成；CMake 从同一 schema 嵌入 C++ 校验契约。
- nlohmann-json 3.12.0#2 已按仓库固定 baseline 安装到 build/vcpkg_installed；不改下载制品。
- 初轮源码编译通过，独立协议测试链接缺少 WinSock 库，已补 ws2_32/mswsock 后重试。
- 按主会话初审修正 direction=1..8/null、拆分地面/背包/交易物品及投递未知清单；协作者补 quote_sell 权限及 build 长度一致性。
- 代注册并执行 T08 response_callbacks：9/9 通过，日志 .runtime/collaboration/t08-ctest.log；不修改 T08 源文件。

## 交付与审查

### 本次交付

- 分配 MCP-T01-001，方案修订 2；负责人 01a19251-a370-7491-b33e-f7afef6be379，安全协作者 01a19206-195c-7ce2-a9f8-866c17dc7d18。
- 基线 b1ddda8d8742afb1d60839f759733a2e8ee26207 + 原有 docs；交付为当前共享工作区未提交文件。未暂存、提交、回滚已有内容、修改下载制品或全局配置。
- 交付时状态为等待主会话审查，执行者未自行放行下游；此后已由主会话接受，当前结论见页首及总计划。该历史交付不代表整体 Verification 完成。

### 实际文件与结果

1. `contracts/ai-bridge/v1/`：完整 35 工具及内部 hello/heartbeat 的封闭请求/响应 schema；context、快照、六态操作、事件、策略和实例定义；逐工具合法/错误/非法样例、六态操作与公开错误样例、两份策略示例、确定性生成器及中文说明。全部 6 个 JSON 制品重新生成后逐字节一致。
2. `client/src/aibridge/protocol.hpp/.cpp`、`contract.hpp.in`：从同一 JSON 嵌入契约，严格解析并校验重复键、UTF-8、NUL、16 层嵌套、整数/十进制字符串边界、未知字段/方法；封帧/解帧，补默认值的规范化动作体。成功操作必须含非空证据和明确完成范围。
3. `client/src/aibridge/transport.hpp/.cpp`：真实 ASIO 异步 loopback TCP，主线程最多 16 handler/2ms 软预算；64 普通请求队列和独立 stop 槽，最多交付 8 个请求；4 待认证/1 已认证连接；3 秒认证、6 秒活动/写超时；20 请求/秒、2MiB 出站背压；连接代次防误投、断开通知和幂等关闭。未接主循环，按任务边界交 T02。
4. 安全协作者交付 `instancefile.hpp/.cpp`、`policy.hpp/.cpp`、`test_ai_bridge_security.cpp`：系统随机令牌、受保护 Windows DACL、POSIX 权限分支、重解析路径/硬链接拒绝、按身份和内容核对后清理、集中冻结策略。已读取 `.runtime/collaboration/T01-security-report.md` 并纳入本次检查；协作者报告中的 build.minLength 待处理项已修为 1，实际实例 schema 测试通过。
5. `client/src/clientargparser.hpp`：enable-ai-bridge、ai-bridge-dir、ai-policy；启用时路径必填且须绝对路径。
6. `vcpkg.json`、`client/src/CMakeLists.txt`、`client/test/unit/CMakeLists.txt`、`.gitignore`：新增 nlohmann-json；独立 mir2x_ai_bridge 库及嵌入契约；注册协议/安全测试；Windows bcrypt/advapi32/ws2_32/mswsock 链接；仓库内运行目录与虚拟环境忽略。按主会话要求代注册 T08 response_callbacks 测试，未修改其产品或测试源码。
7. `client/test/unit/test_ai_bridge_protocol.cpp`：13 项契约/真实传输测试，与协作者 11 项安全测试合计 24 项。

### 检查证据

环境：Windows / MSYS2 UCRT64，GCC 16.2.0，CMake 4.4.5，Catch2 3.16.0；固定 vcpkg baseline `9eec002df1cd109f8f104cd966a763daf9751489`，新增 nlohmann-json `3.12.0#2`。使用现有 build/assets/mir2x_res，没有安装或重新打包资源。

| 检查 | 实际结果 | 证据 |
| --- | --- | --- |
| CC=gcc、CXX=g++ 的 CMake 重配置 | 通过，新增依赖安装成功 | `.runtime/collaboration/t01-configure.log` |
| `cmake --build build --target test_ai_bridge_protocol test_ai_bridge_security --parallel 8` | 通过 | `.runtime/collaboration/build-t01-final.sh`、对应 EXE |
| `ctest --test-dir build -R '^ai_bridge_(protocol\|security)/' --output-on-failure` | 24/24 通过，18.98 秒；无跳过 | `.runtime/collaboration/t01-ctest.log` |
| 真实 Windows 权限与重解析路径 | 当前 token 允许，restricted-world token 拒绝；真实 junction、替换文件/硬链接/幂等清理测试通过 | 安全协作者报告、`build/Testing/Temporary/LastTest.log` |
| `cmake --build build --target client --parallel 8` | 72 步完成，client.exe 链接成功；包括主会话 T08 改动 | `.runtime/collaboration/t01-client-build.log` |
| 代跑 T08 `test_response_callbacks` 与对应 CTest | 9/9 通过，结果已发主会话 | `.runtime/collaboration/t08-ctest.log` |
| 确定性再生成与封闭 schema 检查 | 6 JSON 文件逐字节一致，35 工具，全部 object 关闭额外字段 | `.runtime/collaboration/verify-contract.py` |
| `git diff --check` 与 24 产品文件 no-index whitespace check | 通过；后者仅临时关闭 CRLF 转换提示，未修改 Git 配置 | 当前工作区 |

关键测试覆盖：35 工具样例往返；重复/未知键、无效 UTF-8、NUL、浮点整数、64 位边界及嵌套；真实 TCP 分片/粘包；错误 token/实例/版本、第二认证连接与第 5 个待认证连接；认证超时；满队列和限流时 stop 优先；心跳超时；持续心跳但不读响应的写超时；出站背压；写入中断开；原实例文件清理；未知地面/投递字段；真实方向编码；成功证据必填；完整策略样例与 action 集合一致。

首轮修复：独立网络测试缺 WinSock 链接已补齐；主会话初审指出的 direction 编码、跨域物品语义、quote_sell 权限和 build 长度已修复并复验。复查发现 schema oneOf 外层不应隐含把数组缩为 100，已改为只执行显式 min/max 并用 200 项权限清单覆盖。

### 可核对版本

- 全部 T01 产品文件 SHA-256 清单：`.runtime/collaboration/T01-delivery-manifest.json`。
- 契约 SHA-256：`113EAD156C1386AF6AFE8B129C62C7278C3E366EE2D57556C87D3AC857957668`。
- 协议测试 EXE SHA-256：`27123FE174FA20ABE711224AA76EF51992CC1751531F5690A741681AC733FDDE`。
- client.exe SHA-256：`AD8A97DE2A5B329DEC359BB93CBA0DABB3D40272BC9D151E502C45E33EA705E9`，构建标识 `VENGINEERING-20261031225617`。

### 边界、未覆盖与后续接入

- Windows 上完成真实编译、socket 和 ACL 检查；POSIX 分支未在 Linux/macOS 编译运行，不能据此声明跨平台验收。
- T01 不包含 Client 主循环接入、人工控制面板、操作账本/预算、任何角色动作或 Python MCP stdio。真实游戏 GUI 与端到端验证属于 T02–T07，本次未启动游戏或修改存档。
- `pollBudget` 是有界软预算，不承诺单 handler 硬实时 2ms；T02/T07 需在实际游戏负载下测量。
- 认证不等于动作授权。T02 应先处理 takeDisconnected 再调度；基于 context、策略及人工开启检查动作，维护去重账本；stop 优先且不进入账本。实例 info 含秘密，不能原样返回资源或日志。
- `x-maxUtf8Bytes` 和 `x-decimalMaximum` 是强制附加约束；T06 不能仅用忽略自定义字段的普通 JSON Schema 校验器。
- 仓库同时存在主会话 T08 的 client.cpp/.hpp、responsecallbacks.hpp、test_response_callbacks.cpp 改动，已按权威分工保留，不归为 T01 产品实现。

请主会话核对当前工作区、manifest 与检查日志后审查并重新分配；在接受前不放行下游。

