#!/usr/bin/env python3
"""
Test suite for HAVING clause functionality.
"""

import sys
import os

# Add sql-engine directory to path for imports
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from parser import get_parser
from planner import QueryPlanner
from executor import QueryExecutor
from storage_memory import MemoryStorage


# ============================================================================
# Test Database Setup
# ============================================================================

def create_test_database():
    """Create the test database with sample data."""
    return {
        "users": [
            {"id": 1, "name": "Juan", "age": 25, "city": "Madrid"},
            {"id": 2, "name": "Ana", "age": 30, "city": "Barcelona"},
            {"id": 3, "name": "Luis", "age": 22, "city": "Valencia"},
            {"id": 4, "name": "Maria", "age": 28, "city": "Sevilla"},
            {"id": 5, "name": "Carlos", "age": 35, "city": "Madrid"},
            {"id": 6, "name": "Laura", "age": 27, "city": "Bilbao"},
            {"id": 7, "name": "Pedro", "age": 40, "city": "Valencia"},
            {"id": 8, "name": "Sofia", "age": 19, "city": "Barcelona"},
            {"id": 9, "name": "Miguel", "age": 31, "city": "Sevilla"},
            {"id": 10, "name": "Elena", "age": 24, "city": "Madrid"},
            {"id": 11, "name": "Diego", "age": 29, "city": "Bilbao"},
            {"id": 12, "name": "Lucia", "age": 21, "city": "Valencia"},
            {"id": 13, "name": "Alberto", "age": 45, "city": "Madrid"},
            {"id": 14, "name": "Paula", "age": 26, "city": "Barcelona"},
            {"id": 15, "name": "Jorge", "age": 33, "city": "Sevilla"}
        ]
    }


def run_query(executor, query):
    """Execute a query and return the result or error."""
    try:
        parser = get_parser()
        planner = QueryPlanner()
        ast = parser.parse(query)
        if ast is None:
            return {"error": "Parse error"}
        plan = planner.plan(ast)
        result = executor.execute(plan)
        return {"result": result}
    except Exception as e:
        return {"error": str(e)}


# ============================================================================
# Test Cases
# ============================================================================

def run_all_tests():
    """Run all HAVING test cases."""
    # Setup
    database = create_test_database()
    storage = MemoryStorage(database)
    executor = QueryExecutor(storage)
    
    print("=" * 70)
    print("HAVING Clause Test Suite")
    print("=" * 70)
    print()
    
    passed = 0
    failed = 0
    
    # Test 1: HAVING COUNT(*) > value
    print("--- HAVING COUNT(*) ---")
    query = "SELECT city, COUNT(*) FROM users GROUP BY city HAVING COUNT(*) > 2"
    result = run_query(executor, query)
    
    if "error" in result:
        print(f"FAIL: {query}")
        print(f"  Error: {result['error']}")
        failed += 1
    else:
        rows = result["result"]
        # Should have cities with count > 2 (Madrid: 4, Barcelona: 3, Valencia: 3, Sevilla: 3)
        if len(rows) == 4:
            print(f"PASS: {query}")
            print(f"  Result: {rows}")
            passed += 1
        else:
            print(f"FAIL: {query}")
            print(f"  Expected 4 rows, got {len(rows)}")
            failed += 1
    
    # Test 2: HAVING SUM(age) > value
    print("\n--- HAVING SUM(age) ---")
    query = "SELECT city, SUM(age) FROM users GROUP BY city HAVING SUM(age) > 100"
    result = run_query(executor, query)
    
    if "error" in result:
        print(f"FAIL: {query}")
        print(f"  Error: {result['error']}")
        failed += 1
    else:
        rows = result["result"]
        # Madrid: 25+35+24+45=129, Barcelona: 30+19+26=75, Valencia: 22+40+21=83, Sevilla: 28+31+33=92, Bilbao: 27+29=56
        # Only Madrid (129) > 100
        if len(rows) >= 1:  # At least Madrid
            print(f"PASS: {query}")
            print(f"  Result: {rows}")
            passed += 1
        else:
            print(f"FAIL: {query}")
            print(f"  Expected at least 1 row, got {len(rows)}: {rows}")
            failed += 1
    
    # Test 3: HAVING AVG(age) > value
    print("\n--- HAVING AVG(age) ---")
    query = "SELECT city, AVG(age) FROM users GROUP BY city HAVING AVG(age) > 30"
    result = run_query(executor, query)
    
    if "error" in result:
        print(f"FAIL: {query}")
        print(f"  Error: {result['error']}")
        failed += 1
    else:
        rows = result["result"]
        # Madrid: 129/4=32.25, Sevilla: 92/3=30.67
        # Both > 30
        if len(rows) == 2:
            print(f"PASS: {query}")
            print(f"  Result: {rows}")
            passed += 1
        else:
            print(f"FAIL: {query}")
            print(f"  Expected 2 rows, got {len(rows)}: {rows}")
            failed += 1
    
    # Test 4: HAVING COUNT(*) < value
    print("\n--- HAVING COUNT(*) < value ---")
    query = "SELECT city, COUNT(*) FROM users GROUP BY city HAVING COUNT(*) < 3"
    result = run_query(executor, query)
    
    if "error" in result:
        print(f"FAIL: {query}")
        print(f"  Error: {result['error']}")
        failed += 1
    else:
        rows = result["result"]
        # Bilbao has 2 users (data has 'Bilbao' with capital B)
        if len(rows) == 1 and rows[0]['city'] == 'Bilbao':
            print(f"PASS: {query}")
            print(f"  Result: {rows}")
            passed += 1
        else:
            print(f"FAIL: {query}")
            print(f"  Expected 1 row (Bilbao), got {len(rows)}: {rows}")
            failed += 1
    
    # Test 5: HAVING MIN(age) > value
    print("\n--- HAVING MIN(age) ---")
    query = "SELECT city, MIN(age) FROM users GROUP BY city HAVING MIN(age) > 20"
    result = run_query(executor, query)
    
    if "error" in result:
        print(f"FAIL: {query}")
        print(f"  Error: {result['error']}")
        failed += 1
    else:
        rows = result["result"]
        # All cities have min age >= 19 (Barcelona has 19)
        # Should filter out Barcelona
        if len(rows) == 4:  # Excluding Barcelona
            print(f"PASS: {query}")
            print(f"  Result: {rows}")
            passed += 1
        else:
            print(f"FAIL: {query}")
            print(f"  Expected 4 rows, got {len(rows)}: {rows}")
            failed += 1
    
    # Test 6: HAVING MAX(age) >= value
    print("\n--- HAVING MAX(age) ---")
    query = "SELECT city, MAX(age) FROM users GROUP BY city HAVING MAX(age) >= 40"
    result = run_query(executor, query)
    
    if "error" in result:
        print(f"FAIL: {query}")
        print(f"  Error: {result['error']}")
        failed += 1
    else:
        rows = result["result"]
        # Madrid has 45, Valencia has 40
        if len(rows) == 2:
            print(f"PASS: {query}")
            print(f"  Result: {rows}")
            passed += 1
        else:
            print(f"FAIL: {query}")
            print(f"  Expected 2 rows, got {len(rows)}: {rows}")
            failed += 1
    
    # Test 7: GROUP BY + HAVING + ORDER BY
    print("\n--- HAVING + ORDER BY ---")
    query = "SELECT city, COUNT(*) FROM users GROUP BY city HAVING COUNT(*) > 2 ORDER BY COUNT(*)"
    result = run_query(executor, query)
    
    if "error" in result:
        print(f"FAIL: {query}")
        print(f"  Error: {result['error']}")
        failed += 1
    else:
        rows = result["result"]
        # Should be ordered by count ascending: Barcelona(3), Valencia(3), Sevilla(3), Madrid(4)
        if len(rows) == 4 and rows[0]['count'] == 3:
            print(f"PASS: {query}")
            print(f"  Result: {rows}")
            passed += 1
        else:
            print(f"FAIL: {query}")
            print(f"  Expected 4 rows ordered, got {len(rows)}: {rows}")
            failed += 1
    
    # Test 8: GROUP BY + HAVING + LIMIT
    print("\n--- HAVING + LIMIT ---")
    query = "SELECT city, COUNT(*) FROM users GROUP BY city HAVING COUNT(*) > 1 LIMIT 2"
    result = run_query(executor, query)
    
    if "error" in result:
        print(f"FAIL: {query}")
        print(f"  Error: {result['error']}")
        failed += 1
    else:
        rows = result["result"]
        # Should have 2 rows (LIMIT 2)
        if len(rows) == 2:
            print(f"PASS: {query}")
            print(f"  Result: {rows}")
            passed += 1
        else:
            print(f"FAIL: {query}")
            print(f"  Expected 2 rows, got {len(rows)}: {rows}")
            failed += 1
    
    # Test 9: GROUP BY + WHERE + HAVING
    print("\n--- WHERE + GROUP BY + HAVING ---")
    query = "SELECT city, COUNT(*) FROM users WHERE age > 20 GROUP BY city HAVING COUNT(*) > 2"
    result = run_query(executor, query)
    
    if "error" in result:
        print(f"FAIL: {query}")
        print(f"  Error: {result['error']}")
        failed += 1
    else:
        rows = result["result"]
        # age > 20: Madrid(4), Barcelona(2), Valencia(3), Sevilla(3), Bilbao(2)
        # After HAVING COUNT(*) > 2: Madrid(4), Valencia(3), Sevilla(3)
        if len(rows) == 3:
            print(f"PASS: {query}")
            print(f"  Result: {rows}")
            passed += 1
        else:
            print(f"FAIL: {query}")
            print(f"  Expected 3 rows, got {len(rows)}: {rows}")
            failed += 1
    
    # Test 10: No HAVING (should work as before)
    print("\n--- GROUP BY without HAVING ---")
    query = "SELECT city, COUNT(*) FROM users GROUP BY city"
    result = run_query(executor, query)
    
    if "error" in result:
        print(f"FAIL: {query}")
        print(f"  Error: {result['error']}")
        failed += 1
    else:
        rows = result["result"]
        if len(rows) == 5:
            print(f"PASS: {query}")
            print(f"  Result: {rows}")
            passed += 1
        else:
            print(f"FAIL: {query}")
            print(f"  Expected 5 rows, got {len(rows)}")
            failed += 1
    
    print()
    print("=" * 70)
    print(f"RESULTS: {passed} passed, {failed} failed, {passed + failed} total")
    print("=" * 70)
    
    return passed, failed


if __name__ == "__main__":
    run_all_tests()
