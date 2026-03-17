# UI Helper for SQL Engine REPL
# Uses rich library to create a dashboard-style terminal layout

from rich.console import Console
from rich.layout import Layout
from rich.panel import Panel
from rich.text import Text
from rich.syntax import Syntax
from rich import box

from lexer import SQLLexer
from ast_printer import print_ast
import io
import sys


console = Console()

# Pagination constants
PAGE_SIZE = 5


def format_table(rows):
    """
    Format query results as an ASCII table.
    
    Args:
        rows: List of dictionaries, where each dict represents a row
        
    Returns:
        String containing the formatted ASCII table
    """
    # Handle empty results
    if not rows:
        return "(0 rows)"
    
    # Extract column names from the first row
    columns = list(rows[0].keys())
    
    # Calculate column widths based on longest value in each column
    widths = {}
    for col in columns:
        # Start with column header length
        widths[col] = len(str(col))
        # Check each row for max value length
        for row in rows:
            value_len = len(str(row.get(col, '')))
            if value_len > widths[col]:
                widths[col] = value_len
    
    # Build the table
    lines = []
    
    # Header separator: +-----+-----+-----+
    separator = "+" + "".join("-" * (widths[col] + 2) + "+" for col in columns)
    lines.append(separator)
    
    # Header row: | col1 | col2 | col3 |
    header = "|" + "".join(f" {col:<{widths[col]}} |" for col in columns)
    lines.append(header)
    
    # Header separator again
    lines.append(separator)
    
    # Data rows
    for row in rows:
        row_str = "|" + "".join(f" {str(row.get(col, '')):<{widths[col]}} |" for col in columns)
        lines.append(row_str)
    
    # Bottom separator
    lines.append(separator)
    
    return "\n".join(lines)


def format_table_paginated(rows, page=0, page_size=PAGE_SIZE):
    """
    Format query results as an ASCII table with pagination.
    
    Args:
        rows: List of dictionaries, where each dict represents a row
        page: Current page number (0-indexed)
        page_size: Number of rows per page
        
    Returns:
        Tuple of (table_string, page_info_dict)
    """
    # Handle empty results
    if not rows:
        return "(0 rows)", {"page": 0, "total_pages": 1, "total_rows": 0, "start": 0, "end": 0}
    
    # Calculate pagination info
    total_rows = len(rows)
    total_pages = (total_rows + page_size - 1) // page_size  # Ceiling division
    
    # Clamp page to valid range
    if page < 0:
        page = 0
    if page >= total_pages:
        page = total_pages - 1 if total_pages > 0 else 0
    
    # Calculate visible rows using the specified pattern
    start = page * page_size
    end = start + page_size
    visible_rows = rows[start:end]
    
    # Extract column names from the first row
    columns = list(rows[0].keys())
    
    # Calculate column widths based on longest value in visible rows
    widths = {}
    for col in columns:
        # Start with column header length
        widths[col] = len(str(col))
        # Check each visible row for max value length
        for row in visible_rows:
            value_len = len(str(row.get(col, '')))
            if value_len > widths[col]:
                widths[col] = value_len
    
    # Build the table
    lines = []
    
    # Header separator: +-----+-----+-----+
    separator = "+" + "".join("-" * (widths[col] + 2) + "+" for col in columns)
    lines.append(separator)
    
    # Header row: | col1 | col2 | col3 |
    header = "|" + "".join(f" {col:<{widths[col]}} |" for col in columns)
    lines.append(header)
    
    # Header separator again
    lines.append(separator)
    
    # Data rows (only visible ones)
    for row in visible_rows:
        row_str = "|" + "".join(f" {str(row.get(col, '')):<{widths[col]}} |" for col in columns)
        lines.append(row_str)
    
    # Bottom separator
    lines.append(separator)
    
    # Add pagination info at the bottom
    lines.append("")
    page_indicator = f"Page {page + 1} / {total_pages}"
    lines.append(page_indicator)
    
    # Add navigation help
    if total_pages > 1:
        lines.append("")
        lines.append("Navigation: [n] next  [p] prev  [g] first  [G] last  [q] quit")
    
    table_output = "\n".join(lines)
    
    page_info = {
        "page": page,
        "total_pages": total_pages,
        "total_rows": total_rows,
        "start": start + 1,  # 1-indexed for display
        "end": min(end, total_rows)
    }
    
    return table_output, page_info


def render_page(rows, page, page_size=PAGE_SIZE):
    """
    Render a single page of results as a formatted table string.
    
    Args:
        rows: List of dictionaries (full result set)
        page: Current page number (0-indexed)
        page_size: Number of rows per page
        
    Returns:
        Tuple of (table_string, page_info_dict)
    """
    return format_table_paginated(rows, page, page_size)


def create_paginated_dashboard(query, ast, tokens, result, error=None):
    """
    Create the dashboard layout with pagination support for results.
    Uses terminal pager-like navigation:
      n → next page
      p → previous page
      q → quit pagination
      g → go to first page
      G → go to last page
    
    Args:
        query: SQL query string
        ast: Parsed AST node
        tokens: List of tokens
        result: Query result (list of dicts)
        error: Optional error message
    """
    if error:
        # For errors, show without pagination
        create_simple_dashboard(query, ast, tokens, None, error)
        return
    
    # Calculate total pages
    total_rows = len(result)
    total_pages = (total_rows + PAGE_SIZE - 1) // PAGE_SIZE if total_rows > 0 else 1
    
    # If only one page, just display and return immediately
    if total_pages <= 1:
        table_output, page_info = format_table_paginated(result, 0, PAGE_SIZE)
        
        # Show dashboard once
        console.clear()
        query_text = Text(query, style="bold cyan")
        query_panel = Panel(query_text, title="[bold]QUERY[/bold]", border_style="cyan", box=box.ROUNDED)
        
        ast_output = capture_ast_output(ast)
        ast_text = Text(ast_output or "None", style="green")
        ast_panel = Panel(ast_text, title="[bold]AST[/bold]", border_style="green", box=box.ROUNDED)
        
        tokens_text = "\n".join(tokens) if tokens else "None"
        tokens_text_obj = Text(tokens_text, style="yellow")
        tokens_panel = Panel(tokens_text_obj, title="[bold]TOKENS[/bold]", border_style="yellow", box=box.ROUNDED)
        
        result_text = Text(table_output, style="bold white")
        result_panel = Panel(result_text, title="[bold]RESULT[/bold]", border_style="green", box=box.ROUNDED)
        
        layout = Layout()
        layout.split_column(
            Layout(name="top", size=5),
            Layout(name="middle"),
            Layout(name="bottom", size=14)
        )
        layout["top"].update(query_panel)
        layout["middle"].split_row(Layout(name="ast"), Layout(name="tokens"))
        layout["ast"].update(ast_panel)
        layout["tokens"].update(tokens_panel)
        layout["bottom"].update(result_panel)
        console.print(layout)
        return
    
    # Multiple pages - need pagination navigation
    current_page = 0
    
    while True:
        # Get paginated table
        table_output, page_info = format_table_paginated(result, current_page, PAGE_SIZE)
        
        # Clear screen and show dashboard
        console.clear()
        
        # Top panel - Query
        query_text = Text(query, style="bold cyan")
        query_panel = Panel(
            query_text,
            title="[bold]QUERY[/bold]",
            border_style="cyan",
            box=box.ROUNDED
        )
        
        # Middle section - AST and Tokens
        ast_output = capture_ast_output(ast)
        ast_text = Text(ast_output or "None", style="green")
        ast_panel = Panel(
            ast_text,
            title="[bold]AST[/bold]",
            border_style="green",
            box=box.ROUNDED
        )
        
        tokens_text = "\n".join(tokens) if tokens else "None"
        tokens_text_obj = Text(tokens_text, style="yellow")
        tokens_panel = Panel(
            tokens_text_obj,
            title="[bold]TOKENS[/bold]",
            border_style="yellow",
            box=box.ROUNDED
        )
        
        # Bottom panel - Result with pagination
        result_text = Text(table_output, style="bold white")
        result_panel = Panel(
            result_text,
            title="[bold]RESULT[/bold]",
            border_style="green",
            box=box.ROUNDED
        )
        
        # Build layout
        layout = Layout()
        layout.split_column(
            Layout(name="top", size=5),
            Layout(name="middle"),
            Layout(name="bottom", size=14)
        )
        
        layout["top"].update(query_panel)
        
        layout["middle"].split_row(
            Layout(name="ast"),
            Layout(name="tokens")
        )
        layout["ast"].update(ast_panel)
        layout["tokens"].update(tokens_panel)
        
        layout["bottom"].update(result_panel)
        
        console.print(layout)
        
        # Check if we need navigation
        if total_pages <= 1:
            break
        
        # Check if we need navigation
        if total_pages <= 1:
            break
        
        # Terminal pager-style navigation
        try:
            print()
            key = input(">> n:next p:prev f:first l:last q:quit: ").strip().lower()
            
        except EOFError:
            # Handle piped input ending
            break
        except KeyboardInterrupt:
            # Handle Ctrl+C gracefully
            print()
            break
        
        if key == 'q' or key == 'exit':
            # Quit pagination - return control to main loop
            return
        elif key == 'n':
            # Next page - prevent going past last page
            if current_page < total_pages - 1:
                current_page += 1
        elif key == 'p':
            # Previous page - prevent going before first page
            if current_page > 0:
                current_page -= 1
        elif key == 'f':
            # Go to first page
            current_page = 0
        elif key == 'l':
            # Go to last page
            current_page = total_pages - 1
        # Any other key is ignored, loop continues


def get_tokens(query):
    """
    Tokenize a SQL query and return list of tokens.
    
    Args:
        query: SQL query string
        
    Returns:
        List of formatted token strings
    """
    lexer = SQLLexer()
    lexer.build()
    
    tokens = []
    lexer.lexer.input(query)
    
    while True:
        tok = lexer.lexer.token()
        if not tok:
            break
        
        # Format token nicely
        if tok.type == 'ID':
            tokens.append(f"ID({tok.value})")
        elif tok.type == 'NUMBER':
            tokens.append(f"NUMBER({tok.value})")
        elif tok.type == 'STRING':
            tokens.append(f"STRING({tok.value!r})")
        else:
            tokens.append(tok.type)
    
    return tokens


def capture_ast_output(ast):
    """
    Capture the output of print_ast to a string.
    
    Args:
        ast: AST node
        
    Returns:
        String containing the AST tree output
    """
    # Redirect stdout to capture print_ast output
    old_stdout = sys.stdout
    sys.stdout = io.StringIO()
    
    print_ast(ast)
    
    output = sys.stdout.getvalue()
    sys.stdout = old_stdout
    
    return output.strip()


def create_dashboard(query, ast, tokens, result, error=None):
    """
    Create the dashboard layout with query, AST, tokens, and result.
    
    Args:
        query: SQL query string
        ast: Parsed AST node
        tokens: List of tokens
        result: Query result
        error: Optional error message
    """
    # Clear screen
    console.clear()
    
    # Top panel - Query
    query_panel = Panel(
        Text(query, style="bold cyan"),
        title="[bold]QUERY[/bold]",
        border_style="cyan",
        box=box.ROUNDED,
        padding=(0, 1)
    )
    
    # Middle section - AST and Tokens
    ast_output = capture_ast_output(ast)
    ast_panel = Panel(
        Text(ast_output, style="green"),
        title="[bold]AST[/bold]",
        border_style="green",
        box=box.ROUNDED,
        padding=(0, 1)
    )
    
    # Tokens panel
    tokens_text = "\n".join(tokens)
    tokens_panel = Panel(
        Text(tokens_text, style="yellow"),
        title="[bold]TOKENS[/bold]",
        border_style="yellow",
        box=box.ROUNDED,
        padding=(0, 1)
    )
    
    # Create horizontal split for middle section
    from rich.layout import Layout
    middle_layout = Layout()
    middle_layout.split_row(
        Layout(ast_panel, ratio=1),
        Layout(tokens_panel, ratio=1)
    )
    
    # Bottom panel - Result
    if error:
        result_content = Text(str(error), style="bold red")
        border_style = "red"
    else:
        # Format results as table
        table_output = format_table(result)
        result_content = Text(table_output, style="bold white")
        border_style = "green"
    
    result_panel = Panel(
        result_content,
        title="[bold]RESULT[/bold]",
        border_style=border_style,
        box=box.ROUNDED,
        padding=(0, 1)
    )
    
    # Create main layout
    layout = Layout()
    layout.split_column(
        Layout(query_panel, size=3),
        Layout(middle_layout, ratio=1),
        Layout(result_panel, size=10)
    )
    
    console.print(layout)


def create_simple_dashboard(query, ast, tokens, result, error=None):
    """
    Create the dashboard layout with query, AST, tokens, and result.
    Uses rich.layout.Layout for side-by-side AST and TOKENS panels.
    
    Args:
        query: SQL query string
        ast: Parsed AST node
        tokens: List of tokens
        result: Query result
        error: Optional error message
    """
    # Clear screen
    console.clear()
    
    # Create the main layout structure
    layout = Layout()
    
    # Split into: top (query), middle, bottom (result)
    layout.split_column(
        Layout(name="top", size=5),
        Layout(name="middle"),
        Layout(name="bottom", size=10)
    )
    
    # Top panel - Query
    query_text = Text(query, style="bold cyan")
    query_panel = Panel(
        query_text,
        title="[bold]QUERY[/bold]",
        border_style="cyan",
        box=box.ROUNDED
    )
    layout["top"].update(query_panel)
    
    # Middle section - split horizontally into AST (left) and TOKENS (right)
    layout["middle"].split_row(
        Layout(name="ast"),
        Layout(name="tokens")
    )
    
    # AST panel
    ast_output = capture_ast_output(ast)
    ast_text = Text(ast_output or "None", style="green")
    ast_panel = Panel(
        ast_text,
        title="[bold]AST[/bold]",
        border_style="green",
        box=box.ROUNDED
    )
    layout["ast"].update(ast_panel)
    
    # Tokens panel
    tokens_text = "\n".join(tokens) if tokens else "None"
    tokens_text_obj = Text(tokens_text, style="yellow")
    tokens_panel = Panel(
        tokens_text_obj,
        title="[bold]TOKENS[/bold]",
        border_style="yellow",
        box=box.ROUNDED
    )
    layout["tokens"].update(tokens_panel)
    
    # Bottom panel - Result
    if error:
        result_text = Text(str(error), style="bold red")
        border = "red"
    else:
        # Format results as table
        table_output = format_table(result)
        result_text = Text(table_output, style="bold white")
        border = "green"
    
    result_panel = Panel(
        result_text,
        title="[bold]RESULT[/bold]",
        border_style=border,
        box=box.ROUNDED
    )
    layout["bottom"].update(result_panel)
    
    # Render the layout
    console.print(layout)
