#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "schema.h"

// ============================================================================
// Test Utilities
// ============================================================================

#define TEST_DIR "./test_data"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s\n", msg); \
        return 0; \
    } \
} while(0)

#define ASSERT_INT(expected, actual, msg) do { \
    if ((expected) != (actual)) { \
        printf("  FAIL: %s (expected %d, got %d)\n", msg, expected, actual); \
        return 0; \
    } \
} while(0)

#define ASSERT_STR(expected, actual, msg) do { \
    if (strcmp(expected, actual) != 0) { \
        printf("  FAIL: %s (expected '%s', got '%s')\n", msg, expected, actual); \
        return 0; \
    } \
} while(0)

static void create_test_dir() {
    system("mkdir -p " TEST_DIR);
}

static void cleanup_test_dir() {
    system("rm -rf " TEST_DIR);
}

// ============================================================================
// Test 1: Serialize and deserialize schema with all 4 types
// ============================================================================

static int test_serialize_deserialize() {
    printf("Test 1: Serialize and deserialize schema with all 4 types\n");

    // Create schema
    Schema schema;
    memset(&schema, 0, sizeof(Schema));

    strcpy(schema.table_name, "test_table");
    schema.num_columns = 4;

    // Column 0: INT
    strcpy(schema.columns[0].name, "id");
    schema.columns[0].type = TYPE_INT;
    schema.columns[0].max_size = 4;
    schema.columns[0].nullable = 0;
    schema.columns[0].is_primary_key = 1;

    // Column 1: FLOAT
    strcpy(schema.columns[1].name, "price");
    schema.columns[1].type = TYPE_FLOAT;
    schema.columns[1].max_size = 4;
    schema.columns[1].nullable = 1;
    schema.columns[1].is_primary_key = 0;

    // Column 2: BOOLEAN
    strcpy(schema.columns[2].name, "active");
    schema.columns[2].type = TYPE_BOOLEAN;
    schema.columns[2].max_size = 1;
    schema.columns[2].nullable = 1;
    schema.columns[2].is_primary_key = 0;

    // Column 3: VARCHAR
    strcpy(schema.columns[3].name, "name");
    schema.columns[3].type = TYPE_VARCHAR;
    schema.columns[3].max_size = 100;
    schema.columns[3].nullable = 1;
    schema.columns[3].is_primary_key = 0;

    // Serialize
    char page_buf[PAGE_SIZE];
    ASSERT_INT(0, schema_serialize(&schema, page_buf), "serialize failed");

    // Deserialize
    Schema schema2;
    memset(&schema2, 0, sizeof(Schema));
    ASSERT_INT(0, schema_deserialize(&schema2, page_buf), "deserialize failed");

    // Verify
    ASSERT_STR("test_table", schema2.table_name, "table name mismatch");
    ASSERT_INT(4, schema2.num_columns, "num_columns mismatch");

    // Check each column
    ASSERT_STR("id", schema2.columns[0].name, "col 0 name mismatch");
    ASSERT_INT(TYPE_INT, schema2.columns[0].type, "col 0 type mismatch");
    ASSERT_INT(4, schema2.columns[0].max_size, "col 0 max_size mismatch");
    ASSERT_INT(0, schema2.columns[0].nullable, "col 0 nullable mismatch");
    ASSERT_INT(1, schema2.columns[0].is_primary_key, "col 0 pk mismatch");

    ASSERT_STR("price", schema2.columns[1].name, "col 1 name mismatch");
    ASSERT_INT(TYPE_FLOAT, schema2.columns[1].type, "col 1 type mismatch");

    ASSERT_STR("active", schema2.columns[2].name, "col 2 name mismatch");
    ASSERT_INT(TYPE_BOOLEAN, schema2.columns[2].type, "col 2 type mismatch");

    ASSERT_STR("name", schema2.columns[3].name, "col 3 name mismatch");
    ASSERT_INT(TYPE_VARCHAR, schema2.columns[3].type, "col 3 type mismatch");
    ASSERT_INT(100, schema2.columns[3].max_size, "col 3 max_size mismatch");

    printf("  PASS\n");
    return 1;
}

// ============================================================================
// Test 2: Save schema to disk and reload
// ============================================================================

static int test_save_load() {
    printf("Test 2: Save schema to disk and reload\n");

    create_test_dir();

    // Create schema
    Schema schema;
    memset(&schema, 0, sizeof(Schema));

    strcpy(schema.table_name, "users");
    schema.num_columns = 3;

    strcpy(schema.columns[0].name, "user_id");
    schema.columns[0].type = TYPE_INT;
    schema.columns[0].max_size = 4;
    schema.columns[0].nullable = 0;
    schema.columns[0].is_primary_key = 1;

    strcpy(schema.columns[1].name, "email");
    schema.columns[1].type = TYPE_VARCHAR;
    schema.columns[1].max_size = 255;
    schema.columns[1].nullable = 0;
    schema.columns[1].is_primary_key = 0;

    strcpy(schema.columns[2].name, "balance");
    schema.columns[2].type = TYPE_FLOAT;
    schema.columns[2].max_size = 4;
    schema.columns[2].nullable = 1;
    schema.columns[2].is_primary_key = 0;

    // Save
    ASSERT_INT(0, schema_save(&schema, TEST_DIR), "schema_save failed");

    // Load
    Schema schema2;
    memset(&schema2, 0, sizeof(Schema));
    ASSERT_INT(0, schema_load(&schema2, "users", TEST_DIR), "schema_load failed");

    // Verify
    ASSERT_STR("users", schema2.table_name, "table name mismatch");
    ASSERT_INT(3, schema2.num_columns, "num_columns mismatch");

    ASSERT_STR("user_id", schema2.columns[0].name, "col 0 name mismatch");
    ASSERT_INT(TYPE_INT, schema2.columns[0].type, "col 0 type mismatch");

    ASSERT_STR("email", schema2.columns[1].name, "col 1 name mismatch");
    ASSERT_INT(TYPE_VARCHAR, schema2.columns[1].type, "col 1 type mismatch");

    ASSERT_STR("balance", schema2.columns[2].name, "col 2 name mismatch");
    ASSERT_INT(TYPE_FLOAT, schema2.columns[2].type, "col 2 type mismatch");

    cleanup_test_dir();

    printf("  PASS\n");
    return 1;
}

// ============================================================================
// Test 3: Serialize and deserialize row with some NULL values
// ============================================================================

static int test_row_nulls() {
    printf("Test 3: Serialize and deserialize row with some NULL values\n");

    // Create schema
    Schema schema;
    memset(&schema, 0, sizeof(Schema));

    strcpy(schema.table_name, "products");
    schema.num_columns = 4;

    strcpy(schema.columns[0].name, "id");
    schema.columns[0].type = TYPE_INT;
    schema.columns[0].max_size = 4;

    strcpy(schema.columns[1].name, "name");
    schema.columns[1].type = TYPE_VARCHAR;
    schema.columns[1].max_size = 100;

    strcpy(schema.columns[2].name, "price");
    schema.columns[2].type = TYPE_FLOAT;
    schema.columns[2].max_size = 4;

    strcpy(schema.columns[3].name, "description");
    schema.columns[3].type = TYPE_VARCHAR;
    schema.columns[3].max_size = 500;

    // Values: id=1, name="Test", price=NULL, description=NULL
    int id_val = 1;
    char name_val[] = "Test";
    float price_val = 19.99;
    char desc_val[] = "A great product";

    void *values[] = { &id_val, name_val, &price_val, desc_val };
    int sizes[] = { 4, 4, 0, 0 }; // price and description are NULL

    // Serialize
    char buf[PAGE_SIZE];
    int out_size;
    int result = row_serialize(&schema, values, sizes, buf, PAGE_SIZE, &out_size);
    ASSERT(result == 0, "row_serialize failed");
    ASSERT(out_size > 0, "row_serialize returned zero size");

    // Deserialize
    int id_out;
    char name_out[100];
    float price_out;
    char desc_out[500];

    void *values_out[] = { &id_out, name_out, &price_out, desc_out };
    int sizes_out[4];

    int deserialized_size = row_deserialize(&schema, buf, out_size, values_out, sizes_out);
    ASSERT(deserialized_size > 0, "row_deserialize failed");

    // Verify
    ASSERT_INT(1, id_out, "id mismatch");
    ASSERT_INT(4, sizes_out[0], "id size mismatch");

    name_out[sizes_out[1]] = '\0';
    ASSERT_STR("Test", name_out, "name mismatch");
    ASSERT_INT(4, sizes_out[1], "name size mismatch");

    // NULL columns should have size 0
    ASSERT_INT(0, sizes_out[2], "price should be NULL");
    ASSERT_INT(0, sizes_out[3], "description should be NULL");

    printf("  PASS\n");
    return 1;
}

// ============================================================================
// Test 4: Serialize and deserialize row with all types filled
// ============================================================================

static int test_row_all_types() {
    printf("Test 4: Serialize and deserialize row with all types filled\n");

    // Create schema
    Schema schema;
    memset(&schema, 0, sizeof(Schema));

    strcpy(schema.table_name, "mixed");
    schema.num_columns = 4;

    strcpy(schema.columns[0].name, "int_col");
    schema.columns[0].type = TYPE_INT;
    schema.columns[0].max_size = 4;

    strcpy(schema.columns[1].name, "float_col");
    schema.columns[1].type = TYPE_FLOAT;
    schema.columns[1].max_size = 4;

    strcpy(schema.columns[2].name, "bool_col");
    schema.columns[2].type = TYPE_BOOLEAN;
    schema.columns[2].max_size = 1;

    strcpy(schema.columns[3].name, "varchar_col");
    schema.columns[3].type = TYPE_VARCHAR;
    schema.columns[3].max_size = 100;

    // Values: all filled
    int int_val = 42;
    float float_val = 3.14159f;
    char bool_val = 1;
    char varchar_val[] = "Hello, World!";

    void *values[] = { &int_val, &float_val, &bool_val, varchar_val };
    int sizes[] = { 4, 4, 1, 14 }; // 14 = strlen("Hello, World!")

    // Serialize
    char buf[PAGE_SIZE];
    int out_size;
    int result = row_serialize(&schema, values, sizes, buf, PAGE_SIZE, &out_size);
    ASSERT(result == 0, "row_serialize failed");
    ASSERT(out_size > 0, "row_serialize returned zero size");

    // Deserialize
    int int_out;
    float float_out;
    char bool_out;
    char varchar_out[100];

    void *values_out[] = { &int_out, &float_out, &bool_out, varchar_out };
    int sizes_out[4];

    int deserialized_size = row_deserialize(&schema, buf, out_size, values_out, sizes_out);
    ASSERT(deserialized_size > 0, "row_deserialize failed");

    // Verify
    ASSERT_INT(42, int_out, "int mismatch");
    ASSERT_INT(4, sizes_out[0], "int size mismatch");

    // Float comparison with small epsilon
    float diff = float_out - 3.14159f;
    if (diff < 0) diff = -diff;
    ASSERT(diff < 0.001, "float mismatch");
    ASSERT_INT(4, sizes_out[1], "float size mismatch");

    ASSERT_INT(1, bool_out, "bool mismatch");
    ASSERT_INT(1, sizes_out[2], "bool size mismatch");

    ASSERT_STR("Hello, World!", varchar_out, "varchar mismatch");
    ASSERT_INT(14, sizes_out[3], "varchar size mismatch");

    printf("  PASS\n");
    return 1;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    printf("=== Schema Module Tests ===\n\n");

    // Test 1: Serialize/deserialize
    if (test_serialize_deserialize()) {
        tests_passed++;
    } else {
        tests_failed++;
    }
    printf("\n");

    // Test 2: Save/load
    if (test_save_load()) {
        tests_passed++;
    } else {
        tests_failed++;
    }
    printf("\n");

    // Test 3: Row with NULLs
    if (test_row_nulls()) {
        tests_passed++;
    } else {
        tests_failed++;
    }
    printf("\n");

    // Test 4: Row with all types
    if (test_row_all_types()) {
        tests_passed++;
    } else {
        tests_failed++;
    }
    printf("\n");

    printf("=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
