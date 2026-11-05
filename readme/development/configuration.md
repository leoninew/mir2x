# 配置系统

[返回知识库](README.md)

mir2x 当前没有一个覆盖整个游戏的统一配置文件。名称里带 `config` 的结构分属不同 owner、生命周期和保存方式；新增设置前先确定配置作用于构建、服务进程、玩家角色，还是静态游戏内容。玩法配置的 UI 评估另见[游戏玩法配置与可视化管理研究](../../docs/research/20261104-runtime-gameplay-config.md)。

2026-11-06 的[集中配置管理评估](../../docs/research/20261106-centralized-configuration-management.md)按“不增加配置项、前期不要求即时生效”讨论可视化方向；[全量配置盘点意图](../../docs/intent/20261106-centralized-configuration-catalog.md)要求先列出全部仓库可见配置，具体改动范围和方式由用户决策。

## 配置分层

| 层 | 主要来源与 owner | 保存及同步 | 生效方式 |
| --- | --- | --- | --- |
| 构建配置 | [`build.py`](../../build.py)、CMake cache、vcpkg manifest / baseline | 构建树与本机依赖缓存；不是游戏存档 | 重新配置或构建。运行中的程序不会读取这些值 |
| 进程启动配置 | [`ClientArgParser`](../../client/src/clientargparser.hpp)、[`ServerArgParser`](../../server/src/serverargparser.hpp) | 命令行参数；没有统一的游戏级配置文件 | 进程启动时解析。监听端口、启动开关及客户端资源路径等在初始化路径中使用 |
| 服务端主窗口配置 | [`ServerConfigureWindow`](../../server/src/serverconfigurewindow.fl) / `ServerConfig` | 当前进程的内存对象，由互斥锁保护；没有跨重启持久化 | 由各使用点读取。地图、脚本在加载时读取；监听端口与最大连接槽在网络启动时使用；掉落倍率在每次计算时读取 |
| 玩家运行时配置 | [`SDRuntimeConfig`](../../common/src/sdruntimeconfig.hpp)，属于 `SDPlayerConfig` | 以序列化 BLOB 写入 [`tbl_playerconfig`](../../server/src/server.sql) 的 `fld_runtimeconfig`，主键为玩家 `fld_dbid` | 游戏窗口修改后发 `CM_SETRUNTIMECONFIG`；Player 持久化，登录场景时通过 `SM_PLAYERCONFIG` 下发 |
| 主从节点配置 | `ServerArgParser::MasterSharedConfig` 与 [`SDPeerConfig`](../../common/src/serdesmsg.hpp) | 共享启动参数随 peer 初始化发送；掉落/金币倍率由 master 经 `AM_PEERCONFIG` 更新到 slave 的 [`PeerConfig`](../../server/src/peerconfig.hpp) | actor 初始化配置只在握手时传递；倍率通过独立更新消息提供给 slave |
| 游戏静态与脚本数据 | `common/src/*record.inc`、`server/script/`、资源 DB | 编译期记录、脚本文件或已打包资源；不是上述运行时配置对象 | 静态记录需要重编相关程序；地图/NPC/任务 Lua 按服务端加载时机读取；资源修改要重新打包并部署 |

这些层可以在同一个界面出现，但不是可以互换的存储后端。尤其 `SDRuntimeConfig` 的“runtime”指角色运行时偏好与选项，不表示它能热更新服务端掉率、经验、刷怪或地图脚本。

## 服务端主窗口

`ServerConfigureWindow` 内含 `ServerConfig`，目前默认项包括地图包路径、脚本根目录、最大玩家槽、经验倍率、掉率、金币率及端口。窗口启动时用启动参数填充端口，再以表单默认值执行一次配置校验；`Apply` 更新进程内存值，不写配置文件。

这些字段并不都有实际消费者，名字也不保证语义已接通：

| 字段 | 当前读取点 | 影响边界 |
| --- | --- | --- |
| `mapPath` | [`Server::loadMapBinDB()`](../../server/src/server.cpp) | 主服务端启动加载地图 DB 时读取 |
| `scriptPath` | ServiceCore、ServerMap 与 Lua 初始化 | 各 Quest/地图/NPC 脚本环境创建时读取；保存新路径不等于已创建的脚本环境全部重载 |
| `maxPlayerCount` | [`NetDriver`](../../server/src/netdriver.cpp) 构造时创建连接槽 | 修改既有内存值不会重建已分配的连接槽 |
| `clientPort` | [`ActorPool`](../../server/src/actorpool.cpp) 启动玩家监听 | 监听建立后，改窗口字段不会自动重绑端口 |
| `slavePort` | 没有通过 `ServerConfig` 读取；slave peer 监听使用 `ServerArgParser::peerPort` | 窗口有显示字段不代表 Apply 后该值成为监听参数 |
| `dropRate`、`goldRate` | [`getMonsterDropItemList()`](../../server/src/dropitemconfig.cpp) | master 直接读窗口当前值；master 也会把这两个值发给各 slave |
| `experienceRate` | 当前只有窗口声明、输入和校验 | 没有接入经验结算；改它不会改变击杀经验 |

`Apply` 将掉率和金币率发送给各 peer；新 peer 建连时另有 `MasterSharedConfig` 初始化数据，但该结构不包含这两个倍率。当前握手路径未展示 `SDPeerConfig` 的初始快照；若 peer 在最近一次 Apply 之后才加入，应核实它是否会收到当前倍率，而不能只因启动握手成功就假定数值一致。

因此“按 Apply 后是否生效”必须按消费者回答：掉率在后续掉落计算读取；地图与脚本路径影响后续创建的加载对象；监听和容量字段依赖初始化时机；未接线字段不产生效果。新增字段要沿到实际读取点，不能只加表单字段。

## 玩家运行时设置

`common/src/sdruntimeconfig.hpp` 用 `SDRuntimeConfig` 保存按 key 查找的序列化字符串。`_MACRO_ADD_RTCFG_TYPE` 为每种设置定义 C++ 值类型、默认值和 key accessor；不存在的 key 返回默认值，显式写回默认值则删除该 key。这使存档通常只存偏离默认值的项目。

典型流向：

```text
RuntimeConfigBoard
  -> 本地更新 SDRuntimeConfig
  -> CM_SETRUNTIMECONFIG(type, serialized key, serialized value)
  -> Player::net_CM_SETRUNTIMECONFIG ([`playernet.cpp`](../../server/src/playernet.cpp))
  -> tbl_playerconfig.fld_runtimeconfig

角色上线
  -> Player::dbLoadPlayerConfig ([`playerdb.cpp`](../../server/src/playerdb.cpp))
  -> SM_PLAYERCONFIG(SDPlayerConfig)
  -> ProcessRun::on_SM_PLAYERCONFIG ([`processrunnet.cpp`](../../client/src/processrunnet.cpp))
  -> RuntimeConfigBoard::setConfig
```

设置按角色 `fld_dbid` 保存，并不是 Windows 客户端全局偏好文件。系统页含窗口/音频/输入法选项；社交和游戏页还包含聊天、组队、交易、界面显示、道具过滤等角色选项。某些选项只是客户端显示行为，部分选项也在服务端检查：直接交易会检查双方的 `RTCFG_允许交易`，而不是只信任请求客户端。

UI 在发送设置消息前已经更新本地值；服务端 handler 在值变化时保存 BLOB，不发送该设置的专用确认响应。需要判断是否持久化时，应看重连后 `SM_PLAYERCONFIG` 返回值，不把一次本地 UI 切换当作 DB 确认。

### 持久化兼容

runtime accessor 的 key tuple 首项是 accessor 编号，目前由定义顺序上的 `__COUNTER__` 生成，后续 extra keys 再追加到 tuple。序列化结果作为 `tbl_playerconfig` BLOB 内的 key；对应 value 也按 accessor 类型序列化。对这些声明排序或在前面插入新项会改变后续编号，可能让已有角色配置指向错误类型或设置。

新增 runtime setting 时，应将声明追加在现有 accessor 列表末尾，并为缺省值、往返序列化、客户端应用和服务端判定补测试。若需重排或改类型，先设计旧 BLOB 的兼容迁移；不能只依赖客户端和服务端同时重编来修复已有角色存档。

## Master 与 slave

`ServerArgParser` 按 master/slave 角色区分参数：master-only、slave-only 与 master 解析后转发的 `MasterSharedConfig`。新 slave 的 `SDSysPeerIndex` 带一份共享配置用于初始化。该初始化与 server configure window 中会变化的字段是两条不同路径。

当前掉率和金币率有清晰的动态路径：master 窗口 Apply 后发 `AM_PEERCONFIG`，slave `PeerCore` 将 `SDPeerConfig` 放入带锁的 `PeerConfig`，掉落计算从中读取。新增需要跨节点动态更新的全服值时，应明确 master 权威、启动新节点初始化以及既有节点更新三件事；只改共享启动结构无法让已运行节点收到后续变化，单独加 AM 消息也不能初始化新节点。

## 改动入口

| 需求 | 主要入口 | 不要默认假设 |
| --- | --- | --- |
| 新构建选项 | `build.py`、CMake option 与 CI | 构建参数不会成为游戏运行时设置 |
| 服务端监听/启动参数 | `server/src/serverargparser.hpp`、调用它们的初始化路径 | 仅修改 FLTK 表单不会改变正在监听的 socket |
| 玩家偏好或同角色游戏选项 | `sdruntimeconfig.hpp`、`runtimeconfigboard.cpp`、CM/SM 消息和 `playerdb.cpp` | 不要把角色偏好存到 server 全局窗口对象 |
| 掉落、刷怪、地图脚本 | [玩法配置研究](../../docs/research/20261104-runtime-gameplay-config.md)与各自计算/加载点 | 字段存在不代表已消费；文件更改不等同热加载 |
| 跨 master/slave 的全服值 | `ServerArgParser::MasterSharedConfig`、peer 初始化消息、`SDPeerConfig` / AM handler | 只送初始化快照无法同步后续 Apply |

改完后先列出唯一权威值和读取点，再定义持久化 owner、校验、可变时机、主从同步及旧存档兼容。若这些答案分属不同设置，应分别实现，不必为了“统一”另造一个覆盖所有层的总配置文件。
