# AST Printer - Visualize AST structure as a tree
# Useful for debugging and understanding the parser output

from ast_nodes import (
    SelectQuery,
    Condition,
    LogicalCondition,
    NotCondition,
    CountQuery,
    SumQuery,
    AvgQuery,
    MinQuery,
    MaxQuery
)


def print_ast(node, indent=0):
    """
    Recursively print the AST structure as a tree.
    
    Args:
        node: AST node to print
        indent: Current indentation level (used for recursion)
    """
    if node is None:
        print("  " * indent + "None")
        return
    
    # Get the class name
    class_name = type(node).__name__
    
    # Handle different node types
    if isinstance(node, SelectQuery):
        print_select_query(node, indent)
    
    elif isinstance(node, Condition):
        print_condition(node, indent)
    
    elif isinstance(node, LogicalCondition):
        print_logical_condition(node, indent)
    
    elif isinstance(node, NotCondition):
        print_not_condition(node, indent)
    
    elif isinstance(node, CountQuery):
        print_count_query(node, indent)
    
    elif isinstance(node, SumQuery):
        print_aggregate_query(node, "SUM", indent)
    
    elif isinstance(node, AvgQuery):
        print_aggregate_query(node, "AVG", indent)
    
    elif isinstance(node, MinQuery):
        print_aggregate_query(node, "MIN", indent)
    
    elif isinstance(node, MaxQuery):
        print_aggregate_query(node, "MAX", indent)
    
    else:
        print("  " * indent + f"Unknown node: {class_name}")


def print_select_query(node: SelectQuery, indent: int):
    """Print a SelectQuery node."""
    print("  " * indent + "SelectQuery")
    
    # Print columns
    if node.columns == '*':
        print("  " * (indent + 1) + "columns: *")
    else:
        print("  " * (indent + 1) + f"columns: {node.columns}")
    
    # Print table
    print("  " * (indent + 1) + f"table: {node.table}")
    
    # Print DISTINCT
    if node.distinct:
        print("  " * (indent + 1) + "DISTINCT: true")
    
    # Print ORDER BY
    if node.order_by:
        print("  " * (indent + 1) + f"ORDER BY: {node.order_by}")
    
    # Print LIMIT
    if node.limit is not None:
        print("  " * (indent + 1) + f"LIMIT: {node.limit}")
    
    # Print WHERE condition
    if node.where is not None:
        print("  " * (indent + 1) + "WHERE")
        print_ast(node.where, indent + 2)


def print_condition(node: Condition, indent: int):
    """Print a Condition node."""
    print("  " * indent + node.operator)
    print("  " * (indent + 1) + f"├── {node.column}")
    print("  " * (indent + 1) + f"└── {repr(node.value)}")


def print_logical_condition(node: LogicalCondition, indent: int):
    """Print a LogicalCondition node (AND/OR)."""
    print("  " * indent + node.operator.upper())
    print("  " * (indent + 1) + "├──")
    print_ast(node.left, indent + 2)
    print("  " * (indent + 1) + "└──")
    print_ast(node.right, indent + 2)


def print_not_condition(node: NotCondition, indent: int):
    """Print a NotCondition node."""
    print("  " * indent + "NOT")
    print_ast(node.condition, indent + 1)


def print_count_query(node: CountQuery, indent: int):
    """Print a CountQuery node."""
    print("  " * indent + "CountQuery")
    print("  " * (indent + 1) + f"table: {node.table}")
    
    if node.where is not None:
        print("  " * (indent + 1) + "WHERE")
        print_ast(node.where, indent + 2)


def print_aggregate_query(node, agg_name: str, indent: int):
    """Print an aggregate query node (SUM, AVG, MIN, MAX)."""
    print("  " * indent + f"{agg_name}Query")
    print("  " * (indent + 1) + f"column: {node.column}")
    print("  " * (indent + 1) + f"table: {node.table}")
    
    if node.where is not None:
        print("  " * (indent + 1) + "WHERE")
        print_ast(node.where, indent + 2)
