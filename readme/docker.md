<a id="manual-how-to-install-and-run-docker-in-wsl-ubuntu-2604"></a>
# 手册：在 WSL（Ubuntu 26.04）中安装和运行 Docker

本手册介绍如何在 Ubuntu 26.04 WSL（适用于 Linux 的 Windows 子系统）环境中直接安装 Docker，并启动一个干净、隔离的非 root 用户沙箱，用于测试安装和构建脚本。

---

<a id="part-1-installing-docker-engine-natively-in-wsl"></a>
## 一、在 WSL 中直接安装 Docker Engine

WSL 2 运行 Linux 内核，因此可以通过 `apt` 直接在发行版内安装 Docker Engine，无需依赖 Windows 桌面应用。

<a id="step-1-update-packages--install-prerequisites"></a>
### 步骤 1：更新软件包并安装前置依赖

打开 Ubuntu WSL 终端并执行：

```bash
sudo apt update
sudo apt install -y ca-certificates curl gnupg

```

<a id="step-2-add-dockers-official-gpg-key-and-repository"></a>
### 步骤 2：添加 Docker 官方 GPG 密钥和软件源

```bash
sudo install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
sudo chmod a+r /etc/apt/keyrings/docker.gpg

echo \
  "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu \
  $(. /etc/os-release && echo "$VERSION_CODENAME") stable" | \
  sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

```

<a id="step-3-install-docker-engine-components"></a>
### 步骤 3：安装 Docker Engine 组件

```bash
sudo apt update
sudo apt install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin

```

<a id="step-4-manage-background-services"></a>
### 步骤 4：管理后台服务

WSL 不一定自动启动系统服务。如果 Docker 守护进程尚未启动，可在工作会话开始时手动启动：

```bash
sudo service docker start

```

---

<a id="part-2-configuring-non-sudo-permissions"></a>
## 二、配置无 sudo 执行权限

默认情况下，Docker Unix socket 归 `root` 所有。要避免每次运行 `docker` 命令都输入 `sudo`，可以将 WSL 用户加入 `docker` 用户组。**该组成员实际上具有接近 root 的权限**，只应授予可信用户。

<a id="step-1-add-user-to-the-group"></a>
### 步骤 1：将用户加入用户组

```bash
sudo usermod -aG docker $USER

```

<a id="step-2-refresh-group-assignments"></a>
### 步骤 2：刷新用户组

无需重启整个 WSL 实例，可让当前终端会话立即重新加载用户组：

```bash
newgrp docker

```

---

<a id="part-3-launching-the-sandbox-environment"></a>
## 三、启动沙箱环境

官方 Ubuntu Docker 镜像为减小体积进行了精简，默认**不包含 `sudo` 软件包**；默认 `ubuntu` 用户也没有设置密码，因此相关 `su` 或 `sudo` 操作可能因认证失败而无法执行。

下面的命令会先以 root 身份启动容器并安装 `sudo`，为普通 `ubuntu` 用户授予免密码管理权限，挂载当前目录，然后进入非 root 环境。此配置只适合可信的本地测试。

<a id="the-comprehensive-test-command"></a>
### 完整测试命令

在 WSL 中进入项目目录，然后执行：

```bash
docker run --rm -it \
  -v "$(pwd)":/workspace -w /workspace \
  ubuntu:26.04 \
  bash -c "apt-get update && apt-get install -y sudo && echo 'ubuntu ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers && exec su - ubuntu -c 'cd /workspace && exec bash'"

```

<a id="command-parameter-breakdown"></a>
### 命令参数说明

* `--rm`：退出后自动删除容器及其内部文件变更，**不会删除或还原挂载目录中的文件变更**。
* `-it`：保持终端交互，将键盘输入连接到容器内的 shell。
* `-v "$(pwd)":/workspace`：将宿主机 WSL 当前目录（`$(pwd)`）挂载到容器的 `/workspace`。
* `-w /workspace`：设置容器默认工作目录，使进程从 `/workspace` 开始。
* `bash -c "..."`：先以 root 身份完成初始化，再切换到目标测试账号。

---

<a id="part-4-testing-inside-the-sandbox"></a>
## 四、在沙箱中测试

命令启动后可能出现类似警告：

```text
bash: cannot set terminal process group (-1): Inappropriate ioctl for device
bash: no job control in this shell
```

这些警告可以忽略，提示符会变为：

```text
ubuntu@<container_id>:/workspace$

```

此时已作为普通非 root 用户进入干净环境，可以检查权限限制和脚本执行情况。

<a id="verification-checkmarks"></a>
### 验证项目

1. **身份检查**：输入 `whoami`，输出应为 `ubuntu`。
2. **提权执行**：运行 `sudo apt update`，应无需输入密码即可更新软件包列表。
3. **文件同步**：对 `/workspace` 中代码或依赖的修改会立即反映到宿主机目录。

要删除容器并返回原终端环境，输入：

```bash
exit

```
