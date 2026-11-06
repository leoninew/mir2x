# Windows 开发环境
最后整理日期: 2026-11-05

[返回知识库](README.md)

本机曾用 MSYS2 UCRT64 完整编译、安装客户端、服务端和资源。以下目录是本机示例，可替换为自己的路径；历史构建结果见[排障与验证基线](troubleshooting-and-baseline.md)，不代表当前 `main` 已重新构建。

## 工具链与目录约定

| 项目 | 本机值 |
| --- | --- |
| 源码和命令起点 | `D:\SourceCodes\opensource\mir2x` |
| MSYS2 | `D:\ProgramFiles\msys64` |
| 环境 | UCRT64，64 位 MinGW，vcpkg `x64-mingw-static` |
| 编译器 | GCC 16.2.0；当前 CMake 要求 C++26 |
| 构建工具 | CMake 4.4.5、Ninja 1.13.2 |
| 构建脚本运行时 | Python 3.14.9 |
| CMake 工作树 | `build/` |
| 安装结果 | `install/client/`、`install/server/`、`install/tools/`；已构建的工作台安装到 `install/workbench/` |
| vcpkg 源码与下载 | `vcpkg/` |
| 已安装依赖 | `build/vcpkg_installed/` |
| 原始游戏资源 | `build/assets/mir2x_assets/` |
| 本机日志、辅助脚本与二进制缓存 | `.vcpkg/`，已被 Git 忽略 |

`build.py --build-dir=.` 的含义是以当前目录为构建根，然后在其中使用 `build/`、`install/` 和 `vcpkg/`，不是把编译产物直接散落到源码目录。

依据：[build.py](../../build.py)、[CMakeLists.txt](../../CMakeLists.txt)、[Windows CI](../../.github/workflows/build.yml)。工具版本是历史本机验证值，不是要求以后只能使用这些补丁版本。

## 当前机器的快捷构建

在根目录 PowerShell 中执行：

```powershell
& 'D:\ProgramFiles\msys64\usr\bin\bash.exe' --noprofile --norc ./scripts/build-msys2.sh
```

这个[构建辅助脚本](../../scripts/README.md)设置 UCRT64、Ninja、编译器、构建根和二进制缓存，并调用项目的 `build.py`。它已纳入版本控制，新的克隆可以直接使用。

辅助脚本接受追加的 `build.py` 参数，例如 `--no-install`、`--run-test`。默认并行数为 8，可用 `MIR2X_BUILD_JOBS` 或 `--parallel=N` 覆盖；代理与已有 vcpkg 缓存配置沿用当前环境。

## 不依赖本机辅助脚本的构建方法

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

随后在同一终端构建：

```bash
cd /d/SourceCodes/opensource/mir2x
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

需要代理时，在当前构建终端设置实际代理地址。代理不是项目运行条件：

```bash
export HTTPS_PROXY=http://127.0.0.1:7890
export HTTP_PROXY=http://127.0.0.1:7890
```

本机曾遇到 MSYS2 Git 在 vcpkg 子进程中初始化 curl 失败。遇到同类问题时，可在 UCRT64 终端临时执行 `export PATH="/c/Program Files/Git/cmd:$PATH"`，再直接运行本节的 `python3 ./build.py ...` 命令，使用 Git for Windows；其他工具仍来自 UCRT64。按实际 Git 安装路径调整，无需修改整个系统的 PATH。

## 日常增量构建

在已完整构建过的 UCRT64 环境和仓库根目录中：

```bash
# 编译客户端或服务端；这一步不更新 install 中的程序。
cmake --build build --target client server --parallel 8

# 将构建结果和内容脚本安装到运行目录。
cmake --install build --config Release
```

工作台也使用这棵构建和安装树：`cmake --build build --target workbench` 构建后执行相同的 `cmake --install build --config Release`，正式入口为 `install/workbench/Mir2xWorkbench.exe`；`build.py` 的 `--target workbench` 也可使用。需已安装 uv、Node.js/Corepack 及 WebView2，默认启动路径对应同级client/server，可在工作台「路径设置」中修改。工具目录的Makefile仅是相同打包脚本的便捷入口，不要求为主CMake流程安装GNU Make。完整安装保留工作台本地偏好和日志。

已有完整资源的运行目录可按组件增量安装：`--component runtime` 更新client/server和脚本，`--component gameplay-config`首次安装玩法默认目录或校验保留已有目录，`--component workbench`更新工作台。`--component resources`重打包地图、纹理等资源，要求构建资源源目录完整；无组件的完整安装仍包含这些步骤。

修改 CMake、编译器、依赖或构建类型时，用 `build.py` 重新配置。增加新 C++ 文件后也应重新配置，因为源文件列表由配置阶段的目录扫描生成。

- 普通业务 C++ 改动可先构建对应 target；公共头文件改动可能同时影响客户端、服务端和工具。
- `server/src/*.lua` 中不少框架脚本通过 `#embed` 编进 C++，修改后需要重编。
- `server/script/` 中的内容脚本由安装步骤复制；修改后至少重新安装并重启加载相应脚本的服务端。
- `--build-type=Debug` 可以切换构建类型，但会改变现有构建树和安装目录中的程序。调试时也需要重新构建实际调试的目标。
- `--no-install` 只生成构建目录中的程序；不要据此认为 `install/` 已更新。
- `--fresh` 会删除整个 `build/`，包括已安装依赖和默认资源克隆。普通改动不需要使用它。

## 从安装目录启动游戏

在两个 PowerShell 终端分别启动服务端和客户端，工作目录必须指向各自的安装目录：

```powershell
Set-Location ./install/server
./server.exe --auto-launch
```

等待服务端完成监听及地图初始化后，在第二个终端执行：

```powershell
Set-Location ./install/client
./client.exe --server-ip=127.0.0.1 --server-port=7000
```

默认玩家端口为 7000，节点端口为 8000；自定义端口时同步修改服务端 `--client-port` 与客户端 `--server-port`。登录、选角与进图流程见[启动与协议](runtime-and-protocol.md)。

## 制品与存档的区别

新程序在 `install/`，此前下载的程序在 `D:\Downloads\mir2x-windows-build-bb6a222`，是两套独立目录。修改源码不会自动更新下载目录。

服务端数据库文件名是 `mir2x.db3`，相对于服务端进程工作目录。用本文命令启动时，它位于 `install/server/mir2x.db3`。重装程序通常不会删除这个运行时文件，也不会自动把旧结构迁移到新结构。

不要在服务器运行时用资源复制或目录清理命令覆盖存档。需要保存一份开发存档时，先正常停止该服务端，再备份数据库及存在的 SQLite 伴随文件。游戏数据结构的更改见 [业务模型与数据](domain-and-data.md#结构变更与存档兼容)。
