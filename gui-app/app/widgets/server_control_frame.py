import tkinter as tk
from tkinter import ttk

from app.config.constants import SUPPORTED_POLICIES
from app.config.theme import COLORS
from app.widgets.status_badge import StatusBadge


class ServerControlFrame(ttk.LabelFrame):
    def __init__(self, master, on_start, on_stop):
        super().__init__(master, text="Server Runtime", style="Panel.TLabelframe")
        self.on_start = on_start
        self.on_stop = on_stop

        self.policy_var = tk.StringVar(value=SUPPORTED_POLICIES[0])
        self.frames_var = tk.StringVar(value="64")
        self.status_detail_var = tk.StringVar(value="Server offline.")
        self.configure(padding=12)

        self.columnconfigure(0, weight=1)
        self.columnconfigure(1, weight=1)
        self.columnconfigure(2, weight=1)

        ttk.Label(self, text="Policy", style="Muted.TLabel").grid(row=0, column=0, sticky="w", padx=10, pady=(10, 6))
        policy_box = ttk.Combobox(
            self,
            textvariable=self.policy_var,
            values=SUPPORTED_POLICIES,
            state="readonly",
            style="Dashboard.TCombobox",
        )
        policy_box.grid(row=1, column=0, sticky="ew", padx=10, pady=(0, 10))

        ttk.Label(self, text="Frames", style="Muted.TLabel").grid(row=0, column=1, sticky="w", padx=10, pady=(10, 6))
        frames_entry = ttk.Entry(self, textvariable=self.frames_var, style="Dashboard.TEntry")
        frames_entry.grid(row=1, column=1, sticky="ew", padx=10, pady=(0, 10))

        button_frame = ttk.Frame(self, style="Panel.TFrame")
        button_frame.grid(row=1, column=2, sticky="e", padx=10, pady=(0, 10))
        button_frame.columnconfigure(0, weight=1)
        button_frame.columnconfigure(1, weight=1)

        ttk.Button(
            button_frame,
            text="Start Server",
            command=self._handle_start,
            style="Primary.TButton",
        ).grid(row=0, column=0, sticky="ew", padx=(0, 10))
        ttk.Button(
            button_frame,
            text="Stop Server",
            command=self.on_stop,
            style="Danger.TButton",
        ).grid(row=0, column=1, sticky="ew")

        ttk.Label(self, text="Status", style="Muted.TLabel").grid(row=2, column=0, sticky="w", padx=10, pady=(0, 6))
        self.badge = StatusBadge(self)
        self.badge.grid(row=2, column=1, sticky="w", padx=10, pady=(0, 6))

        self.status_detail = tk.Label(
            self,
            textvariable=self.status_detail_var,
            bg=COLORS["panel"],
            fg=COLORS["text_muted"],
            justify="left",
            wraplength=720,
            anchor="w",
            font=("TkDefaultFont", 9),
        )
        self.status_detail.grid(row=3, column=0, columnspan=3, sticky="ew", padx=10, pady=(0, 10))

    def _handle_start(self):
        self.on_start(self.policy_var.get(), self.frames_var.get())

    def set_status(self, value: str):
        running = value.startswith("Running")
        if value == "Stopped":
            badge_text = "Stopped"
            detail_text = "Server offline."
        elif "executing" in value.lower():
            badge_text = "Busy"
            detail_text = "Executing query against the active server."
        elif value.startswith("Running"):
            badge_text = "Running"
            detail_text = value.replace("Running ", "", 1)
        elif "Benchmark" in value:
            badge_text = "Benchmark"
            detail_text = "Running benchmark workflow."
        else:
            badge_text = value[:12]
            detail_text = value
        self.badge.set_state(running, badge_text)
        self.status_detail_var.set(detail_text)
