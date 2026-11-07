<template>
  <section class="flex min-h-0 flex-1 flex-col gap-3">
    <DialogRoot :open="Boolean(pendingRestore)" @update:open="setOpen">
      <DialogPortal>
        <DialogOverlay class="fixed inset-0 z-40 bg-slate-950/30" />
        <DialogContent
          class="workbench-dialog max-w-2xl"
          @escape-key-down="preventBusyClose"
          @interact-outside="preventBusyClose"
        >
          <template v-if="pendingRestore">
            <DialogTitle class="text-lg font-semibold">还原配置</DialogTitle>
            <DialogDescription class="mt-2 text-sm text-slate-600">
              将用「{{ pendingRestore.name }}」替换当前配置，当前数据会自动备份。
              <span v-if="hasUnsavedDrafts">未保存草稿将被丢弃。</span>
            </DialogDescription>
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
                <AppLoading v-if="busy" compact label="正在还原配置…" class="text-current" />
                确认还原
              </button>
            </div>
          </template>
        </DialogContent>
      </DialogPortal>
    </DialogRoot>
    <DialogRoot :open="creating" @update:open="setCreatingOpen">
      <DialogPortal>
        <DialogOverlay class="fixed inset-0 z-40 bg-slate-950/30" />
        <DialogContent
          class="workbench-dialog max-w-xl"
          @escape-key-down="preventBusyClose"
          @interact-outside="preventBusyClose"
        >
          <DialogTitle class="text-lg font-semibold">新建备份</DialogTitle>
          <DialogDescription class="mt-2 text-sm text-slate-500">
            备份名使用创建时间，仅备份已保存的配置。
          </DialogDescription>
          <form class="mt-5" @submit.prevent="create">
            <label for="backup-description" class="font-medium text-slate-700">
              说明 <span class="font-normal text-slate-400">可选</span>
            </label>
            <textarea
              id="backup-description"
              v-model="description"
              :disabled="busy"
              maxlength="1000"
              rows="3"
              class="mt-2 w-full resize-y rounded-md border border-slate-300 bg-white px-3 py-2"
              placeholder="记录本次备份内容"
            />
            <div class="mt-6 flex justify-end gap-2">
              <button
                type="button"
                class="secondary-button"
                :disabled="busy"
                @click="setCreatingOpen(false)"
              >
                取消
              </button>
              <button type="submit" class="primary-button" :disabled="busy">
                <AppLoading v-if="busy" compact label="正在创建备份…" class="text-current" />
                创建备份
              </button>
            </div>
          </form>
        </DialogContent>
      </DialogPortal>
    </DialogRoot>
    <WorkbenchTable
      v-model:search="search"
      label="配置备份"
      search-label="搜索备份"
      search-placeholder="备份名称、说明或来源目录"
      :columns="columns"
      :rows="rows"
      :total="backups.length"
      :row-key="(backup) => backup.backup_id"
      :loading="loading"
      :disabled="busy || disabled"
      :empty-text="backups.length ? '没有匹配的备份。' : '暂无备份'"
      table-class="min-w-[880px]"
    >
      <template #actions>
        <button
          type="button"
          class="secondary-button"
          :disabled="busy || loading || disabled"
          @click="refresh"
        >
          刷新列表
        </button>
        <button
          type="button"
          class="primary-button"
          :disabled="busy || disabled"
          @click="setCreatingOpen(true)"
        >
          新建备份
        </button>
      </template>
      <template #cell-name="{ row: backup }">
        <span class="font-medium tabular-nums">{{ backup.name }}</span>
      </template>
      <template #cell-description="{ row: backup }">
        <span class="whitespace-pre-wrap">{{ backup.description || "—" }}</span>
      </template>
      <template #cell-source="{ row: backup }">
        <span class="block max-w-72 truncate text-xs" :title="backup.source_server_root">
          {{ backup.source_server_root || "—" }}
        </span>
      </template>
      <template #cell-state="{ row: backup }">
        <span
          :class="[
            'table-status',
            !backup.available
              ? 'bg-red-50 text-red-700'
              : backup.source_matches
                ? 'bg-emerald-50 text-emerald-700'
                : 'bg-amber-50 text-amber-700',
          ]"
        >
          {{ !backup.available ? "信息损坏" : backup.source_matches ? "可还原" : "其他目录" }}
        </span>
      </template>
      <template #cell-actions="{ row: backup }">
        <button
          type="button"
          class="table-action"
          :disabled="busy || disabled || !backup.available || !backup.source_matches"
          @click="pendingRestore = backup"
        >
          还原
        </button>
      </template>
    </WorkbenchTable>
  </section>
</template>

<script setup lang="ts">
  import {
    DialogContent,
    DialogDescription,
    DialogOverlay,
    DialogPortal,
    DialogRoot,
    DialogTitle,
  } from "reka-ui";
  import { computed, onMounted, ref, watch } from "vue";

  import WorkbenchTable from "@/components/WorkbenchTable.vue";
  import AppLoading from "@/components/AppLoading.vue";
  import { useToastsStore } from "@/stores/toasts";
  import { matchesSearch, type TableColumn } from "@/components/workbenchTable";
  import { gameplayApi, type GameplayBackup } from "@/services/gameplayApi";

  const props = defineProps<{
    workspaceId: string | null;
    disabled: boolean;
    hasUnsavedDrafts: boolean;
  }>();
  const toasts = useToastsStore();
  const emit = defineEmits<{
    restored: [safetyBackupName: string];
    "busy-change": [busy: boolean];
  }>();

  const loading = ref(false);
  const busy = ref(false);
  const creating = ref(false);
  const description = ref("");
  const backups = ref<GameplayBackup[]>([]);
  const pendingRestore = ref<GameplayBackup | null>(null);
  const search = ref("");
  const columns: TableColumn[] = [
    { key: "name", label: "备份名称", class: "w-56" },
    { key: "description", label: "说明", class: "min-w-48 max-w-80 break-words" },
    { key: "source", label: "来源目录", class: "w-72 text-slate-500" },
    { key: "state", label: "状态", class: "w-28" },
    { key: "actions", label: "操作", class: "w-24" },
  ];
  const rows = computed(() =>
    backups.value.filter((backup) =>
      matchesSearch(search.value, backup.name, backup.description, backup.source_server_root),
    ),
  );

  function setOpen(next: boolean): void {
    if (busy.value) return;
    if (!next) {
      pendingRestore.value = null;
      creating.value = false;
    }
  }

  function preventBusyClose(event: Event): void {
    if (busy.value) event.preventDefault();
  }

  async function refresh(): Promise<void> {
    if (!props.workspaceId) return;
    loading.value = true;
    try {
      backups.value = await gameplayApi.backups(props.workspaceId);
    } catch (cause) {
      toasts.show(cause instanceof Error ? cause.message : "读取备份失败", "error");
    } finally {
      loading.value = false;
    }
  }

  function setCreatingOpen(open: boolean): void {
    if (busy.value) return;
    creating.value = open;
    if (!open) description.value = "";
  }

  function cancelRestore(): void {
    pendingRestore.value = null;
  }

  async function create(): Promise<void> {
    if (!props.workspaceId || busy.value || props.disabled) return;
    busy.value = true;
    try {
      const created = await gameplayApi.createBackup(props.workspaceId, description.value.trim());
      description.value = "";
      creating.value = false;
      await refresh();
      toasts.show(`已创建备份：${created.name}`, "success");
    } catch (cause) {
      toasts.show(cause instanceof Error ? cause.message : "创建备份失败", "error");
    } finally {
      busy.value = false;
    }
  }

  async function restore(): Promise<void> {
    if (!props.workspaceId || !pendingRestore.value || busy.value || props.disabled) return;
    busy.value = true;
    try {
      const result = await gameplayApi.restoreBackup(
        props.workspaceId,
        pendingRestore.value.backup_id,
      );
      const safetyBackupName = result.safety_backup.name;
      pendingRestore.value = null;
      creating.value = false;
      toasts.show(`已还原；当前配置已自动备份：${safetyBackupName}`, "success");
      emit("restored", safetyBackupName);
    } catch (cause) {
      toasts.show(cause instanceof Error ? cause.message : "还原失败", "error");
      pendingRestore.value = null;
    } finally {
      busy.value = false;
    }
  }
  watch(
    () => busy.value || loading.value,
    (value) => emit("busy-change", value),
    { flush: "sync" },
  );
  onMounted(refresh);
</script>
