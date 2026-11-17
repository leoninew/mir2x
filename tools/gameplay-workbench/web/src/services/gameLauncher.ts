export type GameKind = "client" | "server";
export type GameCommunication = {
  client: { serverHost: string; serverPort: number };
  server: { playerPort: number; peerPort: number };
};
export type LaunchPaths = {
  clientPath: string;
  serverPath: string;
  serverConfigPath: string;
  communication: GameCommunication;
};
export type LaunchField =
  | keyof Omit<LaunchPaths, "communication">
  | "communication.client.serverHost"
  | "communication.client.serverPort"
  | "communication.server.playerPort"
  | "communication.server.peerPort";
export type LaunchTarget = {
  executable: string;
  working_directory: string;
  exists: boolean;
  running: boolean;
  pid: number | null;
  exit_code: number | null;
  launched_executable: string | null;
  warning: string;
  player_port: number | null;
  listening: boolean | null;
  ownership: "managed" | "unknown" | null;
  ownership_warning: string | null;
};
export type LaunchStatus = {
  base_directory: string;
  settings: LaunchPaths & { schemaVersion: number };
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

export function validateLaunchPaths(paths: LaunchPaths): Partial<Record<LaunchField, string>> {
  const errors: Partial<Record<LaunchField, string>> = {};
  for (const field of ["clientPath", "serverPath", "serverConfigPath"] as const) {
    const value = paths[field];
    if (!value.trim() || value !== value.trim() || /[\u0000-\u001f]/u.test(value)) {
      errors[field] = "请输入不含空白前后缀的相对路径";
    } else if (/^(?:[a-z]:|[/\\])/iu.test(value)) {
      errors[field] = "请使用相对于工作台目录的路径，不要填写绝对路径";
    } else if (field !== "serverConfigPath" && !/\.exe$/iu.test(value)) {
      errors[field] = "请填写 .exe 程序，不支持脚本或命令";
    }
  }
  if (
    !paths.communication.client.serverHost.trim() ||
    paths.communication.client.serverHost !== paths.communication.client.serverHost.trim() ||
    /[\u0000-\u001f]/u.test(paths.communication.client.serverHost)
  ) {
    errors["communication.client.serverHost"] = "请输入有效的服务端地址";
  }
  const ports = [
    ["communication.client.serverPort", paths.communication.client.serverPort],
    ["communication.server.playerPort", paths.communication.server.playerPort],
    ["communication.server.peerPort", paths.communication.server.peerPort],
  ] as const;
  for (const [field, port] of ports) {
    if (!Number.isInteger(port) || port < 1025 || port > 65535) {
      errors[field] = "端口范围为 1025–65535";
    }
  }
  return errors;
}
