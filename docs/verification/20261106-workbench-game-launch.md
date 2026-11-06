# Verification / 验证：工作台相对路径与游戏启动
最后修改时间: 2026-11-06 23:26:35

## Review status / 审查状态
Accepted，对应修订4：界面精简、统一CMake构建与install交付完成。按用户要求未进行UI、反向或性能测试；真实游戏启动未验收。

## Intent alignment / 意图对齐
- 对应同标识Intent A01-A05：主页面保留启动栏和配置编辑；client/server路径编辑放入独立设置Dialog，完整路径和cwd折叠到详情，不再常驻免责声明。
- 默认路径以`install/workbench`为基准，对应同级`install/client`和`install/server`；CMake安装组件已实际更新三处程序。启动参数仍为单个EXE，不增加自动登录、HTTP启动或DB操作。
- 关闭工作台不结束游戏；仅跟踪本工作台创建的进程，创建成功不代表游戏已就绪。

## Plan alignment / 计划对齐
- 沿用Accepted修订4；偏好文件仍为工作台目录`game-launch.json`，与玩法JSON独立。支持用户手改后重新读取，保存使用临时文件和原子替换。
- CMake新增workbench target/component；一个build_desktop.py维护打包步骤，Makefile只做代理。包中间目录在主build树，正式入口在同一install前缀。不改native产品逻辑或依赖，原生CMake仅补runtime/resources组件划分。
- 按用户最新要求，不追覆盖率，不继续反向、UI或性能测试；保留安全校验代码，不以测试数量作为完成标准。

## Task deliveries and integration / 子任务交付与集成
不适用，主会话串行实施。

## Corrections and scope / 修复与范围
- 原文件选择方案被用户明确的“GUI配置、相对路径、用户可修改”替换，不保留二套选择模型。
- 新增缓存草稿提示仅用于启动前提醒：切换玩法类别后，其他类别未保存的草稿仍被计入，不自动保存。
- 用户指出此前dist入口与原生install脱节，现已纠正。之前的dist版保持历史，不再作为正式入口；未关闭旧窗口。
- 本机无GNU Make，早期make入口exit127保留；改用正式CMake workbench target构建成功。完整CMake安装在资源阶段因build/assets缺map失败；现有install资源存在，随后通过runtime、gameplay-config、workbench组件成功更新，未为取绿而放宽资源检查。
- 本轮早期已运行的较大定向检查结果只作为历史记录；收到测试范围要求后移除新增UI/反向测试。最终代码只保留2项正常路径检查。
- 隔离helper曾成功运行，后被系统防护拦截读取，最终日志`normal-smoke-final.log`为2pass/1fail；不绕过防护，已移除本轮helper源和测试，最终真实游戏启动未验证。修订后轻量检查日志另存为`normal-smoke-final2.log`，不以新结果覆盖历史。

## Acceptance results / 验收结果
| ID | 结果 | 依据 |
| --- | --- | --- |
| A01 | 实现并静态核对 | 主页面为紧凑操作栏，路径Dialog保留必要字段和详情；未做UI验收 |
| A02 | 实现并静态核对 | 相对路径/字段反馈保持，安装版默认两程序均存在且资源检查提示为空 |
| A03 | 实现并静态核对 | 固定argv、cwd及shell=False代码未改变；原正常路径检查为前轮证据，本轮不重跑 |
| A04 | 实现并静态核对 | 本地JSON持久化；缺程序时启动按钮打开设置，未保存路径不会发送；草稿/目录差异在启动操作时确认 |
| A05 | 通过限定检查 | 静态检查、正式CMake构建/组件安装和安装EXE资源读取通过；未执行UI/真实游戏启动 |

## Check results / 检查结果
当前修订证据目录：`build/workbench-launch/ui-cleanup/`。前轮`build/workbench-launch/`证据保留，仅代表对应旧版本，不拼作当前测试结果。

| 检查 | 结果 / 证据 |
| --- | --- |
| Python Ruff + format | exit0，`python-lint.log` / `python-format.log` |
| Mypy src tests build_desktop.py | exit0，`python-types.log` |
| 前端lint + Vue类型检查/Vite | exit0，`frontend-lint.log` / `frontend-build.log` |
| 正式CMake workbench target | exit0，`workbench-cmake-build.log`，含锁定依赖、前端和PyInstaller构建 |
| 原生构建 + 组件安装 | exit0，`native-build-final.log` / `install-runtime.log` / `install-gameplay-config.log` / `install-workbench.log` |
| 已安装EXE --help / --check-workspace | 均exit0，`installed-smoke.json`，仅加载玩法JSON不打开游戏 |
| 安装字节/默认路径核对 | build与install的三程序一致、前端一致，默认路径解析到正式client/server，无缺资源提示，`installed-audit.json` |
| wheel / sdist | exit0，`wheel-build-final.log` |
| 早期未成功入口 | make缺工具exit127，完整安装缺map exit1，原日志`workbench-build-install.log` / `cmake-install.log`保留 |
| UI/反向/性能/覆盖率 | 根据用户要求，不执行 |

## Risks and incomplete items / 风险与未完成项
- 真实游戏启动及窗口交互不在本轮检查范围，由用户手动确认；本轮只安装程序/脚本/玩法默认目录和工作台，不操作凭据/用户DB。
- build/assets源资源不完整，完整资源重打包仍需补齐map等源目录。本次复用现有install资源，不声称资源重新打包成功。
- 只阻止本工作台内同角色重复启动，不扫描外部进程或保证跨工作台单实例。地图、资源、端口和游戏本身是否就绪仍由游戏反馈。
- 配置文件需应用目录可写；外部编辑与保存最终替换仍可能竞争，不宣称跨编辑器事务。

## Conclusion / 结论
正式入口统一为`install/workbench/Mir2xWorkbench.exe`，需保留旁边的`_internal`。EXE SHA256为`7351e6e48dccf3ef831c3327aca0f41a945b85407512e7bd8ac25c0c9b1c15a5`；同级client/server已由CMake runtime组件更新，玩法目录由原保护安装逻辑校验保留。原资源沿用，完整资源重打包未完成。

当前修订完成静态、构建和组件安装检查；没有UI/反向/性能测试、真实游戏启动或Git提交，原暂存区保持不变。
