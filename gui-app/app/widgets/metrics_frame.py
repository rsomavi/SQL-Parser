import tkinter as tk

from app.config.theme import COLORS
from app.models.query_models import QueryMetrics
from app.widgets.metric_card import MetricCard


class MetricsFrame(tk.Frame):
    def __init__(self, master, on_reset):
        super().__init__(
            master,
            bg=COLORS["panel"],
            highlightthickness=1,
            highlightbackground=COLORS["border"],
            highlightcolor=COLORS["border"],
            bd=0,
        )
        self.on_reset = on_reset
        self.cards = {}

        self.columnconfigure(0, weight=1)
        self.rowconfigure(1, weight=1)

        self._build_header()
        self._build_metrics_grid()
        self._build_footer()

    def _build_header(self):
        header = tk.Frame(self, bg=COLORS["panel"])
        header.grid(row=0, column=0, sticky="ew", padx=10, pady=(10, 0))
        header.columnconfigure(0, weight=1)

        title = tk.Label(
            header,
            text="Live Telemetry",
            bg=COLORS["panel"],
            fg=COLORS["text"],
            anchor="w",
            font=("TkDefaultFont", 12, "bold"),
        )
        title.grid(row=0, column=0, sticky="ew")

        subtitle = tk.Label(
            header,
            text="Per-query deltas with cumulative server totals.",
            bg=COLORS["panel"],
            fg=COLORS["text_muted"],
            anchor="w",
            font=("TkDefaultFont", 10),
        )
        subtitle.grid(row=1, column=0, sticky="ew", pady=(4, 0))

    def _build_metrics_grid(self):
        self.metrics_grid = tk.Frame(self, bg=COLORS["panel"])
        self.metrics_grid.grid(row=1, column=0, sticky="nsew", padx=10, pady=10)
        self.metrics_grid.columnconfigure(0, weight=2)
        self.metrics_grid.columnconfigure(1, weight=1)
        self.metrics_grid.rowconfigure(0, weight=1)
        self.metrics_grid.rowconfigure(1, weight=1)
        self.metrics_grid.rowconfigure(2, weight=1)

        self.create_metric_card("hits", "Cache Hits", COLORS["success"], 0, 0)
        self.create_metric_card("misses", "Misses", COLORS["danger"], 0, 1)
        self.create_metric_card("evictions", "Evictions", COLORS["warning"], 1, 0)
        self.create_metric_card("hit_rate", "Hit Rate", COLORS["text"], 1, 1)
        self.create_metric_card("elapsed_time", "Elapsed Time", COLORS["info"], 2, 0, columnspan=2)

    def _build_footer(self):
        footer = tk.Frame(self, bg=COLORS["panel"])
        footer.grid(row=2, column=0, sticky="ew", padx=10, pady=(0, 10))
        footer.columnconfigure(0, weight=1)

        reset_button = tk.Button(
            footer,
            text="Reset Metrics",
            command=self.on_reset,
            bg=COLORS["accent"],
            fg=COLORS["bg"],
            activebackground="#67d2fb",
            activeforeground=COLORS["bg"],
            relief="flat",
            bd=0,
            highlightthickness=0,
            padx=10,
            pady=10,
            font=("TkDefaultFont", 10, "bold"),
        )
        reset_button.grid(row=0, column=1, sticky="e")

    def create_metric_card(
        self,
        key: str,
        title: str,
        accent: str,
        row: int,
        column: int,
        columnspan: int = 1,
    ):
        card = MetricCard(self.metrics_grid, title=title, accent=accent)
        card.grid(
            row=row,
            column=column,
            columnspan=columnspan,
            sticky="nsew",
            padx=10,
            pady=10,
        )
        self.cards[key] = card
        return card

    def update_metrics(self, metrics: QueryMetrics, total_metrics: QueryMetrics | None = None):
        total = total_metrics or QueryMetrics()
        self.cards["hits"].update_values(str(metrics.hits), str(total.hits))
        self.cards["misses"].update_values(str(metrics.misses), str(total.misses))
        self.cards["evictions"].update_values(str(metrics.evictions), str(total.evictions))
        self.cards["hit_rate"].update_values(f"{metrics.hit_rate:.3f}", f"{total.hit_rate:.3f}")
        self.cards["elapsed_time"].update_values(
            f"{metrics.elapsed_time:.4f}s",
            f"{total.elapsed_time:.4f}s",
        )
