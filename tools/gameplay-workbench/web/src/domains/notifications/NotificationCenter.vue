<template>
  <section
    aria-labelledby="notifications-title"
    class="mx-auto flex h-dvh max-w-6xl flex-col gap-5 p-6"
  >
    <header class="shrink-0">
      <h1 id="notifications-title" class="text-xl font-semibold text-slate-950">Notifications</h1>
      <p class="mt-2 text-sm text-slate-500">{{ unreadCount }} unread notifications</p>
    </header>
    <WorkbenchTable
      v-model:search="search"
      label="Notifications"
      search-label="Search notifications"
      search-placeholder="Search notifications"
      count-label="notifications"
      :columns="columns"
      :rows="rows"
      :total="notifications.items.length"
      :row-key="(notification) => notification.id"
      :empty-text="
        activeFilter === 'unread' && !search
          ? 'No unread notifications.'
          : 'No matching notifications.'
      "
      table-class="min-w-[560px]"
    >
      <template #filters>
        <FilterCombobox
          v-model="activeFilter"
          label="Notification filter"
          :options="[
            { value: 'unread', label: 'Unread' },
            { value: 'read', label: 'Read' },
          ]"
        />
      </template>
      <template #actions>
        <button
          type="button"
          class="secondary-button"
          :disabled="unreadCount === 0"
          @click="notifications.markAllRead"
        >
          Mark all read
        </button>
      </template>
      <template #cell-state="{ row: notification }">
        <span
          :class="[
            'table-status',
            notification.unread ? 'bg-indigo-50 text-indigo-700' : 'bg-slate-100 text-slate-500',
          ]"
        >
          {{ notification.unread ? "Unread" : "Read" }}
        </span>
      </template>
      <template #cell-actions="{ row: notification }">
        <button
          v-if="notification.unread"
          type="button"
          class="table-action"
          @click="notifications.markRead(notification.id)"
        >
          Mark read
        </button>
        <span v-else class="text-slate-400">—</span>
      </template>
    </WorkbenchTable>
  </section>
</template>

<script setup lang="ts">
  import { storeToRefs } from "pinia";
  import { computed, ref } from "vue";
  import WorkbenchTable from "@/components/WorkbenchTable.vue";
  import FilterCombobox from "@/components/FilterCombobox.vue";
  import { matchesSearch, type TableColumn } from "@/components/workbenchTable";
  import { useNotificationsStore } from "@/stores/notifications";

  const activeFilter = ref<string | number | null>(null);
  const search = ref("");
  const notifications = useNotificationsStore();
  const { unreadCount } = storeToRefs(notifications);
  const columns: TableColumn[] = [
    { key: "title", label: "Notification" },
    { key: "state", label: "Status", class: "w-28" },
    { key: "actions", label: "Actions", class: "w-32" },
  ];
  const rows = computed(() =>
    notifications.items.filter(
      (notification) =>
        (activeFilter.value === null ||
          notification.unread === (activeFilter.value === "unread")) &&
        matchesSearch(search.value, notification.title),
    ),
  );
</script>
