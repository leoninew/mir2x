<template>
  <DialogRoot :open="settingsOpen" @update:open="setSettingsOpen">
    <section aria-label="游戏启动" class="relative w-full" :aria-busy="busy">
      <AppLoading
        v-if="busy && pending === 'refresh' && !settingsOpen"
        label="正在加载启动状态…"
        overlay
      />
      <div
        class="mx-auto w-full max-w-2xl overflow-hidden rounded-lg border border-slate-200 bg-white"
      >
        <article
          v-for="kind in kinds"
          :key="kind"
          class="grid gap-4 p-4 sm:grid-cols-[minmax(0,1fr)_auto] sm:items-center sm:gap-6 sm:px-5 sm:py-4"
          :class="kind === 'server' ? 'border-t border-slate-200' : ''"
        >
          <div class="min-w-0">
            <div class="flex flex-wrap items-center gap-x-3 gap-y-1">
              <h2 class="text-base font-semibold text-slate-900">{{ labels[kind] }}</h2>
              <p
                class="flex items-center gap-2 rounded-full px-2 py-1 text-xs font-medium"
                :class="
                  status?.targets[kind].ownership === 'unknown'
                    ? 'bg-amber-50 text-amber-700'
                    : status?.targets[kind].running
                      ? 'bg-emerald-50 text-emerald-700'
                      : 'bg-slate-100 text-slate-600'
                "
              >
                <span
                  :class="[
                    'size-1.5 rounded-full',
                    status?.targets[kind].ownership === 'unknown'
                      ? 'bg-amber-500'
                      : status?.targets[kind].running
                        ? 'bg-emerald-500'
                        : 'bg-slate-400',
                  ]"
                  aria-hidden="true"
                ></span>
                {{
                  status?.targets[kind].ownership === "unknown"
                    ? "状态未知"
                    : status?.targets[kind].running
                      ? "运行中"
                      : status?.targets[kind].exit_code != null
                        ? `已退出 · ${status.targets[kind].exit_code}`
                        : "未启动"
                }}
                <span v-if="status?.targets[kind].pid != null" class="font-normal opacity-70">
                  PID {{ status.targets[kind].pid }}
                </span>
              </p>
            </div>
            <p
              v-if="status?.targets[kind].ownership === 'unknown'"
              role="status"
              class="mt-2 text-sm text-amber-700"
            >
              {{ status.targets[kind].ownership_warning || "无法确认进程身份，已禁用控制" }}
            </p>
            <p v-if="kind === 'client' && status" class="mt-2 text-sm text-slate-500">
              连接目标 {{ status?.settings.communication.client.serverHost }}:{{
                status?.settings.communication.client.serverPort
              }}
            </p>
            <p
              v-if="kind === 'server' && status?.targets.server.running"
              class="mt-2 text-sm text-slate-500"
            >
              {{ serverListenLabel }}
            </p>
            <p v-if="status?.targets[kind].warning" class="mt-2 text-xs leading-5 text-amber-700">
              {{ status.targets[kind].warning }}
            </p>
          </div>
          <div class="flex flex-col gap-1 sm:flex-row sm:items-center sm:gap-3">
            <button
              type="button"
              class="primary-button w-full sm:min-w-36 sm:w-auto"
              :disabled="actionDisabled(kind)"
              @click="toggleGame(kind)"
            >
              <AppLoading
                v-if="pending === kind"
                compact
                :label="`正在${actionLabel(kind)}…`"
                class="text-current"
              />
              <component
                :is="status?.targets[kind].running ? Square : Play"
                v-else
                class="size-4 shrink-0"
                aria-hidden="true"
              />
              {{ actionLabel(kind) }}
            </button>
            <button
              v-if="status?.targets[kind].running || status?.targets[kind].exit_code != null"
              type="button"
              class="text-button justify-center"
              :disabled="!desktop"
              @click="showLog(kind)"
            >
              <component
                :is="logKind === kind ? RefreshCw : FileText"
                class="size-4 shrink-0"
                aria-hidden="true"
              />
              {{ logKind === kind ? "刷新日志" : "查看日志" }}
            </button>
          </div>
          <div
            v-if="logKind === kind"
            class="min-w-0 border-t border-slate-100 px-4 pb-4 pt-3 sm:col-span-2 sm:px-5"
          >
            <div
              class="max-h-56 overflow-auto whitespace-pre-wrap break-all rounded border border-slate-200 bg-slate-50 p-3 text-xs"
            >
              {{ logText || "暂无日志" }}
            </div>
            <button type="button" class="text-button mt-1" @click="logKind = null">关闭日志</button>
          </div>
        </article>
      </div>
      <p v-if="status?.process_state_error" role="alert" class="text-sm text-red-700">
        {{ status.process_state_error }}
      </p>
    </section>

    <DialogPortal>
      <DialogOverlay class="fixed inset-0 z-40 bg-slate-950/30" />
      <DialogContent
        class="workbench-dialog max-h-[calc(100dvh-2rem)] max-w-xl overflow-y-auto"
        @escape-key-down="preventBusyClose"
        @interact-outside="preventBusyClose"
      >
        <AppLoading v-if="busy && pending === 'refresh'" label="正在读取启动设置…" overlay />
        <DialogTitle class="text-lg font-semibold">启动设置</DialogTitle>
        <DialogDescription class="mt-1 text-sm text-slate-500">
          设置程序位置和游戏通信端口，通信配置在下次启动游戏时生效。
        </DialogDescription>

        <form v-if="status" class="mt-5" novalidate @submit.prevent="saveSettings">
          <fieldset :disabled="busy" class="grid min-w-0 gap-5">
            <div v-for="kind in kinds" :key="kind">
              <label :for="`launch-${kind}`" class="text-sm font-medium">
                {{ labels[kind] }}程序 <span class="text-red-600">*</span>
              </label>
              <input
                :id="`launch-${kind}`"
                v-model="draft[fields[kind]]"
                type="text"
                spellcheck="false"
                autocomplete="off"
                :aria-invalid="Boolean(errors[fields[kind]])"
                :aria-describedby="errors[fields[kind]] ? `launch-${kind}-error` : undefined"
                :class="[
                  'mt-2 w-full rounded-md border px-3 py-2 font-mono text-sm',
                  errors[fields[kind]] ? 'border-red-600' : 'border-slate-300',
                ]"
                @input="delete errors[fields[kind]]"
              />
              <p
                v-if="errors[fields[kind]]"
                :id="`launch-${kind}-error`"
                role="alert"
                class="mt-1 text-xs font-normal text-red-600"
              >
                {{ errors[fields[kind]] }}
              </p>
              <p v-else-if="missingSavedFile(kind)" class="mt-1 text-xs text-amber-800">
                未找到该程序
              </p>
            </div>
            <div>
              <label for="server-config-path" class="text-sm font-medium">
                服务端配置目录 <span class="text-red-600">*</span>
              </label>
              <input
                id="server-config-path"
                v-model="draft.serverConfigPath"
                type="text"
                spellcheck="false"
                autocomplete="off"
                :aria-invalid="Boolean(errors.serverConfigPath)"
                aria-describedby="server-config-path-help"
                :class="[
                  'mt-2 w-full rounded-md border px-3 py-2 font-mono text-sm',
                  errors.serverConfigPath ? 'border-red-600' : 'border-slate-300',
                ]"
                @input="delete errors.serverConfigPath"
              />
              <p
                id="server-config-path-help"
                :role="errors.serverConfigPath ? 'alert' : undefined"
                :class="[
                  'mt-1 text-xs',
                  errors.serverConfigPath ? 'text-red-600' : 'text-slate-500',
                ]"
              >
                {{ configPathHelp }}
              </p>
            </div>
            <section class="border-t border-slate-200 pt-4" aria-labelledby="communication-title">
              <h3 id="communication-title" class="text-sm font-semibold text-slate-900">
                游戏通信
              </h3>
              <div class="mt-4 grid gap-4 sm:grid-cols-2">
                <div>
                  <label for="client-server-host" class="text-sm font-medium">
                    客户端连接地址 <span class="text-red-600">*</span>
                  </label>
                  <input
                    id="client-server-host"
                    v-model="draft.communication.client.serverHost"
                    type="text"
                    autocomplete="off"
                    spellcheck="false"
                    :aria-invalid="Boolean(errors['communication.client.serverHost'])"
                    :aria-describedby="
                      errors['communication.client.serverHost']
                        ? 'client-server-host-help client-server-host-error'
                        : 'client-server-host-help'
                    "
                    :class="inputClass(errors['communication.client.serverHost'])"
                    @input="delete errors['communication.client.serverHost']"
                  />
                  <p id="client-server-host-help" class="mt-1 text-xs text-slate-500">
                    客户端连接服务端时使用，例如 localhost 或服务器 IP。
                  </p>
                  <p
                    v-if="errors['communication.client.serverHost']"
                    id="client-server-host-error"
                    role="alert"
                    class="mt-1 text-xs text-red-600"
                  >
                    {{ errors["communication.client.serverHost"] }}
                  </p>
                </div>
                <div>
                  <label for="client-server-port" class="text-sm font-medium">
                    客户端连接端口 <span class="text-red-600">*</span>
                  </label>
                  <input
                    id="client-server-port"
                    v-model.number="draft.communication.client.serverPort"
                    type="number"
                    min="1025"
                    max="65535"
                    step="1"
                    :aria-invalid="Boolean(errors['communication.client.serverPort'])"
                    :aria-describedby="
                      errors['communication.client.serverPort']
                        ? 'client-server-port-error'
                        : undefined
                    "
                    :class="inputClass(errors['communication.client.serverPort'])"
                    @input="delete errors['communication.client.serverPort']"
                  />
                  <p
                    v-if="errors['communication.client.serverPort']"
                    id="client-server-port-error"
                    role="alert"
                    class="mt-1 text-xs text-red-600"
                  >
                    {{ errors["communication.client.serverPort"] }}
                  </p>
                </div>
                <div>
                  <label for="server-client-port" class="text-sm font-medium">
                    服务端玩家端口 <span class="text-red-600">*</span>
                  </label>
                  <input
                    id="server-client-port"
                    v-model.number="draft.communication.server.playerPort"
                    type="number"
                    min="1025"
                    max="65535"
                    step="1"
                    :aria-invalid="Boolean(errors['communication.server.playerPort'])"
                    :aria-describedby="
                      errors['communication.server.playerPort']
                        ? 'server-client-port-error'
                        : undefined
                    "
                    :class="inputClass(errors['communication.server.playerPort'])"
                    @input="delete errors['communication.server.playerPort']"
                  />
                  <p class="mt-1 text-xs text-slate-500">应与客户端连接端口一致。</p>
                  <p
                    v-if="errors['communication.server.playerPort']"
                    id="server-client-port-error"
                    role="alert"
                    class="mt-1 text-xs text-red-600"
                  >
                    {{ errors["communication.server.playerPort"] }}
                  </p>
                </div>
                <div>
                  <label for="server-peer-port" class="text-sm font-medium">
                    服务端节点端口 <span class="text-red-600">*</span>
                  </label>
                  <input
                    id="server-peer-port"
                    v-model.number="draft.communication.server.peerPort"
                    type="number"
                    min="1025"
                    max="65535"
                    step="1"
                    :aria-invalid="Boolean(errors['communication.server.peerPort'])"
                    :aria-describedby="
                      errors['communication.server.peerPort'] ? 'server-peer-port-error' : undefined
                    "
                    :class="inputClass(errors['communication.server.peerPort'])"
                    @input="delete errors['communication.server.peerPort']"
                  />
                  <p
                    v-if="errors['communication.server.peerPort']"
                    id="server-peer-port-error"
                    role="alert"
                    class="mt-1 text-xs text-red-600"
                  >
                    {{ errors["communication.server.peerPort"] }}
                  </p>
                </div>
              </div>
            </section>
          </fieldset>

          <div class="mt-5 rounded-md bg-slate-50 px-3 py-2.5">
            <p class="text-xs text-slate-500">工作台目录</p>
            <p class="mt-1 break-all font-mono text-xs leading-5">{{ status.base_directory }}</p>
          </div>
          <details class="mt-3 text-xs text-slate-500">
            <summary>路径详情</summary>
            <dl class="mt-3 grid gap-3">
              <div v-for="kind in kinds" :key="kind">
                <dt class="font-medium text-slate-700">{{ labels[kind] }}</dt>
                <dd class="mt-1 break-all">程序：{{ status.targets[kind].executable }}</dd>
                <dd class="mt-1 break-all">目录：{{ status.targets[kind].working_directory }}</dd>
                <dd v-if="status.targets[kind].running" class="mt-1">
                  PID {{ status.targets[kind].pid }}
                </dd>
                <dd v-else-if="status.targets[kind].exit_code !== null" class="mt-1">
                  已退出 · 退出码 {{ status.targets[kind].exit_code }}
                </dd>
                <dd
                  v-if="status.targets[kind].exists && status.targets[kind].warning"
                  class="mt-1 text-amber-800"
                >
                  {{ status.targets[kind].warning }}
                </dd>
              </div>
            </dl>
          </details>
          <p v-if="status.settings_error" role="alert" class="mt-4 text-sm text-red-700">
            {{ status.settings_error }}
          </p>
          <div class="mt-6 flex items-center justify-between gap-3 border-t border-slate-100 pt-4">
            <button type="button" class="text-button" :disabled="busy" @click="refresh">
              <RefreshCw class="size-4 shrink-0" aria-hidden="true" />
              重新读取
            </button>
            <div class="flex gap-2">
              <button
                type="button"
                class="secondary-button"
                :disabled="busy"
                @click="setSettingsOpen(false)"
              >
                取消
              </button>
              <button type="submit" class="primary-button" :disabled="busy">
                <AppLoading
                  v-if="pending === 'paths'"
                  compact
                  label="正在保存设置…"
                  class="text-current"
                />
                <Save v-else class="size-4 shrink-0" aria-hidden="true" />
                保存
              </button>
            </div>
          </div>
        </form>
      </DialogContent>
    </DialogPortal>
  </DialogRoot>
</template>

<script setup lang="ts">
  import { FileText, Play, RefreshCw, Save, Square } from "@lucide/vue";
  import {
    DialogContent,
    DialogDescription,
    DialogOverlay,
    DialogPortal,
    DialogRoot,
    DialogTitle,
  } from "reka-ui";
  import { computed, onBeforeUnmount, onMounted, reactive, ref } from "vue";
  import AppLoading from "@/components/AppLoading.vue";
  import { useToastsStore } from "@/stores/toasts";

  import { getDesktopBridge, isDesktopApp } from "@/services/desktopBridge";
  import {
    validateLaunchPaths,
    type GameKind,
    type LaunchField,
    type LaunchPaths,
    type LaunchSettingsResult,
    type LaunchStatus,
  } from "@/services/gameLauncher";

  const props = defineProps<{
    gameplayDirty: boolean;
    gameplayBusy: boolean;
    workspaceRoot: string | null;
  }>();
  const toasts = useToastsStore();
  const kinds = ["client", "server"] as const;
  const labels = { client: "客户端", server: "服务端" };
  const fields = { client: "clientPath", server: "serverPath" } as const;
  const status = ref<LaunchStatus | null>(null);
  const desktop = ref(false);
  const busy = ref(false);
  const settingsOpen = ref(false);
  const logKind = ref<GameKind | null>(null);
  const logText = ref("");
  const pending = ref<GameKind | "refresh" | "paths" | null>(null);
  const draft = reactive<LaunchPaths>({
    clientPath: "",
    serverPath: "",
    serverConfigPath: "",
    communication: {
      client: { serverHost: "localhost", serverPort: 7000 },
      server: { playerPort: 7000, peerPort: 8000 },
    },
  });
  const errors = ref<Partial<Record<LaunchField, string>>>({});
  const configPathHelp = computed(
    () =>
      errors.value.serverConfigPath ||
      "填写包含 config/gameplay 的服务端目录，下次打开工作台生效。",
  );
  let generation = 0;
  let pollTimer: number | undefined;
  let polling = false;
  let pollFailed = false;
  const dirty = computed(() => {
    const saved = status.value?.settings;
    return (
      saved !== undefined &&
      (draft.clientPath !== saved.clientPath ||
        draft.serverPath !== saved.serverPath ||
        draft.serverConfigPath !== saved.serverConfigPath ||
        draft.communication.client.serverHost !== saved.communication.client.serverHost ||
        draft.communication.client.serverPort !== saved.communication.client.serverPort ||
        draft.communication.server.playerPort !== saved.communication.server.playerPort ||
        draft.communication.server.peerPort !== saved.communication.server.peerPort)
    );
  });
  const normalized = (path: string) =>
    path.replaceAll("\\", "/").replace(/\/+$/u, "").toLowerCase();
  const serverMismatch = computed(
    () =>
      status.value !== null &&
      props.workspaceRoot !== null &&
      normalized(props.workspaceRoot) !== normalized(status.value.targets.server.working_directory),
  );
  const serverListenLabel = computed(() => {
    const listening = status.value?.targets.server.listening;
    const port = status.value?.targets.server.player_port;
    if (listening === true) return `玩家端口 ${port} 已监听`;
    if (listening === false) return `玩家端口 ${port} 尚未监听`;
    return "玩家端口监听状态未知";
  });
  const inputClass = (error?: string) =>
    `mt-2 w-full rounded-md border px-3 py-2 text-sm ${error ? "border-red-600" : "border-slate-300"}`;

  async function showLog(kind: GameKind): Promise<void> {
    try {
      const result = await getDesktopBridge().get_game_log(kind);
      if (!result.success) throw new Error(result.message ?? "读取日志失败");
      logKind.value = kind;
      logText.value = result.data ?? "";
    } catch (error) {
      report(error);
    }
  }

  function accept(result: LaunchSettingsResult, reset: boolean): void {
    if (!result.success || !result.data) throw new Error(result.message ?? "读取启动设置失败");
    status.value = result.data;
    if (reset) resetDraft();
  }
  function resetDraft(): void {
    if (!status.value) return;
    draft.clientPath = status.value.settings.clientPath;
    draft.serverPath = status.value.settings.serverPath;
    draft.serverConfigPath = status.value.settings.serverConfigPath;
    draft.communication.client.serverHost = status.value.settings.communication.client.serverHost;
    draft.communication.client.serverPort = status.value.settings.communication.client.serverPort;
    draft.communication.server.playerPort = status.value.settings.communication.server.playerPort;
    draft.communication.server.peerPort = status.value.settings.communication.server.peerPort;
    errors.value = {};
  }
  function setSettingsOpen(open: boolean): void {
    if (busy.value) return;
    if (open) resetDraft();
    settingsOpen.value = open;
  }
  const canConfigure = computed(() => !busy.value && status.value !== null);
  defineExpose({
    canConfigure,
    openSettings: () => {
      if (canConfigure.value) setSettingsOpen(true);
    },
  });
  function preventBusyClose(event: Event): void {
    if (busy.value) event.preventDefault();
  }

  async function refresh(): Promise<void> {
    if (busy.value) return;
    desktop.value = isDesktopApp();
    if (!desktop.value) return;
    if (settingsOpen.value && dirty.value && !window.confirm("放弃启动设置修改并重新读取？"))
      return;
    const current = ++generation;
    busy.value = true;
    pending.value = "refresh";
    try {
      const result = await getDesktopBridge().get_game_launch_status();
      if (current === generation) accept(result, true);
    } catch (error) {
      if (current === generation) report(error);
    } finally {
      if (current === generation) {
        busy.value = false;
        pending.value = null;
      }
    }
  }

  async function saveSettings(): Promise<void> {
    if (busy.value || !status.value) return;
    errors.value = validateLaunchPaths(draft);
    if (Object.keys(errors.value).length) return;
    const current = ++generation;
    busy.value = true;
    pending.value = "paths";
    try {
      const result = await getDesktopBridge().save_game_launch_paths(
        draft.clientPath,
        draft.serverPath,
        status.value.revision,
        draft.serverConfigPath,
        draft.communication,
      );
      if (current !== generation) return;
      for (const issue of result.issues ?? []) {
        if (
          issue.path === "clientPath" ||
          issue.path === "serverPath" ||
          issue.path === "serverConfigPath" ||
          issue.path === "communication.client.serverHost" ||
          issue.path === "communication.client.serverPort" ||
          issue.path === "communication.server.playerPort" ||
          issue.path === "communication.server.peerPort"
        )
          errors.value[issue.path as LaunchField] = issue.message;
      }
      accept(result, true);
      settingsOpen.value = false;
      toasts.show("启动设置已保存", "success");
    } catch (error) {
      if (current === generation && !Object.keys(errors.value).length) report(error);
    } finally {
      if (current === generation) {
        busy.value = false;
        pending.value = null;
      }
    }
  }

  async function toggleGame(kind: GameKind): Promise<void> {
    if (actionDisabled(kind) || !status.value) return;
    const target = status.value.targets[kind];
    const stopping = target.running;
    if (!stopping) {
      if (status.value.settings_error || !target.exists) {
        setSettingsOpen(true);
        return;
      }
      if (props.gameplayDirty && !window.confirm("有未保存的玩法修改，仍要启动？")) return;
      if (
        kind === "server" &&
        serverMismatch.value &&
        !window.confirm("启动目录与当前配置目录不同，仍要启动？")
      )
        return;
    }
    const current = ++generation;
    busy.value = true;
    pending.value = kind;
    try {
      if (stopping && target.pid === null) throw new Error("进程状态无效，请重新读取状态");
      const result = stopping
        ? await getDesktopBridge().stop_game(kind, target.pid!)
        : await getDesktopBridge().launch_game(kind, status.value.revision);
      if (current !== generation) return;
      toasts.show(
        result.success
          ? `${labels[kind]}进程已${stopping ? "停止" : "启动"}${result.pid ? ` · PID ${result.pid}` : ""}`
          : (result.message ?? `${stopping ? "停止" : "启动"}失败`),
        result.success ? "success" : "error",
      );
      const updated = await getDesktopBridge().get_game_launch_status();
      if (current === generation) accept(updated, false);
    } catch (error) {
      if (current === generation) report(error);
    } finally {
      if (current === generation) {
        busy.value = false;
        pending.value = null;
      }
    }
  }
  function actionLabel(kind: GameKind): string {
    if (status.value?.targets[kind].ownership === "unknown") return `${labels[kind]}状态未知`;
    return `${status.value?.targets[kind].running ? "停止" : "启动"}${labels[kind]}`;
  }
  function actionDisabled(kind: GameKind): boolean {
    const target = status.value?.targets[kind];
    return (
      !desktop.value ||
      !status.value ||
      busy.value ||
      target?.ownership === "unknown" ||
      (!target?.running && (props.gameplayBusy || Boolean(status.value.process_state_error)))
    );
  }
  async function pollStatus(): Promise<void> {
    if (
      busy.value ||
      polling ||
      !status.value ||
      !Object.values(status.value.targets).some(
        (target) => target.running || target.ownership === "unknown",
      )
    )
      return;
    const current = generation;
    polling = true;
    try {
      const result = await getDesktopBridge().get_game_launch_status();
      if (current === generation) {
        accept(result, false);
        pollFailed = false;
      }
    } catch (error) {
      if (current === generation && !pollFailed) {
        report(error);
        pollFailed = true;
      }
    } finally {
      polling = false;
    }
  }
  function missingSavedFile(kind: GameKind): boolean {
    return Boolean(
      status.value &&
      !status.value.targets[kind].exists &&
      draft[fields[kind]] === status.value.settings[fields[kind]],
    );
  }
  function report(error: unknown): void {
    const text = error instanceof Error ? error.message : "操作失败";
    toasts.show(text, "error");
  }
  function bridgeReady(): void {
    if (!status.value) void refresh();
  }
  onMounted(() => {
    window.addEventListener("pywebviewready", bridgeReady);
    void refresh();
    pollTimer = window.setInterval(() => void pollStatus(), 1000);
  });
  onBeforeUnmount(() => {
    generation++;
    window.clearInterval(pollTimer);
    window.removeEventListener("pywebviewready", bridgeReady);
  });
</script>
