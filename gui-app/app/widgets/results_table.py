import tkinter.font as tkfont
from tkinter import ttk


class ResultsTable(ttk.Frame):
    def __init__(
        self,
        master,
        empty_message: str = "No rows",
        tree_style: str = "Dashboard.Treeview",
        heading_style: str = "Dashboard.Treeview.Heading",
    ):
        super().__init__(master)
        self.empty_message = empty_message
        self.tree = ttk.Treeview(self, show="headings", style=tree_style)
        self.heading_style = heading_style
        self.body_font = tkfont.nametofont("TkFixedFont") if "Telemetry" in tree_style else tkfont.nametofont("TkDefaultFont")
        self.heading_font = tkfont.Font(font=self.body_font)
        self.heading_font.configure(weight="bold")

        yscroll = ttk.Scrollbar(self, orient="vertical", command=self.tree.yview)
        xscroll = ttk.Scrollbar(self, orient="horizontal", command=self.tree.xview)
        self.tree.configure(yscrollcommand=yscroll.set, xscrollcommand=xscroll.set)

        self.tree.grid(row=0, column=0, sticky="nsew")
        yscroll.grid(row=0, column=1, sticky="ns")
        xscroll.grid(row=1, column=0, sticky="ew")

        self.columnconfigure(0, weight=1)
        self.rowconfigure(0, weight=1)

    def set_rows(self, rows: list[dict]):
        self.tree.delete(*self.tree.get_children())

        if not rows:
            self.tree["columns"] = ("result",)
            self.tree.heading("result", text="result")
            self.tree.column("result", width=400, minwidth=200, anchor="w", stretch=True)
            self.tree.insert("", "end", values=(self.empty_message,))
            return

        columns = list(rows[0].keys())
        self.tree["columns"] = columns
        for column in columns:
            self.tree.heading(column, text=column)
            self.tree.column(
                column,
                width=self._column_width(column, rows),
                minwidth=110,
                anchor="w",
                stretch=False,
            )

        for row in rows:
            self.tree.insert("", "end", values=[row.get(column, "") for column in columns])

    def _column_width(self, column: str, rows: list[dict]) -> int:
        samples = [str(column)]
        samples.extend(str(row.get(column, "")) for row in rows[:100])
        widest = max(self.body_font.measure(sample) for sample in samples)
        return min(max(widest + 28, 120), 520)
