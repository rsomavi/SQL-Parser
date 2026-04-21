import tkinter as tk

from app.config.theme import COLORS


class MetricCard(tk.Frame):
    def __init__(self, master, title: str, accent: str):
        super().__init__(
            master,
            bg=COLORS["panel"],
            highlightthickness=1,
            highlightbackground=COLORS["border"],
            highlightcolor=COLORS["border"],
            bd=0,
        )
        self.default_border = COLORS["border"]
        self.accent = accent
        self.flash_after_id = None

        self.columnconfigure(0, weight=1)
        self.rowconfigure(1, weight=1)

        self.inner = tk.Frame(self, bg=COLORS["panel"])
        self.inner.grid(row=0, column=0, sticky="nsew", padx=10, pady=10)
        self.inner.columnconfigure(0, weight=1)

        self.title_label = tk.Label(
            self.inner,
            text=title,
            bg=COLORS["panel"],
            fg=COLORS["text_muted"],
            anchor="w",
            justify="left",
            font=("TkDefaultFont", 10),
        )
        self.title_label.grid(row=0, column=0, sticky="ew")

        self.value_label = tk.Label(
            self.inner,
            text="0",
            bg=COLORS["panel"],
            fg=accent,
            anchor="w",
            justify="left",
            font=("TkDefaultFont", 22, "bold"),
        )
        self.value_label.grid(row=1, column=0, sticky="ew", pady=(8, 4))

        self.subvalue_label = tk.Label(
            self.inner,
            text="server total 0",
            bg=COLORS["panel"],
            fg=COLORS["text_muted"],
            anchor="w",
            justify="left",
            wraplength=260,
            font=("TkDefaultFont", 10),
        )
        self.subvalue_label.grid(row=2, column=0, sticky="ew")

    def update_values(self, value: str, total_value: str):
        changed = self.value_label.cget("text") != value or total_value not in self.subvalue_label.cget("text")
        self.value_label.configure(text=value)
        self.subvalue_label.configure(text=f"server total {total_value}")
        if changed:
            self.flash()

    def flash(self):
        self.configure(highlightbackground=self.accent, highlightcolor=self.accent)
        if self.flash_after_id:
            self.after_cancel(self.flash_after_id)
        self.flash_after_id = self.after(
            450,
            lambda: self.configure(
                highlightbackground=self.default_border,
                highlightcolor=self.default_border,
            ),
        )
