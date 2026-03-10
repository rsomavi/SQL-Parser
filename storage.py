# SQL Storage Layer
# Provides abstraction for data storage (in-memory for now)

from table import Table


class MemoryStorage:
    """In-memory storage implementation."""
    
    def __init__(self, database: dict):
        """
        Initialize storage with a database dictionary.
        
        Args:
            database: Dict mapping table names to list of rows.
                     Example: {"users": [{"name": "Juan"}, {"name": "Ana"}]}
        """
        # Convert raw lists to Table objects
        self.database = {
            name: Table(name, rows)
            for name, rows in database.items()
        }
    
    def load_table(self, table_name: str):
        """
        Load a table by name.
        
        Args:
            table_name: Name of the table to load.
            
        Returns:
            List of rows in the table.
            
        Raises:
            ValueError: If the table does not exist.
        """
        if table_name not in self.database:
            raise ValueError(f"Table not found: {table_name}")
        return self.database[table_name]