import tkinter as tk

from app.config.theme import COLORS


class StatusBadge(tk.Frame):
    def __init__(self, master):
        super().__init__(master, bg=COLORS["panel"])
        self.dot = tk.Canvas(
            self,
            width=12,
            height=12,
            bg=COLORS["panel"],
            highlightthickness=0,
        )
        self.dot.pack(side="left", padx=(0, 8))
        self.label = tk.Label(
            self,
            text="Stopped",
            bg=COLORS["panel"],
            fg=COLORS["text"],
            font=("TkDefaultFont", 10, "bold"),
            width=12,
            anchor="w",
        )
        self.label.pack(side="left")
        self.set_state(False, "Stopped")

    def set_state(self, running: bool, text: str):
        color = COLORS["success"] if running else COLORS["danger"]
        self.dot.delete("all")
        self.dot.create_oval(1, 1, 11, 11, fill=color, outline=color)
        self.label.configure(text=text)
