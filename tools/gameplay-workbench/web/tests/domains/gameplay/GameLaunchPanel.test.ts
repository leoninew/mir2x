import { enableAutoUnmount, flushPromises, mount } from "@vue/test-utils";
import { createPinia, setActivePinia } from "pinia";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import GameLaunchPanel from "@/domains/gameplay/GameLaunchPanel.vue";
import type { GameKind, LaunchStatus, LaunchTarget } from "@/services/gameLauncher";
import { useToastsStore } from "@/stores/toasts";

const native = vi.hoisted(() => ({
  get_game_launch_status: vi.fn(),
  save_game_launch_paths: vi.fn(),
  launch_game: vi.fn(),
  stop_game: vi.fn(),
}));

vi.mock("@/services/desktopBridge", () => ({
  getDesktopBridge: () => native,
  isDesktopApp: () => true,
}));

enableAutoUnmount(afterEach);
let mountTarget: HTMLDivElement;
beforeEach(() => {
  setActivePinia(createPinia());
  vi.useFakeTimers({ toFake: ["setInterval", "clearInterval"] });
  mountTarget = document.createElement("div");
  document.body.append(mountTarget);
});
afterEach(() => {
  mountTarget.remove();
  vi.useRealTimers();
  vi.resetAllMocks();
  vi.restoreAllMocks();
});

function launchStatus(running?: GameKind): LaunchStatus {
  const target = (kind: GameKind): LaunchTarget => ({
    executable: `D:/app/install/${kind}/${kind}.exe`,
    working_directory: `D:/app/install/${kind}`,
    exists: true,
    running: kind === running,
    pid: kind === running ? 1234 : null,
    exit_code: null,
    launched_executable: kind === running ? `D:/app/install/${kind}/${kind}.exe` : null,
    warning: "",
    player_port: null,
    listening: null,
    ownership: kind === running ? "managed" : null,
    ownership_warning: null,
  });
  return {
    base_directory: "D:/app/install/workbench",
    settings: {
      schemaVersion: 2,
      clientPath: "../client/client.exe",
      serverPath: "../server/server.exe",
      serverConfigPath: "../server",
      communication: {
        client: { serverHost: "localhost", serverPort: 7000 },
        server: { playerPort: 7000, peerPort: 8000 },
      },
    },
    revision: "revision",
    settings_error: null,
    process_state_error: null,
    targets: { client: target("client"), server: target("server") },
  };
}

const props = { gameplayDirty: false, gameplayBusy: false, workspaceRoot: null };

describe("game process controls", () => {
  it("reloads unchanged settings without a discard prompt", async () => {
    native.get_game_launch_status.mockResolvedValue({ success: true, data: launchStatus() });
    const confirm = vi.spyOn(window, "confirm").mockReturnValue(false);
    const wrapper = mount(GameLaunchPanel, {
      props,
      attachTo: mountTarget,
      global: { stubs: { DialogPortal: { template: "<slot />" } } },
    });
    await flushPromises();
    wrapper.vm.openSettings();
    await flushPromises();

    await wrapper
      .findAll("button")
      .find((button) => button.text() === "重新读取")!
      .trigger("click");
    await flushPromises();

    expect(confirm).not.toHaveBeenCalled();
    expect(native.get_game_launch_status).toHaveBeenCalledTimes(2);
  });

  it("shows communication field errors before saving", async () => {
    native.get_game_launch_status.mockResolvedValue({ success: true, data: launchStatus() });
    const wrapper = mount(GameLaunchPanel, {
      props,
      attachTo: mountTarget,
      global: { stubs: { DialogPortal: { template: "<slot />" } } },
    });
    await flushPromises();
    wrapper.vm.openSettings();
    await flushPromises();

    await wrapper.get("#client-server-host").setValue("  ");
    await wrapper.get("#client-server-port").setValue("1024");
    await wrapper.get("form").trigger("submit");

    expect(wrapper.get("#client-server-host").attributes("aria-invalid")).toBe("true");
    expect(wrapper.get("#client-server-host-error").text()).toBe("请输入有效的服务端地址");
    expect(wrapper.get("#client-server-port-error").text()).toBe("端口范围为 1025–65535");
    expect(native.save_game_launch_paths).not.toHaveBeenCalled();
  });

  it("saves communication settings for client and server launches", async () => {
    const updated = launchStatus();
    updated.settings.communication.client.serverHost = "192.0.2.10";
    updated.settings.communication.client.serverPort = 7520;
    updated.settings.communication.server.playerPort = 7520;
    updated.settings.communication.server.peerPort = 8100;
    updated.targets.server.running = true;
    updated.targets.server.pid = 1234;
    updated.targets.server.player_port = 7520;
    updated.targets.server.listening = true;
    updated.targets.server.ownership = "managed";
    native.get_game_launch_status.mockResolvedValue({ success: true, data: launchStatus() });
    native.save_game_launch_paths.mockResolvedValue({ success: true, data: updated });
    const wrapper = mount(GameLaunchPanel, {
      props,
      attachTo: mountTarget,
      global: { stubs: { DialogPortal: { template: "<slot />" } } },
    });
    await flushPromises();
    wrapper.vm.openSettings();
    await flushPromises();

    await wrapper.get("#client-server-host").setValue("192.0.2.10");
    await wrapper.get("#client-server-port").setValue("7520");
    await wrapper.get("#server-client-port").setValue("7520");
    await wrapper.get("#server-peer-port").setValue("8100");
    await wrapper.get("form").trigger("submit");
    await flushPromises();

    expect(native.save_game_launch_paths).toHaveBeenCalledWith(
      "../client/client.exe",
      "../server/server.exe",
      "revision",
      "../server",
      {
        client: { serverHost: "192.0.2.10", serverPort: 7520 },
        server: { playerPort: 7520, peerPort: 8100 },
      },
    );
    expect(wrapper.findAll("article")[0].text()).toContain("连接目标 192.0.2.10:7520");
    expect(wrapper.findAll("article")[1].text()).toContain("玩家端口 7520 已监听");
  });

  it.each([
    ["client", "客户端"],
    ["server", "服务端"],
  ] as const)("uses the same button to start and stop %s", async (kind, label) => {
    native.get_game_launch_status
      .mockResolvedValueOnce({ success: true, data: launchStatus() })
      .mockResolvedValueOnce({ success: true, data: launchStatus(kind) })
      .mockResolvedValueOnce({ success: true, data: launchStatus() });
    native.launch_game.mockResolvedValue({ success: true, pid: 1234 });
    native.stop_game.mockResolvedValue({ success: true, pid: 1234 });
    const wrapper = mount(GameLaunchPanel, { props });
    await flushPromises();
    const button = wrapper
      .findAll("article button")
      .find((item) => item.text() === `启动${label}`)!;

    await button.trigger("click");
    await flushPromises();
    expect(native.launch_game).toHaveBeenCalledWith(kind, "revision");
    expect(button.text()).toBe(`停止${label}`);
    expect(button.attributes("disabled")).toBeUndefined();

    await button.trigger("click");
    await flushPromises();
    expect(native.stop_game).toHaveBeenCalledWith(kind, 1234);
    expect(button.text()).toBe(`启动${label}`);
    expect(useToastsStore().items).toContainEqual(
      expect.objectContaining({ message: `${label}进程已停止 · PID 1234`, tone: "success" }),
    );
  });

  it("stops a running process even when settings are invalid or gameplay is busy", async () => {
    const status = launchStatus("server");
    status.settings_error = "启动配置格式错误";
    status.targets.server.exists = false;
    native.get_game_launch_status.mockResolvedValue({ success: true, data: status });
    native.stop_game.mockResolvedValue({ success: true, pid: 1234 });
    const confirm = vi.spyOn(window, "confirm").mockReturnValue(false);
    const wrapper = mount(GameLaunchPanel, {
      props: { gameplayDirty: true, gameplayBusy: true, workspaceRoot: "D:/other/server" },
    });
    await flushPromises();
    const button = wrapper.findAll("article")[1].get("button");

    expect(button.text()).toBe("停止服务端");
    expect(button.attributes("disabled")).toBeUndefined();
    await button.trigger("click");
    await flushPromises();

    expect(native.stop_game).toHaveBeenCalledWith("server", 1234);
    expect(native.launch_game).not.toHaveBeenCalled();
    expect(confirm).not.toHaveBeenCalled();
  });

  it("shows progress while stopping and retains the stop action after a failure", async () => {
    native.get_game_launch_status.mockResolvedValue({
      success: true,
      data: launchStatus("client"),
    });
    let finishStop!: (result: { success: boolean; message: string }) => void;
    native.stop_game.mockReturnValue(
      new Promise((resolve) => {
        finishStop = resolve;
      }),
    );
    const wrapper = mount(GameLaunchPanel, { props });
    await flushPromises();
    const button = wrapper.findAll("article")[0].get("button");

    await button.trigger("click");
    expect(button.attributes("disabled")).toBeDefined();
    expect(button.get('[role="progressbar"]').attributes("aria-label")).toBe("正在停止客户端…");
    finishStop({ success: false, message: "无法停止 client 进程" });
    await flushPromises();

    expect(button.text()).toBe("停止客户端");
    expect(button.attributes("disabled")).toBeUndefined();
    expect(useToastsStore().items).toContainEqual(
      expect.objectContaining({ message: "无法停止 client 进程", tone: "error" }),
    );
  });

  it("restores the start action when a running process exits on its own", async () => {
    const stopped = launchStatus();
    stopped.targets.client.exit_code = 0;
    native.get_game_launch_status
      .mockResolvedValueOnce({ success: true, data: launchStatus("client") })
      .mockResolvedValueOnce({ success: true, data: stopped });
    const wrapper = mount(GameLaunchPanel, { props });
    await flushPromises();
    const button = wrapper.findAll("article")[0].get("button");
    expect(button.text()).toBe("停止客户端");

    await vi.advanceTimersByTimeAsync(1000);
    await flushPromises();

    expect(button.text()).toBe("启动客户端");
    expect(wrapper.findAll("article")[0].text()).toContain("已退出 · 0");
    expect(native.stop_game).not.toHaveBeenCalled();
  });

  it("does not offer control when a restored process identity is unknown", async () => {
    const status = launchStatus();
    status.targets.client.ownership = "unknown";
    status.targets.client.pid = 1234;
    status.targets.client.ownership_warning = "无法验证 PID 1234 的进程身份";
    native.get_game_launch_status.mockResolvedValue({ success: true, data: status });
    const wrapper = mount(GameLaunchPanel, { props });
    await flushPromises();
    const button = wrapper.findAll("article")[0].get("button");

    expect(button.text()).toBe("客户端状态未知");
    expect(button.attributes("disabled")).toBeDefined();
    expect(wrapper.text()).toContain("无法验证 PID 1234 的进程身份");
    expect(wrapper.findAll("article")[0].text()).not.toContain("未启动");
    expect(wrapper.findAll("article")[0].text()).toContain("PID 1234");
    await button.trigger("click");

    expect(native.stop_game).not.toHaveBeenCalled();
    expect(native.launch_game).not.toHaveBeenCalled();
  });

  it("polls unknown ownership and restores control when identity becomes verifiable", async () => {
    const unknown = launchStatus();
    unknown.targets.client.ownership = "unknown";
    unknown.targets.client.pid = 1234;
    native.get_game_launch_status
      .mockResolvedValueOnce({ success: true, data: unknown })
      .mockResolvedValueOnce({ success: true, data: launchStatus("client") });
    const wrapper = mount(GameLaunchPanel, { props });
    await flushPromises();
    const button = wrapper.findAll("article")[0].get("button");

    expect(button.attributes("disabled")).toBeDefined();
    await vi.advanceTimersByTimeAsync(1000);
    await flushPromises();

    expect(button.text()).toBe("停止客户端");
    expect(button.attributes("disabled")).toBeUndefined();
  });

  it("blocks new launches when the process registry cannot be read", async () => {
    const status = launchStatus();
    status.process_state_error = "进程记录格式错误";
    native.get_game_launch_status.mockResolvedValue({ success: true, data: status });
    const wrapper = mount(GameLaunchPanel, { props });
    await flushPromises();

    const button = wrapper.findAll("article")[0].get("button");
    expect(button.text()).toBe("启动客户端");
    expect(button.attributes("disabled")).toBeDefined();
    expect(wrapper.get('[role="alert"]').text()).toContain("进程记录格式错误");
    await button.trigger("click");

    expect(native.launch_game).not.toHaveBeenCalled();
  });
});
