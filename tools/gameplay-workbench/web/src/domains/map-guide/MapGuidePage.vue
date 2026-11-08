<template>
  <section class="guide-layout min-h-0 min-w-0 flex-1" aria-label="地图指南">
    <aside class="guide-sidebar flex min-h-0 min-w-0 flex-col border border-slate-200 bg-white">
      <div class="shrink-0 border-b border-slate-200 p-3">
        <input
          v-model="query"
          class="table-search px-3"
          type="search"
          placeholder="搜索地图名称或 ID"
          aria-label="搜索地图名称或 ID"
        />
        <div class="mt-3 flex items-center gap-1" aria-label="地图筛选">
          <button
            v-for="option in filters"
            :key="option.value"
            type="button"
            class="rounded px-2 py-1.5 text-xs"
            :class="
              filter === option.value
                ? 'bg-slate-800 text-white'
                : 'text-slate-600 hover:bg-slate-100'
            "
            :aria-pressed="filter === option.value"
            @click="filter = option.value"
          >
            {{ option.label }}
          </button>
          <span class="ml-auto text-xs text-slate-400">{{ filteredMaps.length }} 张</span>
        </div>
      </div>
      <nav class="min-h-0 flex-1 overflow-auto p-2" aria-label="地图列表">
        <MapGuideGroup
          v-for="group in groupedMaps"
          :key="group.key"
          :group="group"
          :expanded="expandedGroups"
          :selected-id="selectedId"
          @toggle="toggleGroup"
          @select="selectMap"
        />
        <p v-if="filteredMaps.length === 0" class="px-3 py-5 text-sm text-slate-500">
          没有匹配的地图
        </p>
      </nav>
    </aside>

    <article class="flex min-h-0 min-w-0 flex-col overflow-hidden border border-slate-200 bg-white">
      <header class="shrink-0 border-b border-slate-200 px-5 pt-5">
        <p class="mb-2 text-xs text-slate-400">{{ selected.groupPath.join(" / ") }}</p>
        <div class="flex flex-wrap items-baseline gap-x-3 gap-y-1">
          <h2 class="break-all text-xl font-semibold text-slate-950">{{ selected.name }}</h2>
          <span class="font-mono text-xs text-slate-400">ID {{ selected.id }}</span>
        </div>
        <p class="mt-2 text-xs text-slate-500">复制 GM 指令，在聊天窗口发送即可传送到对应落点。</p>
        <div class="mt-4 flex gap-5" aria-label="地图信息">
          <button
            v-for="tab in tabs"
            :key="tab.value"
            type="button"
            class="border-b-2 pb-3 text-sm"
            :class="
              view === tab.value
                ? 'border-indigo-600 font-medium text-indigo-700'
                : 'border-transparent text-slate-500 hover:text-slate-800'
            "
            :aria-pressed="view === tab.value"
            @click="view = tab.value"
          >
            {{ tab.label }} <span class="ml-1 text-xs text-slate-400">{{ tab.count }}</span>
          </button>
        </div>
      </header>

      <div v-if="view === 'exits'" class="min-h-0 flex-1 overflow-auto">
        <p class="border-b border-slate-100 px-5 py-3 text-xs text-slate-500">
          出入口为静态配置，地形和任务状态可能影响通行。
        </p>
        <table v-if="selected.exits.length" class="data-table min-w-[46rem]">
          <thead>
            <tr>
              <th>目的地</th>
              <th>入口坐标</th>
              <th>落点</th>
              <th>GM 指令</th>
            </tr>
          </thead>
          <tbody v-for="exit in selected.exits" :key="exit.mapId">
            <tr v-for="(gate, index) in exit.gates" :key="gate.arrival.join(',')">
              <td v-if="index === 0" :rowspan="exit.gates.length">
                <button
                  type="button"
                  class="text-left font-medium text-indigo-700 hover:underline"
                  @click="openMap(exit.mapId)"
                >
                  {{ exit.mapName }}
                </button>
                <div class="mt-1 font-mono text-xs text-slate-400">ID {{ exit.mapId }}</div>
              </td>
              <td>
                <div class="max-w-[22rem] font-mono text-xs leading-6 text-slate-600">
                  <span
                    v-for="entrance in gate.entrances"
                    :key="entrance.join(',')"
                    class="mr-2 inline-block whitespace-nowrap"
                  >
                    {{ formatCoordinate(entrance) }}
                  </span>
                </div>
              </td>
              <td class="whitespace-nowrap font-mono text-xs">
                {{ formatCoordinate(gate.arrival) }}
              </td>
              <td><GMMoveCommand :map-id="exit.mapId" :coordinate="gate.arrival" /></td>
            </tr>
          </tbody>
        </table>
        <p v-else class="px-5 py-10 text-sm text-slate-500">没有配置静态地图出口</p>
      </div>

      <div v-else class="min-h-0 flex-1 overflow-auto">
        <div v-if="selected.teleports.length" class="divide-y divide-slate-200">
          <section v-for="npc in selected.teleports" :key="npc.name" class="px-5 py-4">
            <div class="flex flex-wrap items-baseline gap-3">
              <h3 class="text-sm font-semibold text-slate-900">{{ npc.name }}</h3>
              <span class="font-mono text-xs text-slate-500">{{
                formatCoordinate(npc.location)
              }}</span>
            </div>
            <div class="mt-3 overflow-x-auto">
              <table class="data-table min-w-[44rem]">
                <thead>
                  <tr>
                    <th>目的地</th>
                    <th>落点</th>
                    <th>金币</th>
                    <th>等级</th>
                    <th>GM 指令</th>
                  </tr>
                </thead>
                <tbody>
                  <tr v-for="(destination, index) in npc.destinations" :key="index">
                    <td>
                      <button
                        type="button"
                        class="text-left text-indigo-700 hover:underline"
                        @click="openMap(destination.mapId)"
                      >
                        {{ destination.label }}
                      </button>
                      <span
                        v-if="destination.label !== destination.mapName"
                        class="ml-2 text-xs text-slate-400"
                      >
                        {{ destination.mapName }}
                      </span>
                    </td>
                    <td class="font-mono text-xs">{{ formatCoordinate(destination.arrival) }}</td>
                    <td class="tabular-nums">
                      {{ destination.gold ? destination.gold.toLocaleString() : "免费" }}
                    </td>
                    <td>{{ destination.level ? `${destination.level} 级` : "—" }}</td>
                    <td>
                      <GMMoveCommand
                        :map-id="destination.mapId"
                        :coordinate="destination.arrival"
                      />
                    </td>
                  </tr>
                </tbody>
              </table>
            </div>
          </section>
        </div>
        <p v-else class="px-5 py-10 text-sm text-slate-500">没有配置神石传送</p>
      </div>
    </article>
  </section>
</template>

<script setup lang="ts">
  import { computed, ref, watch } from "vue";
  import GMMoveCommand from "./GMMoveCommand.vue";
  import MapGuideGroup from "./MapGuideGroup.vue";
  import guideData from "./map-guide.json";
  import { groupKeys, groupMaps, type Coordinate, type GuideMap, type MapGroup } from "./mapGuide";

  const maps: GuideMap[] = guideData.maps;
  const query = ref("");
  const filter = ref<"all" | "exits" | "teleports">("all");
  const view = ref<"exits" | "teleports">("exits");
  const selectedId = ref(maps.find((map) => map.name === "道馆_1")?.id ?? maps[0]!.id);
  const filters = [
    { value: "all" as const, label: "全部" },
    { value: "exits" as const, label: "有出口" },
    { value: "teleports" as const, label: "有神石" },
  ];
  const selected = computed(() => maps.find((map) => map.id === selectedId.value) ?? maps[0]!);
  const expandedGroups = ref(new Set(groupKeys(selected.value)));
  const filteredMaps = computed(() => {
    const term = query.value.trim().toLowerCase();
    return maps.filter(
      (map) =>
        (filter.value === "all" ||
          (filter.value === "exits" ? map.exits.length : map.teleports.length)) &&
        (!term || [map.name, map.id, ...map.groupPath].join(" ").toLowerCase().includes(term)),
    );
  });
  const groupedMaps = computed(() => groupMaps(filteredMaps.value));
  const tabs = computed(() => [
    { value: "exits" as const, label: "地图出口", count: selected.value.exits.length },
    { value: "teleports" as const, label: "神石传送", count: selected.value.teleports.length },
  ]);

  watch([query, filter], () => {
    if (!query.value.trim() && filter.value === "all") return;
    const expanded = new Set(expandedGroups.value);
    function expandMatches(groups: MapGroup[]): void {
      for (const group of groups) {
        expanded.add(group.key);
        expandMatches(group.children);
      }
    }
    expandMatches(groupedMaps.value);
    expandedGroups.value = expanded;
  });

  function toggleGroup(key: string): void {
    const expanded = new Set(expandedGroups.value);
    if (expanded.has(key)) expanded.delete(key);
    else expanded.add(key);
    expandedGroups.value = expanded;
  }
  function selectMap(id: number): void {
    selectedId.value = id;
    expandedGroups.value = new Set([...expandedGroups.value, ...groupKeys(selected.value)]);
  }
  function openMap(id: number): void {
    selectMap(id);
    query.value = "";
    filter.value = "all";
    view.value = "exits";
  }
  function formatCoordinate(coordinate: Coordinate): string {
    return `(${coordinate[0]}, ${coordinate[1]})`;
  }
</script>

<style scoped>
  .guide-layout {
    display: grid;
    grid-template-rows: minmax(12rem, 32%) minmax(0, 1fr);
    gap: 1rem;
  }
  @media (min-width: 1024px) {
    .guide-layout {
      grid-template-columns: 20rem minmax(0, 1fr);
      grid-template-rows: minmax(0, 1fr);
    }
  }
</style>
