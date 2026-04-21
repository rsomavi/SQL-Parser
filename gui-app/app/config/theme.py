from tkinter import ttk


COLORS = {
    "bg": "#0f172a",
    "bg_alt": "#111827",
    "panel": "#1e293b",
    "panel_alt": "#172033",
    "panel_soft": "#23314a",
    "border": "#334155",
    "text": "#e2e8f0",
    "text_muted": "#94a3b8",
    "accent": "#38bdf8",
    "accent_alt": "#22c55e",
    "warning": "#f59e0b",
    "danger": "#ef4444",
    "success": "#10b981",
    "info": "#60a5fa",
    "query": "#a78bfa",
}


FONT_FAMILY = "TkDefaultFont"


def apply_theme(root):
    style = ttk.Style(root)
    style.theme_use("clam")

    root.configure(bg=COLORS["bg"])

    style.configure(
        ".",
        background=COLORS["bg"],
        foreground=COLORS["text"],
        fieldbackground=COLORS["panel"],
    )
    style.configure("Dashboard.TFrame", background=COLORS["bg"])
    style.configure("Panel.TFrame", background=COLORS["panel"])
    style.configure("Sidebar.TFrame", background=COLORS["bg_alt"])
    style.configure(
        "Dashboard.TNotebook",
        background=COLORS["bg"],
        borderwidth=0,
        tabmargins=(0, 0, 0, 0),
    )
    style.configure(
        "Dashboard.TNotebook.Tab",
        background=COLORS["panel"],
        foreground=COLORS["text_muted"],
        padding=(18, 10),
        borderwidth=0,
    )
    style.map(
        "Dashboard.TNotebook.Tab",
        background=[("selected", COLORS["panel_soft"])],
        foreground=[("selected", COLORS["text"])],
    )
    style.configure(
        "Panel.TLabelframe",
        background=COLORS["panel"],
        foreground=COLORS["text"],
        borderwidth=1,
        relief="solid",
        bordercolor=COLORS["border"],
        padding=10,
    )
    style.configure(
        "Panel.TLabelframe.Label",
        background=COLORS["panel"],
        foreground=COLORS["accent"],
        font=(FONT_FAMILY, 10, "bold"),
    )
    style.configure(
        "Dashboard.TLabel",
        background=COLORS["bg"],
        foreground=COLORS["text"],
    )
    style.configure(
        "Muted.TLabel",
        background=COLORS["panel"],
        foreground=COLORS["text_muted"],
    )
    style.configure(
        "Heading.TLabel",
        background=COLORS["bg"],
        foreground=COLORS["text"],
        font=(FONT_FAMILY, 18, "bold"),
    )
    style.configure(
        "Subheading.TLabel",
        background=COLORS["bg"],
        foreground=COLORS["text_muted"],
        font=(FONT_FAMILY, 10),
    )
    style.configure(
        "Primary.TButton",
        background=COLORS["accent"],
        foreground=COLORS["bg"],
        borderwidth=0,
        focusthickness=0,
        padding=(14, 10),
        font=(FONT_FAMILY, 10, "bold"),
    )
    style.map(
        "Primary.TButton",
        background=[("active", "#67d2fb"), ("disabled", COLORS["border"])],
        foreground=[("disabled", COLORS["text_muted"])],
    )
    style.configure(
        "Danger.TButton",
        background=COLORS["danger"],
        foreground=COLORS["text"],
        borderwidth=0,
        focusthickness=0,
        padding=(14, 10),
        font=(FONT_FAMILY, 10, "bold"),
    )
    style.map("Danger.TButton", background=[("active", "#f87171")])
    style.configure(
        "Dashboard.Treeview",
        background=COLORS["panel"],
        foreground=COLORS["text"],
        fieldbackground=COLORS["panel"],
        bordercolor=COLORS["border"],
        rowheight=28,
    )
    style.configure(
        "Telemetry.Treeview",
        background=COLORS["panel"],
        foreground=COLORS["text"],
        fieldbackground=COLORS["panel"],
        bordercolor=COLORS["border"],
        rowheight=28,
        font=("TkFixedFont", 10),
    )
    style.configure(
        "Dashboard.Treeview.Heading",
        background=COLORS["panel_soft"],
        foreground=COLORS["text"],
        relief="flat",
        font=(FONT_FAMILY, 10, "bold"),
    )
    style.configure(
        "Telemetry.Treeview.Heading",
        background=COLORS["panel_soft"],
        foreground=COLORS["text"],
        relief="flat",
        font=("TkFixedFont", 10, "bold"),
    )
    style.map(
        "Dashboard.Treeview",
        background=[("selected", "#1d4ed8")],
        foreground=[("selected", COLORS["text"])],
    )
    style.map(
        "Telemetry.Treeview",
        background=[("selected", "#1d4ed8")],
        foreground=[("selected", COLORS["text"])],
    )
    style.configure(
        "Dashboard.TCombobox",
        fieldbackground=COLORS["panel_soft"],
        background=COLORS["panel_soft"],
        foreground=COLORS["text"],
        arrowcolor=COLORS["accent"],
        bordercolor=COLORS["border"],
        padding=6,
    )
    style.configure(
        "Dashboard.TEntry",
        fieldbackground=COLORS["panel_soft"],
        foreground=COLORS["text"],
        bordercolor=COLORS["border"],
        padding=6,
    )
