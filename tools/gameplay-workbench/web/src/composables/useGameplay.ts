import { computed, reactive, ref, toRaw } from "vue";

import { ApiClientError } from "@/services/api";
import {
  changesBetween,
  gameplayApi,
  validateNumbers,
  type Catalog,
  type ConfigFile,
  type Domain,
  type GameplayData,
  type Workspace,
} from "@/services/gameplayApi";

function clone<T>(value: T): T {
  return structuredClone(toRaw(value));
}

export function useGameplay() {
  const workspace = ref<Workspace | null>(null);
  const catalog = ref<Catalog | null>(null);
  const base = ref<ConfigFile | null>(null);
  const draft = ref<GameplayData | null>(null);
  const domain = ref<Domain>("monsters");
  const objectId = ref<number | null>(null);
  const busy = ref(false);
  const message = ref("");
  const conflict = ref(false);
  const errors = ref<Record<string, string>>({});
  const drafts = reactive(new Map<string, { base: ConfigFile; draft: GameplayData }>());
  let generation = 0;
  const changes = computed(() => changesBetween(base.value?.data, draft.value));
  const dirty = computed(() => base.value !== null && changes.value.length > 0);
  const key = (file: ConfigFile) =>
    `${workspace.value?.server_root}/${file.domain}/${file.object_id ?? "all"}`;
  const hasUnsavedDrafts = computed(
    () =>
      dirty.value ||
      [...drafts.keys()].some((cachedKey) => !base.value || cachedKey !== key(base.value)),
  );

  function remember(): void {
    if (base.value && draft.value) {
      if (dirty.value)
        drafts.set(key(base.value), { base: clone(base.value), draft: clone(draft.value) });
      else drafts.delete(key(base.value));
    }
  }

  function report(error: unknown): void {
    conflict.value = error instanceof ApiClientError && error.status === 409;
    message.value = error instanceof Error ? error.message : "操作失败";
    if (conflict.value) message.value += "。草稿已保留，可下载后重新读取。";
  }

  async function load(
    nextDomain: Domain,
    nextId: number | null = null,
    reload = false,
  ): Promise<void> {
    remember();
    const current = ++generation;
    const workspaceId = workspace.value?.workspace_id;
    if (!workspaceId) return;
    busy.value = true;
    errors.value = {};
    conflict.value = false;
    message.value = "";
    try {
      const cached = drafts.get(`${workspace.value?.server_root}/${nextDomain}/${nextId ?? "all"}`);
      const file =
        cached && !reload ? cached.base : await gameplayApi.read(workspaceId, nextDomain, nextId);
      if (current !== generation) return;
      domain.value = nextDomain;
      objectId.value = nextId;
      base.value = clone(file);
      base.value.workspace_id = workspaceId;
      draft.value = clone(cached && !reload ? cached.draft : file.data);
      if (reload) drafts.delete(key(file));
    } catch (error) {
      if (current === generation) report(error);
    } finally {
      if (current === generation) busy.value = false;
    }
  }

  async function refreshWorkspace(): Promise<void> {
    remember();
    const current = ++generation;
    busy.value = true;
    try {
      const next = await gameplayApi.workspace();
      if (current !== generation) return;
      const nextCatalog = next.workspace_id ? await gameplayApi.catalog(next.workspace_id) : null;
      if (current !== generation) return;
      workspace.value = next;
      base.value = null;
      draft.value = null;
      catalog.value = nextCatalog;
      if (next.workspace_id) await load("monsters");
    } catch (error) {
      if (current === generation) report(error);
    } finally {
      if (current === generation) busy.value = false;
    }
  }

  async function save(): Promise<void> {
    if (!base.value || !draft.value || busy.value) return;
    errors.value = validateNumbers(draft.value);
    if (Object.keys(errors.value).length) {
      message.value = "请修正标记的字段后再保存";
      return;
    }
    busy.value = true;
    const current = generation;
    message.value = "";
    conflict.value = false;
    try {
      const payload = { ...clone(base.value), data: clone(draft.value) };
      const result = await gameplayApi.validate(payload);
      if (current !== generation) return;
      if (!result.valid) {
        errors.value = Object.fromEntries(
          result.issues.map((issue) => [issue.path, issue.message]),
        );
        message.value = "校验未通过，请检查字段";
        return;
      }
      const saved = await gameplayApi.save(payload);
      if (current !== generation) return;
      drafts.delete(key(saved));
      base.value = clone(saved);
      draft.value = clone(saved.data);
      message.value = "已保存，下次启动游戏服务端生效";
    } catch (error) {
      if (current === generation) {
        remember();
        report(error);
      }
    } finally {
      if (current === generation) busy.value = false;
    }
  }

  return {
    workspace,
    catalog,
    base,
    draft,
    domain,
    objectId,
    busy,
    message,
    conflict,
    errors,
    changes,
    dirty,
    hasUnsavedDrafts,
    load,
    refreshWorkspace,
    save,
    clearError: (path: string) => {
      delete errors.value[path];
    },
  };
}
