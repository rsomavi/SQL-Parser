import tkinter as tk
from tkinter import ttk

from app.config.constants import SUPPORTED_POLICIES
from app.config.theme import COLORS


class BenchmarkFrame(ttk.Frame):
    def __init__(self, master, on_run_benchmark):
        super().__init__(master, style="Panel.TFrame")
        self.on_run_benchmark = on_run_benchmark
        self.info_var = tk.StringVar(value="No benchmark executed yet.")
        self.policy_vars = {
            policy: tk.BooleanVar(value=policy in ("lru", "clock"))
            for policy in SUPPORTED_POLICIES
        }

        self.columnconfigure(0, weight=3)
        self.columnconfigure(1, weight=2)
        self.rowconfigure(2, weight=1)

        header = ttk.Frame(self, style="Panel.TFrame")
        header.grid(row=0, column=0, columnspan=2, sticky="ew", padx=10, pady=(10, 0))
        header.columnconfigure(0, weight=1)

        ttk.Label(header, text="Policy Benchmark", style="Heading.TLabel").grid(row=0, column=0, sticky="w")
        ttk.Label(
            header,
            text="Compare the same workload across multiple startup policies and review the benchmark charts.",
            style="Subheading.TLabel",
        ).grid(row=1, column=0, sticky="w", pady=(2, 0))

        ttk.Label(self, text="Benchmark queries (one per line)", style="Muted.TLabel").grid(
            row=1, column=0, sticky="w", padx=10, pady=(10, 6)
        )

        policy_card = ttk.LabelFrame(self, text="Policies", style="Panel.TLabelframe")
        policy_card.grid(row=1, column=1, rowspan=2, sticky="nsew", padx=10, pady=10)
        policy_card.columnconfigure(0, weight=1)

        for index, (policy, variable) in enumerate(self.policy_vars.items()):
            ttk.Checkbutton(policy_card, text=policy, variable=variable).grid(
                row=index, column=0, sticky="w", padx=10, pady=6
            )

        self.queries_text = tk.Text(
            self,
            height=12,
            wrap="word",
            bg=COLORS["panel_alt"],
            fg=COLORS["text"],
            insertbackground=COLORS["accent"],
            highlightthickness=1,
            highlightbackground=COLORS["border"],
            relief="flat",
            padx=12,
            pady=12,
        )
        self.queries_text.grid(row=2, column=0, sticky="nsew", padx=10, pady=(0, 10))
        self.queries_text.insert(
            "1.0",
            "SELECT * FROM users LIMIT 5;\nSELECT * FROM products LIMIT 5;\nSELECT * FROM orders LIMIT 5;",
        )

        footer = ttk.Frame(self, style="Panel.TFrame")
        footer.grid(row=3, column=0, columnspan=2, sticky="ew", padx=10, pady=(0, 10))
        footer.columnconfigure(0, weight=0)
        footer.columnconfigure(1, weight=1)

        ttk.Button(footer, text="Run Benchmark", command=self._run_benchmark, style="Primary.TButton").grid(
            row=0, column=0, sticky="w"
        )

        self.info_label = tk.Label(
            footer,
            textvariable=self.info_var,
            wraplength=720,
            justify="left",
            anchor="w",
            bg=COLORS["panel"],
            fg=COLORS["text_muted"],
            font=("TkDefaultFont", 10),
        )
        self.info_label.grid(row=0, column=1, sticky="ew")

    def _run_benchmark(self):
        queries = [line.strip() for line in self.queries_text.get("1.0", "end").splitlines()]
        policies = [policy for policy, var in self.policy_vars.items() if var.get()]
        self.on_run_benchmark(queries, policies)

    def set_info(self, message: str):
        self.info_var.set(message)
