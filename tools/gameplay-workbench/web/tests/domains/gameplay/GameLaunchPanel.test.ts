import { enableAutoUnmount, flushPromises, mount } from "@vue/test-utils";
import { createPinia, setActivePinia } from "pinia";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import GameLaunchPanel from "@/domains/gameplay/GameLaunchPanel.vue";
import type { GameKind, LaunchStatus, LaunchTarget } from "@/services/gameLauncher";
import { useToastsStore } from "@/stores/toasts";

const native = vi.hoisted(() => ({
  get_game_launch_status: vi.fn(),
  launch_game: vi.fn(),
  stop_game: vi.fn(),
}));

vi.mock("@/services/desktopBridge", () => ({
  getDesktopBridge: () => native,
  isDesktopApp: () => true,
}));

enableAutoUnmount(afterEach);
beforeEach(() => {
  setActivePinia(createPinia());
  vi.useFakeTimers({ toFake: ["setInterval", "clearInterval"] });
});
afterEach(() => {
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
    listening: null,
    ownership: kind === running ? "managed" : null,
    ownership_warning: null,
  });
  return {
    base_directory: "D:/app/install/workbench",
    settings: {
      schema_version: 1,
      client_path: "../client/client.exe",
      server_path: "../server/server.exe",
      server_config_path: "../server",
    },
    revision: "revision",
    settings_error: null,
    process_state_error: null,
    targets: { client: target("client"), server: target("server") },
  };
}

const props = { gameplayDirty: false, gameplayBusy: false, workspaceRoot: null };

describe("game process controls", () => {
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
