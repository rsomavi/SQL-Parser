#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "schema.h"

// ============================================================================
// Helper Functions
// ============================================================================

// Get the size of a fixed type
int get_type_size(ColumnType type) {
    switch (type) {
        case TYPE_INT:
            return 4;
        case TYPE_FLOAT:
            return 4;
        case TYPE_BOOLEAN:
            return 1;
        case TYPE_VARCHAR:
            return 0; // Variable size
        default:
            return 0;
    }
}

// ============================================================================
// Schema Serialization/Deserialization
// ============================================================================

int schema_serialize(const Schema *schema, char *page_buf) {
    if (!schema || !page_buf) {
        return -1;
    }

    // Clear the buffer first
    memset(page_buf, 0, PAGE_SIZE);

    int offset = 0;

    // Store table_name (fixed size buffer)
    memcpy(page_buf + offset, schema->table_name, MAX_TABLE_NAME);
    offset += MAX_TABLE_NAME;

    // Store num_columns
    memcpy(page_buf + offset, &schema->num_columns, sizeof(int));
    offset += sizeof(int);

    // Store each column
    for (int i = 0; i < schema->num_columns && i < MAX_COLUMNS; i++) {
        const ColumnDef *col = &schema->columns[i];

        // Store name
        memcpy(page_buf + offset, col->name, MAX_COL_NAME);
        offset += MAX_COL_NAME;

        // Store type
        memcpy(page_buf + offset, &col->type, sizeof(ColumnType));
        offset += sizeof(ColumnType);

        // Store max_size
        memcpy(page_buf + offset, &col->max_size, sizeof(int));
        offset += sizeof(int);

        // Store nullable
        memcpy(page_buf + offset, &col->nullable, sizeof(int));
        offset += sizeof(int);

        // Store is_primary_key
        memcpy(page_buf + offset, &col->is_primary_key, sizeof(int));
        offset += sizeof(int);
    }

    return 0;
}

int schema_deserialize(Schema *schema, const char *page_buf) {
    if (!schema || !page_buf) {
        return -1;
    }

    int offset = 0;

    // Read table_name
    memcpy(schema->table_name, page_buf + offset, MAX_TABLE_NAME);
    schema->table_name[MAX_TABLE_NAME - 1] = '\0'; // Ensure null termination
    offset += MAX_TABLE_NAME;

    // Read num_columns
    memcpy(&schema->num_columns, page_buf + offset, sizeof(int));
    offset += sizeof(int);

    // Validate num_columns
    if (schema->num_columns < 0 || schema->num_columns > MAX_COLUMNS) {
        return -1;
    }

    // Read each column
    for (int i = 0; i < schema->num_columns; i++) {
        ColumnDef *col = &schema->columns[i];

        // Read name
        memcpy(col->name, page_buf + offset, MAX_COL_NAME);
        col->name[MAX_COL_NAME - 1] = '\0'; // Ensure null termination
        offset += MAX_COL_NAME;

        // Read type
        memcpy(&col->type, page_buf + offset, sizeof(ColumnType));
        offset += sizeof(ColumnType);

        // Read max_size
        memcpy(&col->max_size, page_buf + offset, sizeof(int));
        offset += sizeof(int);

        // Read nullable
        memcpy(&col->nullable, page_buf + offset, sizeof(int));
        offset += sizeof(int);

        // Read is_primary_key
        memcpy(&col->is_primary_key, page_buf + offset, sizeof(int));
        offset += sizeof(int);
    }

    return 0;
}

int schema_save(const Schema *schema, const char *data_dir) {
    if (!schema || !data_dir) {
        return -1;
    }

    char page_buf[PAGE_SIZE];

    // Serialize schema
    if (schema_serialize(schema, page_buf) != 0) {
        return -1;
    }

    // Use write_page to write to page 0
    write_page(data_dir, schema->table_name, 0, page_buf);

    return 0;
}

int schema_load(Schema *schema, const char *table_name, const char *data_dir) {
    if (!schema || !table_name || !data_dir) {
        return -1;
    }

    char page_buf[PAGE_SIZE];

    // Check if file exists using get_num_pages
    if (get_num_pages(data_dir, table_name) == 0) {
        return -1; // File doesn't exist
    }

    // Use load_page to read from page 0
    load_page(data_dir, table_name, 0, page_buf);

    // Deserialize schema
    return schema_deserialize(schema, page_buf);
}

// ============================================================================
// Row Serialization/Deserialization
// ============================================================================

int row_serialize(const Schema *schema, void **values, int *sizes, char *out_buf, int buf_size, int *out_size) {
    if (!schema || !values || !sizes || !out_buf || !out_size) {
        return -1;
    }

    if (buf_size < 0) {
        return -1;
    }

    int num_columns = schema->num_columns;
    int null_bitmap_size = (num_columns + 7) / 8;

    // Check buffer has space for null_bitmap
    if (buf_size < null_bitmap_size) {
        return -1;
    }

    // Initialize null_bitmap to 0
    memset(out_buf, 0, null_bitmap_size);

    int offset = null_bitmap_size;

    // Process each column
    for (int i = 0; i < num_columns; i++) {
        const ColumnDef *col = &schema->columns[i];

        // Check if NULL (size == 0)
        if (sizes[i] == 0) {
            // Set bit in null_bitmap to indicate NULL
            out_buf[i / 8] |= (1 << (i % 8));
            continue;
        }

        // Get fixed type size
        int fixed_size = get_type_size(col->type);

        // Non-NULL value: write data based on type
        switch (col->type) {
            case TYPE_INT: {
                if (offset + fixed_size > buf_size) return -1;
                int val = *((int *)values[i]);
                memcpy(out_buf + offset, &val, fixed_size);
                offset += fixed_size;
                break;
            }
            case TYPE_FLOAT: {
                if (offset + fixed_size > buf_size) return -1;
                float val = *((float *)values[i]);
                memcpy(out_buf + offset, &val, fixed_size);
                offset += fixed_size;
                break;
            }
            case TYPE_BOOLEAN: {
                if (offset + fixed_size > buf_size) return -1;
                char val = *((char *)values[i]);
                memcpy(out_buf + offset, &val, fixed_size);
                offset += fixed_size;
                break;
            }
            case TYPE_VARCHAR: {
                // 2-byte length prefix + data
                if (offset + 2 + sizes[i] > buf_size) return -1;
                // Store length as 2-byte unsigned short
                unsigned short len = (unsigned short)sizes[i];
                memcpy(out_buf + offset, &len, 2);
                offset += 2;
                // Store data
                memcpy(out_buf + offset, values[i], sizes[i]);
                offset += sizes[i];
                break;
            }
            default:
                return -1;
        }
    }

    *out_size = offset;
    return 0;
}

int row_deserialize(const Schema *schema, const char *buf, int buf_size, void **values, int *sizes) {
    if (!schema || !buf || !values || !sizes) {
        return -1;
    }

    int num_columns = schema->num_columns;
    int null_bitmap_size = (num_columns + 7) / 8;

    // Validate buffer has space for null_bitmap
    if (buf_size < null_bitmap_size) {
        return -1;
    }

    int offset = null_bitmap_size;

    // Process each column
    for (int i = 0; i < num_columns; i++) {
        const ColumnDef *col = &schema->columns[i];

        // Check if NULL (bit i in null_bitmap is 1)
        if (buf[i / 8] & (1 << (i % 8))) {
            sizes[i] = 0;
            continue;
        }

        // Get fixed type size
        int fixed_size = get_type_size(col->type);

        // Non-NULL: read data based on type
        switch (col->type) {
            case TYPE_INT: {
                if (offset + fixed_size > buf_size) return -1;
                int val;
                memcpy(&val, buf + offset, fixed_size);
                *((int *)values[i]) = val;
                sizes[i] = fixed_size;
                offset += fixed_size;
                break;
            }
            case TYPE_FLOAT: {
                if (offset + fixed_size > buf_size) return -1;
                float val;
                memcpy(&val, buf + offset, fixed_size);
                *((float *)values[i]) = val;
                sizes[i] = fixed_size;
                offset += fixed_size;
                break;
            }
            case TYPE_BOOLEAN: {
                if (offset + fixed_size > buf_size) return -1;
                char val;
                memcpy(&val, buf + offset, fixed_size);
                *((char *)values[i]) = val;
                sizes[i] = fixed_size;
                offset += fixed_size;
                break;
            }
            case TYPE_VARCHAR: {
                // 2-byte length prefix + data
                if (offset + 2 > buf_size) return -1;
                unsigned short len;
                memcpy(&len, buf + offset, 2);
                offset += 2;
                if (offset + len > buf_size) return -1;
                // Copy string data (caller provides buffer)
                memcpy(values[i], buf + offset, len);
                sizes[i] = len;
                offset += len;
                break;
            }
            default:
                return -1;
        }
    }

    return offset;
}

// ============================================================================
// Schema Utilities
// ============================================================================

int schema_get_column_index(const Schema *schema, const char *col_name) {
    if (!schema || !col_name) {
        return -1;
    }

    for (int i = 0; i < schema->num_columns; i++) {
        if (strcmp(schema->columns[i].name, col_name) == 0) {
            return i;
        }
    }

    return -1;
}

void schema_print(const Schema *schema) {
    if (!schema) {
        printf("Schema: NULL\n");
        return;
    }

    printf("Table: %s\n", schema->table_name);
    printf("Columns (%d):\n", schema->num_columns);
    printf("----------------------------------------\n");
    printf("%-20s %-10s %-8s %-8s %s\n", "Name", "Type", "MaxSize", "Nullable", "PK");
    printf("----------------------------------------\n");

    for (int i = 0; i < schema->num_columns; i++) {
        const ColumnDef *col = &schema->columns[i];
        const char *type_str;
        switch (col->type) {
            case TYPE_INT: type_str = "INT"; break;
            case TYPE_FLOAT: type_str = "FLOAT"; break;
            case TYPE_BOOLEAN: type_str = "BOOLEAN"; break;
            case TYPE_VARCHAR: type_str = "VARCHAR"; break;
            default: type_str = "UNKNOWN"; break;
        }
        printf("%-20s %-10s %-8d %-8d %s\n",
               col->name,
               type_str,
               col->max_size,
               col->nullable,
               col->is_primary_key ? "YES" : "NO");
    }
    printf("----------------------------------------\n");
}
