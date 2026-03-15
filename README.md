# Mini SQL Engine

A minimal SQL parser and execution engine written in Python. It implements a simplified SQL query pipeline similar to real database engines, featuring a lexer, parser, query planner, and in-memory storage.

## Features

- **SQL Lexer** — Tokenizer that breaks SQL queries into meaningful tokens
- **SQL Parser** — Built with PLY (Python Lex-Yacc) to generate an Abstract Syntax Tree (AST)
- **Query Planner** — Optimizes and plans query execution
- **Query Executor** — Executes queries against in-memory storage
- **In-Memory Storage Engine** — Fast, ephemeral data storage
- **Terminal Dashboard UI** — Interactive CLI interface
- **Comprehensive Test Suite** — Unit tests for core functionality

### Supported SQL Features

| Category | Supported |
|----------|-----------|
| Queries | `SELECT` |
| Filtering | `WHERE` |
| Grouping | `GROUP BY` |
| Group Filtering | `HAVING` |
| Sorting | `ORDER BY` |
| Limiting | `LIMIT` |
| Deduplication | `DISTINCT` |

### Aggregate Functions

- `COUNT(*)`
- `SUM(column)`
- `AVG(column)`
- `MIN(column)`
- `MAX(column)`

### Operators

**Logical:**
- `AND`, `OR`, `NOT`

**Comparison:**
- `=`, `>`, `<`, `>=`, `<=`, `!=`

## Architecture

```
SQL Query
    ↓
Lexer → Tokens
    ↓
Parser → AST
    ↓
Planner → Query Plan
    ↓
Executor → Results
```

### Components

1. **Lexer** (`lexer.py`) — Tokenizes raw SQL strings into a stream of tokens
2. **Parser** (`parser.py`) — Uses PLY to parse tokens and build an AST
3. **AST Nodes** (`ast_nodes.py`) — Defines the node types representing SQL constructs
4. **AST Printer** (`ast_printer.py`) — Utility for visualizing the AST
5. **Planner** (`planner.py`) — Transforms AST into an executable query plan
6. **Executor** (`executor.py`) — Runs the query plan against storage and returns results
7. **Storage** (`storage.py`) — In-memory table storage engine
8. **Table** (`table.py`) — Table data structure and operations
9. **UI** (`ui.py`) — Terminal-based dashboard for interactive queries
10. **Main** (`main.py`) — Entry point for the application

## Example Queries

### Basic SELECT

```sql
SELECT name, age FROM users WHERE age > 25
```

### GROUP BY with HAVING

```sql
SELECT city, COUNT(*) FROM users GROUP BY city HAVING COUNT(*) > 2 ORDER BY COUNT(*) LIMIT 2
```

### Aggregate Functions

```sql
SELECT city, COUNT(*)  FROM users  GROUP BY city;
```
```sql
SELECT city, AVG(age) FROM users GROUP BY city
```

### DISTINCT and LIMIT

```sql
SELECT DISTINCT city FROM users LIMIT 3;
```

### Complex query
```sql
SELECT city, COUNT(*) FROM users WHERE age > 25 AND city = 'Madrid' OR age > 30 GROUP BY city HAVING COUNT(*) > 1 ORDER BY COUNT(*) LIMIT 3
```

## Running the Project

### Prerequisites

- Python 3.8+
- PLY (Python Lex-Yacc)

```bash
pip install ply
```

### Interactive Mode

Start the terminal dashboard:

```bash
python main.py
```

### Programmatic Usage

```python
from parser import get_parser
from planner import QueryPlanner
from executor import QueryExecutor
from storage import MemoryStorage

# Example database
database = {
    "users": [
        {"id": 1, "name": "Juan", "age": 25, "city": "Madrid"},
        {"id": 2, "name": "Ana", "age": 30, "city": "Barcelona"},
    ]
}

# Initialize components
parser = get_parser()
planner = QueryPlanner()
storage = MemoryStorage(database)
executor = QueryExecutor(storage)

# Execute query
query = "SELECT name FROM users WHERE age > 20"

ast = parser.parse(query)
plan = planner.plan(ast)
result = executor.execute(plan)

for row in result:
    print(row)
```

## Running the Tests

Execute the test suite:

```bash
python tests/run_tests.py
```

Run specific test files:

```bash
python tests/test_group_by.py
python tests/test_having.py
```

## Project Structure

```
mini-sql-engine/
├── lexer.py           # SQL tokenizer
├── parser.py          # PLY-based SQL parser
├── planner.py         # Query planning
├── executor.py        # Query execution
├── storage.py         # In-memory storage engine
├── table.py           # Table data structure
├── ast_nodes.py       # AST node definitions
├── ast_printer.py     # AST visualization
├── ui.py              # Terminal dashboard
├── main.py            # Application entry point
│
└── tests/
    ├── run_tests.py   # Test runner
    ├── test_group_by.py
    ├── test_having.py
    └── ...
```

## Future Improvements

- [ ] JOIN operations (INNER, LEFT, RIGHT)
- [ ] INSERT, UPDATE, DELETE statements
- [ ] CREATE TABLE support
- [ ] Indexing for query optimization
- [ ] Subqueries
- [ ] Window functions
- [ ] Transaction support (BEGIN, COMMIT, ROLLBACK)
- [ ] Persistence layer (save/load to disk)
- [ ] Additional data types (DATE, TIMESTAMP, BOOLEAN)
- [ ] Query caching

## Development Status

**Under Development**

This project is actively being developed. Features, APIs, and internal architecture may change. Contributions and feedback are welcome.

---

Built with [PLY (Python Lex-Yacc)](https://www.dabeaz.com/ply/)
