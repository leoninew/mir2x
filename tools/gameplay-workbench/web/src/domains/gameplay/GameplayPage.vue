<template>
  <div class="workbench-layout">
    <WorkbenchSidebar :page="page" :busy="busy || managementBusy" @choose="chooseWorkspace" />
    <main class="relative flex min-h-0 min-w-0 flex-col gap-5 p-4 sm:p-6 lg:p-8" :aria-busy="busy">
      <AppLoading v-if="busy" :label="activity" overlay />
      <header class="shrink-0">
        <div class="flex flex-wrap items-center gap-3">
          <h1 class="text-2xl font-semibold tracking-tight text-slate-950">{{ pageInfo.label }}</h1>
          <button
            v-if="page === 'launcher'"
            type="button"
            class="text-button"
            :disabled="!launchPanel?.canConfigure"
            @click="launchPanel?.openSettings()"
          >
            <Settings2 class="size-4 shrink-0" aria-hidden="true" />
            路径设置
          </button>
          <span
            v-if="isConfigPage(page)"
            class="rounded bg-indigo-50 px-2 py-1 text-xs text-indigo-600"
          >
            重启生效
          </span>
        </div>
        <p class="mt-2 text-sm leading-6 text-slate-500">{{ pageInfo.description }}</p>
      </header>
      <div
        v-show="page === 'launcher'"
        class="flex min-h-0 flex-1 items-center justify-center overflow-auto"
      >
        <GameLaunchPanel
          ref="launchPanel"
          :gameplay-dirty="hasUnsavedDrafts"
          :gameplay-busy="busy"
          :workspace-root="workspace?.server_root ?? null"
        />
      </div>
      <div v-if="page === 'gm'" class="flex min-h-0 min-w-0 flex-1">
        <GMCommandsPage />
      </div>
      <div v-if="page === 'map-guide'" class="flex min-h-0 min-w-0 flex-1">
        <MapGuidePage />
      </div>
      <div
        v-if="
          !busy && !workspace?.bound && page !== 'launcher' && page !== 'gm' && page !== 'map-guide'
        "
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
          class="flex min-h-0 min-w-0 flex-1 flex-col overflow-hidden"
          aria-label="配置编辑区"
        >
          <fieldset
            v-if="domain === page"
            :disabled="busy"
            class="flex min-h-0 min-w-0 flex-1 flex-col"
          >
            <MonsterExperienceEditor
              v-if="draft && 'monsters' in draft"
              v-model="draft"
              :errors="errors"
              @edit="clearError"
            >
              <template #actions>
                <GameplayActions
                  :busy="busy"
                  :ready="Boolean(base)"
                  :dirty="dirty"
                  @refresh="reload"
                  @save="save"
                />
              </template>
            </MonsterExperienceEditor>
            <MonsterDropEditor
              v-else-if="domain === 'drops'"
              :model-value="dropDrafts"
              :errors="errors"
              @edit="clearError"
            >
              <template #actions>
                <GameplayActions
                  :busy="busy"
                  :ready="Boolean(base)"
                  :dirty="dirty"
                  @refresh="reload"
                  @save="save"
                />
              </template>
            </MonsterDropEditor>
            <GlobalMultipliersEditor
              v-else-if="draft && 'experience' in draft"
              v-model="draft"
              :errors="errors"
              @edit="clearError"
            >
              <template #actions>
                <GameplayActions
                  :busy="busy"
                  :ready="Boolean(base)"
                  :dirty="dirty"
                  @refresh="reload"
                  @save="save"
                />
              </template>
            </GlobalMultipliersEditor>
            <MapSpawnEditor
              v-else-if="domain === 'spawns'"
              :model-value="spawnDrafts"
              :errors="errors"
              @edit="clearError"
            >
              <template #actions>
                <GameplayActions
                  :busy="busy"
                  :ready="Boolean(base)"
                  :dirty="dirty"
                  @refresh="reload"
                  @save="save"
                />
              </template>
            </MapSpawnEditor>
          </fieldset>
        </section>
        <details v-if="dirty" class="shrink-0 rounded-lg border border-slate-200 bg-white p-4">
          <summary class="text-sm font-medium text-slate-700">
            变更预览 · {{ changes.length }} 处
          </summary>
          <div class="mt-3 max-h-40 overflow-auto">
            <table class="data-table">
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
            显示前 100 处；保存作用于当前页面的完整修改。
          </p>
        </details>
      </template>
      <div v-if="workspace?.bound && page === 'backups'" class="flex min-h-0 flex-1 flex-col">
        <GameplayBackupPanel
          :workspace-id="workspace.workspace_id"
          :disabled="busy"
          :has-unsaved-drafts="hasUnsavedDrafts"
          @restored="onBackupRestored"
          @busy-change="managementBusy = $event"
        />
      </div>
      <MigrationRecordsTable
        v-if="workspace?.bound && catalog && page === 'migration'"
        :records="catalog.migration_notes.records"
      />
    </main>
  </div>
</template>
<script setup lang="ts">
  import { Settings2 } from "@lucide/vue";
  import { computed, onMounted, ref } from "vue";
  import { onBeforeRouteLeave, onBeforeRouteUpdate, useRoute, useRouter } from "vue-router";
  import { useGameplay } from "@/composables/useGameplay";
  import AppLoading from "@/components/AppLoading.vue";
  import { useToastsStore } from "@/stores/toasts";
  import GameLaunchPanel from "./GameLaunchPanel.vue";
  import GMCommandsPage from "@/domains/gm/GMCommandsPage.vue";
  import MapGuidePage from "@/domains/map-guide/MapGuidePage.vue";
  import GameplayBackupPanel from "./GameplayBackupPanel.vue";
  import GameplayActions from "./GameplayActions.vue";
  import GlobalMultipliersEditor from "./GlobalMultipliersEditor.vue";
  import MapSpawnEditor from "./MapSpawnEditor.vue";
  import MigrationRecordsTable from "./MigrationRecordsTable.vue";
  import MonsterDropEditor from "./MonsterDropEditor.vue";
  import MonsterExperienceEditor from "./MonsterExperienceEditor.vue";
  import WorkbenchSidebar from "./WorkbenchSidebar.vue";
  import { isConfigPage, navigationGroups, type WorkbenchPage } from "./gameplayNavigation";
  import { getDesktopBridge, isDesktopApp } from "@/services/desktopBridge";
  const toasts = useToastsStore();

  const {
    workspace,
    catalog,
    base,
    draft,
    dropDrafts,
    spawnDrafts,
    domain,
    busy,
    activity,
    errors,
    changes,
    dirty,
    hasUnsavedDrafts,
    load,
    refreshWorkspace,
    reload: reloadCurrent,
    save,
    clearError,
  } = useGameplay();
  const route = useRoute();
  const router = useRouter();
  const page = computed(() => (route.meta.workbenchPage as WorkbenchPage) ?? "monsters");
  const managementBusy = ref(false);
  const launchPanel = ref<InstanceType<typeof GameLaunchPanel> | null>(null);
  const pageInfo = computed(() =>
    navigationGroups.flatMap((group) => group.items).find((item) => item.page === page.value)!,
  );
  async function preparePage(next: WorkbenchPage): Promise<boolean> {
    if (!isConfigPage(next) || !workspace.value?.bound) return true;
    if (domain.value === next) return true;
    await load(next);
    return domain.value === next;
  }
  async function chooseWorkspace(): Promise<void> {
    if (!isDesktopApp()) {
      toasts.show("请在桌面版中选择目录", "info");
      return;
    }
    busy.value = true;
    activity.value = "正在选择配置目录…";
    try {
      const result = await getDesktopBridge().select_gameplay_workspace();
      if (result.success) {
        if (await refreshWorkspace()) await router.replace({ name: "monsters" });
      } else if (result.message && result.message !== "未选择目录")
        toasts.show(result.message, "error");
    } catch (error) {
      toasts.show(error instanceof Error ? error.message : "选择目录失败", "error");
    } finally {
      busy.value = false;
    }
  }
  async function onBackupRestored(): Promise<void> {
    if (await refreshWorkspace(true)) {
      await router.replace({ name: "monsters" });
    }
  }
  async function reload(): Promise<void> {
    if (dirty.value && !window.confirm("刷新会丢弃当前页面未保存的修改。继续？")) return;
    await reloadCurrent();
  }
  onBeforeRouteUpdate(async (to) => {
    if (busy.value || managementBusy.value) return false;
    return preparePage(to.meta.workbenchPage as WorkbenchPage);
  });
  onBeforeRouteLeave(async (to) => {
    if (busy.value || managementBusy.value) return false;
    if (to.meta.workbenchPage) return preparePage(to.meta.workbenchPage as WorkbenchPage);
    return !hasUnsavedDrafts.value || window.confirm("离开工作台会丢弃未保存草稿。继续？");
  });
  onMounted(async () => {
    if (await refreshWorkspace()) await preparePage(page.value);
  });
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
