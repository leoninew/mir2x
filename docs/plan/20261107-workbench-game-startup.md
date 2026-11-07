# Plan / 计划：工作台游戏启动改进

最后修改时间: 2026-11-07 15:05:03

## Review status / 审查状态

Accepted，修订 2

## Intent basis / 意图依据

- `docs/intent/20261107-workbench-game-startup.md` A01-A07。

## Technical decisions / 技术设计与决策

- 已有 `--auto-launch` 使用原生 Server::launch；服务端 argv 固定传此参数，不新增用户参数面。
- Windows 创建进程增加 CREATE_NO_WINDOW 与既有 CREATE_NEW_PROCESS_GROUP，其他平台不变；stdout/stderr 合并追加到工作台 logs 目录，每次启动记录时间，不重定向到空设备。
- 状态使用 psutil 查询受工作台持有的 server PID 上 7000 TCP LISTEN；无权读取、进程消失等返回未知，不以端口可达代替 PID 归属，不声明业务完全就绪。
- 在 Workbench 应用目录单独保存 `game-processes.json`，不混入 `game-launch.json` 路径偏好。每项记录 PID、实际可执行文件路径和 psutil 创建时间，经临时文件 + `os.replace` 原子更新；`.gitignore` 和 CMake 安装排除此用户运行态文件。
- 每次新建 Workbench 时恢复登记项，通过 psutil 重新检查 PID、创建时间及规范化 executable path；仅全部匹配时重新管理，恢复进程后以 `psutil.Process` 提供停止与监听查询。停止前再次验证身份。缺 PID 或身份不匹配为陈旧记录并清理；记录对应进程存活但检查受限时显示未知并禁用启动/停止，避免重复启动或误杀。
- 不搜索进程名、路径或端口以补认旧版进程。更新前已失去进程表的旧版 Workbench 子进程没有可验证所有权记录，不能安全恢复；跨会话恢复从新版本启动并登记的进程开始。
- 桌面受信桥接只接受 client/server 读取固定日志尾部，在运行页查看；不接受任意文件路径。
- 移除手动 Launch 对已初始化 Server 的删除和重建，复用启动快照。保留该窗口的密码交互及自动路径不变。
- 保留当前工作区既有停止、轮询与其他在途修改；禁止改写/回退其他人的工作。

## Implementation steps / 实施步骤

1. 实施启动参数、窗口标志、日志和 PID 监听状态；接入受信桥接与运行页。
2. 给 launcher 增加独立的原子运行态注册表；启动后落盘，Workbench 重开时校验身份、恢复受管进程并让停止操作适用于恢复进程。
3. 让运行页表达身份未知状态并禁用不安全操作；保留安装目录的本地注册表，README 说明恢复范围及旧版限制。
4. 修复原生手动 Launch 对象生命周期；更新相应服务端验证记录。
5. 用 mock 与隔离临时子进程验证跨实例恢复、身份不匹配/重用、未知权限和停止；检查 CMake/源码配置，不启动真实 client/server。
6. 执行受影响的 Python 与前端检查；构建匹配 server 与 Workbench，并仅安装对应组件，不安装资源或触碰用户数据库。
7. 记录实际验证、未验证项和剩余风险；不暂存、不提交。

## Files to change / 修改范围

- `server/src/mainwindow.fl`；Workbench `game_launcher.py`、desktop bridge、运行态模型/注册表、进程状态 UI 类型与视图、CMake 安装排除规则、`.gitignore`、定向测试与 README。
- `docs/intent/20261107-workbench-game-startup.md`、本计划和同名 verification。

## Tasks and coordination / 子任务与协调

不适用。

## Verification plan / 检查与验证安排

- 不启动真实游戏；用隔离临时子进程覆盖启动登记、第二个 launcher 实例恢复与停止；用 mock 覆盖 PID 复用、路径不匹配、不可访问及陈旧项清理，断言不向未验证进程发信号。
- 验证状态文件独立于 `game-launch.json`、原子写入以及更新安装时目标用户记录不被覆盖；运行定向 Python 测试、Vue 单测、typecheck、lint 与 format。
- 最终构建/安装 server 和 Workbench；验证安装文件与配置保留，不安装资源或操作用户数据库。
- 不声称验证了真实窗口可视效果或加密 NPC 功能。

## Risks and recovery / 风险与恢复

- 日志随时间增长；输出路径不可写时明确失败，不丢弃启动错误。
- psutil 平台权限可能令监听状态为未知；原生日志可用于后续人工排障。
- 有用户工作台/游戏进程运行时不强制结束，不覆盖正在使用的 EXE。
- 更新前退出的 Workbench 没有保留进程身份；旧版已启动进程无法安全补认，须由用户决定其迁移处置方式。新注册表读写失败时应失败关闭，不能声称持久追踪成功。

## User review notes / 用户审查记录

- 用户授权本会话完成上述范围；开发和验证串行进行。
- 2026-11-07：用户指出重开后丢失 client/server 状态，并指定沿用同一 Workbench Intent 继续；Intent A05-A07 接受身份校验与旧版无记录进程不追认的安全边界。

## Implementation delivery / 实现交付

- 进程身份与路径偏好分开保存；启动登记采用同目录临时文件、fsync 和原子替换，登记失败会停止本次新建进程并保留原记录。
- 重开与停止前复核 PID、创建时间和实际程序路径；已退出、PID 复用和路径不符时清理记录，不发送信号。权限不足时保留 PID，显示未知并禁用控制；轮询可在身份恢复可读后重新启用停止按钮。
- 运行态文件加入 Git 和安装排除规则；README 更新跨会话恢复、自动更新状态及旧版无记录进程的限制。
- 开发检查：`make check` 通过；`make test` 后端 98 通过、2 个可选原生联动测试跳过，前端 56 通过。隔离真实 Python 子进程验证第二个 launcher 实例恢复与停止，不操作现场游戏。
- `cmake --build build --target workbench` 通过，默认 dev 制品安装至 `install/workbench`；安装版 `--check-workspace` exit 0。403 个已有配置及备份文件 hash 不变，构建和安装 EXE hash 一致。动态日志另有并发启动失败记录，不计入静态文件保留结论。
- 本次新增进程恢复没有原生代码变化。服务端生命周期部分已由 `20261107-server-gameplay-startup` 独立任务交付，见其验证记录及提交 `e3490ee`；本次不重复原生构建安装。
- 用户说明另一会话在添加启动失败提示；保留其修改，不扩展启动失败提示功能。不暂存、不提交。
- 完整验收、制品身份和证据路径见 `docs/verification/20261107-workbench-game-startup.md`。
