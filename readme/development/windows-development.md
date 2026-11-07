# Windows 开发环境
最后整理日期: 2026-11-07

[返回知识库](README.md)

Windows 原生构建使用 MSYS2 UCRT64。以下构建命令均在仓库根目录的 UCRT64 终端执行；工具由当前终端的 `PATH` 提供，MSYS2 安装目录和终端启动方式由本机环境管理。历史构建结果见[排障与验证基线](troubleshooting-and-baseline.md)。

## 工具链与目录约定

| 项目 | 约定 |
| --- | --- |
| 源码和命令起点 | 仓库根目录 |
| 环境 | UCRT64，64 位 MinGW，vcpkg `x64-mingw-static` |
| 编译器 | GCC 16；当前 CMake 要求 C++26 |
| 构建工具 | CMake 3.21+、Ninja |
| 构建脚本运行时 | Python 3 |
| CMake 工作树 | `build/` |
| 安装结果 | `install/client/`、`install/server/`、`install/tools/`；已构建的工作台安装到 `install/workbench/` |
| vcpkg 源码与下载 | `vcpkg/` |
| 已安装依赖 | `build/vcpkg_installed/` |
| 原始游戏资源 | `build/assets/mir2x_assets/` |
| 本机日志与二进制缓存 | `.vcpkg/`，已被 Git 忽略 |

`build.py --build-dir=.` 的含义是以当前目录为构建根，然后在其中使用 `build/`、`install/` 和 `vcpkg/`，不是把编译产物直接散落到源码目录。

依据：[build.py](../../build.py)、[CMakeLists.txt](../../CMakeLists.txt)、[Windows CI](../../.github/workflows/build.yml)。

## 快捷构建

完成下节的工具链准备后，在仓库根目录的 UCRT64 终端中执行：

```bash
bash ./scripts/build-msys2.sh
```

这个[构建辅助脚本](../../scripts/README.md)设置 UCRT64、Ninja、编译器、构建根和二进制缓存，并调用项目的 `build.py`。它已纳入版本控制，新的克隆可以直接使用。

辅助脚本接受追加的 `build.py` 参数，例如 `--no-install`、`--run-test`。默认并行数为 8，可用 `MIR2X_BUILD_JOBS` 或 `--parallel=N` 覆盖；代理与已有 vcpkg 缓存配置沿用当前环境。

## 首次准备与手动构建

在 **MSYS2 UCRT64** 终端中准备环境。首次安装 MSYS2 时先完成 `pacman -Syu`，如终端被更新关闭，重新打开后再次检查更新。依赖工具与项目 CI 保持一致：

```bash
pacman -S --needed \
    mingw-w64-ucrt-x86_64-toolchain \
    mingw-w64-ucrt-x86_64-git \
    mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-ninja \
    mingw-w64-ucrt-x86_64-pkgconf \
    mingw-w64-ucrt-x86_64-python
```

随后在同一终端的仓库根目录构建；也可直接使用上节的辅助脚本：

```bash
export CMAKE_GENERATOR=Ninja
mkdir -p .vcpkg/binary-cache
export VCPKG_BINARY_SOURCES="clear;files,$(cygpath -m "$PWD/.vcpkg/binary-cache"),readwrite"

python3 ./build.py \
    --build-dir=. \
    --triplet=x64-mingw-static \
    --host-triplet=x64-mingw-static \
    --c-compiler=gcc \
    --cxx-compiler=g++ \
    --parallel=8
```

vcpkg 注册表基线由 [vcpkg-configuration.json](../../vcpkg-configuration.json) 固定，额外移植位于 [ports](../../ports)。本机验证基线为 `9eec002df1cd109f8f104cd966a763daf9751489`。

需要代理时，在当前构建终端配置 `HTTPS_PROXY` 和 `HTTP_PROXY`，地址由本机环境管理。辅助脚本继承这些变量。

若 MSYS2 Git 在 vcpkg 子进程中初始化 curl 失败，可将本机 Git for Windows 的 `cmd` 目录临时放到当前终端 `PATH` 的最前面，再直接运行本节的 `python3 ./build.py ...` 命令；其他工具仍来自 UCRT64。

## 日常增量构建

在已配置好的 UCRT64 构建环境和仓库根目录中，按顺序构建并安装 `client`、`server` 和 Workbench：

```bash
# 编译客户端和服务端。
cmake --build build --target client server --parallel 8

# 安装原生程序、内容脚本和玩法配置；复用已有游戏资源。
cmake --install build --component runtime
cmake --install build --component gameplay-config

# 打包 Workbench 并自动安装到 install/workbench/。
cmake --build build --target workbench
```

`client` 和 `server` 需要先安装 `runtime`，否则 `install/` 不会更新。Workbench 需要 Python 3.12+、uv、Node.js 22+、Corepack 和 WebView2；默认按 `profile=dev` 打包，构建目标会自动执行 Workbench 组件安装，入口为 `install/workbench/Mir2xWorkbench.exe`。也可在仓库根目录运行 `make -C tools/gameplay-workbench binary`；追加 `profile=prod` 可覆盖默认 profile。详见[工作台说明](../../tools/gameplay-workbench/README.md)。

各安装组件的职责如下：

| 组件 | 安装内容 |
| --- | --- |
| `runtime` | 更新原生程序和内容脚本 |
| `gameplay-config` | 首次安装默认玩法配置，已有目录则校验并保留 |
| `workbench` | 更新已打包的工作台，保留本地偏好和日志 |
| `resources` | 重新打包地图、纹理等资源，要求源资源完整 |

需要完整安装或重新打包资源时，执行 `cmake --install build --config Release`。日常更新使用上面的组件命令复用已有资源。

修改 CMake、编译器、依赖或构建类型时，用 `build.py` 重新配置。增加新 C++ 文件后也应重新配置，因为源文件列表由配置阶段的目录扫描生成。

- 普通业务 C++ 改动可先构建对应 target；公共头文件改动可能同时影响客户端、服务端和工具。
- `server/src/*.lua` 中不少框架脚本通过 `#embed` 编进 C++，修改后需要重编。
- `server/script/` 中的内容脚本由安装步骤复制；修改后至少重新安装并重启加载相应脚本的服务端。
- `--build-type=Debug` 可以切换构建类型，但会改变现有构建树和安装目录中的程序。调试时也需要重新构建实际调试的目标。
- `--no-install` 只生成构建目录中的程序；不要据此认为 `install/` 已更新。
- `--fresh` 会删除整个 `build/`，包括已安装依赖和默认资源克隆。普通改动不需要使用它。

## 从安装目录启动游戏

在仓库根目录打开两个 MSYS2 UCRT64 Bash 终端，分别启动服务端和客户端；先切换到各自的安装目录，使程序使用正确的工作目录。

```bash
cd install/server
./server.exe --auto-launch
```

等待服务端完成监听及地图初始化后，在第二个终端执行：

```bash
cd install/client
./client.exe --server-ip=127.0.0.1 --server-port=7000
```

默认玩家端口为 7000，节点端口为 8000；自定义端口时同步修改服务端 `--client-port` 与客户端 `--server-port`。登录、选角与进图流程见[启动与协议](runtime-and-protocol.md)。

## 制品与存档的区别

新程序在 `install/`，从 Release 下载并解压的程序位于各自的解压目录，是独立的安装目录。修改源码不会自动更新下载目录。

服务端数据库文件名是 `mir2x.db3`，相对于服务端进程工作目录。用本文命令启动时，它位于 `install/server/mir2x.db3`。重装程序通常不会删除这个运行时文件，也不会自动把旧结构迁移到新结构。

不要在服务器运行时用资源复制或目录清理命令覆盖存档。需要保存一份开发存档时，先正常停止该服务端，再备份数据库及存在的 SQLite 伴随文件。游戏数据结构的更改见 [业务模型与数据](domain-and-data.md#结构变更与存档兼容)。
