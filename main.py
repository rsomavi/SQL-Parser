from parser import get_parser
from planner import QueryPlanner
from executor import QueryExecutor
from storage import MemoryStorage


def main():
    # In-memory database with multiple tables
    database = {
        "users": [
            {"id": 1, "name": "Juan", "age": 25, "city": "Madrid"},
            {"id": 2, "name": "Ana", "age": 30, "city": "Barcelona"},
            {"id": 3, "name": "Luis", "age": 22, "city": "Valencia"},
            {"id": 4, "name": "Maria", "age": 28, "city": "Sevilla"}
        ],
        "products": [
            {"id": 1, "name": "Laptop", "price": 1200},
            {"id": 2, "name": "Phone", "price": 800},
            {"id": 3, "name": "Keyboard", "price": 100}
        ]
    }

    # Create parser, planner, storage, and executor
    parser = get_parser()
    planner = QueryPlanner()
    storage = MemoryStorage(database)
    executor = QueryExecutor(storage)

    # Print welcome message
    print("\033[H\033[2J", end="")
    print("MiniSQL Engine")
    print("Type 'exit' to quit\n")

    # REPL loop
    while True:
        query = input("sql> ").strip()
        
        if query.lower() == "exit":
            break
        
        if not query:
            continue
        
        try:
            # Generate AST
            ast = parser.parse(query)
            if ast is None:
                print("ERROR: invalid SQL syntax")
                continue
            
            # Create query plan
            plan = planner.plan(ast)
            
            # Execute query
            result = executor.execute(plan)
            print(f"Result: {result}")
        except ValueError as e:
            error_msg = str(e)
            if "Table not found" in error_msg:
                table_name = error_msg.split(":")[-1].strip()
                print(f"ERROR: table '{table_name}' does not exist")
            elif "Column" in error_msg and "not found" in error_msg:
                import re
                match = re.search(r"Column '(\w+)'", error_msg)
                if match:
                    col_name = match.group(1)
                    print(f"ERROR: column '{col_name}' does not exist")
                else:
                    print("ERROR: query execution failed")
            else:
                print("ERROR: query execution failed")
        except Exception:
            print("ERROR: invalid SQL syntax")


if __name__ == "__main__":
    main()
