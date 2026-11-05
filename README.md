# mir2x

<a href="https://github.com/etorth/mir2x/actions/workflows/build.yml">
  <img alt="GitHub Actions 构建状态"
       src="https://github.com/etorth/mir2x/actions/workflows/build.yml/badge.svg"/>
</a>
<a href="https://scan.coverity.com/projects/etorth-mir2x">
  <img alt="Coverity Scan 扫描状态"
       src="https://scan.coverity.com/projects/9270/badge.svg"/>
</a>
<a href="https://gitter.im/mir2x/community?utm_source=share-link&utm_medium=link&utm_campaign=share-link">
  <img alt="Gitter 聊天"
       src="https://badges.gitter.im/org.png"/>
</a>

mir2x 是一个验证 MMORPG 中基于 Actor 模型的并行处理能力的实验项目，采用客户端/服务端架构，支持多个平台，包含玩家和开发者使用的组件：

  - `client`：客户端
  - `server`：服务端
  - `pkgviewer`：资源包查看器
  - `animaker`：动画制作工具
  - `mapeditor`：地图编辑器

<a id="prebuilt-binaries"></a>
### 预编译制品

`release` 分支构建成功后会更新滚动的 `latest` GitHub Release，其中包含 Linux 和 Windows MinGW UCRT64 安装目录；其他分支进行构建验证，但不会替换该发布版本。发布页面：<https://github.com/etorth/mir2x/releases/tag/latest>。

<a id="development-knowledge-base"></a>
### 开发知识库

请从[开发知识库](readme/development/README.md)查阅代码地图、启动和登录流程、业务与持久化模型、Lua 内容开发、测试和排障。[Windows 开发指南](readme/development/windows-development.md)记录了已经验证的 MSYS2 UCRT64 环境，以及将构建和安装目录保留在仓库内的方式。

日常 Windows 构建可使用 [UCRT64 辅助脚本](scripts/README.md)。地图、装备与技能见[玩法参考](readme/gameplay/README.md)，已有配置的可视化管理讨论见[研究记录](docs/research/20261104-runtime-gameplay-config.md)。

配置的现有 owner、持久化方式和生效时机见[配置系统分析](readme/development/configuration.md)。

<a id="notes"></a>
### 说明
- 本项目使用 C++ 协程实现 Actor 模型；当前 CMake 配置要求 C++26，CI 使用 GCC 16。
- 本项目以经典的 mir2 v1.45 作为参考实现，可通过以下方式体验原版游戏：
  - 安装 [win-xp](https://github.com/etorth/winxp-zh) 以运行游戏服务端和客户端；已在实体机和 VirtualBox 虚拟机上尝试。
  - 安装 [mir2-v1.45](https://github.com/etorth/CBWCQ3) 的服务端和客户端。
  - 将屏幕颜色模式改为 16 位后运行游戏。

<a id="public-server"></a>
### 公共服务器
- 参照[教程](https://github.com/etorth/mir2x/wiki/Host-your-monoserver-on-Oracle-Cloud)，可在 Oracle Cloud 上运行公共 `server`。
- 可尝试通过以下命令连接公共测试服务器 `192.9.241.118`；该服务器近期未维护：
  ```shell
  client --server-ip=192.9.241.118
  ```

YouTube 视频：[1](https://youtu.be/Yz-bGOkDyEQ) [2](https://youtu.be/jl1LPxe2EAA) [3](https://youtu.be/TtGONA83Mb8)

<https://user-images.githubusercontent.com/1754214/162589720-7dd9453b-55e4-4119-a1ee-c879093cf017.mp4>


SDL 全屏模式的输入法：

<https://user-images.githubusercontent.com/1754214/213572554-785e826c-226d-43fa-a196-ee4f92112db2.mp4>


<a id="building-from-source"></a>
### 从源码构建

在 64 位原生 Linux 和 64 位 MSYS2 UCRT64/MinGW 上，mir2x 使用 vcpkg manifest 模式管理第三方依赖。构建脚本会在当前工作目录中克隆并初始化本地 vcpkg，配置 CMake，然后执行编译和安装。


<a id="linux-ubuntu-2604"></a>
#### Linux (Ubuntu 26.04)

使用 GCC 16 构建 mir2x：

```sh
sudo apt update
sudo apt install -y \
    autoconf autoconf-archive automake \
    build-essential cmake curl \
    g++-16 gcc-16 gawk gettext git libtool ninja-build pkg-config \
    python3 tar unzip \
    libgl1-mesa-dev libglu1-mesa-dev \
    libice-dev libltdl-dev libsm-dev \
    libx11-dev libxcursor-dev libxext-dev libxfixes-dev \
    libxft-dev libxinerama-dev libxrender-dev \
    libasound2-dev libdbus-1-dev libibus-1.0-dev libpulse-dev libudev-dev \
    libxi-dev libxkbcommon-dev libxrandr-dev libxss-dev libxtst-dev
```

然后克隆并构建：

```sh
git clone https://github.com/etorth/mir2x.git
mkdir b_mir2x && cd b_mir2x
python3 /path/to/mir2x/build.py --c-compiler=gcc-16 --cxx-compiler=g++-16 --parallel=10
```

<a id="windows-msys2-ucrt64"></a>
#### Windows (MSYS2 UCRT64)

安装 [MSYS2](https://www.msys2.org/)，然后在 UCRT64 终端安装工具链：

```sh
pacman -S --needed \
    mingw-w64-ucrt-x86_64-toolchain \
    mingw-w64-ucrt-x86_64-git \
    mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-ninja \
    mingw-w64-ucrt-x86_64-pkgconf \
    mingw-w64-ucrt-x86_64-python
```

随后在同一 UCRT64 终端克隆并构建；脚本默认选择 `x64-mingw-static` vcpkg triplet：

```sh
git clone https://github.com/etorth/mir2x.git
mkdir b_mir2x && cd b_mir2x
python3 /path/to/mir2x/build.py --build-dir=/path/to/b_mir2x --parallel=10
```

<a id="helper-script-options"></a>
#### 构建脚本选项

默认执行增量构建：重复运行同一命令会保留 `<build-dir>/build`，包括 CMake 对象文件、`vcpkg_installed` 和默认资源克隆。只有确实需要全新构建时才使用 `--fresh`：它会删除整个 `<build-dir>/build`，包括 `vcpkg_installed` 和 `<build-dir>/build/assets/mir2x_assets`，之后依赖需要重新安装或编译，默认资源也需要重新克隆。

安装阶段始终执行客户端和服务端资源打包。省略 `--assets-path` 时，CMake 会在构建阶段将 `https://github.com/etorth/mir2x_assets.git` 克隆到 `<build-dir>/build/assets/mir2x_assets`。要使用已有资源仓库，可指定：

```sh
/path/to/mir2x/build.py [options] --assets-path=/path/to/mir2x_assets
```

其他常用选项：

- `--c-compiler=<cc> --cxx-compiler=<cxx>` 为 vcpkg ports 和 mir2x 构建目标选择编译器，内部会启用 `VCPKG_CHAINLOAD_TOOLCHAIN_FILE`。
- `--parallel=<N>` 控制构建并行度。
- `--verbose` 显示详细的 CMake/vcpkg 命令输出。

<a id="first-time-run"></a>
### 首次运行
单节点服务端可以部署在 Linux 机器上，项目作者曾使用 `Oracle Cloud Infrastructure` 的 `always-free` 方案。启动客户端之前，需要通过服务端菜单 `server/launch` 或启动参数启动服务：

```sh
cd b_mir2x/install/server
./server --auto-launch
```

启动客户端；当前可以使用默认账号（账号 `test`，密码 `123456`）进行尝试：

```sh
cd b_mir2x/install/client
./client --server-ip=localhost --auto-login=test:123456
```

<a id="packages"></a>
### 开源依赖

mir2x 使用以下开源项目，自身也在 GitHub 上开源；如有遗漏，欢迎提醒。

* [SDL3](https://www.libsdl.org/)：提供硬件抽象层的跨平台开发库。
* [FLTK](http://www.fltk.org)：支持 UNIX/Linux（X11）、Windows 和 macOS 的跨平台 C++ GUI 工具包。
* [asio](http://www.think-async.com/)：用于网络和底层 I/O 编程的跨平台 C++ 库。
* [g3log](https://github.com/KjellKod/g3log)：易于使用的异步日志库，支持崩溃安全处理。
* [lua](https://www.lua.org/)：高效、轻量、可嵌入的脚本语言。
* [sol2](https://github.com/ThePhD/sol2)：C++ 和 Lua 之间的绑定库。
* [tinyxml2](http://www.grinninglizard.com/tinyxml2/)：小巧高效的 C++ XML 解析器。
* [utf8-cpp](http://utfcpp.sourceforge.net/)：用于处理 UTF-8 字符串的可移植 C++ 库。
* [SQLiteCpp](https://github.com/SRombauts/SQLiteCpp)：C++ SQLite3 封装库。
