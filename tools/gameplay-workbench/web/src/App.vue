<template>
  <RouterView />
  <AppToasts />
</template>

<script setup lang="ts">
  import { RouterView } from "vue-router";
  import AppToasts from "@/components/AppToasts.vue";
</script>
