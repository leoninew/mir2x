<template>
  <ToastProvider label="通知" swipe-direction="right">
    <ToastRoot
      v-for="toast in toasts.items"
      :key="toast.id"
      :duration="toast.duration"
      :type="toast.tone === 'error' ? 'foreground' : 'background'"
      class="app-toast flex items-start gap-3 rounded-lg border border-slate-200 bg-white p-4 shadow-lg"
      @update:open="
        (open) => {
          if (!open) toasts.dismiss(toast.id);
        }
      "
    >
      <svg
        viewBox="0 0 24 24"
        fill="none"
        stroke="currentColor"
        stroke-width="1.75"
        :class="['mt-0.5 size-5 shrink-0', colors[toast.tone]]"
        aria-hidden="true"
      >
        <circle cx="12" cy="12" r="9" />
        <path v-if="toast.tone === 'success'" d="m8 12 3 3 5-6" />
        <path v-else-if="toast.tone === 'error'" d="m9 9 6 6m0-6-6 6" />
        <path v-else d="M12 7v6m0 3h.01" />
      </svg>
      <div class="min-w-0 flex-1">
        <ToastTitle class="text-sm font-medium text-slate-950">{{ titles[toast.tone] }}</ToastTitle>
        <ToastDescription as-child>
          <p class="mt-1 text-sm leading-6 break-words whitespace-pre-wrap text-slate-600">
            {{ toast.message }}
          </p>
        </ToastDescription>
      </div>
      <ToastClose aria-label="关闭通知" class="filter-control -mt-1 -mr-1">
        <svg
          viewBox="0 0 16 16"
          fill="none"
          stroke="currentColor"
          class="size-4"
          aria-hidden="true"
        >
          <path d="m4 4 8 8m0-8-8 8" />
        </svg>
      </ToastClose>
    </ToastRoot>
    <ToastPortal>
      <ToastViewport
        label="通知（{hotkey}）"
        class="pointer-events-auto fixed top-4 right-4 z-[100] m-0 flex max-h-[calc(100dvh-2rem)] w-96 max-w-[calc(100vw-2rem)] list-none flex-col gap-3 overflow-y-auto p-0 outline-none"
      />
    </ToastPortal>
  </ToastProvider>
</template>

<script setup lang="ts">
  import {
    ToastProvider,
    ToastRoot,
    ToastTitle,
    ToastDescription,
    ToastClose,
    ToastPortal,
    ToastViewport,
  } from "reka-ui";
  import { useToastsStore } from "@/stores/toasts";

  const toasts = useToastsStore();
  const titles = { success: "成功", error: "操作失败", warning: "请注意", info: "提示" };
  const colors = {
    success: "text-emerald-600",
    error: "text-red-600",
    warning: "text-amber-600",
    info: "text-indigo-600",
  };
</script>
