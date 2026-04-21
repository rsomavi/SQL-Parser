import tkinter as tk

from app.config.theme import COLORS


class BenchmarkChartsFrame(tk.Frame):
    def __init__(self, master):
        super().__init__(
            master,
            bg=COLORS["panel"],
            highlightthickness=1,
            highlightbackground=COLORS["border"],
            highlightcolor=COLORS["border"],
            bd=0,
        )
        self.chart_specs = {
            "hit_rate": ("Hit Rate", COLORS["success"], "{value:.3f}"),
            "misses": ("Misses", COLORS["danger"], "{value:.0f}"),
            "evictions": ("Evictions", COLORS["warning"], "{value:.0f}"),
            "elapsed_time": ("Elapsed Time", COLORS["accent"], "{value:.4f}s"),
        }
        self.cards = {}

        self.columnconfigure(0, weight=1)
        self.columnconfigure(1, weight=1)
        self.rowconfigure(1, weight=1)
        self.rowconfigure(2, weight=1)

        title = tk.Label(
            self,
            text="Benchmark Charts",
            bg=COLORS["panel"],
            fg=COLORS["text"],
            anchor="w",
            font=("TkDefaultFont", 12, "bold"),
        )
        title.grid(row=0, column=0, columnspan=2, sticky="ew", padx=10, pady=(10, 0))

        subtitle = tk.Label(
            self,
            text="Tkinter canvas charts for hit rate, misses, evictions and elapsed time by policy.",
            bg=COLORS["panel"],
            fg=COLORS["text_muted"],
            anchor="w",
            font=("TkDefaultFont", 10),
        )
        subtitle.grid(row=1, column=0, columnspan=2, sticky="ew", padx=10, pady=(4, 10))

        positions = [
            ("hit_rate", 2, 0),
            ("misses", 2, 1),
            ("evictions", 3, 0),
            ("elapsed_time", 3, 1),
        ]
        self.rowconfigure(2, weight=1)
        self.rowconfigure(3, weight=1)

        for key, row, column in positions:
            self.cards[key] = self._create_chart_card(key, row, column)

    def _create_chart_card(self, key: str, row: int, column: int):
        title, color, formatter = self.chart_specs[key]
        card = tk.Frame(
            self,
            bg=COLORS["panel_alt"],
            highlightthickness=1,
            highlightbackground=COLORS["border"],
            highlightcolor=COLORS["border"],
            bd=0,
        )
        card.grid(row=row, column=column, sticky="nsew", padx=10, pady=10)
        card.columnconfigure(0, weight=1)
        card.rowconfigure(1, weight=1)

        label = tk.Label(
            card,
            text=title,
            bg=COLORS["panel_alt"],
            fg=COLORS["text"],
            anchor="w",
            font=("TkDefaultFont", 10, "bold"),
        )
        label.grid(row=0, column=0, sticky="ew", padx=10, pady=(10, 0))

        canvas = tk.Canvas(
            card,
            bg=COLORS["panel_alt"],
            highlightthickness=0,
            bd=0,
        )
        canvas.grid(row=1, column=0, sticky="nsew", padx=10, pady=10)
        canvas.bind(
            "<Configure>",
            lambda event, metric_key=key: self._redraw_metric(metric_key),
        )

        return {"canvas": canvas, "color": color, "formatter": formatter}

    def render_summary(self, summary: dict[str, dict[str, float]]):
        self.summary = summary
        for key in self.cards:
            self._redraw_metric(key)

    def _redraw_metric(self, metric_key: str):
        canvas = self.cards[metric_key]["canvas"]
        canvas.delete("all")

        width = max(canvas.winfo_width(), 240)
        height = max(canvas.winfo_height(), 180)
        left = 42
        right = width - 12
        top = 16
        bottom = height - 36

        canvas.create_line(left, top, left, bottom, fill=COLORS["border"])
        canvas.create_line(left, bottom, right, bottom, fill=COLORS["border"])

        if not getattr(self, "summary", None):
            canvas.create_text(
                width / 2,
                height / 2,
                text="Run a benchmark to populate this chart.",
                fill=COLORS["text_muted"],
                font=("TkDefaultFont", 10),
            )
            return

        policies = list(self.summary.keys())
        values = [float(self.summary[policy].get(metric_key, 0.0)) for policy in policies]
        maximum = max(values) if values else 0.0
        maximum = maximum if maximum > 0 else 1.0
        plot_width = max(right - left, 1)
        slot_width = plot_width / max(len(policies), 1)
        bar_width = max(slot_width * 0.55, 18)

        for index, policy in enumerate(policies):
            value = values[index]
            x0 = left + (index * slot_width) + ((slot_width - bar_width) / 2)
            x1 = x0 + bar_width
            bar_height = ((bottom - top) * value) / maximum
            y0 = bottom - bar_height

            canvas.create_rectangle(
                x0,
                y0,
                x1,
                bottom,
                fill=self.cards[metric_key]["color"],
                outline="",
            )
            canvas.create_text(
                (x0 + x1) / 2,
                y0 - 10,
                text=self.cards[metric_key]["formatter"].format(value=value),
                fill=COLORS["text"],
                font=("TkDefaultFont", 9, "bold"),
            )
            canvas.create_text(
                (x0 + x1) / 2,
                bottom + 14,
                text=policy,
                fill=COLORS["text_muted"],
                font=("TkDefaultFont", 9),
            )
