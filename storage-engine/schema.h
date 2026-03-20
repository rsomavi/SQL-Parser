#ifndef SCHEMA_H
#define SCHEMA_H

#include "disk.h"

// ============================================================================
// Type Definitions
// ============================================================================

typedef enum {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_BOOLEAN,
    TYPE_VARCHAR
} ColumnType;

#define MAX_COLUMNS 32
#define MAX_COL_NAME 64
#define MAX_TABLE_NAME 64

typedef struct {
    char name[MAX_COL_NAME];
    ColumnType type;
    int max_size;      // VARCHAR(N): max bytes, fixed types: their size
    int nullable;      // 1=nullable, 0=NOT NULL
    int is_primary_key;
} ColumnDef;

typedef struct {
    char table_name[MAX_TABLE_NAME];
    int num_columns;
    ColumnDef columns[MAX_COLUMNS];
} Schema;

// ============================================================================
// Schema Serialization/Deserialization
// ============================================================================

// Serialize schema into PAGE_SIZE buffer, returns 0 on success, -1 on error
int schema_serialize(const Schema *schema, char *page_buf);

// Deserialize schema from PAGE_SIZE buffer, returns 0 on success, -1 on error
int schema_deserialize(Schema *schema, const char *page_buf);

// Save schema to page 0 of table .db file, create file if not exists
int schema_save(const Schema *schema, const char *data_dir);

// Load schema from page 0 of table .db file
int schema_load(Schema *schema, const char *table_name, const char *data_dir);

// ============================================================================
// Row Serialization/Deserialization
// ============================================================================

// Serialize a row given parallel arrays of values and sizes
// sizes[i] = actual size for VARCHAR, 0 for NULL
// buf_size = max buffer size, out_size = pointer to store actual bytes written
// Returns 0 on success, -1 on error
int row_serialize(const Schema *schema, void **values, int *sizes, char *out_buf, int buf_size, int *out_size);

// Deserialize row bytes into pre-allocated parallel arrays
// buf_size = size of input buffer
// Returns bytes read, -1 on error
int row_deserialize(const Schema *schema, const char *buf, int buf_size, void **values, int *sizes);

// ============================================================================
// Schema Utilities
// ============================================================================

// Return column index by name or -1 if not found
int schema_get_column_index(const Schema *schema, const char *col_name);

// Print schema to stdout for debugging
void schema_print(const Schema *schema);

#endif /* SCHEMA_H */
