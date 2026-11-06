/// <reference types="vite/client" />

import type { LaunchSettingsResult } from "@/services/gameLauncher";

interface DesktopResult {
  success: boolean;
  message?: string;
}

interface PyWebViewAPI {
  get_game_launch_status(): Promise<LaunchSettingsResult>;
  save_game_launch_paths(
    clientPath: string,
    serverPath: string,
    revision: string,
    serverConfigPath: string,
  ): Promise<LaunchSettingsResult>;
  launch_game(kind: string, revision: string): Promise<DesktopResult & { pid?: number }>;
  select_gameplay_workspace(): Promise<DesktopResult>;
  say_hello(name: string): Promise<{ success: boolean; message: string; timestamp: string }>;
  get_system_info(): Promise<{
    success: boolean;
    data: Record<string, string>;
  }>;
  resize_window(width: number, height: number): Promise<DesktopResult>;
  get_window_info(): Promise<DesktopResult & { title?: string; resizable?: boolean }>;
  test_notification(title: string, message: string): Promise<DesktopResult>;
  run_shell_command(command: string): Promise<
    DesktopResult & {
      stdout?: string;
      stderr?: string;
      returncode?: number;
    }
  >;
}

declare global {
  interface Window {
    pywebview?: { api: PyWebViewAPI };
  }
}

export {};
