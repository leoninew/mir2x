import { apiClient } from "@/services/api";

export type Domain = "monsters" | "drops" | "spawns";
export type IdentityRecord = { id: number; name: string };
export type Monster = { monster_id: number; monster_name: string; exp: number };
export type DropItem = {
  item_id: number;
  item_name: string;
  group: number;
  prob_recip: number;
  repeat: number;
  count: number;
};
export type Region = {
  x: number;
  y: number;
  w: number;
  h: number;
  count: number;
  time: number;
  cratio?: number;
};
export type SpawnGroup = { monster_id: number; monster_name: string; loc: Region[] };
export type Monsters = { schema_version: number; monsters: Monster[] };
export type Drops = {
  schema_version: number;
  monster_id: number;
  monster_name: string;
  items: DropItem[];
};
export type Spawns = {
  schema_version: number;
  map_id: number;
  map_name: string;
  groups: SpawnGroup[];
};
export type GameplayData = Monsters | Drops | Spawns;
export type Workspace = { bound: boolean; workspace_id: string | null; server_root: string | null };
export type GameplayBackup = {
  backup_id: string;
  name: string;
  created_at: string;
  description: string;
  available: boolean;
  source_server_root: string;
  source_matches: boolean;
};
export type Catalog = {
  workspace_id: string;
  catalog_revision: string;
  catalog: { monsters: IdentityRecord[]; items: IdentityRecord[]; maps: IdentityRecord[] };
  manifest: { schema_version: number; drop_monsters: number[]; spawn_maps: number[] };
  migration_notes: {
    records: {
      domain: string;
      source_path: string;
      source_position: number;
      reason: string;
      raw: unknown;
    }[];
  };
  fields: Record<string, string>;
};
export type ConfigFile = {
  workspace_id: string;
  domain: Domain;
  object_id: number | null;
  catalog_revision: string;
  revision: string;
  data: GameplayData;
};
export type Issue = { path: string; message: string };

export const gameplayApi = {
  async workspace(): Promise<Workspace> {
    return (await apiClient.get<Workspace>("/api/gameplay/workspace")).data;
  },
  async catalog(workspaceId: string): Promise<Catalog> {
    return (
      await apiClient.get<Catalog>("/api/gameplay/catalog", {
        params: { workspace_id: workspaceId },
      })
    ).data;
  },
  async read(workspaceId: string, domain: Domain, objectId: number | null): Promise<ConfigFile> {
    return (
      await apiClient.get<ConfigFile>("/api/gameplay/config", {
        params: { workspace_id: workspaceId, domain, object_id: objectId ?? undefined },
      })
    ).data;
  },
  async validate(file: ConfigFile): Promise<{ valid: boolean; issues: Issue[] }> {
    const { workspace_id, domain, object_id, catalog_revision, data } = file;
    return (
      await apiClient.post<{ valid: boolean; issues: Issue[] }>("/api/gameplay/config/validate", {
        workspace_id,
        domain,
        object_id,
        catalog_revision,
        data,
      })
    ).data;
  },
  async save(file: ConfigFile): Promise<ConfigFile> {
    return (await apiClient.put<ConfigFile>("/api/gameplay/config", file)).data;
  },
  async backups(workspaceId: string): Promise<GameplayBackup[]> {
    return (
      await apiClient.get<{ backups: GameplayBackup[] }>("/api/gameplay/backups", {
        params: { workspace_id: workspaceId },
      })
    ).data.backups;
  },
  async createBackup(workspaceId: string, description: string): Promise<GameplayBackup> {
    return (
      await apiClient.post<GameplayBackup>("/api/gameplay/backups", {
        workspace_id: workspaceId,
        description,
      })
    ).data;
  },
  async restoreBackup(
    workspaceId: string,
    backupId: string,
  ): Promise<{ safety_backup: GameplayBackup }> {
    return (
      await apiClient.post<{ safety_backup: GameplayBackup }>(
        `/api/gameplay/backups/${backupId}/restore`,
        { workspace_id: workspaceId },
      )
    ).data;
  },
};

export function validateNumbers(data: GameplayData): Record<string, string> {
  const errors: Record<string, string> = {};
  function number(value: number, path: string, minimum: number): void {
    if (!Number.isInteger(value) || value < minimum || value > 2147483647) {
      errors[path] = `请输入 ${minimum} 到 2147483647 的整数`;
    }
  }
  if ("monsters" in data) {
    data.monsters.forEach((row, index) => number(row.exp, `monsters.${index}.exp`, 0));
  } else if ("items" in data) {
    data.items.forEach((row, index) => {
      number(row.group, `items.${index}.group`, 0);
      (["prob_recip", "repeat", "count"] as const).forEach((field) =>
        number(row[field], `items.${index}.${field}`, 1),
      );
    });
  } else {
    data.groups.forEach((group, index) =>
      group.loc.forEach((row, region) => {
        const prefix = `groups.${index}.loc.${region}`;
        (["x", "y"] as const).forEach((field) =>
          number(row[field], `${prefix}.${field}`, -2147483648),
        );
        (["w", "h", "time"] as const).forEach((field) =>
          number(row[field], `${prefix}.${field}`, 1),
        );
        number(row.count, `${prefix}.count`, 0);
      }),
    );
  }
  return errors;
}

export type Change = { path: string; before: string; after: string };
export function changesBetween(before: unknown, after: unknown, path = ""): Change[] {
  if (Object.is(before, after)) return [];
  if (before && after && typeof before === "object" && typeof after === "object") {
    const old = before as Record<string, unknown>;
    const next = after as Record<string, unknown>;
    return [...new Set([...Object.keys(old), ...Object.keys(next)])].flatMap((key) =>
      changesBetween(old[key], next[key], path ? `${path}.${key}` : key),
    );
  }
  return [{ path, before: String(before), after: String(after) }];
}
