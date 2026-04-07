# test_create.py — ejecutar desde sql-engine/
import sys, os, time
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from parser import get_parser
from planner import QueryPlanner
from executor import QueryExecutor
from storage_server import ServerStorage

storage  = ServerStorage()
executor = QueryExecutor(storage)
planner  = QueryPlanner()
parser   = get_parser()

table = f"test_create_{int(time.time())}"
ast  = parser.parse(f"CREATE TABLE {table} (id INT PRIMARY KEY, nombre VARCHAR(100) NOT NULL, poblacion INT)")
plan = planner.plan(ast)
result = executor.execute(plan)
print(result)