# Plan / 计划：工作台启动客户端与服务端
最后修改时间: 2026-11-06 23:26:35

## Review status / 审查状态
Accepted，修订4；按用户直接实现授权及最新界面/统一install反馈执行。当前原配置实现已暂存，基线保存于build/workbench-launch/baseline.json；本任务不stage/commit，保留原index。

## Intent basis / 意图依据
[意图](../intent/20261106-workbench-game-launch.md)，A01-A05。

## Technical decisions / 技术设计与决策
- 相对路径以工作台应用目录为唯一基准：frozen使用EXE所在目录、源码开发使用project_root。默认`../client/client.exe`、`../server/server.exe`，JSON保存在工作台目录`game-launch.json`；与玩法数据分离。Windows和POSIX绝对路径、空路径、NUL、错误类型均拒绝；允许`..`。保存时同目录临时文件/fsync/replace，不写服务器目录。
- GUI主视图改为紧凑启动栏。两个路径字段、相对基准和字段级错误放入已有reka-ui的Dialog，完整路径/cwd/资源信息折叠为详情。未配置时点击启动进入设置；未保存的路径不用于启动。当前页面有未保存玩法草稿或管理目录与待启动server不同时，仅在启动时确认。
- 新game_launcher.py限制client/server角色、路径后缀与存在性，Popen只传[exe]、shell=False、cwd=EXE父目录、无凭据或自动登录参数。不自动查找/复制资源、不强制验证地图资源从而阻止打开原始服务端设置界面。显示缺少config/gameplay或map/res的提示，但不冒称完整验证或游戏ready。
- 启动服务由受信桌面桥接get_game_launch_status/save_game_launch_paths/launch_game调用；无HTTP启动API。服务锁保护配置与子进程状态，重复点击同一角色/在途实例拒绝；刷新读取自己启动的真实进程poll，退出后允许再次启动。不扫描/杀其他进程，不承诺全系统单实例。
- 关闭工作台不关闭游戏进程；拒绝不可信页面。Popen成功只表示进程已创建；无热更新、自动重启或状态轮询承诺。

## Implementation steps / 实施步骤
1. 实现有锁启动服务与窄native桥接；正常路径轻量检查只检查相对路径保存和固定argv/cwd。
2. Vue启动卡与bridge类型、草稿确认和旧响应保护；仅静态检查，不做UI测试。
3. 必要检查后Vite/PyInstaller构建匹配EXE；原生client/server仅完成用户此前要求的增量构建，不安装、不访问实际游戏。更新说明与verification。
4. 按截图收敛界面：删除常驻免责声明、重复未选择提示、示例通知入口及英文装饰标题；增加单一目录空状态，已选目录用紧凑路径栏。只做静态检查和制品构建，不做UI/反向/性能测试。
5. 统一现有install：CMake新增workbench构建target和安装组件，通过一个`build_desktop.py`维护打包步骤，Makefile仅作相同脚本的便捷入口。本机没有GNU Make，正式流程直接用`cmake --build build --target workbench`，并沿用`cmake --install`；包中间目录固定为当前native build树的`workbench/Mir2xWorkbench`，不再把工具dist当正式GUI入口。完整CMake安装可带入已构建GUI，未构建时明确跳过；显式workbench安装缺包时失败。当前缓存prefix确认为仓库`install`，发布到`install/workbench`，默认相对路径直接对应同级client/server。安装排除运行偏好、logs、.env；不删除已有文件，不处理DB。实际完整安装在资源环节发现build资源源目录缺map；已有install资源存在，因此client/server现有安装规则仅增加runtime/resources组件归属，保持完整安装默认行为不变，本次按runtime、gameplay-config、workbench组件更新并保留资源，不手工复制也不重装资源。旧dist工作台不关闭。

## Files to change / 修改范围
仅工作台Python服务/desktop、Vue启动组件/页面/应用外壳/类型、共享打包脚本/Makefile/CMakeLists/pyproject源包清单、README及本功能过程文档，工具总CMakeLists增加子目录、client/server的CMake安装规则补组件归属、开发指南更新正式入口。不删除通知页源码，不改业务计算或native产品逻辑。

## Tasks and coordination / 子任务与协调
不适用；主会话唯一writer，接收/进展/交付记本计划，最终审查记verification。

## Verification plan / 检查与验证安排
遵从用户最新要求，不追覆盖率，不追加/运行反向、UI或性能测试。仅保留两个正常路径pytest检查：偏好保存/重开、固定argv/cwd（模拟进程创建）；Ruff/Mypy/Vue lint/typecheck/format。最终准确EXE help/check-workspace及打包资源检查，不启动真实游戏。早期隔离helper一次通过后被系统安全防护阻止读取，已停止该验证、移除本轮新增helper源与测试，不绕过防护。

## Risks and recovery / 风险与恢复
本轮按用户纠正采用已有install入口，不触及用户存档/凭据，不自动启动游戏或关闭现有窗口。源码改变保留在工作树。目录和相对EXE路径来自用户本机，启动前检查，无法保证外部编辑器的跨进程竞争。

## Reception and progress / 接收与进展
- 2026-11-06 22:47:49：主会话接收；已核安装布局、默认map/script/res相对路径及原master非auto-launch行为。开始Python/Vue实现，先定向检查后构建GUI包。

- 2026-11-06 22:48:36：用户明确路径属于GUI配置，采用工作台目录相对路径、手工编辑/持久化，取代先前文件选择与server-root耦合设计；保留纯桌面桥接及不操作现场边界。
- 2026-11-06 22:56:52：实现完成，按用户最新要求收敛检查范围。移除本轮新增UI/反向测试，仅保留2项正常路径检查（2passed/0.04s）；既有测试仅补齐bridge mock方法形状以便类型检查，不新增UI用例。Python Ruff/format/Mypy、前端lint/format及Vue类型检查/Vite构建通过；client/server增量构建exit0，ninja无待构建项。开始匹配PyInstaller/wheel制品构建。
- 2026-11-06 22:58:24：PyInstaller onedir及wheel/sdist构建exit0；准确EXE help/隔离check-workspace均exit0，资源与归档核对通过。EXE SHA256为`96240bed9b06b6438a174fb7c83eb43128b90c40a720d63019e7dbb32c04a02b`，完整制品信息见`build/workbench-launch/release-audit.json`。原index tree保持`f842616fa8d32dcf230c846e5817d93174a9270b`，未提交、未安装、未启动游戏。
- 2026-11-06 23:25:09：完成修订4。界面收敛为启动栏、路径Dialog和单一目录空状态；移除示例通知入口和常驻免责声明。正式CMake workbench target构建通过，runtime/gameplay-config/workbench三个安装组件exit0；完整安装曾因源资源缺map失败，原始日志保留，不称完整资源安装通过。已安装client/server与build逐字节一致、工作台与中间包一致、前端与安装资产一致，默认路径精确指向install同级程序。准确安装EXE help/check-workspace均exit0，未执行UI/反向/性能测试、未启动真实游戏。新EXE SHA256为`7351e6e48dccf3ef831c3327aca0f41a945b85407512e7bd8ac25c0c9b1c15a5`，证据在`build/workbench-launch/ui-cleanup/`。原index仍未改动。
