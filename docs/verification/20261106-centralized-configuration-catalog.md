# Verification / 验证：玩法配置集中管理与运行时加载
最后修改时间: 2026-11-06 22:24:01

## Review status / 审查状态

Accepted：源码实现、自动化与发行检查通过。真实窗口/窄窗口的目视验收未验证，见限制；不将组件或HTTP检查称作真实GUI点击验收。

## Intent alignment / 意图对齐

用户已采纳标准JSON方案并授权T01-T07全部实现、中间针对性测试、功能完成后统一编译制品。GUI模板由用户复制，提交 `0d1065c1db45bdbfd0378ad21b1be4e9b2e57eb9` 经用户明确确认是预期scaffold基线，HEAD保持该提交。本交付是未暂存工作区，不含新Git提交。

仅迁移基础经验、金币/物品有序掉落和地图刷怪数据，不调初值，不增加倍率，不迁移端口/并发/部署参数。原伤害分配、掉落抽样与金币波动、monGener算法保持；历史未生效项只读保留。用户原调研阶段A08的“只作计划”已被明确实施授权推进，不作为禁止已授权实现的依据。

## Plan alignment / 计划对齐

- UTF-8标准JSON、数字身份及名称校验、C++不可变快照、启动完整校验；master/slave缺配置均在actor/监听/窗口创建前exit1。
- GUI选server根；WorkspaceService与GameplayConfigService分工，固定API不接受任意路径。保存先完整校验/revision检查、备份/fsync/同目录replace；409保留草稿。读取/保存/目录响应均防止迟到覆盖。
- 模板工程命名为mir2x-gameplay-workbench / mir2x_workbench / mir2x-workbench；uv.lock、Yarn锁实际同步。保留onedir发行方式，wheel/sdist携带构建后前端；源码editable不强制前置dist。
- 整目录安装：新目录校验后发布；已有合法目录整体保留，不完整/不兼容则停止，不覆盖或补混默认文件。原生catalog导出只包含编译身份。

## Task deliveries and integration / 子任务交付与集成

全部由主会话直接执行，编号CONFIG-T01-01至CONFIG-T07-01；权威登记见总计划。最终源码和制品散列清单位于 `build/gameplay-baseline/final-manifest.json`。

| ID | 当前交付 | 必要检查与集成证据 | 结果 |
| --- | --- | --- | --- |
| T01 | server/config/gameplay、migration/baseline.json、26共用样本 | 364怪物；5272原行=4886有效+386未生效；5402有效展开；251地图/2082原组/3322有效区域/24cratio；migration3项 | 通过 |
| T02 | GameplayConfig、Server启动与经验消费、移除旧exp | 原生契约/不可变快照；真实Server未初始化/失败后/重复初始化；master/slave实际缺配置exit1 | 通过 |
| T03 | 配置掉落表→原getMonsterDropItemList | 5402项与旧生产表一致；真实生产repeat/group/order/gold范围用例；抽样函数正文逐字不变 | 通过 |
| T04 | getMapSpawnConfig与251脚本数据实参 | 全表/非数据片段对账；真实monGener数量/周期及可变状态隔离 | 通过 |
| T05 | 严格Python模型、目录绑定、API/备份/冲突、依赖 | Python全套64pass；后续扩充共用样本后affected11pass；真实干净wheel HTTP保存→native | 通过 |
| T06 | 三领域Vue页面、IntegerField、diff/草稿/冲突 | 前端37pass，其中玩法9项；lint/typecheck/format、真实Vite build | 自动化通过；窗口目视未验证 |
| T07 | 校验工具、安装保护、成套构建与发行审计 | native check129pass、安装4场景、准确EXE/wheel/资源与HTTP闭环 | 通过；目视限制如实保留 |

## Corrections and scope / 修复与范围

| 依据 | 发现 | 修复 | 复验 |
| --- | --- | --- | --- |
| T05安全保存 | 备份目录旧名称会被*.json扫描命中；外部无效刷怪应先报冲突 | 备份目录去.json后缀，保存先核revision再处理原结构 | Python实际文件/异常用例通过 |
| T06草稿 | 恢复原值后缓存可能复活；旧异步结果可能覆盖当前对象 | 清除已恢复草稿，以目录保留草稿并用代际忽略旧响应 | 玩法9项通过 |
| T07中文目录 | Windows窄argv及路径诊断不保证UTF-8 | CLI读取Unicode命令行，路径诊断显式UTF-8 | 中文+空格真实保存/校验/inspect通过 |
| T07发行资源 | editable前置dist以及sdist重建wheel时缺静态文件 | Hatch hook区分editable/发行，sdist带built assets；显式排除环境/用户文件 | 实际wheel/sdist及干净安装资源逐字审计通过 |
| T07旧回归 | 原check124/129，5项Lua loadfile/io.open不能打开实际中文文件 | 仅Windows测试夹具std::filesystem UTF-8读取；merchant测试read复用该入口；不改生产Lua | 定向5/5→check129/129 |
| T07依赖 | Yarn两处第三方缺peer声明 | 最小packageExtensions补Vue/compiler-dom peer，版本不升级 | immutable安装零peer警告，37项再过；前端bytes与EXE所带资源一致 |

初轮生产drop测试用了不存在的“金币”记录名，修正为实际“金币（小）”后通过；这是夹具失败，不作为产品RED。旧失败日志、首次CMake环境缺失、未命中CTest正则、发行资源缺失与offline缓存缺包均保留在build目录，最终以修复后准确日志为准。

## Acceptance results / 验收结果

| ID | 结果 | 证据 |
| --- | --- | --- |
| A01-A03 | 通过 | 范围盘点、初值/无效项对账；新JSON实际加载；未调数值/公式 |
| A04 | 通过 | catalog导出与冻结目录相等；全部ID/名称校验；client/server编译 |
| A05 | 通过 | 5402展开表、生产掉落、251地图/真实monGener及实例隔离 |
| A06 | 自动化通过，目视未验证 | 正式API、组件、干净发行版资源、HTTP保存；无真实窗口操作证据 |
| A07 | 通过 | 同一批26契约经Pydantic/C++/原生CLI，严格范围/引用/重复键；保存版本与只读快照边界 |
| A08 | 通过授权核对 | 用户已明确推进实施与最终验证；无Git写入/运行服部署/角色操作 |

## Check results / 检查结果

| 命令 / 检查 | 实际结果与日志 |
| --- | --- |
| build.py --no-install、目标client/server/gameplay_config_tool及3玩法test | exit0；`build/gameplay-baseline/final-native-build.log`；最终ninja no work |
| cmake --build build --target check --parallel 8 | **129/129，33.26s，exit0**；`final-check2.log`，包含6项玩法case |
| ctest -R '^gameplay(config\|spawn\|drop)/' | 扩充26契约后6/6，6.04s；`final-native-tests4.log` |
| python tools/gameplayconfig/migration/test_baseline.py | 3/3通过 |
| python tools/gameplayconfig/test_install.py build | 新目录、已有修改值、不完整、身份不兼容共4场景通过；失败场景逐字保留目录 |
| uv ... pytest -q（MIR2X_GAMEPLAY_TOOL为实际工具） | **64pass、0skip、21.23s**；workbench/build-final-tests.log |
| 后继共用样本扩充后的pytest tests/test_gameplay.py | **11pass、0skip、18.34s**；final-contract-api-tests.log；不与64累计 |
| Python Ruff/format/Mypy | 通过；Mypy27源码/测试文件 |
| Yarn lint/typecheck/format及test | **37pass、1.53s**；workbench/build-final-web-tests2.log；玩法9项 |
| Yarn install --immutable、Vue发行build | exit0；最终资源与冻结EXE所带文件散列相同 |
| PyInstaller onedir、uv build | Windows EXE、wheel、sdist生成；build-binary.log / build-wheel-final.log |
| 干净venv锁定依赖+wheel | 实际site-packages导入；初次offline缺缓存，联网按相同锁定版本/哈希装好；资源与最终wheel逐字相同 |
| release_check.py（干净wheel运行） | 真实loopback HTTP保存三域→native inspect完全相等；经验1234/47有效展开/26刷怪组，中文空格目录；关闭服务后native仍能独立读取 |
| 同release_check.py准确EXE与server | EXE help/合法目录exit0，非法manifest exit1；干净wheel离线入口exit0；master/slave缺配置各exit1，无角色/游戏连接 |
| 发行文件审计 | wheel28 member、sdist76 member、onedir281文件；默认配置/日志/前端存在，无.env/用户备份/日志/虚拟环境/node_modules |
| Git/最终范围 | diff-check0；HEAD不变；抽样函数与addmonster.lua保持；未stage/commit |

最终发行闭环原始记录与执行脚本：`build/gameplay-baseline/release-check-proof.json` / `release_check.py`；日志 `release-check-final.log`。安装检查用真实生成的gameplay-config组件，无运行服安装。

## Artifacts / 制品

| 路径（仓库相对） | SHA-256 |
| --- | --- |
| `build/tools/gameplayconfig/gameplay-config.exe` | `10282b5542d3ee2a4836f5b49352928dfa6e3d43ccc57cb15dc5c48b2bcf72c2` |
| `tools/gameplay-workbench/dist/Mir2xWorkbench/Mir2xWorkbench.exe` | `cdf1a1f162e3c733cc2636cdbccd2cef60bbde94adea8e2770da66a4348c2bb1` |
| `tools/gameplay-workbench/dist/mir2x_gameplay_workbench-0.1.0-py3-none-any.whl` | `0ecaf78c493133c14fd63737b9c540384cbf24c5acd8b2c92f9f9cc6fa899b3f` |
| `tools/gameplay-workbench/dist/mir2x_gameplay_workbench-0.1.0.tar.gz` | `6300242b53ecd46afc493dbb3eb91c4c7a3293165427963d1ede625ebdb714f1` |
| `build/client/src/client.exe` | `2e89430957e03de11a68a8856e53550d038abf396a80451c9b3ecdd74c4b91c9` |
| `build/server/src/server.exe` | `8c33b18e01f9f11e11568df826f56603a8b4f4fadaae3364f73f4911ada524e5` |

GUI必须分发整个 `tools/gameplay-workbench/dist/Mir2xWorkbench/`。使用时选server目录，详见workbench和gameplay各README。构建树client/server为二进制，运行仍需现有配套资源；本轮未覆盖现有install部署。

## Risks and incomplete items / 风险与未完成项

- 窗口目视/窄窗口检查未完成：Jev无决策key且browser_open失败；CUA明确报告native pipe和browser host不可用。已停止验收用临时HTTP服务；无法据此声称真实窗口布局/目录对话框已操作通过。
- GUI草稿仅在当前窗口内保留，退出前须保存/下载；多文件不提供事务，与不遵守锁的外部编辑器仍有替换前竞争窗口。
- 服务端只启动读取，GUI保存不更新运行中的进程；多服务节点部署一致性由使用者维护。本期未做热更新、运行服部署或角色验收。
- 一次性迁移基线只能由原始冻结源码重新导出；旧dropgen/CSV不再作为已迁移数值权威。

## Conclusion / 结论

所选玩法数据的外置、C++启动消费、Python/Vue管理和本机发行已实现，原生/Python/前端及发行闭环通过。接受源码与自动化交付，窗口目视项保持未验证；所有工作保留在当前工作区，未自动提交Git。
