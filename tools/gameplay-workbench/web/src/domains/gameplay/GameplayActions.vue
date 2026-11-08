<template>
  <div class="flex items-center gap-2">
    <button
      type="button"
      class="secondary-button"
      :disabled="!ready || busy"
      @click="$emit('refresh')"
    >
      <RefreshCw class="size-4 shrink-0" aria-hidden="true" />
      刷新
    </button>
    <button
      type="button"
      class="primary-button"
      :disabled="!ready || !dirty || busy"
      @click="$emit('save')"
    >
      <AppLoading v-if="busy" compact label="正在处理配置…" class="text-current" />
      <Save v-else class="size-4 shrink-0" aria-hidden="true" />
      保存
    </button>
  </div>
</template>
<script setup lang="ts">
  import { RefreshCw, Save } from "@lucide/vue";
  import AppLoading from "@/components/AppLoading.vue";
  defineProps<{
    busy: boolean;
    ready: boolean;
    dirty: boolean;
  }>();
  defineEmits<{ refresh: []; save: [] }>();
</script>
