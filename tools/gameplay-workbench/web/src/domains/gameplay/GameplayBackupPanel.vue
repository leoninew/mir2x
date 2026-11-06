<template>
  <div class="flex flex-wrap items-center gap-2">
    <DialogRoot :open="open" @update:open="setOpen">
      <DialogTrigger as-child>
        <button type="button" class="secondary-button" :disabled="disabled || !workspaceId">
          备份管理
        </button>
      </DialogTrigger>
      <DialogPortal>
        <DialogOverlay class="fixed inset-0 z-40 bg-slate-950/30" />
        <DialogContent
          class="fixed top-1/2 left-1/2 z-50 max-h-[85vh] w-11/12 max-w-2xl -translate-x-1/2 -translate-y-1/2 overflow-y-auto rounded-xl border border-slate-200 bg-white p-6 text-sm text-slate-900 shadow-xl"
          @escape-key-down="preventBusyClose"
          @interact-outside="preventBusyClose"
        >
          <template v-if="pendingRestore">
            <DialogTitle class="text-lg font-semibold">还原配置</DialogTitle>
            <DialogDescription class="mt-2 text-sm text-slate-600">
              将用「{{ pendingRestore.name }}」替换当前配置，当前数据会自动备份。
              <span v-if="hasUnsavedDrafts">未保存草稿将被丢弃。</span>
            </DialogDescription>
            <p v-if="error" role="alert" class="mt-4 text-sm text-red-700">{{ error }}</p>
            <div class="mt-6 flex justify-end gap-2">
              <button
                type="button"
                class="secondary-button"
                :disabled="busy"
                @click="cancelRestore"
              >
                取消
              </button>
              <button type="button" class="primary-button" :disabled="busy" @click="restore">
                {{ busy ? "还原中…" : "确认还原" }}
              </button>
            </div>
          </template>

          <template v-else>
            <DialogTitle class="text-lg font-semibold">玩法配置备份</DialogTitle>
            <DialogDescription class="mt-1 text-sm text-slate-500">
              备份名使用创建时间。
            </DialogDescription>

            <form
              v-if="creating"
              class="mt-5 rounded-lg border border-slate-200 bg-slate-50 p-4"
              @submit.prevent="create"
            >
              <label for="backup-description" class="font-medium text-slate-700">
                说明 <span class="font-normal text-slate-400">可选</span>
              </label>
              <textarea
                id="backup-description"
                v-model="description"
                maxlength="1000"
                rows="3"
                class="mt-2 w-full resize-y rounded-md border border-slate-300 bg-white px-3 py-2"
                placeholder="记录本次备份内容"
              />
              <p class="mt-1 text-xs text-slate-500">仅备份已保存的配置。</p>
              <div class="mt-3 flex justify-end gap-2">
                <button
                  type="button"
                  class="secondary-button"
                  :disabled="busy"
                  @click="cancelCreate"
                >
                  取消
                </button>
                <button type="submit" class="primary-button" :disabled="busy">
                  {{ busy ? "备份中…" : "创建备份" }}
                </button>
              </div>
            </form>

            <div class="mt-5 flex items-center justify-between gap-3">
              <h3 class="font-medium text-slate-800">已有备份</h3>
              <button
                type="button"
                class="text-button"
                :disabled="busy"
                @click="creating = !creating"
              >
                {{ creating ? "收起" : "新建备份" }}
              </button>
            </div>
            <p v-if="error" role="alert" class="mt-3 text-sm text-red-700">{{ error }}</p>
            <p v-else-if="loading" class="mt-3 text-sm text-slate-500">读取中…</p>
            <p v-else-if="!backups.length" class="mt-3 text-sm text-slate-500">暂无备份</p>
            <ul v-else class="mt-3 divide-y divide-slate-100 rounded-lg border border-slate-200">
              <li
                v-for="backup in backups"
                :key="backup.backup_id"
                class="flex items-center justify-between gap-4 px-4 py-3"
              >
                <div class="min-w-0">
                  <p class="truncate font-medium text-slate-800">{{ backup.name }}</p>
                  <p v-if="backup.description" class="mt-1 line-clamp-2 text-xs text-slate-500">
                    {{ backup.description }}
                  </p>
                  <p v-else-if="!backup.available" class="mt-1 text-xs text-red-700">
                    备份信息损坏
                  </p>
                  <p
                    v-if="backup.available && !backup.source_matches"
                    class="mt-1 truncate text-xs text-amber-700"
                  >
                    其他目录 · {{ backup.source_server_root }}
                  </p>
                </div>
                <button
                  type="button"
                  class="secondary-button shrink-0"
                  :disabled="busy || !backup.available || !backup.source_matches"
                  @click="pendingRestore = backup"
                >
                  还原
                </button>
              </li>
            </ul>
          </template>
        </DialogContent>
      </DialogPortal>
    </DialogRoot>
    <p v-if="status" role="status" class="text-sm text-emerald-700">{{ status }}</p>
  </div>
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
  import { ref } from "vue";

  import { gameplayApi, type GameplayBackup } from "@/services/gameplayApi";

  const props = defineProps<{
    workspaceId: string | null;
    disabled: boolean;
    hasUnsavedDrafts: boolean;
  }>();
  const emit = defineEmits<{
    restored: [safetyBackupName: string];
  }>();

  const open = ref(false);
  const loading = ref(false);
  const busy = ref(false);
  const creating = ref(false);
  const description = ref("");
  const backups = ref<GameplayBackup[]>([]);
  const pendingRestore = ref<GameplayBackup | null>(null);
  const error = ref("");
  const status = ref("");

  function setOpen(next: boolean): void {
    if (busy.value) return;
    open.value = next;
    if (next) void refresh();
    else {
      pendingRestore.value = null;
      creating.value = false;
      error.value = "";
    }
  }

  function preventBusyClose(event: Event): void {
    if (busy.value) event.preventDefault();
  }

  async function refresh(): Promise<void> {
    if (!props.workspaceId) return;
    loading.value = true;
    error.value = "";
    try {
      backups.value = await gameplayApi.backups(props.workspaceId);
    } catch (cause) {
      error.value = cause instanceof Error ? cause.message : "读取备份失败";
    } finally {
      loading.value = false;
    }
  }

  function cancelCreate(): void {
    creating.value = false;
    description.value = "";
  }

  function cancelRestore(): void {
    pendingRestore.value = null;
    error.value = "";
  }

  async function create(): Promise<void> {
    if (!props.workspaceId || busy.value) return;
    busy.value = true;
    error.value = "";
    status.value = "";
    try {
      const created = await gameplayApi.createBackup(props.workspaceId, description.value.trim());
      description.value = "";
      creating.value = false;
      await refresh();
      status.value = `已创建备份：${created.name}`;
    } catch (cause) {
      error.value = cause instanceof Error ? cause.message : "创建备份失败";
    } finally {
      busy.value = false;
    }
  }

  async function restore(): Promise<void> {
    if (!props.workspaceId || !pendingRestore.value || busy.value) return;
    busy.value = true;
    error.value = "";
    status.value = "";
    try {
      const result = await gameplayApi.restoreBackup(
        props.workspaceId,
        pendingRestore.value.backup_id,
      );
      const safetyBackupName = result.safety_backup.name;
      open.value = false;
      pendingRestore.value = null;
      creating.value = false;
      emit("restored", safetyBackupName);
    } catch (cause) {
      error.value = cause instanceof Error ? cause.message : "还原失败";
      pendingRestore.value = null;
    } finally {
      busy.value = false;
    }
  }
</script>

<style scoped>
  @reference "../../style.css";
  .primary-button {
    @apply rounded-md bg-slate-900 px-4 py-2 text-sm font-medium text-white hover:bg-slate-700 disabled:opacity-50;
  }
  .secondary-button {
    @apply rounded-md border border-slate-300 bg-white px-4 py-2 text-sm text-slate-700 hover:bg-slate-50 disabled:opacity-50;
  }
</style>
