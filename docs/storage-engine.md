# Storage Engine

The storage engine is implemented in C and manages all physical data on disk. It is organized in four layers stacked on top of each other: disk I/O, slotted pages, heap files, and schema management. The buffer manager sits on top of all four and is documented separately in `buffer-pool.md`.

## File overview

| File | Responsibility |
|------|---------------|
| `disk.c / disk.h` | Raw page I/O: open, seek, read, write `.db` files |
| `page.c / page.h` | Slotted page layout: insert, delete, read rows within a page |
| `heap.c / heap.h` | Multi-page heap file: find space across pages, RowID encoding |
| `schema.c / schema.h` | Table schema: column definitions, serialization, row binary format |
| `buffer_frame.c / .h` | Buffer pool frames and pool state |
| `page_table.c / .h` | Hash map `(table_name, page_id) -> frame_id` |
| `eviction_policy.c / .h` | Pluggable replacement policies (see `buffer-pool.md`) |

---

## Disk layer — `disk.c`

The disk layer handles raw I/O. Each table is stored in a single file `data/<table>.db` divided into fixed-size pages of 4096 bytes (`PAGE_SIZE`).

### Functions

```c
int write_page(const char *data_dir, const char *table, int page_id, char *page);
int load_page (const char *data_dir, const char *table, int page_id, char *page);
int read_page (const char *data_dir, const char *table, int page_id);
int get_num_pages(const char *data_dir, const char *table);
```

`write_page` opens the file in `rb+` mode (creates it if it does not exist), seeks to `page_id * PAGE_SIZE`, and writes exactly 4096 bytes. Returns 0 on success, -1 on error.

`load_page` opens the file in `rb` mode, validates that `page_id` is within bounds by checking the file size, then reads exactly 4096 bytes into the caller-supplied buffer. Returns 0 on success, -1 on error.

`get_num_pages` returns `file_size / PAGE_SIZE`. If the file size is not a multiple of 4096 it logs a corruption error and returns -1.

### File layout

```
data/empleados.db
  offset 0x0000  page 0   schema
  offset 0x1000  page 1   first data page
  offset 0x2000  page 2   second data page (if needed)
  ...
```

Page 0 is always reserved for the schema. Data starts at page 1.

---

## Page layer — `page.c`

A page is a 4096-byte buffer with a **slotted page** layout. Rows grow from the end of the page towards the center; the slot directory grows from the start of the page towards the center. The page is full when the two sides meet.

### Page layout

```
┌────────────────────────────────────────────────┐         |  offset 0                                      |
│  PageHeader                                    │
│    num_slots         (int, 4 bytes)            │
│    free_space_start  (int, 4 bytes)            │
│    free_space_end    (int, 4 bytes)            │
├────────────────────────────────────────────────┤
│  Slot directory                                │
│    slot[0]  → offset of row 0  (int, 4 bytes)  │
│    slot[1]  → offset of row 1  (int, 4 bytes)  │
│    slot[N]  → -1 if deleted                    │  
│                    ↓ grows down                │
├ ─ ─ ─ ─ ─ ─ ─ ─ free space ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┤    
│                    ↑ grows up                  │
│  Row data (binary)                             │
│    [4 bytes: row_size][row_size bytes: data]   │
│    [4 bytes: row_size][row_size bytes: data]   │
└────────────────────────────────────────────────┘  offset 4095
```

### Functions

```c
void  init_page   (char *page);
int   insert_row  (char *page, const void *data, int data_size);
void  delete_row  (char *page, int slot_id);
char *read_row    (char *page, int slot_id);
int   get_row_size(char *page, int slot_id);
```

`init_page` zeroes the page and initializes the header. Sets `free_space_start` to `sizeof(PageHeader)` and `free_space_end` to `PAGE_SIZE`. All slot directory entries are set to -1.

`insert_row` checks available space (`free_space_end - free_space_start`). If a deleted slot exists it is reused; otherwise a new slot is appended. The row is written at `free_space_end - (sizeof(int) + data_size)`: first the 4-byte size, then the data bytes. Returns the slot index on success, -1 if the page is full.

`delete_row` sets `slot_dir[slot_id] = -1`. The space is not reclaimed — compaction is not implemented in v1.

`read_row` returns a pointer directly into the page buffer at the row data (skipping the 4-byte size prefix). The caller must not free this pointer.

---

## Heap layer — `heap.c`

The heap layer manages a multi-page file for a single table. It finds space for new rows across pages and encodes their location as a RowID.

### RowID encoding

```c
RowID = (page_id << 16) | slot_id
```

Examples:
- `RowID 65536` = page 1, slot 0  (`1 << 16 | 0`)
- `RowID 65537` = page 1, slot 1  (`1 << 16 | 1`)
- `RowID 131072` = page 2, slot 0  (`2 << 16 | 0`)

Decode:
```c
int page_id = rowid >> 16;
int slot_id = rowid & 0xFFFF;
```

### heap_insert_bm

The main insert function used by the server. It goes through the buffer manager rather than calling disk I/O directly:

```c
int heap_insert_bm(const char *data_dir, const char *table_name,
                   const void *data, int size, BufferManager *bm);
```

Algorithm:

1. Call `get_num_pages` to find existing pages.
2. For each existing data page (starting from page 1), call `bm_fetch_page` and try `insert_row`. If it fits, call `bm_unpin_page(dirty=1)` and return the RowID.
3. If no page has space, create a new page: write an empty initialized page to disk with `write_page`, then load it via `bm_fetch_page`, insert the row, and unpin with `dirty=1`.

The dirty flag ensures the modified page is flushed to disk when the buffer manager evicts it or when `bm_destroy` is called at server shutdown.

### insert_into_table

An older direct-to-disk version that does not go through the buffer pool. Used in tests and the migration tool. Not used by the server.

```c
int insert_into_table(const char *data_dir, const char *table,
                      const void *data, int size);
```

---

## Schema layer — `schema.c`

The schema describes the column structure of a table. It is serialized into page 0 of the `.db` file and loaded from there on every server start.

### Data structures

```c
typedef enum { TYPE_INT, TYPE_FLOAT, TYPE_BOOLEAN, TYPE_VARCHAR } ColumnType;

typedef struct {
    char       name[64];
    ColumnType type;
    int        max_size;       // VARCHAR(N): max bytes. Fixed types: their natural size.
    int        nullable;       // 1 = nullable, 0 = NOT NULL
    int        is_primary_key;
} ColumnDef;

typedef struct {
    char      table_name[64];
    int       num_columns;
    ColumnDef columns[32];     // MAX_COLUMNS = 32
} Schema;
```

### Schema serialization (page 0)

`schema_serialize` writes the schema into a 4096-byte buffer with a simple flat layout:

```
[table_name: 64 bytes]
[num_columns: 4 bytes]
per column:
  [name:          64 bytes]
  [type:           4 bytes]
  [max_size:       4 bytes]
  [nullable:       4 bytes]
  [is_primary_key: 4 bytes]
```

`schema_save` calls `schema_serialize` and then `write_page` to page 0. `schema_load` calls `load_page` on page 0 and then `schema_deserialize`.

### Row binary format

`row_serialize` encodes a row into a compact binary format. The same format is used by the Python layer (`storage_server.py`) to serialize rows before sending them over TCP.

```
[null_bitmap: ceil(num_columns / 8) bytes]
per column (skipped if NULL):
  INT:     4 bytes, native endian int32
  FLOAT:   4 bytes, native endian float32
  BOOL:    1 byte  (0 or 1)
  VARCHAR: 2 bytes uint16 (length) + N bytes of data
```

The null bitmap has one bit per column. Bit `i` is set to 1 if column `i` is NULL. Columns marked NULL in the bitmap are not written to the data section, which keeps rows compact.

Example — table with columns `(id INT, nombre VARCHAR(50), salario INT)`, row `(1, 'Ana', 35000)`:

```
null_bitmap:  0x00 0x00          (1 byte, all columns non-null, ceil(3/8)=1)
id:           0x01 0x00 0x00 0x00  (int32 little-endian: 1)
nombre len:   0x03 0x00            (uint16: 3)
nombre data:  0x41 0x6E 0x61       ('A' 'n' 'a')
salario:      0xB8 0x88 0x00 0x00  (int32 little-endian: 35000)
```

`row_deserialize` reads the null bitmap first, then reads each non-NULL column in the same order, allocating values into caller-supplied pointer arrays.

---

## Type sizes

| Type | Stored size |
|------|------------|
| INT | 4 bytes |
| FLOAT | 4 bytes |
| BOOLEAN | 1 byte |
| VARCHAR(N) | 2 bytes (length) + actual string length |
| NULL (any type) | 0 bytes (marked in bitmap only) |

---

## Constants

| Constant | Value | Defined in |
|----------|-------|-----------|
| PAGE_SIZE | 4096 | `disk.h` |
| MAX_COLUMNS | 32 | `schema.h` |
| MAX_COL_NAME | 64 | `schema.h` |
| MAX_TABLE_NAME | 64 | `schema.h` |
| BUFFER_POOL_MAX_FRAMES | 1024 | `buffer_frame.h` |
