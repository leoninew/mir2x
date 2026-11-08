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
      <component
        :is="icons[toast.tone]"
        :class="['mt-0.5 size-5 shrink-0', colors[toast.tone]]"
        aria-hidden="true"
      />
      <div class="min-w-0 flex-1">
        <ToastTitle class="text-sm font-medium text-slate-950">{{ titles[toast.tone] }}</ToastTitle>
        <ToastDescription as-child>
          <p class="mt-1 text-sm leading-6 break-words whitespace-pre-wrap text-slate-600">
            {{ toast.message }}
          </p>
        </ToastDescription>
      </div>
      <ToastClose aria-label="关闭通知" class="filter-control -mt-1 -mr-1">
        <X class="size-4" aria-hidden="true" />
      </ToastClose>
    </ToastRoot>
    <ToastPortal>
      <ToastViewport
        label="通知（{hotkey}）"
        class="pointer-events-auto fixed right-4 bottom-4 z-[100] m-0 flex max-h-[calc(100dvh-2rem)] w-96 max-w-[calc(100vw-2rem)] list-none flex-col gap-3 overflow-y-auto p-0 outline-none"
      />
    </ToastPortal>
  </ToastProvider>
</template>

<script setup lang="ts">
  import { CircleCheck, CircleX, Info, TriangleAlert, X } from "@lucide/vue";
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
  const icons = { success: CircleCheck, error: CircleX, warning: TriangleAlert, info: Info };
  const colors = {
    success: "text-emerald-600",
    error: "text-red-600",
    warning: "text-amber-600",
    info: "text-indigo-600",
  };
</script>
