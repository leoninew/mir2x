# 排障与验证基线

[返回知识库](README.md)

## 2026-10-31 本机构建基线

| 项目 | 记录 |
| --- | --- |
| 源码提交 | `bb6a22287abcd9f248d2aba755074c254c68787a` |
| 额外改动 | 注册/改密的提交和关闭按钮修复；忽略根目录 `install/` |
| 工具链 | MSYS2 UCRT64，GCC 16.2.0，Release，`x64-mingw-static` |
| vcpkg 注册表基线 | `9eec002df1cd109f8f104cd966a763daf9751489` |
| 资源提交 | `9e21ea6631317d06e064fbfa7e1ea43af6a2d3e6` |
| 客户端显示的构建签名 | `VENGINEERING-20261031160838` |
| 源码补丁 SHA-256 | `2096e248321d71b425219f3b4bc839214d261aeff2fbf41cdbca4fbef6da82e0` |
| 客户端 SHA-256 | `b70dc5b1ed5d0e96bca81607ddd6b447e9257fdec0576822bd13a3d2dc597a85` |
| 服务端 SHA-256 | `10d167a92e098bdb3564f17ad752c7feae4dea09f56d7a2c629dc16917b8a7d9` |

这是文档编写前已完成的一次构建记录。后续工作树会变化；尤其本文档本身不包含在上述补丁哈希中。复现时需要保存对应源码补丁，不能只凭提交号推断已包含本地修复。

### 确实完成的验证

- 50 个 vcpkg 依赖安装完成，项目完整编译和链接成功。
- 最终 `cmake --install build --config Release` 退出 0。
- 17 个客户端资源包和 1 个服务端地图包可读，条目数与该资源版本输入一致。
- `widget`、`netio` 共 5 个测试通过。
- 客户端和服务端 EXE 导入表只引用 Windows 系统 DLL，没有 MSYS2/MinGW 运行时 DLL 导入。

### 尚未完成的验证

- 新客户端中用鼠标实际注册、改密、关闭页面的 GUI 回归。
- 新客户端与新服务端的完整登录、选角和进图联动。
- 全量 `check` 测试目标；当前通过的是上述 5 个用例。
- 多人、主从节点、长时间运行、交易完整性及公网运行条件的系统验证。

这些是明确的验证缺口，不应在以后写变更说明时被省略成“整个游戏已经测过”。

### 本机记录位置

| 路径 | 内容 |
| --- | --- |
| `.vcpkg/build-provenance.json` | 构建输入、哈希、状态与验证范围 |
| `.vcpkg/source.patch` | 当时构建采用的未提交补丁 |
| `.vcpkg/build-msys2.log` | 依赖与编译日志；原串行安装被主动中断的记录也在这里 |
| `.vcpkg/install-msys2.log` | 随后成功完成的正式安装日志 |
| `.vcpkg/tests-msys2.log` | 5 个基础测试的实际结果 |
| `.vcpkg/pack-resources.log`、`resource-validation.json` | 并行打包与资源条目验证 |
| `.vcpkg/linkage-msys2.json` | 两个主要 EXE 的 DLL 导入 |

上述文件被 Git 忽略，是当前机器的历史记录。这里保存了可共享的结论，但不会把新文档当成替代完整构建证据的证明。

## 注册按钮鼠标无响应

### 现象与已观察事实

原下载客户端中，点击注册提交没有效果；在密码输入框按回车可收到“注册成功”。这说明至少回车路径可以到达服务端注册逻辑，不能一开始就把所有问题归因于端口或数据库。

原下载程序的行为观察并不证明整个二进制由当前工作树编出。下载目录名带 `bb6a222` 也不是完整来源证明。因此修复后另外完成了本地编译，并保留源码补丁及新 EXE 哈希。

### 代码原因

[ButtonBase::processEventDefault()](../../client/src/buttonbase.cpp) 中：

- 普通按钮在鼠标松开时可调用 `onTrigger()`，默认 `onClickDone=true`。
- radio 模式在松开时保留按下状态，不走同一触发分支。
- 原注册/改密页面的提交和退出按钮配置了 `radioMode=true`，同时使用默认的松开触发方式，导致这条鼠标触发路径不执行回调。

修复位于 [processcreateaccount.cpp](../../client/src/processcreateaccount.cpp) 和 [processchangepassword.cpp](../../client/src/processchangepassword.cpp)：去掉这四个操作按钮的 radio 模式，补齐普通按钮使用的 `.off` 贴图。

没有全局改变 `ButtonBase` 的 radio 行为，避免影响真正用于保持选中状态的控件。

### 回归步骤

使用新客户端和新服务端，准备新测试邮箱账号，并满足当前密码规则：

1. 用鼠标提交注册，检查成功提示和账号是否可登录。
2. 重复提交相同账号，检查“账号已存在”等错误提示。
3. 用回车提交，确认原有路径继续正常。
4. 使用鼠标关闭注册页和改密页。
5. 修改密码后检查新密码可登录、旧密码被拒绝。

这里是待执行的操作步骤，不是宣称这些 GUI 用例已经通过。

## 常见现象的定位顺序

| 现象 | 先检查 | 再定位 |
| --- | --- | --- |
| 服务端窗口打开但连不上 | 是否 Launch；是否监听玩家端口 | `Server::launch()`、`ActorPool::launchBalance()` |
| 连不上默认端口 | 服务端 `--client-port` 与客户端 `--server-port` 是否对应 | `NetIO` 日志、TCP 监听；节点端口默认 8000 不供客户端登录 |
| 默认测试账号能用，自己注册失败 | 邮箱和密码规则 | `idstrf.cpp`、`net_CM_CREATEACCOUNT()` 的错误码 |
| 注册成功却没有角色 | 注册仅创建账号 | `CM_QUERYCHAR` / `CM_CREATECHAR` 流程 |
| 登录成功但不能进图 | 是否收到 `SM_ONLINEOK` | 角色坐标、地图资源、Player 创建与 `AM_BINDCHANNEL` |
| 界面看起来还是旧行为 | 实际运行 EXE 路径和时间/哈希；是否已 install | `--no-install` 只构建、未替换运行目录等情况 |
| 资源找不到、出现不同存档 | 进程工作目录 | `res/`、`script/`、`map/`、`mir2x.db3` 的相对路径 |
| 安装成功但某资源缺失 | 是否有 `zsdbmaker` 警告、输出文件是否存在 | 容器可读性、条目数；安装可能忽略单包错误 |
| Lua 内容改了却没生效 | 修改的是内嵌框架还是外部脚本；是否安装和重新加载 | `#embed`、`install/server/script/` 与对象生命周期 |

## 本次构建环境问题

- 早期默认下载镜像慢。本机把 USTC MSYS2 镜像放在 mirrorlist 前面，保留包签名校验。
- Git 不一定自动继承 Windows 的系统代理设置；构建进程曾显式设置 HTTP(S) 代理。
- vcpkg 部分克隆的延迟取对象曾在子进程环境中失败；本机使用 Git for Windows，并预先检出项目注册表基线后完成构建。这是当时的处理，不是要求任何机器都修改 vcpkg checkout。
- 首次资源处理涉及约 49 万个文件。为减少等待，本机中断原串行客户端安装驱动，保留其正在运行的武器包进程，用同一个 `zsdbmaker` 并行处理其他独立包，最后补跑原安装命令成功。

排障时优先保留已完成依赖、资源和构建缓存。只有确定要清理什么、会失去哪些内容时，才考虑重建整个工作树；不要把 `--fresh` 当成通用重试选项。
