# SQL Storage Layer
# Provides abstraction for data storage (in-memory for now)

class MemoryStorage:
    """In-memory storage implementation."""
    
    def __init__(self, database: dict):
        """
        Initialize storage with a database dictionary.
        
        Args:
            database: Dict mapping table names to list of rows.
                     Example: {"users": [{"name": "Juan"}, {"name": "Ana"}]}
        """
        self.database = database
    
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