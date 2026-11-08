export type Coordinate = number[];
type Gate = { entrances: Coordinate[]; arrival: Coordinate };
type MapExit = { mapId: number; mapName: string; gates: Gate[] };
type Destination = {
  mapId: number;
  mapName: string;
  label: string;
  arrival: Coordinate;
  gold: number;
  level: number;
};
export type GroupedMap = {
  id: number;
  name: string;
  groupPath: string[];
};
export type GuideMap = GroupedMap & {
  exits: MapExit[];
  teleports: { name: string; location: Coordinate; destinations: Destination[] }[];
};
export type MapGroup = {
  key: string;
  label: string;
  count: number;
  children: MapGroup[];
  maps: GroupedMap[];
};

const collator = new Intl.Collator("zh-CN", { numeric: true });
const categories = ["城镇与野外", "洞穴与地宫", "矿区", "特殊场景", "未命名与其他"];

export function groupKeys(map: GroupedMap): string[] {
  return map.groupPath.map((_, index) => map.groupPath.slice(0, index + 1).join("/"));
}

export function groupMaps(maps: GroupedMap[]): MapGroup[] {
  const groups: MapGroup[] = [];
  for (const map of maps) {
    let siblings = groups;
    const keys = groupKeys(map);
    map.groupPath.forEach((label, index) => {
      const key = keys[index]!;
      let group = siblings.find((item) => item.key === key);
      if (!group) {
        group = { key, label, count: 0, children: [], maps: [] };
        siblings.push(group);
      }
      group.count += 1;
      if (index === map.groupPath.length - 1) group.maps.push(map);
      siblings = group.children;
    });
  }

  function sortGroups(nodes: MapGroup[]): void {
    nodes.sort((a, b) => collator.compare(a.label, b.label));
    for (const group of nodes) {
      group.maps.sort((a, b) => collator.compare(a.name, b.name) || a.id - b.id);
      sortGroups(group.children);
    }
  }
  sortGroups(groups);
  return groups.sort((a, b) => categories.indexOf(a.label) - categories.indexOf(b.label));
}
