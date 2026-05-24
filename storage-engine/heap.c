#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "heap.h"

#define DEFAULT_DATA_DIR "../data"

static int heap_extract_int_column(const Schema *schema, const char *row_data,
                                   int row_size, int column_index, int *value_out) {
    int null_bitmap_size;
    int offset;

    if (!schema || !row_data || row_size < 0 || !value_out ||
        column_index < 0 || column_index >= schema->num_columns) {
        return -1;
    }

    null_bitmap_size = (schema->num_columns + 7) / 8;
    if (row_size < null_bitmap_size) {
        return -1;
    }

    offset = null_bitmap_size;

    for (int i = 0; i < schema->num_columns; i++) {
        const ColumnDef *col = &schema->columns[i];

        if (((unsigned char)row_data[i / 8] & (1u << (i % 8))) != 0) {
            if (i == column_index) {
                return -1;
            }
            continue;
        }

        switch (col->type) {
            case TYPE_INT: {
                int value;

                if (offset + 4 > row_size) {
                    return -1;
                }

                memcpy(&value, row_data + offset, sizeof(int));
                if (i == column_index) {
                    *value_out = value;
                    return 0;
                }

                offset += 4;
                break;
            }
            case TYPE_FLOAT:
                if (offset + 4 > row_size) {
                    return -1;
                }
                offset += 4;
                break;
            case TYPE_BOOLEAN:
                if (offset + 1 > row_size) {
                    return -1;
                }
                offset += 1;
                break;
            case TYPE_VARCHAR: {
                unsigned short len;

                if (offset + 2 > row_size) {
                    return -1;
                }

                memcpy(&len, row_data + offset, 2);
                offset += 2;

                if (offset + (int)len > row_size) {
                    return -1;
                }

                offset += (int)len;
                break;
            }
            default:
                return -1;
        }
    }

    return -1;
}

static void heap_try_index_insert(const char *table_name, const Schema *schema,
                                  const char *row_data, int row_size, RowID row_id,
                                  IndexManager *im, int *duplicate_out) {
    IndexHandle *handle;
    int pk_column;
    int pk_value;
    int rc;

    if (duplicate_out) {
        *duplicate_out = 0;
    }

    if (!table_name || !schema || !row_data || !im || !schema_has_index(schema)) {
        return;
    }

    pk_column = schema_get_pk_column(schema);
    if (pk_column < 0 || schema->columns[pk_column].type != TYPE_INT) {
        return;
    }

    if (heap_extract_int_column(schema, row_data, row_size, pk_column, &pk_value) != 0) {
        return;
    }

    handle = index_manager_get(im, table_name);
    if (!handle) {
        return;
    }

    rc = btree_insert(handle->fd, &handle->meta, pk_value, row_id);
    if (rc == -1) {
        fprintf(stderr, "[heap] failed to insert key %d into index for table '%s'\n", pk_value, table_name);
    } else if (rc == 1 && duplicate_out) {
        *duplicate_out = 1;
    }
}

static void heap_try_index_delete(const char *table_name, const Schema *schema,
                                  const char *row_data, int row_size,
                                  IndexManager *im) {
    IndexHandle *handle;
    int pk_column;
    int pk_value;

    if (!table_name || !schema || !row_data || !im || !schema_has_index(schema)) {
        return;
    }

    pk_column = schema_get_pk_column(schema);
    if (pk_column < 0 || schema->columns[pk_column].type != TYPE_INT) {
        return;
    }

    if (heap_extract_int_column(schema, row_data, row_size, pk_column, &pk_value) != 0) {
        return;
    }

    handle = index_manager_get(im, table_name);
    if (!handle) {
        return;
    }

    if (btree_delete(handle->fd, &handle->meta, pk_value) != 0) {
        fprintf(stderr, "[heap] failed to delete key %d from index for table '%s'\n", pk_value, table_name);
    }
}

// ============================================================================
// Heap File Functions
// ============================================================================

int insert_into_table(const char *data_dir, const char *table, const void *data, int size) {
    const char *dir = data_dir ? data_dir : DEFAULT_DATA_DIR;
    char page[PAGE_SIZE];
    int num_pages = get_num_pages(dir, table);
    
    // Page 0 is reserved for schema, start scanning from page 1
    int start_page = 1;
    
    // Try to insert into existing pages (starting from page 1)
    for (int page_id = start_page; page_id < num_pages; page_id++) {
        load_page(dir, table, page_id, page);
        
        int slot_id = insert_row(page, data, size);
        if (slot_id >= 0) {
            write_page(dir, table, page_id, page);
            return encode_rowid(page_id, slot_id);
        }
    }
    
    // No page has space, create new page (starting from page 1 if no pages exist)
    int new_page_id = (num_pages >= 1) ? num_pages : 1;
    init_page(page);
    int slot_id = insert_row(page, data, size);
    
    // Validate insert_row on new page
    if (slot_id < 0) {
        fprintf(stderr, "ERROR: row too large to fit in page\n");
        exit(1);
    }
    
    write_page(dir, table, new_page_id, page);
    
    return encode_rowid(new_page_id, slot_id);
}

void scan_table(const char *data_dir, const char *table) {
    const char *dir = data_dir ? data_dir : DEFAULT_DATA_DIR;
    int num_pages = get_num_pages(dir, table);
    
    printf("=== Scanning table '%s' (%d pages) ===\n\n", table, num_pages);
    
    char page[PAGE_SIZE];
    
    // Page 0 is reserved for schema, start scanning from page 1
    for (int page_id = 1; page_id < num_pages; page_id++) {
        printf("--- Page %d ---\n", page_id);
        
        load_page(dir, table, page_id, page);
        
        PageHeader *header = (PageHeader *)page;
        printf("num_slots: %d\n", header->num_slots);
        
        SlotEntry *slot_dir = (SlotEntry *)(page + sizeof(PageHeader));

        for (int slot_id = 0; slot_id < header->num_slots; slot_id++) {
            if (slot_dir[slot_id].state == SLOT_DELETED) {
                printf("  slot %d: DELETED\n", slot_id);
                continue;
            }
            
            int row_size = get_row_size(page, slot_id);
            char *row = read_row(page, slot_id);
            
            if (row && row_size > 0) {
                printf("  slot %d (size=%d): ", slot_id, row_size);
                for (int i = 0; i < row_size; i++) {
                    printf("%c", row[i]);
                }
                printf("\n");
            }
        }
        printf("\n");
    }
}

int scan_table_raw(const char *data_dir, const char *table,
                   char **rows_out, int *sizes_out, int max_rows) {
    if (!data_dir || !table || !rows_out || !sizes_out || max_rows <= 0)
        return -1;

    const char *dir = data_dir ? data_dir : DEFAULT_DATA_DIR;
    int num_pages   = get_num_pages(dir, table);
    int num_rows    = 0;

    static char page_bufs[1024][PAGE_SIZE];  // static — safe for single-threaded v1
    int page_buf_idx = 0;

    for (int page_id = 1; page_id < num_pages && num_rows < max_rows; page_id++) {
        if (page_buf_idx >= 1024) break;

        char *page = page_bufs[page_buf_idx++];
        load_page(dir, table, page_id, page);

        PageHeader *header   = (PageHeader *)page;
        SlotEntry  *slot_dir = (SlotEntry *)(page + sizeof(PageHeader));

        for (int slot_id = 0; slot_id < header->num_slots && num_rows < max_rows; slot_id++) {
            if (slot_dir[slot_id].state == SLOT_DELETED) continue;

            int   row_size = get_row_size(page, slot_id);
            char *row      = read_row(page, slot_id);

            if (row && row_size > 0) {
                rows_out[num_rows]  = row;
                sizes_out[num_rows] = row_size;
                num_rows++;
            }
        }
    }

    return num_rows;
}

int heap_insert_bm_indexed(const char *data_dir, const char *table_name,
                           const Schema *schema, const void *data, int size,
                           BufferManager *bm, IndexManager *im) {
    if (!data_dir || !table_name || !data || size <= 0 || !bm)
        return -1;

    int num_pages = get_num_pages(data_dir, table_name);

    // Primera pasada — buscar espacio en páginas existentes
    for (int page_id = 1; page_id < num_pages; page_id++) {
        char *page = bm_fetch_page(bm, table_name, page_id);
        if (!page) continue;

        int slot_id = insert_row(page, data, size);

        if (slot_id >= 0) {
            int duplicate = 0;
            RowID row_id = encode_rowid(page_id, slot_id);
            bm_unpin_page(bm, table_name, page_id, 1);  // dirty
            heap_try_index_insert(table_name, schema, data, size, row_id, im, &duplicate);
            return duplicate ? -1 : row_id;
        }

        bm_unpin_page(bm, table_name, page_id, 0);  // no dirty
    }

    // Ninguna página tiene espacio — crear página nueva en disco primero
    int new_page_id = (num_pages >= 1) ? num_pages : 1;

    // Inicializar y escribir página vacía en disco para que load_page no falle
    char empty_page[PAGE_SIZE];
    init_page(empty_page);
    if (write_page(data_dir, table_name, new_page_id, empty_page) != 0) {
        return -1;
    }

    // Ahora sí podemos cargarla via buffer pool
    char *page = bm_fetch_page(bm, table_name, new_page_id);
    if (!page) return -1;

    int slot_id = insert_row(page, data, size);

    if (slot_id < 0) {
        bm_unpin_page(bm, table_name, new_page_id, 0);
        return -1;
    }

    {
        int duplicate = 0;
        RowID row_id = encode_rowid(new_page_id, slot_id);

        bm_unpin_page(bm, table_name, new_page_id, 1);  // dirty
        heap_try_index_insert(table_name, schema, data, size, row_id, im, &duplicate);
        return duplicate ? -1 : row_id;
    }
}

int heap_insert_bm(const char *data_dir, const char *table_name,
                   const void *data, int size, BufferManager *bm) {
    return heap_insert_bm_indexed(data_dir, table_name, NULL, data, size, bm, NULL);
}

int heap_delete_row_bm(const char *data_dir, const char *table_name,
                       const Schema *schema, RowID row_id, BufferManager *bm,
                       IndexManager *im) {
    int page_id;
    int slot_id;
    char *page;
    PageHeader *header;
    SlotEntry *slot_dir;
    int row_size;
    char *row;

    if (!data_dir || !table_name || !bm || row_id < 0) {
        return -1;
    }

    (void)data_dir;

    page_id = decode_rowid_page(row_id);
    slot_id = decode_rowid_slot(row_id);
    if (page_id <= 0 || slot_id < 0) {
        return -1;
    }

    page = bm_fetch_page(bm, table_name, page_id);
    if (!page) {
        return -1;
    }

    header = (PageHeader *)page;
    if (slot_id >= header->num_slots) {
        bm_unpin_page(bm, table_name, page_id, 0);
        return -1;
    }

    slot_dir = (SlotEntry *)(page + sizeof(PageHeader));
    if (slot_dir[slot_id].state == SLOT_DELETED) {
        bm_unpin_page(bm, table_name, page_id, 0);
        return -1;
    }

    row_size = get_row_size(page, slot_id);
    row = read_row(page, slot_id);
    if (!row || row_size <= 0) {
        bm_unpin_page(bm, table_name, page_id, 0);
        return -1;
    }

    heap_try_index_delete(table_name, schema, row, row_size, im);
    delete_row(page, slot_id);
    bm_unpin_page(bm, table_name, page_id, 1);  // dirty
    return 0;
}

void debug_print_table(const char *data_dir, const char *table) {
    const char *dir = data_dir ? data_dir : DEFAULT_DATA_DIR;
    int num_pages = get_num_pages(dir, table);
    
    printf("=== Debug: Table '%s' (%d pages) ===\n\n", table, num_pages);
    
    char page[PAGE_SIZE];
    
    // Page 0 is reserved for schema, start scanning from page 1
    for (int page_id = 1; page_id < num_pages; page_id++) {
        printf("=== Page %d ===\n", page_id);
        load_page(dir, table, page_id, page);
        print_page(page);
        printf("\n");
    }
}

int heap_delete_bm(const char *data_dir, const char *table_name,
                   BufferManager *bm,
                   int (*predicate)(const char *row, int size, void *ctx),
                   void *ctx) {
    if (!data_dir || !table_name || !bm || !predicate)
        return -1;

    int num_pages = get_num_pages(data_dir, table_name);
    int deleted   = 0;

    for (int page_id = 1; page_id < num_pages; page_id++) {
        char *page = bm_fetch_page(bm, table_name, page_id);
        if (!page) continue;

        PageHeader *header   = (PageHeader *)page;
        SlotEntry  *slot_dir = (SlotEntry *)(page + sizeof(PageHeader));
        int         dirty    = 0;

        for (int slot_id = 0; slot_id < header->num_slots; slot_id++) {
            if (slot_dir[slot_id].state == SLOT_DELETED) continue;

            int   row_size = get_row_size(page, slot_id);
            char *row      = read_row(page, slot_id);
            if (!row || row_size <= 0) continue;

            if (predicate(row, row_size, ctx)) {
                delete_row(page, slot_id);
                deleted++;
                dirty = 1;
            }
        }

        bm_unpin_page(bm, table_name, page_id, dirty);
    }

    return deleted;
}
