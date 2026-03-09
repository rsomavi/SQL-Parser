# SQL Parser - AST Nodes
# Minimal AST for educational purposes

class ASTNode:
    """Base class for all AST nodes"""
    pass

class SelectQuery(ASTNode):
    """Represents a SELECT query: SELECT columns FROM table;"""
    
    def __init__(self, columns, table):
        # columns can be a list of column names or "*" for SELECT *
        self.columns = columns
        self.table = table
    
    def __repr__(self):
        return f"SelectQuery(columns={self.columns!r}, table={self.table!r})"