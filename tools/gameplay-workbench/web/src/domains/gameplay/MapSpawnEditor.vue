<template>
  <section class="spawn-layout min-h-0 min-w-0 flex-1" aria-label="地图刷怪编辑">
    <MapTreeSidebar v-model="mapId" :maps="maps" />
    <WorkbenchTable
      v-model:search="search"
      label="刷怪区域"
      search-label="搜索刷怪怪物"
      search-placeholder="怪物名称或 ID"
      count-label="个区域"
      :columns="columns"
      :rows="rows"
      :total="regions.length"
      :row-key="(entry) => `${entry.document.map_id}-${entry.groupIndex}-${entry.regionIndex}`"
      :empty-text="regions.length ? '没有匹配的刷怪区域。' : '没有有效刷怪区域。'"
      table-class="min-w-[1200px]"
    >
      <template #actions><slot name="actions"></slot></template>
      <template #filters>
        <FilterCombobox v-model="monsterId" label="怪物" :options="monsterOptions" />
        <FilterCombobox v-model="groupIndex" label="组" :options="groupOptions" class="w-28" />
        <FilterCombobox v-model="regionIndex" label="区域" :options="regionOptions" class="w-28" />
      </template>
      <template #cell-id="{ row: entry }">{{ entry.group.monster_id }}</template>
      <template #cell-map="{ row: entry }">{{ entry.document.map_name }}</template>
      <template #cell-group="{ row: entry }">{{ entry.groupIndex + 1 }}</template>
      <template #cell-region="{ row: entry }">{{ entry.regionIndex + 1 }}</template>
      <template #cell-monster="{ row: entry }">
        {{ entry.group.monster_name }}
      </template>
      <template
        v-for="field in spawnFields"
        :key="field.key"
        #[`cell-${field.key}`]="{ row: entry }"
      >
        <IntegerField
          v-model="entry.region[field.key]"
          :path="`${entry.document.map_id}.groups.${entry.groupIndex}.loc.${entry.regionIndex}.${field.key}`"
          :label="`${entry.group.monster_name} 区域 ${entry.regionIndex + 1} ${field.label}`"
          :minimum="field.minimum"
          :error="
            errors[
              `${entry.document.map_id}.groups.${entry.groupIndex}.loc.${entry.regionIndex}.${field.key}`
            ]
          "
          @edit="$emit('edit', $event)"
        />
      </template>
      <template #cell-cratio="{ row: entry }">{{ entry.region.cratio ?? "—" }}</template>
    </WorkbenchTable>
  </section>
</template>

<script setup lang="ts">
  import { computed, ref, watch } from "vue";
  import FilterCombobox from "@/components/FilterCombobox.vue";
  import IntegerField from "@/components/IntegerField.vue";
  import WorkbenchTable from "@/components/WorkbenchTable.vue";
  import MapTreeSidebar from "@/domains/map-guide/MapTreeSidebar.vue";
  import { matchesSearch, type TableColumn } from "@/components/workbenchTable";
  import type { Spawns } from "@/services/gameplayApi";

  const model = defineModel<Spawns[]>({ required: true });
  defineProps<{ errors: Record<string, string> }>();
  defineEmits<{ edit: [path: string] }>();
  const search = ref("");
  const mapId = ref<number | null>(null);
  const monsterId = ref<string | number | null>(null);
  const groupIndex = ref<string | number | null>(null);
  const regionIndex = ref<string | number | null>(null);
  const maps = computed(() =>
    model.value.map((document) => ({
      id: document.map_id,
      name: document.map_name,
    })),
  );
  const mapRegions = computed(() =>
    regions.value.filter((entry) => mapId.value === null || entry.document.map_id === mapId.value),
  );
  const monsterOptions = computed(() => [
    ...new Map(
      mapRegions.value.map(({ group }) => [
        group.monster_id,
        { value: group.monster_id, label: `${group.monster_name}（ID：${group.monster_id}）` },
      ]),
    ).values(),
  ]);
  const monsterRegions = computed(() =>
    mapRegions.value.filter(
      (entry) => monsterId.value === null || entry.group.monster_id === monsterId.value,
    ),
  );
  const groupOptions = computed(() =>
    [...new Set(monsterRegions.value.map((entry) => entry.groupIndex))]
      .sort((left, right) => left - right)
      .map((value) => ({ value, label: `第 ${value + 1} 组` })),
  );
  const groupRegions = computed(() =>
    monsterRegions.value.filter(
      (entry) => groupIndex.value === null || entry.groupIndex === groupIndex.value,
    ),
  );
  const regionOptions = computed(() =>
    [...new Set(groupRegions.value.map((entry) => entry.regionIndex))]
      .sort((left, right) => left - right)
      .map((value) => ({ value, label: `区域 ${value + 1}` })),
  );
  watch(mapId, () => {
    monsterId.value = null;
    groupIndex.value = null;
    regionIndex.value = null;
  });
  watch(monsterId, () => {
    groupIndex.value = null;
    regionIndex.value = null;
  });
  watch(groupIndex, () => {
    regionIndex.value = null;
  });
  const spawnFields = [
    { key: "x", label: "X", minimum: -2147483648 },
    { key: "y", label: "Y", minimum: -2147483648 },
    { key: "w", label: "宽度", minimum: 1 },
    { key: "h", label: "高度", minimum: 1 },
    { key: "count", label: "目标数", minimum: 0 },
    { key: "time", label: "周期（秒）", minimum: 1 },
  ] as const;
  const columns: TableColumn[] = [
    { key: "id", label: "ID", class: "w-20 text-slate-500" },
    { key: "monster", label: "怪物", class: "min-w-36" },
    { key: "map", label: "地图", class: "min-w-36" },
    { key: "group", label: "组", class: "w-20 text-slate-500" },
    { key: "region", label: "区域", class: "w-20 text-slate-500" },
    ...spawnFields.map((field) => ({ ...field, class: "w-28", required: true })),
    { key: "cratio", label: "cratio · 未使用", class: "text-slate-500" },
  ];
  const regions = computed(() =>
    model.value.flatMap((document) =>
      document.groups.flatMap((group, groupIndex) =>
        group.loc.map((region, regionIndex) => ({
          document,
          group,
          groupIndex,
          region,
          regionIndex,
        })),
      ),
    ),
  );
  const rows = computed(() =>
    groupRegions.value.filter(
      (entry) =>
        (regionIndex.value === null || entry.regionIndex === regionIndex.value) &&
        matchesSearch(search.value, entry.group.monster_name, entry.group.monster_id),
    ),
  );
</script>

<style scoped>
  .spawn-layout {
    display: grid;
    grid-template-rows: minmax(12rem, 32%) minmax(0, 1fr);
    gap: 1rem;
  }
  @media (min-width: 1024px) {
    .spawn-layout {
      grid-template-columns: 20rem minmax(0, 1fr);
      grid-template-rows: minmax(0, 1fr);
    }
  }
</style>
