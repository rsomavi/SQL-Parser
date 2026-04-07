# Tests

The project has two independent test suites: one for the storage engine written in C, and one for the SQL engine written in Python. They test different layers and have different requirements — the C tests run standalone, the Python tests require the server running.

---

## Storage engine tests (C) — `storage-engine/tests/`

Nine test files covering each layer of the storage engine independently. All tests are self-contained — they write to temporary files and clean up after themselves. No server required.

### Building and running

Con el Makefile incluido en `storage-engine/tests/`:

```bash
cd storage-engine/tests/

make        # compila todos los tests
make run    # compila y ejecuta todos en secuencia
make clean  # elimina los binarios

# Un test concreto
make test_schema
./test_schema
```

Compilación manual si necesario — cualquier test que incluya `heap.c` requiere también el buffer pool completo:

```bash
gcc -Wall -Wextra -g -o test_disk test_disk.c ../disk.c

gcc -Wall -Wextra -g -o test_schema test_schema.c \
  ../schema.c ../heap.c ../page.c ../disk.c \
  ../buffer_manager.c ../buffer_frame.c ../page_table.c ../eviction_policy.c
```

### Test suites

**test_disk.c** — raw page I/O

Covers `write_page`, `load_page`, `get_num_pages`. Tests include write/read roundtrip, page isolation (writing page 3 does not corrupt page 1), binary data with null bytes, bounds checking on out-of-range page IDs, and a stress test writing 100 pages sequentially.

**test_page.c** — slotted page format

Covers `init_page`, `insert_row`, `delete_row`, `read_row`, `get_row_size`. Tests include basic insert and read, slot reuse after deletion, page capacity (filling a page until full), binary data with null bytes, and multi-row scenarios with interleaved deletes.

**test_heap.c** — heap file and RowID encoding

Covers `insert_into_table`, `scan_table_raw`, RowID encoding/decoding. Tests include RowID encoding correctness (`page_id << 16 | slot_id`), page 0 reservation (data always starts at page 1), overflow to a second page when the first is full, two tables with independent page sequences, and a stress test inserting many rows.

**test_schema.c** — schema serialization

Covers `schema_save`, `schema_load`, `schema_serialize`, `schema_deserialize`, `row_serialize`, `row_deserialize`. Tests include all four column types (INT, FLOAT, BOOLEAN, VARCHAR), MAX_COLUMNS limit (32), save and reload roundtrip, null bitmap for nullable columns, INT_MIN/INT_MAX boundary values, and VARCHAR at 255 bytes.

**test_integration.c** — schema + heap end-to-end

Tests the full pipeline: create schema, insert rows, reload from disk, verify values. Covers schema + heap working together, reload simulation (close and reopen), and two tables interleaved in the same data directory.

**test_buffer_frame.c** — BufferPool and frame lifecycle

Covers `bp_init`, `bp_load_frame`, `bp_pin_frame`, `bp_unpin_frame`, `bp_evict_frame`. Tests include frame state transitions (FREE → OCCUPIED → PINNED → OCCUPIED → FREE), dirty flag propagation on unpin, eviction blocked on PINNED frames, metrics (hits, misses, evictions), and ref_bit and last_access updates.

**test_page_table.c** — hash map correctness

Covers `pt_insert`, `pt_lookup`, `pt_remove`, `pt_clear`. Tests include basic insert and lookup, update of existing entry (same key, new frame_id), removal and re-lookup returning -1, multiple tables with same page_id (no collision), collision handling within same bucket, and `pt_clear` freeing all entries.

**test_eviction_policy.c** — all four replacement policies

Covers NoCache, Clock, LRU, OPT. Tests include NoCache always evicting the first OCCUPIED frame, Clock advancing the hand and giving second chances via ref_bit, LRU selecting the frame with the oldest last_access timestamp, and OPT selecting the frame whose next access is farthest in the trace (or never accessed again).

**test_buffer_manager.c** — full buffer manager integration

Covers `bm_fetch_page`, `bm_unpin_page`, `bm_destroy`. Tests include cache hit on second fetch of the same page, eviction triggered when pool is full, dirty page write-back on eviction, metrics accumulation across operations, and flush of dirty frames on `bm_destroy`.

### Summary

| Suite | Tests | Layer |
|-------|-------|-------|
| test_disk.c | 38 | Disk I/O |
| test_page.c | 33 | Slotted page |
| test_heap.c | 30 | Heap file |
| test_schema.c | 157 | Schema + row serialization |
| test_integration.c | 14 | Schema + heap end-to-end |
| test_buffer_frame.c | 65 | Buffer pool frames |
| test_page_table.c | 43 | Page table hash map |
| test_eviction_policy.c | 36 | NoCache / Clock / LRU / OPT |
| test_buffer_manager.c | 29 | Full buffer manager |

---

## SQL engine tests (Python) — `sql-engine/tests/`

Eight test files covering the SQL pipeline from the lexer to the full TCP round-trip. Most tests use `MemoryStorage` (in-memory dict, no server required). The integration tests against `ServerStorage` require the server running on `localhost:5433`.

### Running

```bash
cd sql-engine/

# Run all SQL engine tests (no server required)
python3 tests/run_tests.py

# Run individual test files
python3 tests/test_lexer_tokens.py
python3 tests/test_queries.py
python3 tests/test_group_by.py
python3 tests/test_having.py
python3 tests/test_sql_engine.py

# These require the server running on localhost:5433
python3 tests/test_server_storage.py
python3 tests/test_create.py
python3 tests/test_insert.py
```

### Test files

**test_lexer_tokens.py** — lexer tokenization

Verifies that the lexer produces the correct token types and values for SQL keywords, identifiers, operators, string literals, and numbers. Runs without server or storage.

**test_queries.py** — SQL execution with MemoryStorage

Uses a fixed in-memory dataset (users, orders tables). Tests include SELECT *, SELECT with column list, WHERE with single condition, WHERE with AND/OR/NOT, ORDER BY ASC/DESC, LIMIT, DISTINCT, INNER JOIN, and SELECT with qualified column references (`table.column`).

**test_group_by.py** — GROUP BY semantics

Verifies GROUP BY with COUNT(\*), SUM, AVG, MIN, MAX. Tests single-column and multi-column grouping, GROUP BY with WHERE, and correct aggregation per group.

**test_having.py** — HAVING clause

Tests HAVING with COUNT(\*), SUM, AVG comparisons. Verifies that HAVING filters groups correctly after aggregation, and that rows not meeting the HAVING condition are excluded from results.

**test_sql_engine.py** — parser and planner unit tests

Tests that the parser produces correct AST nodes and that the planner produces correct plan objects for each query type. Does not execute queries — only verifies the parse and plan stages.

**test_server_storage.py** — ServerStorage integration (requires server)

23 tests across 8 blocks. Tests ping, load_table, column names and types, value correctness, multiple tables, cache warming (hit rate increases on second load), and error handling (non-existent table raises RuntimeError). Also tests executor integration with SELECT \* and SELECT WHERE via TCP.

**test_create.py** — CREATE TABLE via TCP (requires server)

Tests CREATE TABLE end-to-end: sends request to server, verifies table is created, verifies schema is readable via SCHEMA command.

**test_insert.py** — INSERT via TCP (requires server)

32 tests across 6 blocks covering ROW_ID encoding, INSERT with explicit column list, INSERT without column list (`columns=None`), INT/VARCHAR round-trips (zero, negative, INT32 max, long VARCHAR, single char), bulk insert integrity (10 rows all readable), error handling (table not found), and persistence across server restarts.

### Summary

| Suite | Tests | Requires server |
|-------|-------|----------------|
| test_lexer_tokens.py | — | No |
| test_queries.py | — | No |
| test_group_by.py | — | No |
| test_having.py | — | No |
| test_sql_engine.py | 6 | No |
| test_server_storage.py | 23 | Yes |
| test_create.py | — | Yes |
| test_insert.py | 32 | Yes |

---

## What is not yet tested

These areas do not have tests yet and are planned:

- DELETE — pending implementation
- UPDATE — pending implementation
- Buffer pool eviction under load (framework experiments)
- Protocol error handling (malformed requests, oversized payloads)
- Concurrent clients (v1 is single-client by design)
