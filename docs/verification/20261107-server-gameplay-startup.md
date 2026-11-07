# 服务端手动启动保留玩法配置验证
最后修改时间: 2026-11-07 14:03:33

## 审查状态

Accepted。用户明确报告“配置修复我实测生效了”，授权补齐验证记录、暂存相关变更并准备提交。本轮完成原计划A02缺项和限定自动化检查；只暂存，不创建commit。

## 意图与计划对齐

- [意图](../intent/20261107-server-gameplay-startup.md)与[计划修订2](../plan/20261107-server-gameplay-startup.md)的目标保持一致：手动启动使用main.cpp已验证的玩法快照，无配置对象不能进入运行组件初始化。
- mainwindow.fl保留原Server，密码确认、launch调用及成功后菜单禁用/窗口隐藏不变；FLUID生成文件不纳入提交。
- Server::launch在数据库、地图和ActorPool启动前调用gameplayConfig()，使用原有未初始化异常，不增加磁盘重载或默认配置回退。
- 基线：feat/dev，HEAD ebfb8ca91bddc96bfef6ff314037ea4c1f043009。最终修改为两个服务端源、一个测试文件和三份过程文档；工作台等其他工作不在本功能范围。

## 子任务交付与集成

不适用。本会话直接核对、验证和暂存，不向其他会话发送消息。

## 修复与范围

| 对齐依据 | 发现的偏差 | 实际处理 | 复验结果 |
| --- | --- | --- | --- |
| A01 | 手动Launch销毁已加载配置的对象，新对象没有玩法快照 | 保留工作区已存在的删除delete/new修复，不重复或回退修改 | 实际FLTK菜单和密码回调测试通过；用户报告实测生效 |
| A02 | launch没有配置前置检查 | 在首个运行组件初始化前校验gameplayConfig；增加空数据库/地图/actor依赖下的异常负例 | 新负例通过 |
| 开发检查 | 新负例首次编译缺少CHECK_THROWS_WITH声明 | 引入Catch2 matcher头文件 | 最终构建exit0；首轮失败日志保留 |

## 验收结果

| 验收ID | 结果 | 证据 |
| --- | --- | --- |
| A01 | 通过 | 真正调用FLTK菜单注册的密码回调；临时mir2x.db3目录使数据库打开失败，随后同一配置快照地址与地图组数保持；用户人工验收另列 |
| A02 | 通过 | 无配置Server调用launch时返回准确的gameplay configuration not initialized异常，未触及空的运行依赖 |
| A03 | 通过 | gameplayconfig 4项、gameplayspawn 2项、gameplaydrop 3项，共9/9通过；server增量构建exit0 |
| A04 | 通过 | 仅本功能6路径列入暂存范围；不修改配置初值、存档、角色，不连接现场；之前授权安装的407个受保护文件hash一致 |

## 检查结果

命令从仓库根目录运行，原生工具PATH使用D:/ProgramFiles/msys64/ucrt64/bin及D:/ProgramFiles/msys64/usr/bin。

| 命令或检查 | 实际结果 | 证据 |
| --- | --- | --- |
| cmake --build build --target server test_gameplaydrop test_gameplayspawn test_gameplayconfig --parallel 8，首轮 | exit1，缺Catch2 matcher头文件 | build/server-gameplay-startup/verification/build.log及build.exitcode |
| 同一构建命令，补头文件后 | exit0 | build/server-gameplay-startup/verification/build-final.log及build-final.exitcode |
| ctest --test-dir build -R '^(gameplaydrop\|gameplayspawn\|gameplayconfig)/' --output-on-failure --output-junit D:/SourceCodes/opensource/mir2x/build/server-gameplay-startup/verification/ctest.xml | 9/9通过，0失败，8.96秒，exit0 | 同目录ctest.log、ctest.xml及ctest.exitcode |
| 用户自行实测 | 用户确认配置修复生效；不推断未提供的具体地图、角色、操作步骤或怪物数量 | 本会话用户反馈 |
| 之前的client/server/workbench构建及安装 | 构建和三个安装组件exit0；3个EXE与构建产物一致，407个受保护文件未变化 | build/requested-build-20261107-131255/build-result.json |
| git diff --cached --check，限定本功能6路径 | exit0；完整暂存diff已读，范围外2249个index条目保持不变 | build/server-gameplay-startup/verification/stage-proof.json |

早期build/server-gameplay-startup/before.log实际为1个测试、4个断言通过；名称中的before不代表修复前RED。无怪根因依据源码调用链及安装日志中的配置未初始化错误，不虚构自动化失败记录。

## 交付身份

| 对象 | SHA256 |
| --- | --- |
| server/src/mainwindow.fl | 32A766B9D7FE8DD4E734A4F6561DC2C07940DB34561BB8E75E3BF9A061B22433 |
| server/src/server.cpp | 6B4869AAF6283892BD1E6DDA3312625A05C03D6F7C790270039AE0913706BCDC |
| server/test/unit/test_gameplaydrop.cpp | BA487B8D3BA922E5FCD5E62CBB32B66F1E8F7907B0A50B94E3617E830D3491B9 |
| 最终build/server/src/server.exe，含A02保护 | 408134E498F2690064714734BB782BC5A5E890C34CB84609CB57F44DB782C48C |
| 此前install/server/server.exe，含Launch修复、尚无新增A02保护 | 58A42AD0413BB711B0DF4F07E0258A79B54209D6358FA99605489305BA29744B |

源、制品及日志hash另存于build/server-gameplay-startup/verification/identities.json。build目录证据不纳入Git。

## 风险与未检查项

- 用户人工通过与最终自动化检查分列；本轮新构建的A02防御性保护没有再次安装或进行游戏现场测试。
- 保留Server后，计时起点仍为对象构造时；手动等待会计入uptime和昼夜周期。本次不另行重置计时起点，也不实现运行中热重启。
- 本次只运行与改动相关的9项检查，没有重跑全仓原生、工作台或MCP测试；不据此声称这些范围已重新验证。

## 结论

配置生命周期修复与原计划前置保护均已完成，相关构建和回归通过；用户确认已安装的核心修复生效。已暂存本功能6路径供提交，尚未创建commit；建议commit message仅在会话中提供。
