import { defineStore } from "pinia";

export type ToastTone = "success" | "error" | "warning" | "info";
export type Toast = { id: number; message: string; tone: ToastTone; duration: number };

export const useToastsStore = defineStore("toasts", {
  state: () => ({ items: [] as Toast[], sequence: 0 }),
  actions: {
    show(message: string, tone: ToastTone = "info"): void {
      if (!message.trim()) return;
      this.items = this.items.filter((item) => item.message !== message || item.tone !== tone);
      this.items.push({
        id: ++this.sequence,
        message,
        tone,
        duration: tone === "error" ? 8000 : tone === "warning" ? 6000 : 4000,
      });
      this.items = this.items.slice(-5);
    },
    dismiss(id: number): void {
      this.items = this.items.filter((item) => item.id !== id);
    },
  },
});
