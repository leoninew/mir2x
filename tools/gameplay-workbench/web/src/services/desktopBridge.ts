export type DesktopResult = {
  success: boolean;
  message?: string;
};

export type DesktopSystemInfo = {
  system: string;
  release: string;
  version: string;
  machine: string;
  processor: string;
  python_version: string;
};

export type DesktopWindowInfo = DesktopResult & {
  title?: string;
  resizable?: boolean;
};

export type ShellCommandResult = DesktopResult & {
  stdout?: string;
  stderr?: string;
  returncode?: number;
};

export function isDesktopApp(): boolean {
  return window.pywebview !== undefined;
}

export function getDesktopBridge(): PyWebViewAPI {
  const api = window.pywebview?.api;
  if (api === undefined) {
    throw new Error("The native desktop bridge is unavailable in this browser.");
  }
  return api;
}

export const desktopBridge = {
  sayHello(name: string) {
    return getDesktopBridge().say_hello(name);
  },
  getSystemInfo() {
    return getDesktopBridge().get_system_info();
  },
  resizeWindow(width: number, height: number) {
    return getDesktopBridge().resize_window(width, height);
  },
  getWindowInfo() {
    return getDesktopBridge().get_window_info();
  },
  showNotification(title: string, message: string) {
    return getDesktopBridge().test_notification(title, message);
  },
  runShellCommand(command: string) {
    return getDesktopBridge().run_shell_command(command);
  },
};
