import { afterEach, describe, expect, it, vi } from "vitest";

import { desktopBridge, getDesktopBridge, isDesktopApp } from "@/services/desktopBridge";

const originalBridge = window.pywebview;

afterEach(() => {
  window.pywebview = originalBridge;
});

describe("desktop bridge", () => {
  it("reports when the app is running in a browser", () => {
    window.pywebview = undefined;

    expect(isDesktopApp()).toBe(false);
    expect(() => getDesktopBridge()).toThrow("native desktop bridge is unavailable");
  });

  it("forwards calls to the native PyWebView API", async () => {
    const api = {
      select_gameplay_workspace: vi.fn().mockResolvedValue({ success: true }),
      get_game_launch_status: vi.fn().mockResolvedValue({ success: true }),
      save_game_launch_paths: vi.fn().mockResolvedValue({ success: true }),
      launch_game: vi.fn().mockResolvedValue({ success: true }),
      get_game_log: vi.fn().mockResolvedValue({ success: true, data: "" }),
      stop_game: vi.fn().mockResolvedValue({ success: true }),
      say_hello: vi.fn().mockResolvedValue({ success: true, message: "Hello", timestamp: "now" }),
      get_system_info: vi.fn().mockResolvedValue({ success: true, data: { system: "Windows" } }),
      resize_window: vi.fn().mockResolvedValue({ success: true }),
      get_window_info: vi.fn().mockResolvedValue({ success: true, title: "Example Desktop" }),
      test_notification: vi.fn().mockResolvedValue({ success: true }),
      run_shell_command: vi.fn().mockResolvedValue({ success: true, stdout: "ok" }),
    };
    window.pywebview = { api };

    expect(isDesktopApp()).toBe(true);
    await expect(desktopBridge.sayHello("Ada")).resolves.toMatchObject({ success: true });
    await expect(desktopBridge.getSystemInfo()).resolves.toMatchObject({ success: true });
    await expect(desktopBridge.resizeWindow(1200, 800)).resolves.toMatchObject({ success: true });
    await expect(desktopBridge.getWindowInfo()).resolves.toMatchObject({
      title: "Example Desktop",
    });
    await expect(desktopBridge.showNotification("Title", "Message")).resolves.toMatchObject({
      success: true,
    });
    await expect(desktopBridge.runShellCommand("echo ok")).resolves.toMatchObject({
      stdout: "ok",
    });

    expect(api.say_hello).toHaveBeenCalledWith("Ada");
    expect(api.resize_window).toHaveBeenCalledWith(1200, 800);
    expect(api.test_notification).toHaveBeenCalledWith("Title", "Message");
    expect(api.run_shell_command).toHaveBeenCalledWith("echo ok");
  });
});
