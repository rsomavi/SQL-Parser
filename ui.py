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
        result_content = Text(str(result), style="bold white")
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
        result_text = Text(str(result), style="bold white")
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
