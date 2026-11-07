export type GameKind = "client" | "server";
export type LaunchPaths = {
  client_path: string;
  server_path: string;
  server_config_path: string;
};
export type LaunchTarget = {
  executable: string;
  working_directory: string;
  exists: boolean;
  running: boolean;
  pid: number | null;
  exit_code: number | null;
  launched_executable: string | null;
  warning: string;
  listening: boolean | null;
  ownership: "managed" | "unknown" | null;
  ownership_warning: string | null;
};
export type LaunchStatus = {
  base_directory: string;
  settings: LaunchPaths & { schema_version: number };
  revision: string;
  settings_error: string | null;
  process_state_error: string | null;
  targets: Record<GameKind, LaunchTarget>;
};
export type LaunchSettingsResult = {
  success: boolean;
  message?: string;
  data?: LaunchStatus;
  issues?: { path: string; message: string }[];
};

export function validateLaunchPaths(
  paths: LaunchPaths,
): Partial<Record<keyof LaunchPaths, string>> {
  const errors: Partial<Record<keyof LaunchPaths, string>> = {};
  for (const field of ["client_path", "server_path", "server_config_path"] as const) {
    const value = paths[field];
    if (!value.trim() || value !== value.trim() || /[\u0000-\u001f]/u.test(value)) {
      errors[field] = "请输入不含空白前后缀的相对路径";
    } else if (/^(?:[a-z]:|[/\\])/iu.test(value)) {
      errors[field] = "请使用相对于工作台目录的路径，不要填写绝对路径";
    } else if (field !== "server_config_path" && !/\.exe$/iu.test(value)) {
      errors[field] = "请填写 .exe 程序，不支持脚本或命令";
    }
  }
  return errors;
}
