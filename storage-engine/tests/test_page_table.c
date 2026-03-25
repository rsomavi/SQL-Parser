// test_page_table.c - Exhaustive test suite for page_table module
// Compile: gcc -Wall -Wextra -g -o test_page_table test_page_table.c
//          ../page_table.c ../buffer_frame.c ../disk.c ../page.c ../heap.c ../schema.c
// Run:     ./test_page_table

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../page_table.h"

// ============================================================================
// Infrastructure
// ============================================================================

static int tests_passed = 0;
static int tests_failed  = 0;

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("    FAIL: %s (line %d)\n", msg, __LINE__); \
        tests_failed++; \
        return 0; \
    } \
} while(0)

#define ASSERT_INT(expected, actual, msg) do { \
    if ((expected) != (actual)) { \
        printf("    FAIL: %s - expected %d, got %d (line %d)\n", \
               msg, expected, actual, __LINE__); \
        tests_failed++; \
        return 0; \
    } \
} while(0)

#define RUN_TEST(fn) do { \
    printf("  %-65s", #fn); \
    fflush(stdout); \
    int _r = fn(); \
    if (_r) { printf("PASS\n"); tests_passed++; } \
    else    { printf("(see above)\n"); } \
} while(0)

// PageTable is small (buckets are just pointers) - safe on stack
static PageTable make_pt(void) {
    PageTable pt;
    pt_init(&pt);
    return pt;
}


// ============================================================================
// BLOCK 1: pt_init
// ============================================================================

static int test_init_valid(void) {
    PageTable pt;
    ASSERT_INT(0, pt_init(&pt), "pt_init returns 0");
    ASSERT_INT(0, pt_size(&pt), "size == 0 after init");
    return 1;
}

static int test_init_all_buckets_null(void) {
    PageTable pt;
    pt_init(&pt);
    for (int i = 0; i < PT_NUM_BUCKETS; i++)
        ASSERT(pt.buckets[i].head == NULL, "all buckets NULL after init");
    return 1;
}

static int test_init_null(void) {
    ASSERT_INT(-1, pt_init(NULL), "pt_init NULL returns -1");
    return 1;
}

static int test_init_twice_resets(void) {
    PageTable pt;
    pt_init(&pt);
    pt_insert(&pt, "users", 1, 5);
    pt_insert(&pt, "users", 2, 6);
    ASSERT_INT(2, pt_size(&pt), "size == 2 before second init");
    pt_clear(&pt);
    pt_init(&pt);
    ASSERT_INT(0, pt_size(&pt), "size == 0 after second init");
    ASSERT_INT(-1, pt_lookup(&pt, "users", 1), "lookup returns -1 after reinit");
    return 1;
}


// ============================================================================
// BLOCK 2: pt_insert
// ============================================================================

static int test_insert_basic(void) {
    PageTable pt = make_pt();
    ASSERT_INT(0, pt_insert(&pt, "users", 1, 7), "pt_insert returns 0");
    ASSERT_INT(1, pt_size(&pt), "size == 1 after insert");
    pt_clear(&pt);
    return 1;
}

static int test_insert_multiple(void) {
    PageTable pt = make_pt();
    pt_insert(&pt, "users",  1, 0);
    pt_insert(&pt, "users",  2, 1);
    pt_insert(&pt, "users",  3, 2);
    pt_insert(&pt, "orders", 1, 3);
    pt_insert(&pt, "orders", 2, 4);
    ASSERT_INT(5, pt_size(&pt), "size == 5 after 5 inserts");
    pt_clear(&pt);
    return 1;
}

static int test_insert_update_existing(void) {
    PageTable pt = make_pt();
    pt_insert(&pt, "users", 1, 5);
    ASSERT_INT(5, pt_lookup(&pt, "users", 1), "initial frame_id == 5");
    // Update same key with new frame_id
    pt_insert(&pt, "users", 1, 99);
    ASSERT_INT(99, pt_lookup(&pt, "users", 1), "frame_id updated to 99");
    ASSERT_INT(1, pt_size(&pt), "size still 1 after update");
    pt_clear(&pt);
    return 1;
}

static int test_insert_null_params(void) {
    PageTable pt = make_pt();
    ASSERT_INT(-1, pt_insert(NULL,  "t", 1, 0), "NULL pt returns -1");
    ASSERT_INT(-1, pt_insert(&pt, NULL, 1, 0),  "NULL table returns -1");
    pt_clear(&pt);
    return 1;
}

static int test_insert_same_page_different_tables(void) {
    PageTable pt = make_pt();
    pt_insert(&pt, "users",  1, 10);
    pt_insert(&pt, "orders", 1, 20);
    ASSERT_INT(10, pt_lookup(&pt, "users",  1), "users  page 1 -> frame 10");
    ASSERT_INT(20, pt_lookup(&pt, "orders", 1), "orders page 1 -> frame 20");
    ASSERT_INT(2, pt_size(&pt), "size == 2 for same page in 2 tables");
    pt_clear(&pt);
    return 1;
}

static int test_insert_same_table_different_pages(void) {
    PageTable pt = make_pt();
    for (int i = 0; i < 10; i++)
        pt_insert(&pt, "users", i+1, i);
    ASSERT_INT(10, pt_size(&pt), "size == 10 for 10 different pages");
    for (int i = 0; i < 10; i++)
        ASSERT_INT(i, pt_lookup(&pt, "users", i+1), "each page maps to correct frame");
    pt_clear(&pt);
    return 1;
}

static int test_insert_frame_id_zero(void) {
    PageTable pt = make_pt();
    pt_insert(&pt, "t", 1, 0);
    ASSERT_INT(0, pt_lookup(&pt, "t", 1), "frame_id 0 stored and retrieved correctly");
    pt_clear(&pt);
    return 1;
}


// ============================================================================
// BLOCK 3: pt_lookup
// ============================================================================

static int test_lookup_hit(void) {
    PageTable pt = make_pt();
    pt_insert(&pt, "users", 3, 42);
    ASSERT_INT(42, pt_lookup(&pt, "users", 3), "lookup returns correct frame_id");
    pt_clear(&pt);
    return 1;
}

static int test_lookup_miss_empty(void) {
    PageTable pt = make_pt();
    ASSERT_INT(-1, pt_lookup(&pt, "users", 1), "lookup on empty pt returns -1");
    pt_clear(&pt);
    return 1;
}

static int test_lookup_miss_wrong_table(void) {
    PageTable pt = make_pt();
    pt_insert(&pt, "users", 1, 5);
    ASSERT_INT(-1, pt_lookup(&pt, "orders", 1), "wrong table returns -1");
    pt_clear(&pt);
    return 1;
}

static int test_lookup_miss_wrong_page(void) {
    PageTable pt = make_pt();
    pt_insert(&pt, "users", 1, 5);
    ASSERT_INT(-1, pt_lookup(&pt, "users", 2), "wrong page_id returns -1");
    pt_clear(&pt);
    return 1;
}

static int test_lookup_null_params(void) {
    PageTable pt = make_pt();
    ASSERT_INT(-1, pt_lookup(NULL, "t", 1),  "NULL pt returns -1");
    ASSERT_INT(-1, pt_lookup(&pt, NULL, 1),  "NULL table returns -1");
    pt_clear(&pt);
    return 1;
}

static int test_lookup_after_update(void) {
    PageTable pt = make_pt();
    pt_insert(&pt, "t", 1, 5);
    pt_insert(&pt, "t", 1, 99);  // update
    ASSERT_INT(99, pt_lookup(&pt, "t", 1), "lookup returns updated frame_id");
    pt_clear(&pt);
    return 1;
}

static int test_lookup_multiple_entries_same_bucket(void) {
    // Force a collision by inserting many entries
    PageTable pt = make_pt();
    for (int i = 0; i < 100; i++)
        pt_insert(&pt, "t", i+1, i*2);
    // Verify all are retrievable despite potential collisions
    int all_ok = 1;
    for (int i = 0; i < 100; i++) {
        if (pt_lookup(&pt, "t", i+1) != i*2) { all_ok = 0; break; }
    }
    ASSERT(all_ok, "all 100 entries retrievable despite hash collisions");
    pt_clear(&pt);
    return 1;
}


// ============================================================================
// BLOCK 4: pt_remove
// ============================================================================

static int test_remove_basic(void) {
    PageTable pt = make_pt();
    pt_insert(&pt, "users", 1, 5);
    ASSERT_INT(0, pt_remove(&pt, "users", 1), "pt_remove returns 0");
    ASSERT_INT(-1, pt_lookup(&pt, "users", 1), "lookup returns -1 after remove");
    ASSERT_INT(0, pt_size(&pt), "size == 0 after remove");
    pt_clear(&pt);
    return 1;
}

static int test_remove_not_found(void) {
    PageTable pt = make_pt();
    ASSERT_INT(-1, pt_remove(&pt, "users", 1), "remove nonexistent returns -1");
    pt_clear(&pt);
    return 1;
}

static int test_remove_wrong_table(void) {
    PageTable pt = make_pt();
    pt_insert(&pt, "users", 1, 5);
    ASSERT_INT(-1, pt_remove(&pt, "orders", 1), "remove wrong table returns -1");
    ASSERT_INT(5, pt_lookup(&pt, "users", 1), "original entry untouched");
    pt_clear(&pt);
    return 1;
}

static int test_remove_wrong_page(void) {
    PageTable pt = make_pt();
    pt_insert(&pt, "users", 1, 5);
    ASSERT_INT(-1, pt_remove(&pt, "users", 2), "remove wrong page returns -1");
    ASSERT_INT(5, pt_lookup(&pt, "users", 1), "original entry untouched");
    pt_clear(&pt);
    return 1;
}

static int test_remove_middle_of_chain(void) {
    // Insert enough entries to force collisions, then remove from middle
    PageTable pt = make_pt();
    for (int i = 0; i < 20; i++)
        pt_insert(&pt, "t", i+1, i);
    // Remove entry from the middle
    pt_remove(&pt, "t", 10);
    ASSERT_INT(-1, pt_lookup(&pt, "t", 10), "removed entry not found");
    // Neighbors still accessible
    ASSERT_INT(8,  pt_lookup(&pt, "t",  9), "entry before removed still ok");
    ASSERT_INT(10, pt_lookup(&pt, "t", 11), "entry after removed still ok");
    ASSERT_INT(19, pt_size(&pt), "size decremented correctly");
    pt_clear(&pt);
    return 1;
}

static int test_remove_null_params(void) {
    PageTable pt = make_pt();
    ASSERT_INT(-1, pt_remove(NULL, "t", 1),  "NULL pt returns -1");
    ASSERT_INT(-1, pt_remove(&pt, NULL, 1),  "NULL table returns -1");
    pt_clear(&pt);
    return 1;
}

static int test_remove_and_reinsert(void) {
    PageTable pt = make_pt();
    pt_insert(&pt, "t", 1, 5);
    pt_remove(&pt, "t", 1);
    ASSERT_INT(-1, pt_lookup(&pt, "t", 1), "not found after remove");
    pt_insert(&pt, "t", 1, 99);
    ASSERT_INT(99, pt_lookup(&pt, "t", 1), "found after reinsert");
    ASSERT_INT(1, pt_size(&pt), "size == 1 after reinsert");
    pt_clear(&pt);
    return 1;
}

static int test_remove_all_entries(void) {
    PageTable pt = make_pt();
    for (int i = 0; i < 10; i++)
        pt_insert(&pt, "t", i+1, i);
    for (int i = 0; i < 10; i++)
        pt_remove(&pt, "t", i+1);
    ASSERT_INT(0, pt_size(&pt), "size == 0 after removing all");
    for (int i = 0; i < 10; i++)
        ASSERT_INT(-1, pt_lookup(&pt, "t", i+1), "all entries gone after remove");
    pt_clear(&pt);
    return 1;
}


// ============================================================================
// BLOCK 5: pt_clear
// ============================================================================

static int test_clear_basic(void) {
    PageTable pt = make_pt();
    pt_insert(&pt, "users",  1, 0);
    pt_insert(&pt, "users",  2, 1);
    pt_insert(&pt, "orders", 1, 2);
    ASSERT_INT(3, pt_size(&pt), "size == 3 before clear");
    ASSERT_INT(0, pt_clear(&pt), "pt_clear returns 0");
    ASSERT_INT(0, pt_size(&pt), "size == 0 after clear");
    return 1;
}

static int test_clear_lookup_returns_minus1(void) {
    PageTable pt = make_pt();
    pt_insert(&pt, "t", 1, 5);
    pt_clear(&pt);
    ASSERT_INT(-1, pt_lookup(&pt, "t", 1), "lookup returns -1 after clear");
    return 1;
}

static int test_clear_null(void) {
    ASSERT_INT(-1, pt_clear(NULL), "pt_clear NULL returns -1");
    return 1;
}

static int test_clear_empty_table(void) {
    PageTable pt = make_pt();
    ASSERT_INT(0, pt_clear(&pt), "pt_clear on empty table returns 0");
    ASSERT_INT(0, pt_size(&pt), "size still 0 after clearing empty table");
    return 1;
}

static int test_clear_then_reuse(void) {
    PageTable pt = make_pt();
    for (int i = 0; i < 50; i++)
        pt_insert(&pt, "t", i+1, i);
    pt_clear(&pt);
    // Reuse after clear
    pt_insert(&pt, "t", 1, 42);
    ASSERT_INT(42, pt_lookup(&pt, "t", 1), "insert works after clear");
    ASSERT_INT(1, pt_size(&pt), "size == 1 after insert post-clear");
    pt_clear(&pt);
    return 1;
}


// ============================================================================
// BLOCK 6: pt_size
// ============================================================================

static int test_size_empty(void) {
    PageTable pt = make_pt();
    ASSERT_INT(0, pt_size(&pt), "size == 0 on empty table");
    pt_clear(&pt);
    return 1;
}

static int test_size_grows_on_insert(void) {
    PageTable pt = make_pt();
    for (int i = 0; i < 10; i++) {
        pt_insert(&pt, "t", i+1, i);
        ASSERT_INT(i+1, pt_size(&pt), "size increments on each insert");
    }
    pt_clear(&pt);
    return 1;
}

static int test_size_unchanged_on_update(void) {
    PageTable pt = make_pt();
    pt_insert(&pt, "t", 1, 5);
    pt_insert(&pt, "t", 1, 99);  // update, not new entry
    ASSERT_INT(1, pt_size(&pt), "size unchanged after update");
    pt_clear(&pt);
    return 1;
}

static int test_size_decrements_on_remove(void) {
    PageTable pt = make_pt();
    pt_insert(&pt, "t", 1, 0);
    pt_insert(&pt, "t", 2, 1);
    pt_insert(&pt, "t", 3, 2);
    pt_remove(&pt, "t", 2);
    ASSERT_INT(2, pt_size(&pt), "size decrements on remove");
    pt_clear(&pt);
    return 1;
}

static int test_size_null(void) {
    ASSERT_INT(0, pt_size(NULL), "pt_size NULL returns 0");
    return 1;
}


// ============================================================================
// BLOCK 7: hash quality
// ============================================================================

static int test_hash_distributes_across_buckets(void) {
    // Insert many entries and verify they spread across buckets
    PageTable pt = make_pt();
    for (int i = 0; i < 200; i++)
        pt_insert(&pt, "users", i+1, i);
    // Count non-empty buckets
    int non_empty = 0;
    for (int i = 0; i < PT_NUM_BUCKETS; i++)
        if (pt.buckets[i].head) non_empty++;
    // With 200 entries and 2048 buckets, expect at least 150 different buckets
    ASSERT(non_empty >= 150, "200 entries spread across at least 150 buckets");
    pt_clear(&pt);
    return 1;
}

static int test_hash_different_tables_same_page(void) {
    // Same page_id for different tables must hash independently
    PageTable pt = make_pt();
    const char *tables[] = {"users", "orders", "products", "inventory", "logs"};
    for (int i = 0; i < 5; i++)
        pt_insert(&pt, tables[i], 1, i*10);
    for (int i = 0; i < 5; i++)
        ASSERT_INT(i*10, pt_lookup(&pt, tables[i], 1),
                   "each table/page combo maps to correct frame");
    ASSERT_INT(5, pt_size(&pt), "5 distinct entries for same page in 5 tables");
    pt_clear(&pt);
    return 1;
}

static int test_hash_case_sensitive(void) {
    // "Users" and "users" must be treated as different tables
    PageTable pt = make_pt();
    pt_insert(&pt, "users", 1, 10);
    pt_insert(&pt, "Users", 1, 20);
    ASSERT_INT(10, pt_lookup(&pt, "users", 1), "users -> 10");
    ASSERT_INT(20, pt_lookup(&pt, "Users", 1), "Users -> 20");
    ASSERT_INT(2, pt_size(&pt), "2 entries for case-different tables");
    pt_clear(&pt);
    return 1;
}


// ============================================================================
// BLOCK 8: stress
// ============================================================================

static int test_stress_insert_lookup_1000(void) {
    PageTable pt = make_pt();
    for (int i = 0; i < 1000; i++)
        pt_insert(&pt, "t", i+1, i*3);
    ASSERT_INT(1000, pt_size(&pt), "1000 entries inserted");
    int all_ok = 1;
    for (int i = 0; i < 1000; i++)
        if (pt_lookup(&pt, "t", i+1) != i*3) { all_ok = 0; break; }
    ASSERT(all_ok, "all 1000 entries retrieved correctly");
    pt_clear(&pt);
    return 1;
}

static int test_stress_insert_remove_insert(void) {
    PageTable pt = make_pt();
    // Insert 500, remove 250, insert 250 new ones
    for (int i = 0; i < 500; i++)
        pt_insert(&pt, "t", i+1, i);
    for (int i = 0; i < 500; i += 2)
        pt_remove(&pt, "t", i+1);
    ASSERT_INT(250, pt_size(&pt), "250 entries remain after removing evens");
    for (int i = 0; i < 500; i += 2)
        pt_insert(&pt, "t", i+1, i+1000);
    ASSERT_INT(500, pt_size(&pt), "500 entries after reinserting evens");
    // Verify odd entries unchanged
    int all_ok = 1;
    for (int i = 1; i < 500; i += 2)
        if (pt_lookup(&pt, "t", i+1) != i) { all_ok = 0; break; }
    ASSERT(all_ok, "odd entries unchanged after remove/reinsert cycle");
    pt_clear(&pt);
    return 1;
}

static int test_stress_two_tables_interleaved(void) {
    PageTable pt = make_pt();
    for (int i = 0; i < 100; i++) {
        pt_insert(&pt, "users",  i+1, i);
        pt_insert(&pt, "orders", i+1, i+1000);
    }
    ASSERT_INT(200, pt_size(&pt), "200 entries for 2 tables x 100 pages");
    int all_ok = 1;
    for (int i = 0; i < 100; i++) {
        if (pt_lookup(&pt, "users",  i+1) != i)      { all_ok = 0; break; }
        if (pt_lookup(&pt, "orders", i+1) != i+1000) { all_ok = 0; break; }
    }
    ASSERT(all_ok, "all 200 entries correct with no cross-table interference");
    pt_clear(&pt);
    return 1;
}

static int test_stress_update_all_entries(void) {
    PageTable pt = make_pt();
    for (int i = 0; i < 200; i++)
        pt_insert(&pt, "t", i+1, i);
    // Update all with new frame_ids
    for (int i = 0; i < 200; i++)
        pt_insert(&pt, "t", i+1, i+500);
    ASSERT_INT(200, pt_size(&pt), "size still 200 after updating all");
    int all_ok = 1;
    for (int i = 0; i < 200; i++)
        if (pt_lookup(&pt, "t", i+1) != i+500) { all_ok = 0; break; }
    ASSERT(all_ok, "all 200 entries updated correctly");
    pt_clear(&pt);
    return 1;
}


// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    printf("=== TEST PAGE TABLE ===\n\n");

    printf("-- Block 1: pt_init --\n");
    RUN_TEST(test_init_valid);
    RUN_TEST(test_init_all_buckets_null);
    RUN_TEST(test_init_null);
    RUN_TEST(test_init_twice_resets);

    printf("\n-- Block 2: pt_insert --\n");
    RUN_TEST(test_insert_basic);
    RUN_TEST(test_insert_multiple);
    RUN_TEST(test_insert_update_existing);
    RUN_TEST(test_insert_null_params);
    RUN_TEST(test_insert_same_page_different_tables);
    RUN_TEST(test_insert_same_table_different_pages);
    RUN_TEST(test_insert_frame_id_zero);

    printf("\n-- Block 3: pt_lookup --\n");
    RUN_TEST(test_lookup_hit);
    RUN_TEST(test_lookup_miss_empty);
    RUN_TEST(test_lookup_miss_wrong_table);
    RUN_TEST(test_lookup_miss_wrong_page);
    RUN_TEST(test_lookup_null_params);
    RUN_TEST(test_lookup_after_update);
    RUN_TEST(test_lookup_multiple_entries_same_bucket);

    printf("\n-- Block 4: pt_remove --\n");
    RUN_TEST(test_remove_basic);
    RUN_TEST(test_remove_not_found);
    RUN_TEST(test_remove_wrong_table);
    RUN_TEST(test_remove_wrong_page);
    RUN_TEST(test_remove_middle_of_chain);
    RUN_TEST(test_remove_null_params);
    RUN_TEST(test_remove_and_reinsert);
    RUN_TEST(test_remove_all_entries);

    printf("\n-- Block 5: pt_clear --\n");
    RUN_TEST(test_clear_basic);
    RUN_TEST(test_clear_lookup_returns_minus1);
    RUN_TEST(test_clear_null);
    RUN_TEST(test_clear_empty_table);
    RUN_TEST(test_clear_then_reuse);

    printf("\n-- Block 6: pt_size --\n");
    RUN_TEST(test_size_empty);
    RUN_TEST(test_size_grows_on_insert);
    RUN_TEST(test_size_unchanged_on_update);
    RUN_TEST(test_size_decrements_on_remove);
    RUN_TEST(test_size_null);

    printf("\n-- Block 7: hash quality --\n");
    RUN_TEST(test_hash_distributes_across_buckets);
    RUN_TEST(test_hash_different_tables_same_page);
    RUN_TEST(test_hash_case_sensitive);

    printf("\n-- Block 8: stress --\n");
    RUN_TEST(test_stress_insert_lookup_1000);
    RUN_TEST(test_stress_insert_remove_insert);
    RUN_TEST(test_stress_two_tables_interleaved);
    RUN_TEST(test_stress_update_all_entries);

    printf("\n=== RESULT: %d passed, %d failed, %d total ===\n",
           tests_passed, tests_failed, tests_passed + tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
