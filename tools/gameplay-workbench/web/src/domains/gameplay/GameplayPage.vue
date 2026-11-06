<template>
  <section class="grid gap-5">
    <div class="flex flex-wrap items-center justify-between gap-4">
      <div class="flex items-center gap-3">
        <h1 class="text-2xl font-semibold tracking-tight text-slate-950">玩法配置</h1>
        <span
          v-if="workspace?.bound"
          class="rounded bg-slate-200/60 px-2 py-1 text-xs text-slate-500"
        >
          重启生效
        </span>
      </div>
      <button
        v-if="workspace?.bound"
        type="button"
        class="secondary-button"
        :disabled="busy"
        @click="chooseWorkspace"
      >
        更换目录
      </button>
    </div>

    <div v-if="workspace?.bound" class="flex min-w-0 items-center gap-3 text-sm text-slate-500">
      <span class="shrink-0">服务端</span>
      <span class="truncate font-mono text-xs" :title="workspace.server_root ?? undefined">
        {{ workspace.server_root }}
      </span>
    </div>

    <GameLaunchPanel
      :gameplay-dirty="hasUnsavedDrafts"
      :gameplay-busy="busy"
      :workspace-root="workspace?.server_root ?? null"
    />

    <p
      v-if="message"
      role="status"
      :class="[
        'rounded-lg border px-4 py-3 text-sm',
        conflict
          ? 'border-amber-300 bg-amber-50 text-amber-950'
          : 'border-slate-200 bg-slate-50 text-slate-800',
      ]"
    >
      {{ message }}
    </p>

    <div
      v-if="!workspace?.bound"
      class="grid min-h-72 place-items-center rounded-lg border border-dashed border-slate-300 bg-white/60 px-6 py-10 text-center"
    >
      <div>
        <svg
          aria-hidden="true"
          viewBox="0 0 40 40"
          class="mx-auto mb-4 size-10 fill-none stroke-slate-400"
          stroke-width="1.5"
        >
          <path d="M5 12a3 3 0 0 1 3-3h9l4 4h11a3 3 0 0 1 3 3v15a3 3 0 0 1-3 3H8a3 3 0 0 1-3-3z" />
          <path d="M5 19h30" />
        </svg>
        <h2 class="text-base font-medium text-slate-800">打开服务端配置</h2>
        <p class="mt-2 text-sm text-slate-500">选择 server 目录，编辑经验、掉落与刷怪。</p>
        <button type="button" class="primary-button mt-5" :disabled="busy" @click="chooseWorkspace">
          选择服务端目录
        </button>
      </div>
    </div>

    <template v-if="workspace?.bound && catalog">
      <nav aria-label="玩法配置类别" class="flex gap-2 border-b border-slate-200 pb-3">
        <button
          v-for="tab in tabs"
          :key="tab.domain"
          type="button"
          :disabled="busy"
          :aria-current="domain === tab.domain ? 'page' : undefined"
          :class="[
            'rounded-md px-4 py-2 text-sm',
            domain === tab.domain
              ? 'bg-slate-900 text-white'
              : 'bg-white text-slate-700 hover:bg-slate-100',
          ]"
          @click="selectDomain(tab.domain)"
        >
          {{ tab.label }}
        </button>
      </nav>

      <div class="flex flex-wrap items-end gap-4">
        <label class="grid gap-1.5 text-sm text-slate-700">
          搜索{{ domain === "spawns" ? "地图" : "怪物" }}
          <input
            v-model="search"
            class="rounded-md border border-slate-300 bg-white px-3 py-2"
            placeholder="名称或 ID"
          />
        </label>
        <label v-if="domain !== 'monsters'" class="grid min-w-64 gap-1.5 text-sm text-slate-700">
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
        <span v-if="base" class="pb-2 text-xs text-slate-500">
          文件版本 {{ base.revision.slice(0, 12) }}
        </span>
      </div>

      <fieldset v-if="draft" :disabled="busy" class="min-w-0">
        <div v-if="'monsters' in draft" class="table-scroll">
          <table class="config-table">
            <thead>
              <tr>
                <th>怪物 ID</th>
                <th>怪物名称</th>
                <th>基础经验 <span class="required">*</span></th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="entry in monsterRows" :key="entry.row.monster_id">
                <td class="text-slate-500">{{ entry.row.monster_id }}</td>
                <td>{{ entry.row.monster_name }}</td>
                <td>
                  <IntegerField
                    v-model="entry.row.exp"
                    :path="`monsters.${entry.index}.exp`"
                    :label="`${entry.row.monster_name} 基础经验`"
                    :error="errors[`monsters.${entry.index}.exp`]"
                    @edit="clearError"
                  />
                </td>
              </tr>
            </tbody>
          </table>
        </div>

        <template v-else-if="'items' in draft">
          <p class="mb-3 text-sm text-slate-600">
            {{ catalog.fields.prob_recip }}。{{ catalog.fields.group }}。
          </p>
          <div class="table-scroll">
            <table class="config-table">
              <thead>
                <tr>
                  <th>原始顺序</th>
                  <th>物品</th>
                  <th v-for="field in dropFields" :key="field.key">
                    {{ field.label }} <span class="required">*</span>
                  </th>
                </tr>
              </thead>
              <tbody>
                <tr v-for="(row, index) in draft.items" :key="index">
                  <td class="text-slate-500">{{ index + 1 }}</td>
                  <td>
                    {{ row.item_name
                    }}<span class="block text-xs text-slate-500">ID {{ row.item_id }}</span>
                  </td>
                  <td v-for="field in dropFields" :key="field.key">
                    <IntegerField
                      v-model="row[field.key]"
                      :path="`items.${index}.${field.key}`"
                      :label="`第 ${index + 1} 项 ${field.label}`"
                      :minimum="field.key === 'group' ? 0 : 1"
                      :error="errors[`items.${index}.${field.key}`]"
                      @edit="clearError"
                    />
                  </td>
                </tr>
                <tr v-if="!draft.items.length">
                  <td colspan="6">此怪物没有配置掉落条目。</td>
                </tr>
              </tbody>
            </table>
          </div>
        </template>

        <template v-else>
          <p class="mb-3 text-sm text-slate-600">
            周期单位为秒；目标数为 0 时仍沿原逻辑至少生成 1 个。cratio 当前未消费，只读保留。
          </p>
          <div
            v-for="(group, groupIndex) in draft.groups"
            :key="groupIndex"
            class="mb-5 overflow-hidden rounded-lg border border-slate-200 bg-white"
          >
            <h2 class="border-b border-slate-200 bg-slate-50 px-4 py-3 text-sm font-medium">
              {{ groupIndex + 1 }} · {{ group.monster_name }}
              <span class="font-normal text-slate-500">ID {{ group.monster_id }}</span>
            </h2>
            <div class="overflow-x-auto">
              <table class="config-table border-0">
                <thead>
                  <tr>
                    <th v-for="field in spawnFields" :key="field.key">
                      {{ field.label }} <span class="required">*</span>
                    </th>
                    <th>cratio · 未使用</th>
                  </tr>
                </thead>
                <tbody>
                  <tr v-for="(region, regionIndex) in group.loc" :key="regionIndex">
                    <td v-for="field in spawnFields" :key="field.key">
                      <IntegerField
                        v-model="region[field.key]"
                        :path="`groups.${groupIndex}.loc.${regionIndex}.${field.key}`"
                        :label="`${group.monster_name} 区域 ${regionIndex + 1} ${field.label}`"
                        :minimum="field.minimum"
                        :error="errors[`groups.${groupIndex}.loc.${regionIndex}.${field.key}`]"
                        @edit="clearError"
                      />
                    </td>
                    <td class="text-slate-500">{{ region.cratio ?? "—" }}</td>
                  </tr>
                </tbody>
              </table>
            </div>
          </div>
          <p
            v-if="!draft.groups.length"
            class="rounded-lg border border-slate-200 bg-white p-5 text-sm"
          >
            此地图没有有效刷怪组。
          </p>
        </template>
      </fieldset>

      <section v-if="dirty" class="rounded-lg border border-slate-300 bg-white p-5">
        <h2 class="text-sm font-semibold">变更预览 · {{ changes.length }} 处</h2>
        <div class="mt-3 max-h-64 overflow-auto">
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
                <td class="font-medium">{{ change.after }}</td>
              </tr>
            </tbody>
          </table>
        </div>
        <p v-if="changes.length > 100" class="mt-2 text-xs text-slate-500">
          预览显示前 100 处；保存作用于当前完整文件。
        </p>
      </section>

      <div
        class="sticky bottom-4 flex flex-wrap items-center justify-between gap-3 rounded-lg border border-slate-200 bg-white/95 p-4 shadow-sm backdrop-blur"
      >
        <p class="text-sm text-slate-600">
          {{ busy ? "处理中…" : dirty ? `${changes.length} 处未保存` : "已保存" }}
        </p>
        <div class="flex flex-wrap gap-2">
          <button
            type="button"
            class="secondary-button"
            :disabled="!draft || busy"
            @click="downloadDraft"
          >
            下载草稿
          </button>
          <button type="button" class="secondary-button" :disabled="!base || busy" @click="reload">
            重新读取
          </button>
          <button type="button" class="primary-button" :disabled="!dirty || busy" @click="save">
            校验并保存当前文件
          </button>
        </div>
      </div>

      <details class="rounded-lg border border-slate-200 bg-white p-5">
        <summary class="text-sm font-medium">
          迁移保留记录 · {{ catalog.migration_notes.records.length }} 项未生效数据
        </summary>
        <p class="mt-3 text-sm text-slate-600">
          这些是旧代码已过滤的原始记录，只读保留，不会被此界面启用。
        </p>
        <div class="mt-3 max-h-96 overflow-auto">
          <details
            v-for="(record, index) in catalog.migration_notes.records"
            :key="index"
            class="border-t border-slate-100 py-3 text-sm"
          >
            <summary>{{ record.source_path }} : {{ record.source_position }}</summary>
            <p class="my-2 text-slate-600">{{ record.reason }}</p>
            <pre class="overflow-auto rounded bg-slate-50 p-3 text-xs">{{
              JSON.stringify(record.raw, null, 2)
            }}</pre>
          </details>
        </div>
      </details>
    </template>
  </section>
</template>

<script setup lang="ts">
  import { computed, onMounted, ref } from "vue";

  import IntegerField from "@/components/IntegerField.vue";
  import { useGameplay } from "@/composables/useGameplay";
  import GameLaunchPanel from "@/domains/gameplay/GameLaunchPanel.vue";
  import { getDesktopBridge, isDesktopApp } from "@/services/desktopBridge";
  import { gameplayApi, type Domain } from "@/services/gameplayApi";

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
  const search = ref("");
  const tabs: { domain: Domain; label: string }[] = [
    { domain: "monsters", label: "基础经验" },
    { domain: "drops", label: "金币与物品掉落" },
    { domain: "spawns", label: "地图刷怪" },
  ];
  const dropFields = [
    { key: "group", label: "组" },
    { key: "prob_recip", label: "概率分母" },
    { key: "repeat", label: "尝试次数" },
    { key: "count", label: "数量" },
  ] as const;
  const spawnFields = [
    { key: "x", label: "X", minimum: -2147483648 },
    { key: "y", label: "Y", minimum: -2147483648 },
    { key: "w", label: "宽度", minimum: 1 },
    { key: "h", label: "高度", minimum: 1 },
    { key: "count", label: "目标数", minimum: 0 },
    { key: "time", label: "周期（秒）", minimum: 1 },
  ] as const;
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
  const monsterRows = computed(() =>
    draft.value && "monsters" in draft.value
      ? draft.value.monsters
          .map((row, index) => ({ row, index }))
          .filter(({ row }) => matches(row.monster_name, row.monster_id))
      : [],
  );

  async function selectDomain(next: Domain): Promise<void> {
    search.value = "";
    const id =
      next === "monsters"
        ? null
        : next === "drops"
          ? catalog.value?.manifest.drop_monsters[0]
          : catalog.value?.manifest.spawn_maps[0];
    if (next !== "monsters" && id === undefined) return;
    await load(next, id ?? null);
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
      if (result.success) await refreshWorkspace();
      else if (result.message && result.message !== "未选择目录") message.value = result.message;
    } catch (error) {
      message.value = error instanceof Error ? error.message : "选择目录失败";
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
  @reference "../../style.css";
  .primary-button {
    @apply rounded-md bg-slate-900 px-4 py-2 text-sm font-medium text-white hover:bg-slate-700 disabled:opacity-50;
  }
  .secondary-button {
    @apply rounded-md border border-slate-300 bg-white px-4 py-2 text-sm text-slate-700 hover:bg-slate-50 disabled:opacity-50;
  }
  .table-scroll {
    @apply max-h-[38rem] overflow-auto rounded-lg border border-slate-200 bg-white;
  }
  .config-table {
    @apply w-full border-collapse text-left text-sm;
  }
  .config-table th {
    @apply sticky top-0 border-b border-slate-200 bg-slate-50 px-4 py-3 text-xs font-medium whitespace-nowrap text-slate-600;
  }
  .config-table td {
    @apply border-b border-slate-100 px-4 py-3 align-top;
  }
  .required {
    @apply text-red-600;
  }
</style>
