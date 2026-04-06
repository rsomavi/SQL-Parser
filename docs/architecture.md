# Architecture

minidbms is a database management system implemented from the ground up in C and Python, without relying on any existing database libraries or storage frameworks. The system is split into two major subsystems connected via a TCP socket: a **SQL engine** (Python) that parses and plans queries, and a **storage engine** (C) that manages pages, buffer pool, and disk I/O.

## Layer diagram

```
┌───────────────────────────────────────────────────┐
│                SQL Client (Python)                │
│                                                   │
│ sql> SELECT * FROM empleados WHERE salario > 30000│
└─────────────────────┬─────────────────────────────┘
                        │
        ┌───────────────▼───────────────┐
        │           SQL Engine          │
        │                               │
        │  Lexer  →  Parser  →  AST     │
        │               ↓               │
        │           Planner             │
        │               ↓               │
        │           Executor            │
        │               ↓               │
        │        ServerStorage          │
        └───────────────┬───────────────┘
                        │  TCP :5433
                        │  MINIDBMS-RESP v1.2
        ┌───────────────▼───────────────┐
        │         C Server              │
        │                               │
        │   protocol_read_request()     │
        │          ↓                    │
        │   handler_dispatch()          │
        │          ↓                    │
        │   handler_{ping,scan,         │
        │     schema,create,insert}     │
        └───────────────┬───────────────┘
                        │
        ┌───────────────▼───────────────┐
        │         Buffer Manager        │
        │                               │
        │   bm_fetch_page()             │
        │   bm_unpin_page(dirty)        │
        │          ↓                    │
        │   BufferPool  +  PageTable    │
        │          ↓                    │
        │   EvictionPolicy              │
        │   (NoCache/Clock/LRU/OPT)     │
        └───────────────┬───────────────┘
                        │
        ┌───────────────▼───────────────┐
        │         Storage Engine        │
        │                               │
        │   Heap  →  Page  →  Disk      │
        │   Schema (page 0)             │
        │                               │
        │   ../data/tabla.db            │
        └───────────────────────────────┘
```

## Components

### SQL Engine (Python) — `sql-engine/`

Parses SQL text and executes queries against the storage layer.

| File | Responsibility |
|------|---------------|
| `lexer.py` | Tokenizes SQL strings. Keywords, operators, literals, identifiers. Uses PLY. |
| `parser.py` | PLY/Yacc grammar → AST. Handles SELECT, INSERT, CREATE TABLE, operator precedence (NOT > AND > OR). |
| `ast_nodes.py` | AST node types: SelectQuery, InsertQuery, CreateTableQuery, Condition, LogicalCondition, etc. |
| `planner.py` | Transforms AST into executable plan objects (SelectPlan, InsertPlan, CreateTablePlan...). |
| `executor.py` | Runs plans: WHERE, INNER JOIN, GROUP BY, HAVING, DISTINCT, ORDER BY, LIMIT, aggregates. |
| `storage_server.py` | `ServerStorage` — connects to the C server via TCP, serializes/deserializes binary rows. |
| `ui.py` | Terminal dashboard (QUERY / AST / TOKENS / RESULT panels). |
| `main.py` | Entry point — interactive REPL. |

### C Server — `server/`

TCP server on port 5433. Accepts one client connection at a time (v1). Owns the buffer manager.

| File | Responsibility |
|------|---------------|
| `server.c / server.h` | Lifecycle: `server_init`, `server_run`, `server_stop`, `server_destroy`. Owns `BufferManager`. Signal handler for SIGINT/SIGTERM flushes dirty pages before exit. |
| `protocol.c / protocol.h` | MINIDBMS-RESP v1.2 framing. Parses requests, builds `ResponseBuf`, sends responses. Operations: PING, SCAN, SCHEMA, CREATE, INSERT. |
| `handlers.c / handlers.h` | One handler per operation. `handler_dispatch` routes to the correct one. Each handler validates, calls the storage layer, and writes the response. |

### Buffer Manager — `storage-engine/buffer_manager.c`

Sits between the server handlers and disk. Keeps frequently used pages in RAM.

```
BufferManager
  ├── BufferPool     — array of up to 1024 frames (4 KB each)
  ├── PageTable      — hash map (table_name, page_id) → frame_id, O(1) lookup
  └── EvictionPolicy — pluggable: NoCache / Clock / LRU / OPT
```

Public API used by handlers:

```c
char *bm_fetch_page(bm, table_name, page_id);   // pin page into RAM
int   bm_unpin_page(bm, table_name, page_id, dirty);  // release, mark dirty if modified
```

On `bm_destroy` (called at server shutdown), all dirty frames are flushed to disk with `write_page`.

Eviction policies:

| Policy | Algorithm | Use |
|--------|-----------|-----|
| NoCache | Always evicts first OCCUPIED frame. Hit rate = 0%. | Baseline for experiments |
| Clock | Rotating hand + ref_bit. O(1) amortized. | Used by PostgreSQL (Clock Sweep) |
| LRU | Evicts frame with oldest `last_access` timestamp. O(n). | Strong temporal locality |
| OPT | Belady's optimal — offline, requires pre-recorded trace. | Theoretical upper bound |

### Storage Engine (C) — `storage-engine/`

Manages the physical layout of data on disk.

| File | Responsibility |
|------|---------------|
| `disk.c / disk.h` | Raw page I/O: `write_page`, `load_page`, `get_num_pages`. Files: `data/<table>.db`. `PAGE_SIZE = 4096` bytes. |
| `page.c / page.h` | Slotted page format. `init_page`, `insert_row`, `delete_row`, `read_row`. Header: `num_slots`, `free_space_start`, `free_space_end`. |
| `heap.c / heap.h` | Multi-page heap file. `heap_insert_bm` inserts via buffer pool. Page 0 reserved for schema. `RowID = (page_id << 16) \| slot_id`. |
| `schema.c / schema.h` | Table schema: `TYPE_INT`, `TYPE_FLOAT`, `TYPE_BOOLEAN`, `TYPE_VARCHAR`. Serialized to page 0. `row_serialize` / `row_deserialize` with null bitmap. |
| `buffer_frame.c / .h` | `BufferPool` and `BufferFrame`. Frame states: FREE / OCCUPIED / PINNED. |
| `page_table.c / .h` | Hash map `(table_name, page_id) → frame_id`. |
| `eviction_policy.c / .h` | Abstract interface + NoCache, Clock, LRU, OPT implementations. |

## Data flow: INSERT

```
sql> INSERT INTO empleados (id, nombre) VALUES (1, 'Ana')

1. Lexer           → tokens: INSERT, INTO, ID(empleados), ...
2. Parser          → InsertQuery(table='empleados', columns=[id,nombre], values=[1,'Ana'])
3. Planner         → InsertPlan
4. Executor        → storage.insert_row('empleados', columns, values)
5. ServerStorage   → _fetch_schema() via TCP SCHEMA request
                   → _serialize_row() → binary: null_bitmap + INT32 + VARCHAR(len+bytes)
                   → TCP: "INSERT empleados 11\n<11 bytes>"
6. handler_insert  → heap_insert_bm(data_dir, 'empleados', payload, size, &bm)
7. heap_insert_bm  → bm_fetch_page(bm, 'empleados', page_id)
                   → insert_row(page, data, size)  [modifies RAM]
                   → bm_unpin_page(bm, 'empleados', page_id, dirty=1)
8. Response        → "OK\nROW_ID 65536\n...END\n"
9. On Ctrl+C       → bm_destroy() → write_page() flushes dirty frame to disk
```

## Data flow: SELECT

```
sql> SELECT * FROM empleados

1–4. Same parse/plan/execute path as INSERT
5. ServerStorage   → TCP: "SCAN empleados"
6. handler_scan    → iterates pages via bm_fetch_page
                   → sends: "OK\nROWS N\n<size>\n<binary row>\n..."
7. ServerStorage   → _deserialize_row() per row → Python dict
8. Executor        → applies WHERE, ORDER BY, etc. in Python
9. ui.py           → renders result table
```

## Binary row format

Rows are serialized in the same format by both `schema.c` (C) and `storage_server.py` (Python):

```
[null_bitmap: ceil(n_cols/8) bytes]
per column (if not NULL):
  INT:     4 bytes, little-endian int32
  FLOAT:   4 bytes, little-endian float32
  BOOL:    1 byte
  VARCHAR: 2 bytes uint16 (length) + N bytes UTF-8
```

## Physical file layout

```
data/empleados.db
  Page 0  (bytes 0x0000–0x0FFF):  schema
  Page 1  (bytes 0x1000–0x1FFF):  data rows (slot 0, slot 1, ...)
  Page N  (bytes N*4096–...):     overflow pages
```

Page 0 is always the schema. Data starts at page 1. `RowID 65536 = (1 << 16) | 0` means page 1, slot 0.

## Protocol (summary)

See `docs/protocol.md` for full spec. Quick reference:

| Operation | Request | Response |
|-----------|---------|----------|
| PING | `PING\n` | `OK\nPONG\n...END\n` |
| SCAN | `SCAN <table>\n` | `OK\nROWS N\n<binary rows>...END\n` |
| SCHEMA | `SCHEMA <table>\n` | `OK\nCOLUMNS N\n<col:type:size:nullable:pk>...END\n` |
| CREATE | `CREATE <table> <col_defs>\n` | `OK\nCREATED <table>\n...END\n` |
| INSERT | `INSERT <table> <size>\n<bytes>` | `OK\nROW_ID N\n...END\n` |

## Current status

| Feature | Status |
|---------|--------|
| SELECT (WHERE, JOIN, GROUP BY, HAVING, ORDER BY, DISTINCT, LIMIT) | done |
| CREATE TABLE | done |
| INSERT (with and without column list) | done |
| Buffer pool (NoCache / Clock / LRU / OPT) | done |
| TCP server + MINIDBMS-RESP protocol | done |
| Persistence (signal handler flush) | done |
| DELETE | pending |
| UPDATE | pending |
| WAL (Write-Ahead Log) | pending |
| B+ tree indexes | pending |
| Comparative experiments (NoCache vs LRU vs Clock vs OPT) | pending |
