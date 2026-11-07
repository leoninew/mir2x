<template>
  <footer
    class="flex shrink-0 flex-wrap items-center justify-between gap-3 rounded-lg border border-slate-200 bg-white p-4"
  >
    <p class="text-sm text-slate-600" aria-live="polite">
      {{
        busy
          ? "处理中…"
          : dirty
            ? `${changeCount} 处未保存`
            : hasUnsavedDrafts
              ? "当前文件已保存，其他页面有草稿"
              : "所有修改已保存"
      }}
    </p>
    <div class="flex flex-wrap gap-2">
      <button
        type="button"
        class="secondary-button"
        :disabled="!ready || busy"
        @click="$emit('download')"
      >
        下载草稿
      </button>
      <button
        type="button"
        class="secondary-button"
        :disabled="!ready || busy"
        @click="$emit('reload')"
      >
        重新读取
      </button>
      <button
        type="button"
        class="primary-button"
        :disabled="!dirty || busy"
        @click="$emit('save')"
      >
        校验并保存当前文件
      </button>
    </div>
  </footer>
</template>
<script setup lang="ts">
  defineProps<{
    busy: boolean;
    ready: boolean;
    dirty: boolean;
    hasUnsavedDrafts: boolean;
    changeCount: number;
  }>();
  defineEmits<{ download: []; reload: []; save: [] }>();
</script>
