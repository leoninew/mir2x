<template>
  <div class="workbench-layout">
    <WorkbenchSidebar
      :page="page"
      :server-root="workspace?.server_root ?? null"
      :busy="busy || managementBusy"
      @navigate="navigate"
      @choose="chooseWorkspace"
    />
    <main class="flex min-h-0 min-w-0 flex-col gap-5 p-4 sm:p-6 lg:p-8">
      <header class="shrink-0">
        <div class="flex flex-wrap items-center gap-3">
          <h1 class="text-2xl font-semibold tracking-tight text-slate-950">{{ pageInfo.label }}</h1>
          <span
            v-if="isConfigPage(page)"
            class="rounded bg-indigo-50 px-2 py-1 text-xs text-indigo-600"
          >
            重启生效
          </span>
        </div>
        <p class="mt-2 text-sm leading-6 text-slate-500">{{ pageInfo.description }}</p>
      </header>
      <p
        v-if="message"
        role="status"
        :class="[
          'shrink-0 rounded-lg border px-4 py-3 text-sm',
          conflict
            ? 'border-amber-300 bg-amber-50 text-amber-950'
            : 'border-slate-200 bg-white text-slate-700',
        ]"
      >
        {{ message }}
      </p>
      <div v-show="page === 'launcher'" class="min-h-0 overflow-auto">
        <GameLaunchPanel
          :gameplay-dirty="hasUnsavedDrafts"
          :gameplay-busy="busy"
          :workspace-root="workspace?.server_root ?? null"
        />
      </div>
      <div
        v-if="!workspace?.bound && page !== 'launcher'"
        class="grid min-h-0 flex-1 place-items-center rounded-lg border border-dashed border-slate-300 bg-white/60 p-6 text-center"
      >
        <div>
          <h2 class="text-base font-medium text-slate-800">打开服务端配置</h2>
          <p class="mt-2 text-sm text-slate-500">选择 server 目录，编辑玩法配置并管理备份。</p>
          <button
            type="button"
            class="primary-button mt-5"
            :disabled="busy"
            @click="chooseWorkspace"
          >
            选择服务端目录
          </button>
        </div>
      </div>
      <template v-if="workspace?.bound && catalog && isConfigPage(page)">
        <section
          class="flex min-h-0 flex-1 flex-col overflow-hidden rounded-lg border border-slate-200 bg-white"
          aria-label="配置编辑区"
        >
          <div
            v-if="domain !== 'multipliers'"
            class="flex shrink-0 flex-wrap items-end gap-4 border-b border-slate-200 p-4"
          >
            <label class="grid gap-1.5 text-sm text-slate-700">
              搜索{{ domain === "spawns" ? "地图" : "怪物" }}
              <input
                v-model="search"
                class="rounded-md border border-slate-300 bg-white px-3 py-2"
                placeholder="名称或 ID"
              />
            </label>
            <label
              v-if="domain === 'drops' || domain === 'spawns'"
              class="grid min-w-56 gap-1.5 text-sm text-slate-700"
            >
              {{ domain === "spawns" ? "地图" : "怪物" }} <span class="sr-only">必填</span>
              <select
                :value="objectId ?? ''"
                :disabled="busy"
                class="rounded-md border border-slate-300 bg-white px-3 py-2"
                @change="selectObject"
              >
                <option v-if="!options.length" value="" disabled>无匹配配置</option>
                <option v-for="option in options" :key="option.id" :value="option.id">
                  {{ option.name }} · {{ option.id }}
                </option>
              </select>
            </label>
            <span
              v-if="domain === 'monsters' && draft && 'monsters' in draft"
              class="pb-2 text-xs text-slate-500"
            >
              {{ monsterCount }} / {{ draft.monsters.length }} 个怪物
            </span>
          </div>
          <div class="min-h-0 flex-1 overflow-auto" aria-label="配置列表" tabindex="0">
            <fieldset v-if="draft" :disabled="busy" class="min-w-0">
              <MonsterExperienceEditor
                v-if="'monsters' in draft"
                v-model="draft"
                :search="search"
                :errors="errors"
                @edit="clearError"
              />
              <MonsterDropEditor
                v-else-if="'items' in draft"
                v-model="draft"
                :errors="errors"
                :fields="catalog.fields"
                @edit="clearError"
              />
              <GlobalMultipliersEditor
                v-else-if="'experience' in draft"
                v-model="draft"
                :errors="errors"
                @edit="clearError"
              />
              <MapSpawnEditor v-else v-model="draft" :errors="errors" @edit="clearError" />
            </fieldset>
          </div>
          <div
            v-if="base"
            class="shrink-0 border-t border-slate-100 px-4 py-2 text-xs text-slate-400"
          >
            文件版本 {{ base.revision.slice(0, 12) }} ·
            {{ domain === "multipliers" ? "独立倍率文件" : "按 ID 区分同名配置" }}
          </div>
        </section>
        <details v-if="dirty" class="shrink-0 rounded-lg border border-slate-200 bg-white p-4">
          <summary class="text-sm font-medium text-slate-700">
            变更预览 · {{ changes.length }} 处
          </summary>
          <div class="mt-3 max-h-40 overflow-auto">
            <table class="config-table">
              <thead>
                <tr>
                  <th>字段路径</th>
                  <th>读取值</th>
                  <th>草稿值</th>
                </tr>
              </thead>
              <tbody>
                <tr v-for="change in changes.slice(0, 100)" :key="change.path">
                  <td>{{ change.path }}</td>
                  <td>{{ change.before }}</td>
                  <td>{{ change.after }}</td>
                </tr>
              </tbody>
            </table>
          </div>
          <p v-if="changes.length > 100" class="mt-2 text-xs text-slate-500">
            显示前 100 处；保存作用于当前完整文件。
          </p>
        </details>
        <GameplaySaveBar
          :busy="busy"
          :ready="Boolean(draft && base)"
          :dirty="dirty"
          :has-unsaved-drafts="hasUnsavedDrafts"
          :change-count="changes.length"
          @download="downloadDraft"
          @reload="reload"
          @save="save"
        />
      </template>
      <div v-if="workspace?.bound && page === 'backups'" class="min-h-0 flex-1 overflow-auto">
        <GameplayBackupPanel
          :workspace-id="workspace.workspace_id"
          :disabled="busy"
          :has-unsaved-drafts="hasUnsavedDrafts"
          @restored="onBackupRestored"
          @busy-change="managementBusy = $event"
        />
      </div>
      <section
        v-if="workspace?.bound && catalog && page === 'migration'"
        class="min-h-0 flex-1 overflow-auto rounded-lg border border-slate-200 bg-white p-5"
      >
        <h2 class="text-sm font-medium">
          {{ catalog.migration_notes.records.length }} 项未生效数据
        </h2>
        <p class="mt-2 text-sm text-slate-500">这些是旧代码已过滤的原始记录，只读保留。</p>
        <details
          v-for="(record, index) in catalog.migration_notes.records"
          :key="index"
          class="mt-4 border-t border-slate-100 pt-4 text-sm"
        >
          <summary>{{ record.source_path }} : {{ record.source_position }}</summary>
          <p class="my-2 text-slate-600">{{ record.reason }}</p>
          <pre class="overflow-auto rounded bg-slate-50 p-3 text-xs">{{
            JSON.stringify(record.raw, null, 2)
          }}</pre>
        </details>
      </section>
    </main>
  </div>
</template>
<script setup lang="ts">
  import { computed, onMounted, ref } from "vue";
  import { useGameplay } from "@/composables/useGameplay";
  import GameLaunchPanel from "./GameLaunchPanel.vue";
  import GameplayBackupPanel from "./GameplayBackupPanel.vue";
  import GameplaySaveBar from "./GameplaySaveBar.vue";
  import GlobalMultipliersEditor from "./GlobalMultipliersEditor.vue";
  import MapSpawnEditor from "./MapSpawnEditor.vue";
  import MonsterDropEditor from "./MonsterDropEditor.vue";
  import MonsterExperienceEditor from "./MonsterExperienceEditor.vue";
  import WorkbenchSidebar from "./WorkbenchSidebar.vue";
  import { isConfigPage, navigationGroups, type WorkbenchPage } from "./gameplayNavigation";
  import { getDesktopBridge, isDesktopApp } from "@/services/desktopBridge";
  import { gameplayApi } from "@/services/gameplayApi";

  const {
    workspace,
    catalog,
    base,
    draft,
    domain,
    objectId,
    busy,
    message,
    conflict,
    errors,
    changes,
    dirty,
    hasUnsavedDrafts,
    load,
    refreshWorkspace,
    save,
    clearError,
  } = useGameplay();
  const page = ref<WorkbenchPage>("monsters");
  const search = ref("");
  const managementBusy = ref(false);
  const pageInfo = computed(() =>
    navigationGroups.flatMap((group) => group.items).find((item) => item.page === page.value)!,
  );
  const matches = (name: string, id: number) => `${name} ${id}`.includes(search.value.trim());
  const options = computed(() => {
    if (!catalog.value) return [];
    const identities =
      domain.value === "spawns" ? catalog.value.catalog.maps : catalog.value.catalog.monsters;
    const ids =
      domain.value === "spawns"
        ? catalog.value.manifest.spawn_maps
        : catalog.value.manifest.drop_monsters;
    return identities.filter((row) => ids.includes(row.id) && matches(row.name, row.id));
  });
  const monsterCount = computed(() =>
    draft.value && "monsters" in draft.value
      ? draft.value.monsters.filter((row) => matches(row.monster_name, row.monster_id)).length
      : 0,
  );
  async function navigate(next: WorkbenchPage): Promise<void> {
    if (busy.value || managementBusy.value) return;
    if (!isConfigPage(next)) {
      page.value = next;
      return;
    }
    if (!workspace.value?.bound) {
      page.value = next;
      return;
    }
    search.value = "";
    const id =
      next === "monsters" || next === "multipliers"
        ? null
        : next === "drops"
          ? catalog.value?.manifest.drop_monsters[0]
          : catalog.value?.manifest.spawn_maps[0];
    if (id === undefined) return;
    await load(next, id);
    page.value = domain.value;
  }
  async function selectObject(event: Event): Promise<void> {
    await load(domain.value, Number((event.target as HTMLSelectElement).value));
  }
  async function chooseWorkspace(): Promise<void> {
    if (!isDesktopApp()) {
      message.value = "请在桌面版中选择目录";
      return;
    }
    try {
      const result = await getDesktopBridge().select_gameplay_workspace();
      if (result.success) {
        if (await refreshWorkspace()) page.value = "monsters";
      } else if (result.message && result.message !== "未选择目录") message.value = result.message;
    } catch (error) {
      message.value = error instanceof Error ? error.message : "选择目录失败";
    }
  }
  async function onBackupRestored(safetyBackupName: string): Promise<void> {
    if (await refreshWorkspace(true)) {
      page.value = "monsters";
      message.value = `已还原；当前配置已自动备份：${safetyBackupName}`;
    }
  }
  async function reload(): Promise<void> {
    if (dirty.value && !window.confirm("重新读取会替换当前草稿，建议先下载草稿。继续？")) return;
    try {
      if (workspace.value?.workspace_id)
        catalog.value = await gameplayApi.catalog(workspace.value.workspace_id);
      await load(domain.value, objectId.value, true);
    } catch (error) {
      message.value = error instanceof Error ? error.message : "读取失败，草稿已保留";
    }
  }
  function downloadDraft(): void {
    if (!draft.value) return;
    const url = URL.createObjectURL(
      new Blob([JSON.stringify(draft.value, null, 2)], { type: "application/json" }),
    );
    const link = document.createElement("a");
    link.href = url;
    link.download = `draft-${domain.value}-${objectId.value ?? "all"}.json`;
    link.click();
    URL.revokeObjectURL(url);
  }
  onMounted(refreshWorkspace);
</script>
<style scoped>
  .workbench-layout {
    display: grid;
    height: 100dvh;
    grid-template-rows: auto minmax(0, 1fr);
    overflow: hidden;
    background: #f7f8fa;
  }
  @media (min-width: 768px) {
    .workbench-layout {
      grid-template-columns: 220px minmax(0, 1fr);
      grid-template-rows: minmax(0, 1fr);
    }
  }
</style>
