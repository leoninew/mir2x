import { flushPromises, mount } from "@vue/test-utils";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { reactive, type SetupContext } from "vue";
import { createPinia, setActivePinia } from "pinia";
import { useToastsStore } from "@/stores/toasts";

import IntegerField from "@/components/IntegerField.vue";
import { useGameplay } from "@/composables/useGameplay";
import GameplayPage from "@/domains/gameplay/GameplayPage.vue";
import type { WorkbenchPage } from "@/domains/gameplay/gameplayNavigation";
import { ApiClientError } from "@/services/api";
import {
  gameplayApi,
  validateNumbers,
  type Catalog,
  type ConfigFile,
  type Domain,
  type GameplayBackup,
} from "@/services/gameplayApi";

type Route = { meta: { workbenchPage: WorkbenchPage } };
const route = reactive<Route>({ meta: { workbenchPage: "monsters" } });
let navigationGuard: ((to: Route) => boolean | Promise<boolean>) | undefined;
const router = {
  push: vi.fn(async ({ name }: { name: WorkbenchPage }) => {
    const next = { meta: { workbenchPage: name } };
    if ((await navigationGuard?.(next)) !== false) route.meta.workbenchPage = name;
  }),
  replace: vi.fn(async ({ name }: { name: WorkbenchPage }) => {
    const next = { meta: { workbenchPage: name } };
    if ((await navigationGuard?.(next)) !== false) route.meta.workbenchPage = name;
  }),
};

vi.mock("vue-router", () => ({
  useRoute: () => route,
  useRouter: () => router,
  onBeforeRouteUpdate: (guard: typeof navigationGuard) => {
    navigationGuard = guard;
  },
  onBeforeRouteLeave: (guard: typeof navigationGuard) => {
    navigationGuard = guard;
  },
  RouterLink: {
    props: ["to", "custom"],
    setup(props: { to: { name: WorkbenchPage } }, { slots }: SetupContext) {
      return () =>
        slots.default?.({
          href: `/${props.to.name}`,
          navigate: (event: MouseEvent) => {
            event.preventDefault();
            return router.push(props.to);
          },
        });
    },
  },
}));

function file(domain: Domain = "monsters", id: number | null = null): ConfigFile {
  return {
    workspace_id: "workspace",
    catalog_revision: "catalog",
    revision: "old-revision",
    domain,
    object_id: id,
    data:
      domain === "monsters"
        ? { schema_version: 1, monsters: [{ monster_id: 1, monster_name: "鹿", exp: 10 }] }
        : domain === "drops"
          ? {
              schema_version: 1,
              monster_id: id ?? 1,
              monster_name: "鹿",
              items: [
                { item_id: 2, item_name: "金币", group: 0, prob_recip: 1, repeat: 1, count: 2 },
              ],
            }
          : domain === "multipliers"
            ? { schema_version: 1, experience: 1, gold: 1, drop: 1 }
            : { schema_version: 1, map_id: 3, map_name: "道馆", groups: [] },
  };
}

const catalog: Catalog = {
  workspace_id: "workspace",
  catalog_revision: "catalog",
  catalog: {
    monsters: [{ id: 1, name: "鹿" }],
    items: [{ id: 2, name: "金币" }],
    maps: [{ id: 3, name: "道馆" }],
  },
  manifest: { schema_version: 1, drop_monsters: [1], spawn_maps: [3] },
  migration_notes: { records: [] },
  fields: {},
};

beforeEach(() => {
  setActivePinia(createPinia());
  navigationGuard = undefined;
  route.meta.workbenchPage = "monsters";
  router.push.mockClear();
  router.replace.mockClear();
  vi.spyOn(gameplayApi, "workspace").mockResolvedValue({
    bound: true,
    workspace_id: "workspace",
    server_root: "server",
  });
  vi.spyOn(gameplayApi, "catalog").mockResolvedValue(structuredClone(catalog));
  vi.spyOn(gameplayApi, "read").mockImplementation(async (_workspace, domain, id) =>
    file(domain, id),
  );
  vi.spyOn(gameplayApi, "validate").mockResolvedValue({ valid: true, issues: [] });
  vi.spyOn(gameplayApi, "backups").mockResolvedValue([]);
  vi.spyOn(gameplayApi, "save").mockImplementation(async (payload) => ({
    ...payload,
    revision: "new-revision",
  }));
});
afterEach(() => vi.restoreAllMocks());

describe("gameplay editing", () => {
  it("preserves drafts for all drop files and validates every changed file before saving", async () => {
    const state = useGameplay();
    await state.refreshWorkspace();
    state.catalog.value!.manifest.drop_monsters = [1, 2];
    await state.load("drops");
    expect(state.dropDrafts.value.map((data) => data.monster_id)).toEqual([1, 2]);
    state.dropDrafts.value[0].items[0].count = 5;
    state.dropDrafts.value[1].items[0].count = 0;
    await state.load("monsters");
    await state.load("drops");
    expect(state.dropDrafts.value.map((data) => data.items[0].count)).toEqual([5, 0]);
    await state.save();
    expect(state.errors.value["2.items.0.count"]).toBeDefined();
    expect(gameplayApi.save).not.toHaveBeenCalled();
    state.dropDrafts.value[1].items[0].count = 6;
    vi.mocked(gameplayApi.validate)
      .mockResolvedValueOnce({ valid: true, issues: [] })
      .mockResolvedValueOnce({
        valid: false,
        issues: [{ path: "items.0.count", message: "数量错误" }],
      });
    await state.save();
    expect(state.errors.value["2.items.0.count"]).toBe("数量错误");
    expect(gameplayApi.save).not.toHaveBeenCalled();
    await state.save();
    expect(vi.mocked(gameplayApi.save).mock.calls.map(([payload]) => payload.object_id)).toEqual([
      1, 2,
    ]);
    expect(state.dirty.value).toBe(false);
  });

  it("retains only unsaved changes when saving a later file conflicts", async () => {
    const state = useGameplay();
    await state.refreshWorkspace();
    state.catalog.value!.manifest.drop_monsters = [1, 2];
    await state.load("drops");
    state.dropDrafts.value.forEach((data) => {
      data.items[0].count = 8;
    });
    vi.mocked(gameplayApi.save)
      .mockImplementationOnce(async (payload) => ({ ...payload, revision: "saved" }))
      .mockRejectedValueOnce(new ApiClientError("File changed", "http_error", { status: 409 }));
    await state.save();
    expect(state.base.value?.revision).toBe("saved");
    expect(state.changes.value.map((change) => change.path)).toEqual(["2.items.0.count"]);
    expect(useToastsStore().items.at(-1)?.message).toContain("已保存 1 个配置");
    expect(state.conflict.value).toBe(true);
  });

  it("loads the editor selected by the initial route", async () => {
    route.meta.workbenchPage = "multipliers";
    const wrapper = mount(GameplayPage);
    await flushPromises();
    expect(wrapper.get("h1").text()).toBe("全局倍率");
    expect(wrapper.find("#experience").exists()).toBe(true);
    expect(wrapper.get(".table-toolbar").text()).toContain("刷新");
    expect(wrapper.get(".table-toolbar").text()).toContain("保存");
    wrapper.unmount();
  });

  it("preserves drafts across running, backups and other editors", async () => {
    const wrapper = mount(GameplayPage);
    await flushPromises();
    await wrapper.get('input[aria-label="鹿 基础经验"]').setValue("25");
    for (const label of ["游戏运行", "配置备份", "全局倍率", "基础经验"]) {
      await wrapper
        .get('nav[aria-label="工作台导航"]')
        .findAll("a")
        .find((button) => button.text() === label)!
        .trigger("click");
      await flushPromises();
    }
    expect((wrapper.get('input[aria-label="鹿 基础经验"]').element as HTMLInputElement).value).toBe(
      "25",
    );
    await wrapper
      .findAll("button")
      .find((button) => button.text() === "保存")!
      .trigger("click");
    await flushPromises();
    expect(gameplayApi.save).toHaveBeenCalledWith(
      expect.objectContaining({
        domain: "monsters",
        data: { schema_version: 1, monsters: [{ monster_id: 1, monster_name: "鹿", exp: 25 }] },
      }),
    );
    wrapper.unmount();
  });

  it("shows a clear empty search result", async () => {
    const wrapper = mount(GameplayPage);
    await flushPromises();
    await wrapper.get('input[placeholder="名称或 ID"]').setValue("不存在的怪物");
    expect(wrapper.text()).toContain("没有匹配的怪物");
    expect(wrapper.text()).toContain("0 / 1 个怪物");
    wrapper.unmount();
  });

  it("keeps restore confirmation and reloads the editor after restoring", async () => {
    const backup: GameplayBackup = {
      backup_id: "backup",
      name: "2026-11-07 08:00:00",
      created_at: "2026-11-07T08:00:00+08:00",
      description: "调整前",
      available: true,
      source_server_root: "server",
      source_matches: true,
    };
    vi.mocked(gameplayApi.backups).mockResolvedValue([backup]);
    const restore = vi
      .spyOn(gameplayApi, "restoreBackup")
      .mockResolvedValue({ safety_backup: { ...backup, name: "还原前备份" } });
    const wrapper = mount(GameplayPage, { attachTo: document.body });
    await flushPromises();
    await wrapper.get('input[aria-label="鹿 基础经验"]').setValue("25");
    await wrapper
      .findAll("nav a")
      .find((button) => button.text() === "配置备份")!
      .trigger("click");
    await flushPromises();
    await wrapper
      .findAll("button")
      .find((button) => button.text() === "还原")!
      .trigger("click");
    await flushPromises();
    expect(document.body.textContent).toContain("未保存草稿将被丢弃");
    expect(restore).not.toHaveBeenCalled();
    (
      Array.from(document.body.querySelectorAll("button")).find(
        (button) => button.textContent?.trim() === "确认还原",
      ) as HTMLButtonElement
    ).click();
    await flushPromises();
    expect(restore).toHaveBeenCalledWith("workspace", "backup");
    expect(wrapper.get("h1").text()).toBe("基础经验");
    expect((wrapper.get('input[aria-label="鹿 基础经验"]').element as HTMLInputElement).value).toBe(
      "10",
    );
    expect(useToastsStore().items.at(-1)?.message).toContain("还原前备份");
    wrapper.unmount();
  });

  it("edits independent decimal multipliers including zero with local field errors", async () => {
    const wrapper = mount(GameplayPage);
    await flushPromises();
    await wrapper
      .findAll("nav a")
      .find((button) => button.text() === "全局倍率")!
      .trigger("click");
    await flushPromises();
    expect(wrapper.find('input[placeholder="名称或 ID"]').exists()).toBe(false);
    await wrapper.get("#experience").setValue("1.5");
    await wrapper.get("#gold").setValue("0");
    await wrapper.get("#drop").setValue("");
    const save = wrapper.findAll("button").find((button) => button.text() === "保存")!;
    await save.trigger("click");
    await flushPromises();
    expect(wrapper.get("#drop").attributes("aria-invalid")).toBe("true");
    expect(gameplayApi.save).not.toHaveBeenCalled();
    await wrapper.get("#drop").setValue("0.25");
    await save.trigger("click");
    await flushPromises();
    expect(gameplayApi.save).toHaveBeenCalledWith(
      expect.objectContaining({
        domain: "multipliers",
        object_id: null,
        data: { schema_version: 1, experience: 1.5, gold: 0, drop: 0.25 },
      }),
    );
    wrapper.unmount();
  });

  it("keeps empty and out-of-range inputs as field errors without sending", async () => {
    const wrapper = mount(GameplayPage);
    await flushPromises();
    await wrapper.get('input[aria-label="鹿 基础经验"]').setValue("");
    await wrapper
      .findAll("button")
      .find((button) => button.text() === "保存")!
      .trigger("click");
    await flushPromises();
    expect(wrapper.get('input[aria-label="鹿 基础经验"]').attributes("aria-invalid")).toBe("true");
    expect(wrapper.find(".field-error").text()).toContain("整数");
    expect(gameplayApi.save).not.toHaveBeenCalled();
    await wrapper.get('input[aria-label="鹿 基础经验"]').setValue("20");
    expect(wrapper.get('input[aria-label="鹿 基础经验"]').attributes("aria-invalid")).toBe("false");
    wrapper.unmount();
  });

  it("keeps per-file drafts and preserves a conflict without replacing the baseline", async () => {
    const state = useGameplay();
    await state.refreshWorkspace();
    if (!state.draft.value || !("monsters" in state.draft.value))
      throw new Error("missing fixture");
    state.draft.value.monsters[0].exp = 25;
    await state.load("drops", 1);
    await state.load("monsters");
    expect(state.draft.value).toEqual({
      ...file().data,
      monsters: [{ monster_id: 1, monster_name: "鹿", exp: 25 }],
    });
    vi.mocked(gameplayApi.save).mockRejectedValueOnce(
      new ApiClientError("File changed", "http_error", { status: 409 }),
    );
    await state.save();
    expect(state.conflict.value).toBe(true);
    expect(state.dirty.value).toBe(true);
    expect(state.base.value?.revision).toBe("old-revision");
    expect(useToastsStore().items.at(-1)).toMatchObject({
      message: expect.stringContaining("草稿已保留"),
      tone: "warning",
    });
  });

  it("shows a successful save and uses the returned file revision", async () => {
    const state = useGameplay();
    await state.refreshWorkspace();
    if (!state.draft.value || !("monsters" in state.draft.value))
      throw new Error("missing fixture");
    state.draft.value.monsters[0].exp = 30;
    await state.save();
    expect(state.dirty.value).toBe(false);
    expect(state.base.value?.revision).toBe("new-revision");
    expect(useToastsStore().items.at(-1)).toMatchObject({
      message: expect.stringContaining("下次启动"),
      tone: "success",
    });
  });

  it("checks integer constraints without converting invalid values", () => {
    const data = file("drops", 1).data;
    if (!("items" in data)) throw new Error("missing fixture");
    data.items[0].prob_recip = 0;
    data.items[0].count = 1.5;
    expect(Object.keys(validateNumbers(data))).toEqual(["items.0.prob_recip", "items.0.count"]);
  });

  it("forgets a cached draft after the user restores its original values", async () => {
    const state = useGameplay();
    await state.refreshWorkspace();
    if (!state.draft.value || !("monsters" in state.draft.value))
      throw new Error("missing fixture");
    state.draft.value.monsters[0].exp = 25;
    await state.load("drops", 1);
    await state.load("monsters");
    if (!state.draft.value || !("monsters" in state.draft.value))
      throw new Error("missing fixture");
    state.draft.value.monsters[0].exp = 10;
    await state.load("drops", 1);
    await state.load("monsters");
    expect(state.dirty.value).toBe(false);
    expect(state.draft.value).toEqual(file().data);
  });

  it("associates an integer error with its real input", () => {
    const wrapper = mount(IntegerField, {
      props: { modelValue: -1, path: "items.0.count", label: "数量", error: "请输入正整数" },
    });
    const input = wrapper.get("input");
    expect(input.attributes("aria-describedby")).toBe("items.0.count-error");
    expect(wrapper.get("#items\\.0\\.count-error").text()).toBe("请输入正整数");
    wrapper.unmount();
  });
});
