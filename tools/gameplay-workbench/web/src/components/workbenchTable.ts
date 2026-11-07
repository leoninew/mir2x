export type TableColumn = { key: string; label: string; class?: string; required?: boolean };

export function matchesSearch(search: string, ...values: unknown[]): boolean {
  return values.join(" ").toLocaleLowerCase().includes(search.trim().toLocaleLowerCase());
}
