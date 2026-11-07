import { computed, reactive, ref, toRaw } from "vue";

import { ApiClientError } from "@/services/api";
import { useToastsStore } from "@/stores/toasts";
import {
  changesBetween,
  gameplayApi,
  validateNumbers,
  type Catalog,
  type ConfigFile,
  type Domain,
  type GameplayData,
  type Workspace,
  type Drops,
  type Spawns,
} from "@/services/gameplayApi";

function clone<T>(value: T): T {
  return structuredClone(toRaw(value));
}

export function useGameplay() {
  const toasts = useToastsStore();
  const workspace = ref<Workspace | null>(null);
  const catalog = ref<Catalog | null>(null);
  type Document = { base: ConfigFile; draft: GameplayData };
  const documents = ref<Document[]>([]);
  const base = computed(() => documents.value[0]?.base ?? null);
  const draft = computed({
    get: () => documents.value[0]?.draft ?? null,
    set: (value: GameplayData | null) => {
      if (value && documents.value[0]) documents.value[0].draft = value;
    },
  });
  const dropDrafts = computed(() =>
    documents.value
      .map((document) => document.draft)
      .filter((data): data is Drops => "items" in data),
  );
  const spawnDrafts = computed(() =>
    documents.value
      .map((document) => document.draft)
      .filter((data): data is Spawns => "groups" in data),
  );
  const domain = ref<Domain>("monsters");
  const objectId = ref<number | null>(null);
  const busy = ref(false);
  const activity = ref("正在加载配置…");
  const conflict = ref(false);
  const errors = ref<Record<string, string>>({});
  const drafts = reactive(new Map<string, { base: ConfigFile; draft: GameplayData }>());
  let generation = 0;
  const fieldPath = (file: ConfigFile, path: string) =>
    objectId.value === null && (file.domain === "drops" || file.domain === "spawns")
      ? `${file.object_id}.${path}`
      : path;
  const changes = computed(() =>
    documents.value.flatMap((document) =>
      changesBetween(document.base.data, document.draft).map((change) => ({
        ...change,
        path: fieldPath(document.base, change.path),
      })),
    ),
  );
  const dirty = computed(() => base.value !== null && changes.value.length > 0);
  const key = (file: ConfigFile) =>
    `${workspace.value?.server_root}/${file.domain}/${file.object_id ?? "all"}`;
  const hasUnsavedDrafts = computed(
    () =>
      dirty.value ||
      [...drafts.keys()].some(
        (cachedKey) => !documents.value.some((document) => cachedKey === key(document.base)),
      ),
  );

  function remember(): void {
    for (const document of documents.value) {
      if (changesBetween(document.base.data, document.draft).length)
        drafts.set(key(document.base), clone(document));
      else drafts.delete(key(document.base));
    }
  }

  function report(error: unknown, prefix = ""): void {
    conflict.value = error instanceof ApiClientError && error.status === 409;
    const message = error instanceof Error ? error.message : "操作失败";
    toasts.show(
      prefix + message + (conflict.value ? "。草稿已保留，请核对外部修改后再刷新。" : ""),
      conflict.value ? "warning" : "error",
    );
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
    activity.value = "正在加载配置…";
    errors.value = {};
    conflict.value = false;
    try {
      const ids =
        nextId === null && (nextDomain === "drops" || nextDomain === "spawns")
          ? ((nextDomain === "drops"
              ? catalog.value?.manifest.drop_monsters
              : catalog.value?.manifest.spawn_maps) ?? [])
          : [nextId];
      const loaded: Document[] = [];
      for (let offset = 0; offset < ids.length; offset += 8) {
        const batch = await Promise.all(
          ids.slice(offset, offset + 8).map(async (id) => {
            const cached = drafts.get(
              `${workspace.value?.server_root}/${nextDomain}/${id ?? "all"}`,
            );
            const file =
              cached && !reload ? cached.base : await gameplayApi.read(workspaceId, nextDomain, id);
            return {
              base: { ...clone(file), workspace_id: workspaceId },
              draft: clone(cached && !reload ? cached.draft : file.data),
            };
          }),
        );
        if (current !== generation) return;
        loaded.push(...batch);
      }
      if (current !== generation) return;
      domain.value = nextDomain;
      objectId.value = nextId;
      documents.value = loaded;
      if (reload) loaded.forEach((document) => drafts.delete(key(document.base)));
    } catch (error) {
      if (current === generation) report(error);
    } finally {
      if (current === generation) busy.value = false;
    }
  }

  async function refreshWorkspace(discardDrafts = false): Promise<boolean> {
    if (discardDrafts) {
      drafts.clear();
      workspace.value = null;
      documents.value = [];
      catalog.value = null;
    } else {
      remember();
    }
    const current = ++generation;
    busy.value = true;
    activity.value = "正在打开配置目录…";
    conflict.value = false;
    errors.value = {};
    try {
      const next = await gameplayApi.workspace();
      if (current !== generation) return false;
      const nextCatalog = next.workspace_id ? await gameplayApi.catalog(next.workspace_id) : null;
      if (current !== generation) return false;
      workspace.value = next;
      documents.value = [];
      catalog.value = nextCatalog;
      if (next.workspace_id) await load("monsters");
      return true;
    } catch (error) {
      if (current === generation) report(error);
      return false;
    } finally {
      if (current === generation) busy.value = false;
    }
  }

  async function save(): Promise<void> {
    if (!documents.value.length || busy.value) return;
    const pending = documents.value.filter(
      (document) => changesBetween(document.base.data, document.draft).length,
    );
    if (!pending.length) return;
    errors.value = Object.fromEntries(
      pending.flatMap((document) =>
        Object.entries(validateNumbers(document.draft)).map(([path, error]) => [
          fieldPath(document.base, path),
          error,
        ]),
      ),
    );
    if (Object.keys(errors.value).length) {
      toasts.show("请修正标记的字段后再保存", "warning");
      return;
    }
    busy.value = true;
    activity.value = "正在校验并保存配置…";
    const current = generation;
    let savedCount = 0;
    conflict.value = false;
    try {
      const payloads = pending.map((document) => ({
        ...clone(document.base),
        data: clone(document.draft),
      }));
      for (const payload of payloads) {
        const result = await gameplayApi.validate(payload);
        if (current !== generation) return;
        for (const issue of result.issues)
          errors.value[fieldPath(payload, issue.path)] = issue.message;
        if (!result.valid && !result.issues.length) {
          toasts.show("校验未通过，请检查配置", "warning");
          return;
        }
      }
      if (Object.keys(errors.value).length) {
        toasts.show("校验未通过，请检查字段", "warning");
        return;
      }
      for (const [index, payload] of payloads.entries()) {
        const saved = await gameplayApi.save(payload);
        if (current !== generation) return;
        drafts.delete(key(saved));
        pending[index].base = clone(saved);
        pending[index].draft = clone(saved.data);
        savedCount++;
      }
      toasts.show("已保存，下次启动游戏服务端生效", "success");
    } catch (error) {
      if (current === generation) {
        remember();
        report(error, savedCount ? `已保存 ${savedCount} 个配置，其余修改已保留。` : "");
      }
    } finally {
      if (current === generation) busy.value = false;
    }
  }

  async function reload(): Promise<void> {
    if (busy.value || !workspace.value?.workspace_id) return;
    const current = ++generation;
    busy.value = true;
    activity.value = "正在刷新配置…";
    try {
      const nextCatalog = await gameplayApi.catalog(workspace.value.workspace_id);
      if (current !== generation) return;
      catalog.value = nextCatalog;
      await load(domain.value, objectId.value, true);
    } catch (error) {
      if (current === generation) report(error);
    } finally {
      if (current === generation) busy.value = false;
    }
  }

  return {
    workspace,
    catalog,
    base,
    draft,
    dropDrafts,
    spawnDrafts,
    domain,
    objectId,
    busy,
    activity,
    conflict,
    errors,
    changes,
    dirty,
    hasUnsavedDrafts,
    load,
    refreshWorkspace,
    reload,
    save,
    clearError: (path: string) => {
      delete errors.value[path];
    },
  };
}
