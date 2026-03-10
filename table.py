# Table Abstraction Layer
# Provides a Table class to represent database tables


class Table:
    """Represents a database table with a name and rows."""
    
    def __init__(self, name: str, rows: list):
        """
        Initialize a Table.
        
        Args:
            name: Name of the table.
            rows: List of row dictionaries.
        """
        self.name = name
        self.rows = rows
    
    def get_rows(self) -> list:
        """
        Get the rows in this table.
        
        Returns:
            List of row dictionaries.
        """
        return self.rows