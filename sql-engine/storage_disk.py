# SQL Disk Storage Layer
# Loads tables from disk using a C program

import subprocess


class DiskStorage:
    """Disk-based storage implementation using external C program."""
    
    def __init__(self, data_dir: str = None):
        """
        Initialize disk storage.
        
        Args:
            data_dir: Path to data directory (optional, defaults to ../data)
        """
        self.data_dir = data_dir or "../data"
    
    def load_table(self, table_name: str):
        """
        Load a table by name from disk.
        
        Args:
            table_name: Name of the table to load.
            
        Returns:
            Table object with rows loaded from disk.
            
        Raises:
            RuntimeError: If the C program fails.
        """
        result = subprocess.run(
            ["../storage-engine/disk", "read", table_name],
            capture_output=True,
            text=True
        )
        
        if result.returncode != 0:
            raise RuntimeError(result.stderr)
        
        rows = self._parse_csv(result.stdout)
        
        # Import here to avoid circular imports
        from table import Table
        return Table(table_name, rows)
    
    def _parse_csv(self, text):
        """
        Parse CSV text into list of dictionaries.
        
        Args:
            text: CSV text with header row.
            
        Returns:
            List of dictionaries representing rows.
        """
        lines = text.strip().split("\n")
        
        if not lines:
            return []
        
        headers = lines[0].split(",")
        
        rows = []
        for line in lines[1:]:
            values = line.split(",")
            row = {}
            for i, header in enumerate(headers):
                value = values[i]
                # Try to convert to int or float
                try:
                    if '.' in value:
                        row[header] = float(value)
                    else:
                        row[header] = int(value)
                except ValueError:
                    row[header] = value
            rows.append(row)
        
        return rows
