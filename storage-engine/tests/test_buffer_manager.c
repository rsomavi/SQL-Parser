// test_buffer_manager.c - Exhaustive test suite for buffer_manager module
// Compile: gcc -Wall -Wextra -g -o test_buffer_manager test_buffer_manager.c
//          ../buffer_manager.c ../eviction_policy.c ../page_table.c
//          ../buffer_frame.c ../disk.c ../page.c ../heap.c ../schema.c
// Run:     ./test_buffer_manager

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../buffer_manager.h"

// ============================================================================
// Infrastructure
// ============================================================================

static int tests_passed = 0;
static int tests_failed  = 0;
static char TEST_DIR[64];

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

#define ASSERT_MEM(expected, actual, size, msg) do { \
    if (memcmp(expected, actual, size) != 0) { \
        printf("    FAIL: %s - buffers differ (line %d)\n", msg, __LINE__); \
        tests_failed++; \
        return 0; \
    } \
} while(0)

#define RUN_TEST(fn) do { \
    printf("  %-65s", #fn); \
    fflush(stdout); \
    cleanup_files(); \
    int _r = fn(); \
    if (_r) { printf("PASS\n"); tests_passed++; } \
    else    { printf("(see above)\n"); } \
} while(0)

static void cleanup_files(void) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -f %s/*.db 2>/dev/null", TEST_DIR);
    system(cmd);
}

// Helper: write a known page to disk so bm_fetch_page can load it
static void write_test_page(const char *table, int page_id, char val) {
    char buf[PAGE_SIZE];
    memset(buf, val, PAGE_SIZE);
    write_page(TEST_DIR, table, page_id, buf);
}

// Helper: create a buffer manager with NoCache policy
static BufferManager *make_bm_nocache(int num_frames) {
    BufferManager *bm = malloc(sizeof(BufferManager));
    bm_init(bm, num_frames, TEST_DIR, policy_nocache_create());
    return bm;
}

// Helper: create a buffer manager with Clock policy
static BufferManager *make_bm_clock(int num_frames) {
    BufferManager *bm = malloc(sizeof(BufferManager));
    bm_init(bm, num_frames, TEST_DIR, policy_clock_create(num_frames));
    return bm;
}

// Helper: create a buffer manager with LRU policy
static BufferManager *make_bm_lru(int num_frames) {
    BufferManager *bm = malloc(sizeof(BufferManager));
    bm_init(bm, num_frames, TEST_DIR, policy_lru_create());
    return bm;
}

static void destroy_bm(BufferManager *bm) {
    bm_destroy(bm);
    free(bm);
}


// ============================================================================
// BLOCK 1: bm_init / bm_destroy
// ============================================================================

static int test_init_valid(void) {
    BufferManager *bm = malloc(sizeof(BufferManager));
    EvictionPolicy *p = policy_nocache_create();
    ASSERT_INT(0, bm_init(bm, 8, TEST_DIR, p), "bm_init returns 0");
    ASSERT_INT(0, bm->pool.num_frames - 8,      "pool has 8 frames");
    ASSERT_INT(0, pt_size(&bm->pt),             "page table empty after init");
    bm_destroy(bm); free(bm);
    return 1;
}

static int test_init_null_params(void) {
    BufferManager bm;
    EvictionPolicy *p = policy_nocache_create();
    ASSERT_INT(-1, bm_init(NULL, 8, TEST_DIR, p), "NULL bm returns -1");
    ASSERT_INT(-1, bm_init(&bm, 8, NULL,     p),  "NULL data_dir returns -1");
    ASSERT_INT(-1, bm_init(&bm, 8, TEST_DIR, NULL),"NULL policy returns -1");
    POLICY_DESTROY(p); free(p);
    return 1;
}

static int test_destroy_flushes_dirty(void) {
    // Write a page, load it, modify it, destroy BM -> must flush to disk
    write_test_page("t", 1, 0x11);
    BufferManager *bm = make_bm_nocache(4);

    char *data = bm_fetch_page(bm, "t", 1);
    ASSERT(data != NULL, "fetch returns non-NULL");
    memset(data, 0xCC, PAGE_SIZE);
    bm_unpin_page(bm, "t", 1, 1);  // dirty=1

    bm_destroy(bm); free(bm);

    // Read back from disk — should have 0xCC
    char readback[PAGE_SIZE];
    load_page(TEST_DIR, "t", 1, readback);
    char expected[PAGE_SIZE]; memset(expected, 0xCC, PAGE_SIZE);
    ASSERT_MEM(expected, readback, PAGE_SIZE, "dirty page flushed on destroy");
    return 1;
}

static int test_destroy_null(void) {
    ASSERT_INT(-1, bm_destroy(NULL), "bm_destroy NULL returns -1");
    return 1;
}


// ============================================================================
// BLOCK 2: bm_fetch_page — basic behavior
// ============================================================================

static int test_fetch_loads_from_disk(void) {
    write_test_page("users", 1, 0xAB);
    BufferManager *bm = make_bm_nocache(4);

    char *data = bm_fetch_page(bm, "users", 1);
    ASSERT(data != NULL, "bm_fetch_page returns non-NULL");

    char expected[PAGE_SIZE]; memset(expected, 0xAB, PAGE_SIZE);
    ASSERT_MEM(expected, data, PAGE_SIZE, "fetched data matches disk content");

    bm_unpin_page(bm, "users", 1, 0);
    destroy_bm(bm);
    return 1;
}

static int test_fetch_miss_increments_misses(void) {
    write_test_page("t", 1, 0x11);
    BufferManager *bm = make_bm_nocache(4);

    ASSERT_INT(0, (int)bm_get_misses(bm), "misses == 0 before fetch");
    bm_fetch_page(bm, "t", 1);
    ASSERT_INT(1, (int)bm_get_misses(bm), "misses == 1 after first fetch");

    bm_unpin_page(bm, "t", 1, 0);
    destroy_bm(bm);
    return 1;
}

static int test_fetch_hit_increments_hits(void) {
    write_test_page("t", 1, 0x22);
    BufferManager *bm = make_bm_nocache(4);

    bm_fetch_page(bm, "t", 1);  // MISS
    ASSERT_INT(0, (int)bm_get_hits(bm), "hits == 0 after first fetch");

    bm_fetch_page(bm, "t", 1);  // HIT
    ASSERT_INT(1, (int)bm_get_hits(bm), "hits == 1 after second fetch");

    bm_unpin_page(bm, "t", 1, 0);
    bm_unpin_page(bm, "t", 1, 0);
    destroy_bm(bm);
    return 1;
}

static int test_fetch_returns_same_pointer_on_hit(void) {
    write_test_page("t", 1, 0x33);
    BufferManager *bm = make_bm_nocache(4);

    char *p1 = bm_fetch_page(bm, "t", 1);
    char *p2 = bm_fetch_page(bm, "t", 1);
    ASSERT(p1 == p2, "second fetch returns same pointer (same frame)");

    bm_unpin_page(bm, "t", 1, 0);
    bm_unpin_page(bm, "t", 1, 0);
    destroy_bm(bm);
    return 1;
}

static int test_fetch_null_params(void) {
    BufferManager *bm = make_bm_nocache(4);
    ASSERT(bm_fetch_page(NULL, "t", 1) == NULL, "NULL bm returns NULL");
    ASSERT(bm_fetch_page(bm, NULL, 1)  == NULL, "NULL table returns NULL");
    destroy_bm(bm);
    return 1;
}

static int test_fetch_multiple_pages(void) {
    write_test_page("t", 1, 0xAA);
    write_test_page("t", 2, 0xBB);
    write_test_page("t", 3, 0xCC);
    BufferManager *bm = make_bm_nocache(8);

    char *p1 = bm_fetch_page(bm, "t", 1);
    char *p2 = bm_fetch_page(bm, "t", 2);
    char *p3 = bm_fetch_page(bm, "t", 3);

    ASSERT(p1 != NULL && p2 != NULL && p3 != NULL, "all fetches return non-NULL");
    ASSERT(p1 != p2 && p2 != p3 && p1 != p3, "different pages in different frames");

    char e1[PAGE_SIZE]; memset(e1, 0xAA, PAGE_SIZE);
    char e2[PAGE_SIZE]; memset(e2, 0xBB, PAGE_SIZE);
    char e3[PAGE_SIZE]; memset(e3, 0xCC, PAGE_SIZE);
    ASSERT_MEM(e1, p1, PAGE_SIZE, "page 1 data correct");
    ASSERT_MEM(e2, p2, PAGE_SIZE, "page 2 data correct");
    ASSERT_MEM(e3, p3, PAGE_SIZE, "page 3 data correct");

    bm_unpin_page(bm, "t", 1, 0);
    bm_unpin_page(bm, "t", 2, 0);
    bm_unpin_page(bm, "t", 3, 0);
    destroy_bm(bm);
    return 1;
}

static int test_fetch_two_tables_no_interference(void) {
    write_test_page("users",  1, 0x11);
    write_test_page("orders", 1, 0x22);
    BufferManager *bm = make_bm_nocache(8);

    char *pu = bm_fetch_page(bm, "users",  1);
    char *po = bm_fetch_page(bm, "orders", 1);

    char eu[PAGE_SIZE]; memset(eu, 0x11, PAGE_SIZE);
    char eo[PAGE_SIZE]; memset(eo, 0x22, PAGE_SIZE);
    ASSERT_MEM(eu, pu, PAGE_SIZE, "users page 1 data correct");
    ASSERT_MEM(eo, po, PAGE_SIZE, "orders page 1 data correct");
    ASSERT(pu != po, "different tables loaded in different frames");

    bm_unpin_page(bm, "users",  1, 0);
    bm_unpin_page(bm, "orders", 1, 0);
    destroy_bm(bm);
    return 1;
}


// ============================================================================
// BLOCK 3: bm_unpin_page
// ============================================================================

static int test_unpin_basic(void) {
    write_test_page("t", 1, 0x55);
    BufferManager *bm = make_bm_nocache(4);

    bm_fetch_page(bm, "t", 1);
    ASSERT_INT(0, bm_unpin_page(bm, "t", 1, 0), "bm_unpin_page returns 0");

    destroy_bm(bm);
    return 1;
}

static int test_unpin_dirty_marks_frame(void) {
    write_test_page("t", 1, 0x66);
    BufferManager *bm = make_bm_nocache(4);

    bm_fetch_page(bm, "t", 1);
    bm_unpin_page(bm, "t", 1, 1);  // dirty=1

    // Find the frame and check dirty flag
    int fid = pt_lookup(&bm->pt, "t", 1);
    ASSERT(fid >= 0, "frame still in page table after unpin");
    ASSERT_INT(1, bm->pool.frames[fid].dirty, "frame marked dirty after unpin with dirty=1");

    destroy_bm(bm);
    return 1;
}

static int test_unpin_not_found_returns_minus1(void) {
    BufferManager *bm = make_bm_nocache(4);
    ASSERT_INT(-1, bm_unpin_page(bm, "t", 99, 0),
               "unpin non-loaded page returns -1");
    destroy_bm(bm);
    return 1;
}

static int test_unpin_null_params(void) {
    BufferManager *bm = make_bm_nocache(4);
    ASSERT_INT(-1, bm_unpin_page(NULL, "t", 1, 0), "NULL bm returns -1");
    ASSERT_INT(-1, bm_unpin_page(bm, NULL, 1, 0),  "NULL table returns -1");
    destroy_bm(bm);
    return 1;
}


// ============================================================================
// BLOCK 4: eviction behavior
// ============================================================================

static int test_eviction_when_pool_full(void) {
    // Pool of 2 frames, fetch 3 pages -> eviction must occur
    write_test_page("t", 1, 0x11);
    write_test_page("t", 2, 0x22);
    write_test_page("t", 3, 0x33);
    BufferManager *bm = make_bm_nocache(2);

    char *p1 = bm_fetch_page(bm, "t", 1);
    ASSERT(p1 != NULL, "page 1 loaded");
    bm_unpin_page(bm, "t", 1, 0);

    char *p2 = bm_fetch_page(bm, "t", 2);
    ASSERT(p2 != NULL, "page 2 loaded");
    bm_unpin_page(bm, "t", 2, 0);

    // Pool full — fetching page 3 must evict page 1 or 2
    char *p3 = bm_fetch_page(bm, "t", 3);
    ASSERT(p3 != NULL, "page 3 loaded after eviction");
    ASSERT_INT(1, (int)bm_get_evictions(bm), "one eviction occurred");

    char e3[PAGE_SIZE]; memset(e3, 0x33, PAGE_SIZE);
    ASSERT_MEM(e3, p3, PAGE_SIZE, "evicted and reloaded page has correct data");

    bm_unpin_page(bm, "t", 3, 0);
    destroy_bm(bm);
    return 1;
}

static int test_eviction_dirty_page_written_to_disk(void) {
    write_test_page("t", 1, 0x11);
    write_test_page("t", 2, 0x22);
    write_test_page("t", 3, 0x33);
    BufferManager *bm = make_bm_nocache(2);

    // Load page 1, modify it, unpin as dirty
    char *p1 = bm_fetch_page(bm, "t", 1);
    memset(p1, 0xFF, PAGE_SIZE);
    bm_unpin_page(bm, "t", 1, 1);  // dirty

    // Load page 2
    bm_fetch_page(bm, "t", 2);
    bm_unpin_page(bm, "t", 2, 0);

    // Load page 3 -> evicts page 1 (NoCache: first OCCUPIED)
    // Page 1 is dirty -> must write 0xFF to disk
    bm_fetch_page(bm, "t", 3);
    bm_unpin_page(bm, "t", 3, 0);

    // Verify page 1 was written to disk
    char readback[PAGE_SIZE];
    load_page(TEST_DIR, "t", 1, readback);
    char expected[PAGE_SIZE]; memset(expected, 0xFF, PAGE_SIZE);
    ASSERT_MEM(expected, readback, PAGE_SIZE,
               "dirty evicted page written to disk");

    destroy_bm(bm);
    return 1;
}

static int test_eviction_page_table_updated(void) {
    write_test_page("t", 1, 0x11);
    write_test_page("t", 2, 0x22);
    write_test_page("t", 3, 0x33);
    BufferManager *bm = make_bm_nocache(2);

    bm_fetch_page(bm, "t", 1);
    bm_unpin_page(bm, "t", 1, 0);

    bm_fetch_page(bm, "t", 2);
    bm_unpin_page(bm, "t", 2, 0);

    // Evict page 1 by loading page 3
    bm_fetch_page(bm, "t", 3);

    // Page 1 must no longer be in the page table
    ASSERT_INT(-1, pt_lookup(&bm->pt, "t", 1),
               "evicted page removed from page table");
    // Page 3 must be in the page table
    ASSERT(pt_lookup(&bm->pt, "t", 3) >= 0,
           "newly loaded page in page table");

    bm_unpin_page(bm, "t", 3, 0);
    destroy_bm(bm);
    return 1;
}

static int test_fetch_evicted_page_reloads_from_disk(void) {
    write_test_page("t", 1, 0xAA);
    write_test_page("t", 2, 0xBB);
    write_test_page("t", 3, 0xCC);
    BufferManager *bm = make_bm_nocache(2);

    bm_fetch_page(bm, "t", 1);
    bm_unpin_page(bm, "t", 1, 0);

    bm_fetch_page(bm, "t", 2);
    bm_unpin_page(bm, "t", 2, 0);

    // Evict page 1
    bm_fetch_page(bm, "t", 3);
    bm_unpin_page(bm, "t", 3, 0);

    // Fetch page 1 again — must reload from disk
    char *p1 = bm_fetch_page(bm, "t", 1);
    ASSERT(p1 != NULL, "evicted page reloaded successfully");
    char expected[PAGE_SIZE]; memset(expected, 0xAA, PAGE_SIZE);
    ASSERT_MEM(expected, p1, PAGE_SIZE, "reloaded page has original disk content");

    bm_unpin_page(bm, "t", 1, 0);
    destroy_bm(bm);
    return 1;
}


// ============================================================================
// BLOCK 5: metrics
// ============================================================================

static int test_hit_rate_zero_initially(void) {
    BufferManager *bm = make_bm_nocache(4);
    ASSERT(bm_get_hit_rate(bm) == 0.0, "hit rate 0.0 with no accesses");
    destroy_bm(bm);
    return 1;
}

static int test_hit_rate_all_misses(void) {
    write_test_page("t", 1, 0x11);
    write_test_page("t", 2, 0x22);
    write_test_page("t", 3, 0x33);
    BufferManager *bm = make_bm_nocache(8);

    bm_fetch_page(bm, "t", 1);
    bm_fetch_page(bm, "t", 2);
    bm_fetch_page(bm, "t", 3);

    ASSERT(bm_get_hit_rate(bm) == 0.0, "hit rate 0.0 when all misses");

    bm_unpin_page(bm, "t", 1, 0);
    bm_unpin_page(bm, "t", 2, 0);
    bm_unpin_page(bm, "t", 3, 0);
    destroy_bm(bm);
    return 1;
}

static int test_hit_rate_mixed(void) {
    write_test_page("t", 1, 0x11);
    BufferManager *bm = make_bm_nocache(4);

    bm_fetch_page(bm, "t", 1);  // miss
    bm_fetch_page(bm, "t", 1);  // hit
    bm_fetch_page(bm, "t", 1);  // hit

    // 2 hits, 1 miss -> hit rate = 2/3
    double hr = bm_get_hit_rate(bm);
    ASSERT(hr > 0.66 && hr < 0.68, "hit rate ~0.667 with 2 hits and 1 miss");

    bm_unpin_page(bm, "t", 1, 0);
    bm_unpin_page(bm, "t", 1, 0);
    bm_unpin_page(bm, "t", 1, 0);
    destroy_bm(bm);
    return 1;
}

static int test_reset_metrics(void) {
    write_test_page("t", 1, 0x11);
    BufferManager *bm = make_bm_nocache(4);

    bm_fetch_page(bm, "t", 1);
    bm_fetch_page(bm, "t", 1);
    ASSERT(bm_get_hits(bm) > 0 || bm_get_misses(bm) > 0,
           "metrics non-zero after fetches");

    bm_reset_metrics(bm);
    ASSERT_INT(0, (int)bm_get_hits(bm),      "hits == 0 after reset");
    ASSERT_INT(0, (int)bm_get_misses(bm),    "misses == 0 after reset");
    ASSERT_INT(0, (int)bm_get_evictions(bm), "evictions == 0 after reset");
    ASSERT(bm_get_hit_rate(bm) == 0.0,       "hit rate 0.0 after reset");

    bm_unpin_page(bm, "t", 1, 0);
    bm_unpin_page(bm, "t", 1, 0);
    destroy_bm(bm);
    return 1;
}

static int test_metrics_null(void) {
    ASSERT_INT(0, (int)bm_get_hits(NULL),      "hits NULL returns 0");
    ASSERT_INT(0, (int)bm_get_misses(NULL),    "misses NULL returns 0");
    ASSERT_INT(0, (int)bm_get_evictions(NULL), "evictions NULL returns 0");
    ASSERT(bm_get_hit_rate(NULL) == 0.0,       "hit rate NULL returns 0.0");
    return 1;
}


// ============================================================================
// BLOCK 6: policy comparison
// ============================================================================

static int test_lru_higher_hit_rate_than_nocache(void) {
    // With a repeating access pattern, LRU should have higher hit rate
    // Pattern: page 1, page 2, page 1, page 2, page 1 (pool size 2)
    write_test_page("t", 1, 0x11);
    write_test_page("t", 2, 0x22);

    BufferManager *bm_nc  = make_bm_nocache(2);
    BufferManager *bm_lru = make_bm_lru(2);

    // Access pattern: 1,2,1,2,1,2
    const int pages[] = {1, 2, 1, 2, 1, 2};
    int n = 6;

    for (int i = 0; i < n; i++) {
        bm_fetch_page(bm_nc,  "t", pages[i]);
        bm_unpin_page(bm_nc,  "t", pages[i], 0);
        bm_fetch_page(bm_lru, "t", pages[i]);
        bm_unpin_page(bm_lru, "t", pages[i], 0);
    }

    double hr_nc  = bm_get_hit_rate(bm_nc);
    double hr_lru = bm_get_hit_rate(bm_lru);

    // With 2 frames and alternating pattern, LRU keeps both pages in pool
    ASSERT(hr_lru >= hr_nc,
           "LRU hit rate >= NoCache hit rate on repeating pattern");

    destroy_bm(bm_nc);
    destroy_bm(bm_lru);
    return 1;
}

static int test_different_policies_same_result_hot_page(void) {
    // A hot page accessed many times should be a HIT under any policy
    // when pool is large enough
    write_test_page("t", 1, 0x11);

    BufferManager *bm_nc    = make_bm_nocache(8);
    BufferManager *bm_clock = make_bm_clock(8);
    BufferManager *bm_lru   = make_bm_lru(8);

    // First access is always a miss
    bm_fetch_page(bm_nc,    "t", 1); bm_unpin_page(bm_nc,    "t", 1, 0);
    bm_fetch_page(bm_clock, "t", 1); bm_unpin_page(bm_clock, "t", 1, 0);
    bm_fetch_page(bm_lru,   "t", 1); bm_unpin_page(bm_lru,   "t", 1, 0);

    bm_reset_metrics(bm_nc);
    bm_reset_metrics(bm_clock);
    bm_reset_metrics(bm_lru);

    // Subsequent accesses to same page should all be HITs
    for (int i = 0; i < 10; i++) {
        bm_fetch_page(bm_nc,    "t", 1); bm_unpin_page(bm_nc,    "t", 1, 0);
        bm_fetch_page(bm_clock, "t", 1); bm_unpin_page(bm_clock, "t", 1, 0);
        bm_fetch_page(bm_lru,   "t", 1); bm_unpin_page(bm_lru,   "t", 1, 0);
    }

    ASSERT(bm_get_hit_rate(bm_nc)    == 1.0, "NoCache: hot page is 100% HIT");
    ASSERT(bm_get_hit_rate(bm_clock) == 1.0, "Clock:   hot page is 100% HIT");
    ASSERT(bm_get_hit_rate(bm_lru)   == 1.0, "LRU:     hot page is 100% HIT");

    destroy_bm(bm_nc);
    destroy_bm(bm_clock);
    destroy_bm(bm_lru);
    return 1;
}


// ============================================================================
// BLOCK 7: stress
// ============================================================================

static int test_stress_sequential_scan(void) {
    // Sequential scan: access pages 1..20 in order, pool size 4
    // All should be misses — classic sequential scan pattern
    int npages = 20;
    for (int i = 1; i <= npages; i++)
        write_test_page("t", i, (char)(i % 256));

    BufferManager *bm = make_bm_lru(4);

    for (int i = 1; i <= npages; i++) {
        char *data = bm_fetch_page(bm, "t", i);
        ASSERT(data != NULL, "page loaded in sequential scan");
        bm_unpin_page(bm, "t", i, 0);
    }

    // Sequential scan: all misses expected
    ASSERT_INT(npages, (int)bm_get_misses(bm),
               "sequential scan has all misses");

    destroy_bm(bm);
    return 1;
}

static int test_stress_repeated_access_same_page(void) {
    write_test_page("t", 1, 0x77);
    BufferManager *bm = make_bm_lru(4);

    // First fetch is a miss
    bm_fetch_page(bm, "t", 1);
    bm_unpin_page(bm, "t", 1, 0);

    // Next 99 fetches should all be hits
    for (int i = 0; i < 99; i++) {
        bm_fetch_page(bm, "t", 1);
        bm_unpin_page(bm, "t", 1, 0);
    }

    ASSERT_INT(1,  (int)bm_get_misses(bm), "only 1 miss for 100 accesses to same page");
    ASSERT_INT(99, (int)bm_get_hits(bm),   "99 hits for repeated access");

    destroy_bm(bm);
    return 1;
}

static int test_stress_two_tables_interleaved(void) {
    for (int i = 1; i <= 5; i++) {
        write_test_page("users",  i, (char)(0xA0 + i));
        write_test_page("orders", i, (char)(0xB0 + i));
    }

    BufferManager *bm = make_bm_lru(8);

    // Interleave access to both tables
    for (int i = 1; i <= 5; i++) {
        char *pu = bm_fetch_page(bm, "users",  i);
        char *po = bm_fetch_page(bm, "orders", i);
        ASSERT(pu != NULL && po != NULL, "both tables accessible");
        bm_unpin_page(bm, "users",  i, 0);
        bm_unpin_page(bm, "orders", i, 0);
    }

    // Verify data integrity
    for (int i = 1; i <= 5; i++) {
        char *pu = bm_fetch_page(bm, "users",  i);
        char *po = bm_fetch_page(bm, "orders", i);
        char eu[PAGE_SIZE]; memset(eu, (char)(0xA0 + i), PAGE_SIZE);
        char eo[PAGE_SIZE]; memset(eo, (char)(0xB0 + i), PAGE_SIZE);
        ASSERT_MEM(eu, pu, PAGE_SIZE, "users page data intact");
        ASSERT_MEM(eo, po, PAGE_SIZE, "orders page data intact");
        bm_unpin_page(bm, "users",  i, 0);
        bm_unpin_page(bm, "orders", i, 0);
    }

    destroy_bm(bm);
    return 1;
}


// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    strcpy(TEST_DIR, "/tmp/test_bm_XXXXXX");
    if (!mkdtemp(TEST_DIR)) { perror("mkdtemp"); return 1; }

    printf("=== TEST BUFFER MANAGER ===\n");
    printf("Temp directory: %s\n\n", TEST_DIR);

    printf("-- Block 1: bm_init / bm_destroy --\n");
    RUN_TEST(test_init_valid);
    RUN_TEST(test_init_null_params);
    RUN_TEST(test_destroy_flushes_dirty);
    RUN_TEST(test_destroy_null);

    printf("\n-- Block 2: bm_fetch_page --\n");
    RUN_TEST(test_fetch_loads_from_disk);
    RUN_TEST(test_fetch_miss_increments_misses);
    RUN_TEST(test_fetch_hit_increments_hits);
    RUN_TEST(test_fetch_returns_same_pointer_on_hit);
    RUN_TEST(test_fetch_null_params);
    RUN_TEST(test_fetch_multiple_pages);
    RUN_TEST(test_fetch_two_tables_no_interference);

    printf("\n-- Block 3: bm_unpin_page --\n");
    RUN_TEST(test_unpin_basic);
    RUN_TEST(test_unpin_dirty_marks_frame);
    RUN_TEST(test_unpin_not_found_returns_minus1);
    RUN_TEST(test_unpin_null_params);

    printf("\n-- Block 4: eviction --\n");
    RUN_TEST(test_eviction_when_pool_full);
    RUN_TEST(test_eviction_dirty_page_written_to_disk);
    RUN_TEST(test_eviction_page_table_updated);
    RUN_TEST(test_fetch_evicted_page_reloads_from_disk);

    printf("\n-- Block 5: metrics --\n");
    RUN_TEST(test_hit_rate_zero_initially);
    RUN_TEST(test_hit_rate_all_misses);
    RUN_TEST(test_hit_rate_mixed);
    RUN_TEST(test_reset_metrics);
    RUN_TEST(test_metrics_null);

    printf("\n-- Block 6: policy comparison --\n");
    RUN_TEST(test_lru_higher_hit_rate_than_nocache);
    RUN_TEST(test_different_policies_same_result_hot_page);

    printf("\n-- Block 7: stress --\n");
    RUN_TEST(test_stress_sequential_scan);
    RUN_TEST(test_stress_repeated_access_same_page);
    RUN_TEST(test_stress_two_tables_interleaved);

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", TEST_DIR);
    system(cmd);

    printf("\n=== RESULT: %d passed, %d failed, %d total ===\n",
           tests_passed, tests_failed, tests_passed + tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
