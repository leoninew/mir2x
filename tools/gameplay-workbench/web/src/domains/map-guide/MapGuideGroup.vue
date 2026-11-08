<template>
  <div>
    <button
      type="button"
      class="flex w-full items-center gap-2 rounded px-2 py-2 text-left text-sm text-slate-700 hover:bg-slate-50"
      :aria-expanded="expanded.has(group.key)"
      @click="emit('toggle', group.key)"
    >
      <ChevronRight
        class="size-3 shrink-0 text-slate-400 transition-transform"
        :class="{ 'rotate-90': expanded.has(group.key) }"
        aria-hidden="true"
      />
      <span class="min-w-0 flex-1 truncate font-medium">{{ group.label }}</span>
      <span class="text-xs tabular-nums text-slate-400">{{ group.count }}</span>
    </button>
    <div v-if="expanded.has(group.key)" class="ml-3 border-l border-slate-100 pl-1">
      <MapGuideGroup
        v-for="child in group.children"
        :key="child.key"
        :group="child"
        :expanded="expanded"
        :selected-id="selectedId"
        @toggle="emit('toggle', $event)"
        @select="emit('select', $event)"
      />
      <button
        v-for="map in group.maps"
        :key="map.id"
        type="button"
        class="flex w-full items-center justify-between gap-2 rounded px-3 py-2 text-left text-sm"
        :class="
          selectedId === map.id
            ? 'bg-indigo-50 font-medium text-indigo-800'
            : 'text-slate-600 hover:bg-slate-50'
        "
        :aria-current="selectedId === map.id ? 'true' : undefined"
        :title="map.name"
        @click="emit('select', map.id)"
      >
        <span class="min-w-0 truncate">{{ map.name }}</span>
        <span class="shrink-0 font-mono text-xs text-slate-400">{{ map.id }}</span>
      </button>
    </div>
  </div>
</template>

<script setup lang="ts">
  import { ChevronRight } from "@lucide/vue";
  import type { MapGroup } from "./mapGuide";

  defineProps<{ group: MapGroup; expanded: Set<string>; selectedId: number | null }>();
  const emit = defineEmits<{ toggle: [key: string]; select: [id: number] }>();
</script>
