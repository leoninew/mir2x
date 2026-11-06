<template>
  <DialogRoot :open="settingsOpen" @update:open="setSettingsOpen">
    <section aria-label="游戏启动" class="rounded-lg border border-slate-200 bg-white px-4 py-3">
      <div class="flex flex-wrap items-center gap-3">
        <span class="mr-1 text-sm text-slate-500">游戏</span>
        <button
          v-for="kind in kinds"
          :key="kind"
          type="button"
          class="secondary-button inline-flex items-center gap-2"
          :disabled="launchDisabled(kind)"
          @click="launch(kind)"
        >
          <span
            v-if="status?.targets[kind].running"
            aria-hidden="true"
            class="size-2 rounded-full bg-emerald-500"
          ></span>
          <svg v-else aria-hidden="true" viewBox="0 0 16 16" class="size-3.5 fill-current">
            <path d="M4 2.5v11L13 8z" />
          </svg>
          {{ status?.targets[kind].running ? `${labels[kind]}已启动` : `启动${labels[kind]}` }}
        </button>
        <div class="ml-auto flex items-center gap-1">
          <button
            type="button"
            aria-label="刷新启动状态"
            title="刷新状态"
            class="rounded-md p-2 text-slate-500 hover:bg-slate-100 disabled:opacity-50"
            :disabled="busy || !desktop"
            @click="refresh"
          >
            <svg
              aria-hidden="true"
              viewBox="0 0 20 20"
              class="size-4 fill-none stroke-current"
              stroke-width="1.5"
            >
              <path d="M16 7a6.5 6.5 0 1 0 .4 5M16 2.5V7h-4.5" />
            </svg>
          </button>
          <DialogTrigger as-child>
            <button type="button" class="text-button" :disabled="busy || !status">路径设置</button>
          </DialogTrigger>
        </div>
      </div>
      <p v-if="!desktop" class="mt-2 text-sm text-slate-500">启动功能仅在桌面版可用</p>
      <p
        v-if="message"
        role="status"
        :class="['mt-2 text-sm', failed ? 'text-red-700' : 'text-emerald-700']"
      >
        {{ message }}
      </p>
    </section>

    <DialogPortal>
      <DialogOverlay class="fixed inset-0 z-40 bg-slate-950/30" />
      <DialogContent
        class="fixed top-1/2 left-1/2 z-50 max-h-[85vh] w-11/12 max-w-xl -translate-x-1/2 -translate-y-1/2 overflow-y-auto rounded-xl border border-slate-200 bg-white p-6 text-sm text-slate-900 shadow-xl"
        @escape-key-down="preventBusyClose"
        @interact-outside="preventBusyClose"
      >
        <DialogTitle class="text-lg font-semibold">路径设置</DialogTitle>
        <DialogDescription class="mt-1 text-sm text-slate-500">
          路径相对于工作台所在目录。
        </DialogDescription>

        <form v-if="status" class="mt-5" @submit.prevent="savePaths">
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
                v-model="draft.server_config_path"
                type="text"
                spellcheck="false"
                autocomplete="off"
                :aria-invalid="Boolean(errors.server_config_path)"
                aria-describedby="server-config-path-help"
                :class="[
                  'mt-2 w-full rounded-md border px-3 py-2 font-mono text-sm',
                  errors.server_config_path ? 'border-red-600' : 'border-slate-300',
                ]"
                @input="delete errors.server_config_path"
              />
              <p
                id="server-config-path-help"
                :role="errors.server_config_path ? 'alert' : undefined"
                :class="[
                  'mt-1 text-xs',
                  errors.server_config_path ? 'text-red-600' : 'text-slate-500',
                ]"
              >
                {{ configPathHelp }}
              </p>
            </div>
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
          <p v-if="formMessage" role="alert" class="mt-4 text-sm text-red-700">{{ formMessage }}</p>
          <div class="mt-6 flex items-center justify-between gap-3 border-t border-slate-100 pt-4">
            <button type="button" class="text-button" :disabled="busy" @click="refresh">
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
                {{ busy ? "保存中…" : "保存" }}
              </button>
            </div>
          </div>
        </form>
      </DialogContent>
    </DialogPortal>
  </DialogRoot>
</template>

<script setup lang="ts">
  import {
    DialogContent,
    DialogDescription,
    DialogOverlay,
    DialogPortal,
    DialogRoot,
    DialogTitle,
    DialogTrigger,
  } from "reka-ui";
  import { computed, onBeforeUnmount, onMounted, reactive, ref } from "vue";

  import { getDesktopBridge, isDesktopApp } from "@/services/desktopBridge";
  import {
    validateLaunchPaths,
    type GameKind,
    type LaunchPaths,
    type LaunchSettingsResult,
    type LaunchStatus,
  } from "@/services/gameLauncher";

  const props = defineProps<{
    gameplayDirty: boolean;
    gameplayBusy: boolean;
    workspaceRoot: string | null;
  }>();
  const kinds = ["client", "server"] as const;
  const labels = { client: "客户端", server: "服务端" };
  const fields = { client: "client_path", server: "server_path" } as const;
  const status = ref<LaunchStatus | null>(null);
  const desktop = ref(false);
  const busy = ref(false);
  const settingsOpen = ref(false);
  const message = ref("");
  const failed = ref(false);
  const formMessage = ref("");
  const draft = reactive<LaunchPaths>({ client_path: "", server_path: "", server_config_path: "" });
  const errors = ref<Partial<Record<keyof LaunchPaths, string>>>({});
  const configPathHelp = computed(
    () =>
      errors.value.server_config_path ||
      "填写包含 config/gameplay 的服务端目录，下次打开工作台生效。",
  );
  let generation = 0;
  const dirty = computed(
    () =>
      status.value !== null &&
      (Object.keys(draft) as (keyof LaunchPaths)[]).some(
        (field) => draft[field] !== status.value!.settings[field],
      ),
  );
  const normalized = (path: string) =>
    path.replaceAll("\\", "/").replace(/\/+$/u, "").toLowerCase();
  const serverMismatch = computed(
    () =>
      status.value !== null &&
      props.workspaceRoot !== null &&
      normalized(props.workspaceRoot) !== normalized(status.value.targets.server.working_directory),
  );

  function accept(result: LaunchSettingsResult, reset: boolean): void {
    if (!result.success || !result.data) throw new Error(result.message ?? "读取启动设置失败");
    status.value = result.data;
    if (reset) resetDraft();
  }
  function resetDraft(): void {
    if (!status.value) return;
    draft.client_path = status.value.settings.client_path;
    draft.server_path = status.value.settings.server_path;
    draft.server_config_path = status.value.settings.server_config_path;
    errors.value = {};
    formMessage.value = "";
  }
  function setSettingsOpen(open: boolean): void {
    if (busy.value) return;
    if (open) resetDraft();
    settingsOpen.value = open;
  }
  function preventBusyClose(event: Event): void {
    if (busy.value) event.preventDefault();
  }

  async function refresh(): Promise<void> {
    if (busy.value) return;
    desktop.value = isDesktopApp();
    if (!desktop.value) return;
    if (settingsOpen.value && dirty.value && !window.confirm("放弃路径修改并重新读取？")) return;
    const current = ++generation;
    busy.value = true;
    message.value = "";
    formMessage.value = "";
    try {
      const result = await getDesktopBridge().get_game_launch_status();
      if (current === generation) accept(result, true);
    } catch (error) {
      if (current === generation) report(error);
    } finally {
      if (current === generation) busy.value = false;
    }
  }

  async function savePaths(): Promise<void> {
    if (busy.value || !status.value) return;
    errors.value = validateLaunchPaths(draft);
    if (Object.keys(errors.value).length) return;
    const current = ++generation;
    busy.value = true;
    formMessage.value = "";
    try {
      const result = await getDesktopBridge().save_game_launch_paths(
        draft.client_path,
        draft.server_path,
        status.value.revision,
        draft.server_config_path,
      );
      if (current !== generation) return;
      for (const issue of result.issues ?? []) {
        if (
          issue.path === "client_path" ||
          issue.path === "server_path" ||
          issue.path === "server_config_path"
        )
          errors.value[issue.path] = issue.message;
      }
      accept(result, true);
      settingsOpen.value = false;
      failed.value = false;
      message.value = "路径已保存";
    } catch (error) {
      if (current === generation && !Object.keys(errors.value).length) report(error);
    } finally {
      if (current === generation) busy.value = false;
    }
  }

  async function launch(kind: GameKind): Promise<void> {
    if (launchDisabled(kind)) return;
    if (!status.value || status.value.settings_error || !status.value.targets[kind].exists) {
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
    const current = ++generation;
    busy.value = true;
    message.value = "";
    try {
      const result = await getDesktopBridge().launch_game(kind, status.value.revision);
      if (current !== generation) return;
      failed.value = !result.success;
      message.value = result.success
        ? `${labels[kind]}已启动${result.pid ? ` · PID ${result.pid}` : ""}`
        : (result.message ?? "启动失败");
      const updated = await getDesktopBridge().get_game_launch_status();
      if (current === generation) accept(updated, true);
    } catch (error) {
      if (current === generation) report(error);
    } finally {
      if (current === generation) busy.value = false;
    }
  }
  function launchDisabled(kind: GameKind): boolean {
    return (
      !desktop.value ||
      !status.value ||
      busy.value ||
      props.gameplayBusy ||
      Boolean(status.value.targets[kind].running)
    );
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
    if (settingsOpen.value) formMessage.value = text;
    else {
      failed.value = true;
      message.value = text;
    }
  }
  function bridgeReady(): void {
    if (!status.value) void refresh();
  }
  onMounted(() => {
    window.addEventListener("pywebviewready", bridgeReady);
    void refresh();
  });
  onBeforeUnmount(() => {
    generation++;
    window.removeEventListener("pywebviewready", bridgeReady);
  });
</script>

<style scoped>
  @reference "../../style.css";
  .primary-button {
    @apply rounded-md bg-slate-900 px-4 py-2 text-sm font-medium text-white hover:bg-slate-700 disabled:opacity-50;
  }
  .secondary-button {
    @apply rounded-md border border-slate-300 bg-white px-3 py-2 text-sm text-slate-700 hover:bg-slate-50 disabled:opacity-50;
  }
  .text-button {
    @apply rounded-md px-3 py-2 text-sm text-slate-600 hover:bg-slate-100 disabled:opacity-50;
  }
</style>
