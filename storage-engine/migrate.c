#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "schema.h"
#include "heap.h"

#define DATA_DIR "../data"

// ============================================================================
// Schema Definitions
// ============================================================================

static Schema users_schema(void) {
    Schema schema;
    memset(&schema, 0, sizeof(Schema));
    
    strcpy(schema.table_name, "users");
    schema.num_columns = 4;
    
    // Column 0: id (INT, NOT NULL, PRIMARY KEY)
    strcpy(schema.columns[0].name, "id");
    schema.columns[0].type = TYPE_INT;
    schema.columns[0].max_size = 4;
    schema.columns[0].nullable = 0;
    schema.columns[0].is_primary_key = 1;
    
    // Column 1: name (VARCHAR 50, NOT NULL)
    strcpy(schema.columns[1].name, "name");
    schema.columns[1].type = TYPE_VARCHAR;
    schema.columns[1].max_size = 50;
    schema.columns[1].nullable = 0;
    schema.columns[1].is_primary_key = 0;
    
    // Column 2: age (INT, NOT NULL)
    strcpy(schema.columns[2].name, "age");
    schema.columns[2].type = TYPE_INT;
    schema.columns[2].max_size = 4;
    schema.columns[2].nullable = 0;
    schema.columns[2].is_primary_key = 0;
    
    // Column 3: city (VARCHAR 30, NOT NULL)
    strcpy(schema.columns[3].name, "city");
    schema.columns[3].type = TYPE_VARCHAR;
    schema.columns[3].max_size = 30;
    schema.columns[3].nullable = 0;
    schema.columns[3].is_primary_key = 0;
    
    return schema;
}

static Schema products_schema(void) {
    Schema schema;
    memset(&schema, 0, sizeof(Schema));
    
    strcpy(schema.table_name, "products");
    schema.num_columns = 3;
    
    // Column 0: id (INT, NOT NULL, PRIMARY KEY)
    strcpy(schema.columns[0].name, "id");
    schema.columns[0].type = TYPE_INT;
    schema.columns[0].max_size = 4;
    schema.columns[0].nullable = 0;
    schema.columns[0].is_primary_key = 1;
    
    // Column 1: name (VARCHAR 50, NOT NULL)
    strcpy(schema.columns[1].name, "name");
    schema.columns[1].type = TYPE_VARCHAR;
    schema.columns[1].max_size = 50;
    schema.columns[1].nullable = 0;
    schema.columns[1].is_primary_key = 0;
    
    // Column 2: price (INT, NOT NULL)
    strcpy(schema.columns[2].name, "price");
    schema.columns[2].type = TYPE_INT;
    schema.columns[2].max_size = 4;
    schema.columns[2].nullable = 0;
    schema.columns[2].is_primary_key = 0;
    
    return schema;
}

static Schema orders_schema(void) {
    Schema schema;
    memset(&schema, 0, sizeof(Schema));
    
    strcpy(schema.table_name, "orders");
    schema.num_columns = 4;
    
    // Column 0: id (INT, NOT NULL, PRIMARY KEY)
    strcpy(schema.columns[0].name, "id");
    schema.columns[0].type = TYPE_INT;
    schema.columns[0].max_size = 4;
    schema.columns[0].nullable = 0;
    schema.columns[0].is_primary_key = 1;
    
    // Column 1: user_id (INT, NOT NULL)
    strcpy(schema.columns[1].name, "user_id");
    schema.columns[1].type = TYPE_INT;
    schema.columns[1].max_size = 4;
    schema.columns[1].nullable = 0;
    schema.columns[1].is_primary_key = 0;
    
    // Column 2: product (VARCHAR 50, NOT NULL)
    strcpy(schema.columns[2].name, "product");
    schema.columns[2].type = TYPE_VARCHAR;
    schema.columns[2].max_size = 50;
    schema.columns[2].nullable = 0;
    schema.columns[2].is_primary_key = 0;
    
    // Column 3: amount (INT, NOT NULL)
    strcpy(schema.columns[3].name, "amount");
    schema.columns[3].type = TYPE_INT;
    schema.columns[3].max_size = 4;
    schema.columns[3].nullable = 0;
    schema.columns[3].is_primary_key = 0;
    
    return schema;
}

// ============================================================================
// Migration Functions
// ============================================================================

static int migrate_table(const char *tbl_filename, const char *db_filename, Schema *schema) {

    char db_path[256];
    snprintf(db_path, sizeof(db_path), "%s/%s.db", DATA_DIR, schema->table_name);
    remove(db_path);

    // Save schema to page 0
    if (schema_save(schema, DATA_DIR) != 0) {
        fprintf(stderr, "ERROR: failed to save schema for %s\n", db_filename);
        return -1;
    }
    printf("Saved schema for %s\n", db_filename);
    
    // Open CSV file
    FILE *fp = fopen(tbl_filename, "r");
    if (!fp) {
        fprintf(stderr, "ERROR: cannot open %s\n", tbl_filename);
        return -1;
    }
    
    // Skip header line
    char line[1024];
    if (!fgets(line, sizeof(line), fp)) {
        fprintf(stderr, "ERROR: empty file %s\n", tbl_filename);
        fclose(fp);
        return -1;
    }
    
    int row_count = 0;
    char buf[PAGE_SIZE];
    
    // Read each data line
    while (fgets(line, sizeof(line), fp)) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        
        // Parse CSV fields
        char *fields[10];
        int field_count = 0;
        char *token = strtok(line, ",");
        while (token && field_count < 10) {
            fields[field_count++] = token;
            token = strtok(NULL, ",");
        }
        
        if (field_count != schema->num_columns) {
            fprintf(stderr, "WARNING: expected %d fields, got %d\n", schema->num_columns, field_count);
            continue;
        }
        
        // Prepare values and sizes based on schema
        void *values[10];
        int sizes[10];
        
        for (int i = 0; i < schema->num_columns; i++) {
            ColumnDef *col = &schema->columns[i];
            switch (col->type) {
                case TYPE_INT: {
                    static int int_vals[10];
                    int_vals[i] = atoi(fields[i]);
                    values[i] = &int_vals[i];
                    sizes[i] = 4;
                    break;
                }
                case TYPE_VARCHAR: {
                    static char varchar_vals[10][64];
                    strncpy(varchar_vals[i], fields[i], 63);
                    varchar_vals[i][63] = '\0';
                    values[i] = varchar_vals[i];
                    sizes[i] = strlen(fields[i]);
                    break;
                }
                case TYPE_FLOAT: {
                    static float float_vals[10];
                    float_vals[i] = atof(fields[i]);
                    values[i] = &float_vals[i];
                    sizes[i] = 4;
                    break;
                }
                case TYPE_BOOLEAN: {
                    static char bool_vals[10];
                    bool_vals[i] = (strcmp(fields[i], "1") == 0 || 
                                    strcasecmp(fields[i], "true") == 0) ? 1 : 0;
                    values[i] = &bool_vals[i];
                    sizes[i] = 1;
                    break;
                }
            }
        }
        
        // Serialize row
        int out_size;
        if (row_serialize(schema, values, sizes, buf, PAGE_SIZE, &out_size) != 0) {
            fprintf(stderr, "ERROR: row_serialize failed\n");
            fclose(fp);
            return -1;
        }
        
        // Insert into heap file (starts from page 1)
        insert_into_table(DATA_DIR, schema->table_name, buf, out_size);
        
        row_count++;
    }
    
    fclose(fp);
    
    printf("Migrated %d rows into %s\n", row_count, db_filename);
    return row_count;
}

// ============================================================================
// Verify Function
// ============================================================================

static void verify_table(const char *table_name) {
    printf("\n=== Verifying %s ===\n", table_name);
    
    // Load and print schema from page 0
    char page[PAGE_SIZE];
    Schema schema;
    if (schema_load(&schema, table_name, DATA_DIR) == 0) {
        printf("Schema: %s (%d columns)\n", schema.table_name, schema.num_columns);
        for (int i = 0; i < schema.num_columns; i++) {
            printf("  - %s\n", schema.columns[i].name);
        }
    }
    
    // Scan data pages (starting from page 1, since page 0 is schema)
    int num_pages = get_num_pages(DATA_DIR, table_name);
    printf("Data pages: %d\n", num_pages > 0 ? num_pages - 1 : 0);
    
    for (int page_id = 1; page_id < num_pages; page_id++) {
        printf("--- Page %d ---\n", page_id);
        load_page(DATA_DIR, table_name, page_id, page);
        
        PageHeader *header = (PageHeader *)page;
        printf("num_slots: %d\n", header->num_slots);
        
        int *slot_dir = (int *)(page + sizeof(PageHeader));
        
        for (int slot_id = 0; slot_id < header->num_slots; slot_id++) {
            if (slot_dir[slot_id] == -1) {
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
    }
}

// ============================================================================
// Main
// ============================================================================

int main(void) {
    printf("=== Data Migration Tool ===\n\n");
    
    // Migrate users
    printf("--- Migrating users ---\n");
    Schema users = users_schema();
    migrate_table("../data/users.tbl", "../data/users.db", &users);
    
    // Migrate products
    printf("\n--- Migrating products ---\n");
    Schema products = products_schema();
    migrate_table("../data/products.tbl", "../data/products.db", &products);
    
    // Migrate orders
    printf("\n--- Migrating orders ---\n");
    Schema orders = orders_schema();
    migrate_table("../data/orders.tbl", "../data/orders.db", &orders);
    
    // Verify all tables
    printf("\n=== Verification ===\n");
    verify_table("users");
    verify_table("products");
    verify_table("orders");
    
    printf("\n=== Migration Complete ===\n");
    
    return 0;
}
