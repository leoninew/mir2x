<template>
  <aside
    class="flex min-h-0 min-w-0 flex-col overflow-hidden rounded-lg border border-slate-200 bg-white"
    aria-label="刷怪地图"
  >
    <div class="shrink-0 border-b border-slate-200 p-3">
      <input
        v-model="query"
        type="search"
        class="table-search px-3"
        aria-label="搜索地图、分组或 ID"
        placeholder="搜索地图、分组或 ID"
      />
    </div>
    <nav class="min-h-0 flex-1 overflow-auto p-2" aria-label="刷怪地图列表">
      <button
        type="button"
        class="mb-1 flex w-full items-center justify-between rounded px-3 py-2 text-left text-sm"
        :class="
          model === null
            ? 'bg-indigo-50 font-medium text-indigo-800'
            : 'text-slate-600 hover:bg-slate-50'
        "
        :aria-current="model === null ? 'true' : undefined"
        @click="model = null"
      >
        <span>全部地图</span>
        <span class="text-xs tabular-nums text-slate-400">{{ maps.length }}</span>
      </button>
      <MapGuideGroup
        v-for="group in groups"
        :key="group.key"
        :group="group"
        :expanded="expanded"
        :selected-id="model"
        @toggle="toggleGroup"
        @select="model = $event"
      />
      <p v-if="filteredMaps.length === 0" class="px-3 py-5 text-sm text-slate-500">
        没有匹配的地图
      </p>
    </nav>
  </aside>
</template>

<script setup lang="ts">
  import { computed, ref, watch } from "vue";
  import type { IdentityRecord } from "@/services/gameplayApi";
  import MapGuideGroup from "./MapGuideGroup.vue";
  import guideData from "./map-guide.json";
  import { groupKeys, groupMaps, type GroupedMap } from "./mapGuide";

  const model = defineModel<number | null>({ default: null });
  const props = defineProps<{ maps: IdentityRecord[] }>();
  const knownMaps = new Map(guideData.maps.map((map) => [map.id, map]));
  const maps = computed<GroupedMap[]>(() =>
    props.maps.map((map) => {
      const known = knownMaps.get(map.id);
      return {
        ...map,
        groupPath: known?.name === map.name ? known.groupPath : ["未命名与其他", "其他地图"],
      };
    }),
  );
  const query = ref("");
  const expanded = ref(new Set<string>());
  const selected = computed(() => maps.value.find((map) => map.id === model.value));
  const filteredMaps = computed(() => {
    const term = query.value.trim().toLowerCase();
    return maps.value.filter((map) =>
      [map.name, map.id, ...map.groupPath].join(" ").toLowerCase().includes(term),
    );
  });
  const groups = computed(() => groupMaps(filteredMaps.value));

  watch(maps, (available) => {
    if (model.value !== null && !available.some((map) => map.id === model.value)) {
      model.value = null;
    }
  });
  watch(
    selected,
    (map) => {
      if (map) expanded.value = new Set([...expanded.value, ...groupKeys(map)]);
    },
    { immediate: true },
  );
  watch(query, () => {
    if (query.value.trim()) {
      expanded.value = new Set([...expanded.value, ...filteredMaps.value.flatMap(groupKeys)]);
    }
  });
  function toggleGroup(key: string): void {
    const next = new Set(expanded.value);
    if (next.has(key)) next.delete(key);
    else next.add(key);
    expanded.value = next;
  }
</script>
