<template>
  <div class="flex items-center justify-between gap-3">
    <code
      class="min-w-0 select-all overflow-x-auto whitespace-nowrap font-mono text-xs text-slate-700"
    >
      {{ command }}
    </code>
    <button
      type="button"
      class="table-action shrink-0"
      :aria-label="`复制 GM 指令：${command}`"
      @click="copyCommand"
    >
      <Copy class="size-3.5 shrink-0" aria-hidden="true" />
      复制
    </button>
  </div>
</template>

<script setup lang="ts">
  import { Copy } from "@lucide/vue";
  import { computed } from "vue";
  import { useToastsStore } from "@/stores/toasts";

  const props = defineProps<{ mapId: number; coordinate: number[] }>();
  const toasts = useToastsStore();
  const command = computed(
    () => `@moveTo ${props.mapId} ${props.coordinate[0]} ${props.coordinate[1]}`,
  );

  async function copyCommand(): Promise<void> {
    try {
      await navigator.clipboard.writeText(command.value);
      toasts.show("GM 指令已复制", "success");
    } catch {
      toasts.show("无法访问剪贴板，请手动选择指令复制", "warning");
    }
  }
</script>
