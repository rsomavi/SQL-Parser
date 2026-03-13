# SQL Executor - Query Execution Layer
# Executes query plans against a storage backend

from ast_nodes import Condition, LogicalCondition, NotCondition
from planner import SelectPlan, CountPlan, SumPlan, AvgPlan, MinPlan, MaxPlan


class QueryExecutor:
    """Minimal executor for SQL queries"""
    
    def __init__(self, storage):
        """
        Initialize executor with a storage object.
        
        Args:
            storage: Storage object with load_table method.
        """
        self.storage = storage
    
    def execute(self, plan):
        """
        Execute a query plan and return results.
        
        Args:
            plan: Query plan (SelectPlan, etc.)
            
        Returns:
            Query results (list of values for SELECT) or count (int)
        """
        if isinstance(plan, SelectPlan):
            return self._execute_select(plan)
        elif isinstance(plan, CountPlan):
            return self._execute_count(plan)
        elif isinstance(plan, SumPlan):
            return self._execute_sum(plan)
        elif isinstance(plan, AvgPlan):
            return self._execute_avg(plan)
        elif isinstance(plan, MinPlan):
            return self._execute_min(plan)
        elif isinstance(plan, MaxPlan):
            return self._execute_max(plan)
        else:
            raise ValueError(f"Unsupported plan: {type(plan).__name__}")
    
    # =========================================================================
    # Helper: Common table loading and WHERE filtering
    # =========================================================================
    
    def _get_filtered_rows(self, table_name, where):
        """
        Load table and optionally filter by WHERE condition.
        
        Args:
            table_name: Name of the table
            where: Optional WHERE condition (Condition, LogicalCondition, NotCondition)
            
        Returns:
            List of filtered rows
        """
        table = self.storage.load_table(table_name)
        rows = table.get_rows()
        
        if where is not None:
            def matches(row):
                return self._evaluate_condition(row, where, table_name)
            rows = [row for row in rows if matches(row)]
        
        return rows
    
    # =========================================================================
    # Condition evaluation
    # =========================================================================
    
    def _evaluate_condition(self, row, condition, table_name):
        """Recursively evaluate a condition (simple or logical)."""
        if isinstance(condition, Condition):
            if condition.column not in row:
                raise ValueError(f"Column '{condition.column}' not found in table '{table_name}'")
            col_value = row[condition.column]
            op = condition.operator
            value = condition.value
            if op == '=':
                return col_value == value
            elif op == '>':
                return col_value > value
            elif op == '<':
                return col_value < value
            elif op == '>=':
                return col_value >= value
            elif op == '<=':
                return col_value <= value
            else:
                raise ValueError(f"Unknown operator: {op}")
            
        elif isinstance(condition, LogicalCondition):
            left_result = self._evaluate_condition(row, condition.left, table_name)
            right_result = self._evaluate_condition(row, condition.right, table_name)
            op = condition.operator.upper()
            if op == 'AND':
                return left_result and right_result
            elif op == 'OR':
                return left_result or right_result
            else:
                raise ValueError(f"Unknown logical operator: {condition.operator}")
        elif isinstance(condition, NotCondition):
            return not self._evaluate_condition(row, condition.condition, table_name)
        else:
            raise ValueError(f"Unknown condition type: {type(condition).__name__}")
    
    # =========================================================================
    # SELECT execution and helpers
    # =========================================================================
    
    def _execute_select(self, plan: SelectPlan):
        """
        Execute a SELECT query plan.
        
        Args:
            plan: SelectPlan object
            
        Returns:
            List of values from the requested columns, or full rows for SELECT *
        """
        # Load and filter rows
        rows = self._get_filtered_rows(plan.table, plan.where)
        
        # Apply transformations in order
        rows = self._apply_distinct(rows, plan)
        rows = self._apply_order_by(rows, plan)
        rows = self._apply_limit(rows, plan)
        
        return self._apply_projection(rows, plan)
    
    def _apply_distinct(self, rows, plan: SelectPlan):
        """Apply DISTINCT to rows if specified."""
        distinct = getattr(plan, 'distinct', False)
        if not distinct:
            return rows
        
        columns = plan.columns
        seen = set()
        unique_rows = []
        for row in rows:
            # Create key from all column values
            if columns == '*':
                key = tuple(sorted(row.items()))
            else:
                key = tuple(row.get(col) for col in columns)
            if key not in seen:
                seen.add(key)
                unique_rows.append(row)
        return unique_rows
    
    def _apply_order_by(self, rows, plan: SelectPlan):
        """Apply ORDER BY to rows if specified."""
        order_by = plan.order_by
        if order_by is None:
            return rows
        
        # Validate ORDER BY column exists
        if rows:
            if order_by not in rows[0]:
                raise ValueError(f"Column '{order_by}' not found in table '{plan.table}'")
        
        return sorted(rows, key=lambda r: r[order_by])
    
    def _apply_limit(self, rows, plan: SelectPlan):
        """Apply LIMIT to rows if specified."""
        limit = plan.limit
        if limit is None:
            return rows
        return rows[:limit]
    
    def _apply_projection(self, rows, plan: SelectPlan):
        """Apply column projection (SELECT columns)."""
        columns = plan.columns
        table_name = plan.table
        
        # Handle SELECT *
        if columns == '*':
            return [row.copy() for row in rows]
        
        # Extract the requested columns from each row
        results = []
        for row in rows:
            result_row = {}
            for column_name in columns:
                if column_name not in row:
                    raise ValueError(f"Column '{column_name}' not found in table '{table_name}'")
                result_row[column_name] = row[column_name]
            results.append(result_row)
        
        return results
    
    # =========================================================================
    # Aggregation execution
    # =========================================================================
    
    def _execute_count(self, plan: CountPlan):
        """
        Execute a COUNT(*) query plan.
        
        Args:
            plan: CountPlan object
            
        Returns:
            Integer count of rows
        """
        rows = self._get_filtered_rows(plan.table, plan.where)
        return len(rows)
    
    def _execute_sum(self, plan: SumPlan):
        """
        Execute a SUM(column) query plan.
        
        Args:
            plan: SumPlan object
            
        Returns:
            Sum of column values
        """
        rows = self._get_filtered_rows(plan.table, plan.where)
        
        total = 0
        for row in rows:
            if plan.column not in row:
                raise ValueError(f"Column '{plan.column}' not found in table '{plan.table}'")
            total += row[plan.column]
        
        return total
    
    def _execute_avg(self, plan: AvgPlan):
        """
        Execute a AVG(column) query plan.
        
        Args:
            plan: AvgPlan object
            
        Returns:
            Average of column values
        """
        rows = self._get_filtered_rows(plan.table, plan.where)
        
        if not rows:
            return 0
        
        total = 0
        for row in rows:
            if plan.column not in row:
                raise ValueError(f"Column '{plan.column}' not found in table '{plan.table}'")
            total += row[plan.column]
        
        return total / len(rows)
    
    def _execute_min(self, plan: MinPlan):
        """
        Execute a MIN(column) query plan.
        
        Args:
            plan: MinPlan object
            
        Returns:
            Minimum value of column
        """
        rows = self._get_filtered_rows(plan.table, plan.where)
        
        if not rows:
            return 0
        
        min_value = None
        for row in rows:
            if plan.column not in row:
                raise ValueError(f"Column '{plan.column}' not found in table '{plan.table}'")
            value = row[plan.column]
            if min_value is None or value < min_value:
                min_value = value
        
        return min_value
    
    def _execute_max(self, plan: MaxPlan):
        """
        Execute a MAX(column) query plan.
        
        Args:
            plan: MaxPlan object
            
        Returns:
            Maximum value of column
        """
        rows = self._get_filtered_rows(plan.table, plan.where)
        
        if not rows:
            return 0
        
        max_value = None
        for row in rows:
            if plan.column not in row:
                raise ValueError(f"Column '{plan.column}' not found in table '{plan.table}'")
            value = row[plan.column]
            if max_value is None or value > max_value:
                max_value = value
        
        return max_value
