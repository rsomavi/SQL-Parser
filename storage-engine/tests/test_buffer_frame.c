// test_buffer_frame.c - Exhaustive test suite for buffer_frame module
// Compile: gcc -Wall -Wextra -g -o test_buffer_frame test_buffer_frame.c
//          ../buffer_frame.c ../disk.c ../page.c ../heap.c ../schema.c
// Run:     ./test_buffer_frame

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../buffer_frame.h"
#include "../disk.h"

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
    int _r = fn(); \
    if (_r) { printf("PASS\n"); tests_passed++; } \
    else    { printf("(see above)\n"); } \
} while(0)

static void cleanup_files(void) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -f %s/*.db 2>/dev/null", TEST_DIR);
    system(cmd);
}

static void fill_page(char *buf, char val) {
    memset(buf, val, PAGE_SIZE);
}

// BufferPool is ~4MB - always allocate on heap to avoid stack overflow
static BufferPool *make_pool(int num_frames) {
    BufferPool *p = malloc(sizeof(BufferPool));
    bp_init(p, num_frames, TEST_DIR);
    return p;
}


// ============================================================================
// BLOCK 1: bp_init
// ============================================================================

static int test_init_valid(void) {
    BufferPool *p = malloc(sizeof(BufferPool));
    ASSERT_INT(0, bp_init(p, 4, TEST_DIR), "bp_init valid returns 0");
    ASSERT_INT(4, p->num_frames, "num_frames set correctly");
    ASSERT(strcmp(p->data_dir, TEST_DIR) == 0, "data_dir stored correctly");
    ASSERT_INT(0, (int)p->hits,         "hits initialized to 0");
    ASSERT_INT(0, (int)p->misses,       "misses initialized to 0");
    ASSERT_INT(0, (int)p->evictions,    "evictions initialized to 0");
    ASSERT_INT(0, (int)p->access_clock, "access_clock initialized to 0");
    free(p);
    return 1;
}

static int test_init_all_frames_free(void) {
    BufferPool *p = malloc(sizeof(BufferPool));
    bp_init(p, 8, TEST_DIR);
    for (int i = 0; i < 8; i++) {
        ASSERT(p->frames[i].state == FRAME_FREE,
               "frame state is FREE after init");
        ASSERT_INT(FRAME_INVALID_PAGE, p->frames[i].page_id,
               "page_id is INVALID after init");
        ASSERT_INT(0, p->frames[i].pin_count, "pin_count is 0 after init");
        ASSERT_INT(0, p->frames[i].dirty,     "dirty is 0 after init");
    }
    free(p);
    return 1;
}

static int test_init_null_pool(void) {
    ASSERT_INT(-1, bp_init(NULL, 4, TEST_DIR), "bp_init NULL pool returns -1");
    return 1;
}

static int test_init_null_data_dir(void) {
    BufferPool *p = malloc(sizeof(BufferPool));
    ASSERT_INT(-1, bp_init(p, 4, NULL), "bp_init NULL data_dir returns -1");
    free(p);
    return 1;
}

static int test_init_zero_frames(void) {
    BufferPool *p = malloc(sizeof(BufferPool));
    ASSERT_INT(-1, bp_init(p, 0, TEST_DIR), "bp_init 0 frames returns -1");
    free(p);
    return 1;
}

static int test_init_negative_frames(void) {
    BufferPool *p = malloc(sizeof(BufferPool));
    ASSERT_INT(-1, bp_init(p, -1, TEST_DIR), "bp_init -1 frames returns -1");
    free(p);
    return 1;
}

static int test_init_max_frames(void) {
    BufferPool *p = malloc(sizeof(BufferPool));
    ASSERT_INT(0, bp_init(p, BUFFER_POOL_MAX_FRAMES, TEST_DIR),
               "bp_init MAX_FRAMES returns 0");
    ASSERT_INT(BUFFER_POOL_MAX_FRAMES, p->num_frames,
               "num_frames == MAX_FRAMES");
    free(p);
    return 1;
}

static int test_init_exceeds_max_frames(void) {
    BufferPool *p = malloc(sizeof(BufferPool));
    ASSERT_INT(-1, bp_init(p, BUFFER_POOL_MAX_FRAMES + 1, TEST_DIR),
               "bp_init MAX+1 frames returns -1");
    free(p);
    return 1;
}

static int test_init_twice_resets(void) {
    BufferPool *p = malloc(sizeof(BufferPool));
    bp_init(p, 4, TEST_DIR);
    char data[PAGE_SIZE]; fill_page(data, 0xAA);
    bp_load_frame(p, 0, "t", 1, data);
    bp_init(p, 4, TEST_DIR);
    ASSERT(p->frames[0].state == FRAME_FREE,
           "frame 0 is FREE after second init");
    ASSERT_INT(0, (int)p->hits, "hits reset after second init");
    free(p);
    return 1;
}


// ============================================================================
// BLOCK 2: bp_reset_metrics
// ============================================================================

static int test_reset_metrics(void) {
    BufferPool *p = make_pool(4);
    char data[PAGE_SIZE]; fill_page(data, 0x11);
    bp_load_frame(p, 0, "t", 1, data);
    bp_pin_frame(p, 0);
    ASSERT(p->hits > 0 || p->misses > 0, "metrics non-zero after operations");
    bp_reset_metrics(p);
    ASSERT_INT(0, (int)p->hits,         "hits == 0 after reset");
    ASSERT_INT(0, (int)p->misses,       "misses == 0 after reset");
    ASSERT_INT(0, (int)p->evictions,    "evictions == 0 after reset");
    ASSERT_INT(0, (int)p->access_clock, "access_clock == 0 after reset");
    free(p);
    return 1;
}

static int test_reset_metrics_null(void) {
    bp_reset_metrics(NULL); // must not crash
    return 1;
}


// ============================================================================
// BLOCK 3: bp_find_frame
// ============================================================================

static int test_find_frame_miss(void) {
    BufferPool *p = make_pool(4);
    ASSERT_INT(-1, bp_find_frame(p, "users", 1),
               "find_frame returns -1 on empty pool");
    free(p);
    return 1;
}

static int test_find_frame_hit(void) {
    BufferPool *p = make_pool(4);
    char data[PAGE_SIZE]; fill_page(data, 0xBB);
    bp_load_frame(p, 0, "users", 3, data);
    ASSERT_INT(0, bp_find_frame(p, "users", 3),
               "find_frame returns correct frame_id");
    free(p);
    return 1;
}

static int test_find_frame_wrong_table(void) {
    BufferPool *p = make_pool(4);
    char data[PAGE_SIZE]; fill_page(data, 0xCC);
    bp_load_frame(p, 0, "users", 1, data);
    ASSERT_INT(-1, bp_find_frame(p, "orders", 1),
               "find_frame returns -1 for wrong table");
    free(p);
    return 1;
}

static int test_find_frame_wrong_page(void) {
    BufferPool *p = make_pool(4);
    char data[PAGE_SIZE]; fill_page(data, 0xDD);
    bp_load_frame(p, 0, "users", 1, data);
    ASSERT_INT(-1, bp_find_frame(p, "users", 2),
               "find_frame returns -1 for wrong page_id");
    free(p);
    return 1;
}

static int test_find_frame_multiple_frames(void) {
    BufferPool *p = make_pool(8);
    char data[PAGE_SIZE]; fill_page(data, 0x55);
    bp_load_frame(p, 0, "users",  1, data);
    bp_load_frame(p, 1, "users",  2, data);
    bp_load_frame(p, 2, "orders", 1, data);
    bp_load_frame(p, 3, "orders", 2, data);
    ASSERT_INT(0, bp_find_frame(p, "users",  1), "find users  page 1 -> frame 0");
    ASSERT_INT(1, bp_find_frame(p, "users",  2), "find users  page 2 -> frame 1");
    ASSERT_INT(2, bp_find_frame(p, "orders", 1), "find orders page 1 -> frame 2");
    ASSERT_INT(3, bp_find_frame(p, "orders", 2), "find orders page 2 -> frame 3");
    free(p);
    return 1;
}

static int test_find_frame_null_params(void) {
    BufferPool *p = make_pool(4);
    ASSERT_INT(-1, bp_find_frame(NULL, "t", 1), "find_frame NULL pool returns -1");
    ASSERT_INT(-1, bp_find_frame(p, NULL, 1),   "find_frame NULL table returns -1");
    free(p);
    return 1;
}

static int test_find_frame_same_page_two_tables(void) {
    BufferPool *p = make_pool(4);
    char da[PAGE_SIZE]; fill_page(da, 0xAA);
    char db[PAGE_SIZE]; fill_page(db, 0xBB);
    bp_load_frame(p, 0, "users",  1, da);
    bp_load_frame(p, 1, "orders", 1, db);
    ASSERT_INT(0, bp_find_frame(p, "users",  1), "users  page 1 -> frame 0");
    ASSERT_INT(1, bp_find_frame(p, "orders", 1), "orders page 1 -> frame 1");
    free(p);
    return 1;
}

static int test_find_frame_not_found_after_evict(void) {
    BufferPool *p = make_pool(4);
    char data[PAGE_SIZE]; fill_page(data, 0x77);
    bp_load_frame(p, 0, "t", 1, data);
    bp_unpin_frame(p, 0, 0);
    bp_evict_frame(p, 0);
    ASSERT_INT(-1, bp_find_frame(p, "t", 1),
               "find_frame returns -1 after eviction");
    free(p);
    return 1;
}


// ============================================================================
// BLOCK 4: bp_find_free_frame
// ============================================================================

static int test_find_free_frame_empty_pool(void) {
    BufferPool *p = make_pool(4);
    ASSERT_INT(0, bp_find_free_frame(p),
               "first free frame is 0 in empty pool");
    free(p);
    return 1;
}

static int test_find_free_frame_all_occupied(void) {
    BufferPool *p = make_pool(3);
    char data[PAGE_SIZE]; fill_page(data, 0x11);
    bp_load_frame(p, 0, "t", 1, data);
    bp_load_frame(p, 1, "t", 2, data);
    bp_load_frame(p, 2, "t", 3, data);
    ASSERT_INT(-1, bp_find_free_frame(p),
               "find_free_frame returns -1 when pool full");
    free(p);
    return 1;
}

static int test_find_free_frame_after_evict(void) {
    BufferPool *p = make_pool(2);
    char data[PAGE_SIZE]; fill_page(data, 0x22);
    bp_load_frame(p, 0, "t", 1, data);
    bp_load_frame(p, 1, "t", 2, data);
    ASSERT_INT(-1, bp_find_free_frame(p), "no free frames before evict");
    bp_unpin_frame(p, 0, 0);
    bp_evict_frame(p, 0);
    ASSERT_INT(0, bp_find_free_frame(p), "frame 0 free after evict");
    free(p);
    return 1;
}

static int test_find_free_frame_null(void) {
    ASSERT_INT(-1, bp_find_free_frame(NULL),
               "find_free_frame NULL returns -1");
    return 1;
}


// ============================================================================
// BLOCK 5: bp_load_frame
// ============================================================================

static int test_load_frame_basic(void) {
    BufferPool *p = make_pool(4);
    char data[PAGE_SIZE]; fill_page(data, 0xAB);
    ASSERT_INT(0, bp_load_frame(p, 0, "users", 3, data),
               "bp_load_frame returns 0");
    BufferFrame *f = &p->frames[0];
    ASSERT(f->state == FRAME_OCCUPIED, "state is OCCUPIED after load");
    ASSERT_INT(1, f->pin_count, "pin_count is 1 after load");
    ASSERT_INT(0, f->dirty,     "dirty is 0 after load");
    ASSERT_INT(1, f->ref_bit,   "ref_bit is 1 after load");
    ASSERT_INT(3, f->page_id,   "page_id stored correctly");
    ASSERT(strcmp(f->table_name, "users") == 0,
           "table_name stored correctly");
    ASSERT_MEM(data, f->data, PAGE_SIZE, "page data stored correctly");
    free(p);
    return 1;
}

static int test_load_frame_increments_misses(void) {
    BufferPool *p = make_pool(4);
    char data[PAGE_SIZE]; fill_page(data, 0x33);
    long long before = p->misses;
    bp_load_frame(p, 0, "t", 1, data);
    ASSERT_INT((int)(before + 1), (int)p->misses,
               "misses incremented after load");
    free(p);
    return 1;
}

static int test_load_frame_increments_access_clock(void) {
    BufferPool *p = make_pool(4);
    char data[PAGE_SIZE]; fill_page(data, 0x44);
    bp_load_frame(p, 0, "t", 1, data);
    ASSERT(p->frames[0].last_access == 1,
           "last_access == 1 after first load");
    bp_load_frame(p, 1, "t", 2, data);
    ASSERT(p->frames[1].last_access == 2,
           "last_access == 2 after second load");
    free(p);
    return 1;
}

static int test_load_frame_invalid_frame_id(void) {
    BufferPool *p = make_pool(4);
    char data[PAGE_SIZE]; fill_page(data, 0xFF);
    ASSERT_INT(-1, bp_load_frame(p, -1, "t", 1, data),
               "load_frame frame_id -1 returns -1");
    ASSERT_INT(-1, bp_load_frame(p,  4, "t", 1, data),
               "load_frame frame_id == num_frames returns -1");
    ASSERT_INT(-1, bp_load_frame(p, 99, "t", 1, data),
               "load_frame out-of-bounds returns -1");
    free(p);
    return 1;
}

static int test_load_frame_null_params(void) {
    BufferPool *p = make_pool(4);
    char data[PAGE_SIZE]; fill_page(data, 0x11);
    ASSERT_INT(-1, bp_load_frame(NULL, 0, "t",  1, data), "NULL pool returns -1");
    ASSERT_INT(-1, bp_load_frame(p,    0, NULL, 1, data), "NULL table returns -1");
    ASSERT_INT(-1, bp_load_frame(p,    0, "t",  1, NULL), "NULL data returns -1");
    free(p);
    return 1;
}

static int test_load_frame_data_integrity(void) {
    BufferPool *p = make_pool(4);
    char data[PAGE_SIZE];
    for (int i = 0; i < PAGE_SIZE; i++) data[i] = (char)(i % 256);
    bp_load_frame(p, 0, "t", 1, data);
    ASSERT_MEM(data, p->frames[0].data, PAGE_SIZE,
               "cyclic pattern preserved after load");
    free(p);
    return 1;
}

static int test_load_frame_null_bytes(void) {
    BufferPool *p = make_pool(4);
    char data[PAGE_SIZE];
    for (int i = 0; i < PAGE_SIZE; i++)
        data[i] = (i % 3 == 0) ? 0x00 : (char)0xFF;
    bp_load_frame(p, 0, "t", 1, data);
    ASSERT_MEM(data, p->frames[0].data, PAGE_SIZE,
               "null bytes preserved correctly");
    free(p);
    return 1;
}

static int test_load_frame_overwrites_previous(void) {
    BufferPool *p = make_pool(4);
    char d1[PAGE_SIZE]; fill_page(d1, 0xAA);
    char d2[PAGE_SIZE]; fill_page(d2, 0xBB);
    bp_load_frame(p, 0, "t", 1, d1);
    bp_unpin_frame(p, 0, 0);
    bp_evict_frame(p, 0);
    bp_load_frame(p, 0, "t", 2, d2);
    ASSERT_MEM(d2, p->frames[0].data, PAGE_SIZE,
               "second load overwrites first correctly");
    ASSERT_INT(2, p->frames[0].page_id, "page_id updated on second load");
    free(p);
    return 1;
}


// ============================================================================
// BLOCK 6: bp_pin_frame
// ============================================================================

static int test_pin_frame_basic(void) {
    BufferPool *p = make_pool(4);
    char data[PAGE_SIZE]; fill_page(data, 0x55);
    bp_load_frame(p, 0, "t", 1, data);
    // After load: pin_count=1, state=OCCUPIED. Pin again -> pin_count=2, PINNED
    ASSERT_INT(0, bp_pin_frame(p, 0), "bp_pin_frame returns 0");
    ASSERT_INT(2, p->frames[0].pin_count, "pin_count == 2 after second pin");
    ASSERT(p->frames[0].state == FRAME_PINNED, "state is PINNED after pin");
    ASSERT_INT(1, p->frames[0].ref_bit, "ref_bit is 1 after pin");
    free(p);
    return 1;
}

static int test_pin_frame_increments_hits(void) {
    BufferPool *p = make_pool(4);
    char data[PAGE_SIZE]; fill_page(data, 0x66);
    bp_load_frame(p, 0, "t", 1, data);
    long long before = p->hits;
    bp_pin_frame(p, 0);
    ASSERT_INT((int)(before + 1), (int)p->hits, "hits incremented after pin");
    free(p);
    return 1;
}

static int test_pin_frame_free_frame_fails(void) {
    BufferPool *p = make_pool(4);
    ASSERT_INT(-1, bp_pin_frame(p, 0), "pin_frame on FREE frame returns -1");
    free(p);
    return 1;
}

static int test_pin_frame_invalid_id(void) {
    BufferPool *p = make_pool(4);
    ASSERT_INT(-1, bp_pin_frame(p, -1),  "pin_frame -1 returns -1");
    ASSERT_INT(-1, bp_pin_frame(p,  4),  "pin_frame == num_frames returns -1");
    ASSERT_INT(-1, bp_pin_frame(p, 999), "pin_frame out-of-bounds returns -1");
    free(p);
    return 1;
}

static int test_pin_frame_null_pool(void) {
    ASSERT_INT(-1, bp_pin_frame(NULL, 0), "pin_frame NULL pool returns -1");
    return 1;
}

static int test_pin_frame_increments_access_clock(void) {
    BufferPool *p = make_pool(4);
    char data[PAGE_SIZE]; fill_page(data, 0x77);
    bp_load_frame(p, 0, "t", 1, data);
    long long before = p->access_clock;
    bp_pin_frame(p, 0);
    ASSERT(p->access_clock == before + 1, "access_clock incremented after pin");
    ASSERT(p->frames[0].last_access == p->access_clock,
           "last_access updated after pin");
    free(p);
    return 1;
}

static int test_pin_frame_multiple_pins(void) {
    BufferPool *p = make_pool(4);
    char data[PAGE_SIZE]; fill_page(data, 0x88);
    bp_load_frame(p, 0, "t", 1, data);
    bp_pin_frame(p, 0);
    bp_pin_frame(p, 0);
    bp_pin_frame(p, 0);
    ASSERT_INT(4, p->frames[0].pin_count, "pin_count == 4 after 3 extra pins");
    ASSERT(p->frames[0].state == FRAME_PINNED,
           "state PINNED after multiple pins");
    free(p);
    return 1;
}


// ============================================================================
// BLOCK 7: bp_unpin_frame
// ============================================================================

static int test_unpin_frame_basic(void) {
    BufferPool *p = make_pool(4);
    char data[PAGE_SIZE]; fill_page(data, 0x99);
    bp_load_frame(p, 0, "t", 1, data);
    ASSERT_INT(0, bp_unpin_frame(p, 0, 0), "bp_unpin_frame returns 0");
    ASSERT_INT(0, p->frames[0].pin_count,  "pin_count == 0 after unpin");
    ASSERT(p->frames[0].state == FRAME_OCCUPIED,
           "state OCCUPIED after unpin to 0");
    free(p);
    return 1;
}

static int test_unpin_frame_stays_pinned(void) {
    BufferPool *p = make_pool(4);
    char data[PAGE_SIZE]; fill_page(data, 0xAA);
    bp_load_frame(p, 0, "t", 1, data);
    bp_pin_frame(p, 0);          // pin_count=2
    bp_unpin_frame(p, 0, 0);     // pin_count=1
    ASSERT_INT(1, p->frames[0].pin_count,
               "pin_count == 1 after partial unpin");
    ASSERT(p->frames[0].state == FRAME_PINNED,
           "state still PINNED with pin_count > 0");
    free(p);
    return 1;
}

static int test_unpin_frame_sets_dirty(void) {
    BufferPool *p = make_pool(4);
    char data[PAGE_SIZE]; fill_page(data, 0xBB);
    bp_load_frame(p, 0, "t", 1, data);
    ASSERT_INT(0, p->frames[0].dirty, "dirty is 0 before unpin");
    bp_unpin_frame(p, 0, 1);
    ASSERT_INT(1, p->frames[0].dirty, "dirty is 1 after unpin with dirty=1");
    free(p);
    return 1;
}

static int test_unpin_frame_dirty_not_cleared(void) {
    // If dirty was already 1, unpin with dirty=0 must NOT clear it
    BufferPool *p = make_pool(4);
    char data[PAGE_SIZE]; fill_page(data, 0xCC);
    bp_load_frame(p, 0, "t", 1, data);
    bp_pin_frame(p, 0);           // pin_count=2
    bp_unpin_frame(p, 0, 1);      // mark dirty, pin_count=1
    bp_unpin_frame(p, 0, 0);      // unpin with dirty=0, pin_count=0
    ASSERT_INT(1, p->frames[0].dirty,
               "dirty stays 1 even after unpin with dirty=0");
    free(p);
    return 1;
}

static int test_unpin_frame_zero_pin_count_fails(void) {
    BufferPool *p = make_pool(4);
    char data[PAGE_SIZE]; fill_page(data, 0xDD);
    bp_load_frame(p, 0, "t", 1, data);
    bp_unpin_frame(p, 0, 0);      // pin_count -> 0
    ASSERT_INT(-1, bp_unpin_frame(p, 0, 0),
               "unpin with pin_count==0 returns -1");
    free(p);
    return 1;
}

static int test_unpin_frame_invalid_id(void) {
    BufferPool *p = make_pool(4);
    ASSERT_INT(-1, bp_unpin_frame(p, -1, 0), "unpin -1 returns -1");
    ASSERT_INT(-1, bp_unpin_frame(p,  4, 0), "unpin == num_frames returns -1");
    ASSERT_INT(-1, bp_unpin_frame(p, 99, 0), "unpin out-of-bounds returns -1");
    free(p);
    return 1;
}

static int test_unpin_frame_null_pool(void) {
    ASSERT_INT(-1, bp_unpin_frame(NULL, 0, 0), "unpin NULL pool returns -1");
    return 1;
}


// ============================================================================
// BLOCK 8: bp_evict_frame
// ============================================================================

static int test_evict_frame_basic(void) {
    BufferPool *p = make_pool(4);
    char data[PAGE_SIZE]; fill_page(data, 0xEE);
    bp_load_frame(p, 0, "t", 1, data);
    bp_unpin_frame(p, 0, 0);
    ASSERT_INT(0, bp_evict_frame(p, 0), "bp_evict_frame returns 0");
    ASSERT(p->frames[0].state == FRAME_FREE, "state FREE after evict");
    ASSERT_INT(FRAME_INVALID_PAGE, p->frames[0].page_id,
               "page_id reset after evict");
    ASSERT_INT(0, p->frames[0].pin_count, "pin_count 0 after evict");
    ASSERT_INT(0, p->frames[0].dirty,     "dirty 0 after evict");
    free(p);
    return 1;
}

static int test_evict_frame_pinned_fails(void) {
    BufferPool *p = make_pool(4);
    char data[PAGE_SIZE]; fill_page(data, 0xFF);
    bp_load_frame(p, 0, "t", 1, data);
    bp_pin_frame(p, 0);  // pin_count=2, PINNED
    ASSERT_INT(-1, bp_evict_frame(p, 0), "evict PINNED frame returns -1");
    ASSERT(p->frames[0].state == FRAME_PINNED,
           "frame still PINNED after failed evict");
    free(p);
    return 1;
}

static int test_evict_frame_increments_evictions(void) {
    BufferPool *p = make_pool(4);
    char data[PAGE_SIZE]; fill_page(data, 0x12);
    bp_load_frame(p, 0, "t", 1, data);
    bp_unpin_frame(p, 0, 0);
    long long before = p->evictions;
    bp_evict_frame(p, 0);
    ASSERT_INT((int)(before + 1), (int)p->evictions,
               "evictions incremented after evict");
    free(p);
    return 1;
}

static int test_evict_frame_invalid_id(void) {
    BufferPool *p = make_pool(4);
    ASSERT_INT(-1, bp_evict_frame(p, -1), "evict -1 returns -1");
    ASSERT_INT(-1, bp_evict_frame(p,  4), "evict == num_frames returns -1");
    ASSERT_INT(-1, bp_evict_frame(p, 99), "evict out-of-bounds returns -1");
    free(p);
    return 1;
}

static int test_evict_frame_null_pool(void) {
    ASSERT_INT(-1, bp_evict_frame(NULL, 0), "evict NULL pool returns -1");
    return 1;
}

static int test_evict_frame_free_frame(void) {
    // Evicting a FREE frame must not crash and must leave it FREE
    BufferPool *p = make_pool(4);
    bp_evict_frame(p, 0);
    ASSERT(p->frames[0].state == FRAME_FREE,
           "FREE frame stays FREE after evict");
    free(p);
    return 1;
}

static int test_evict_dirty_writes_to_disk(void) {
    cleanup_files();
    char original[PAGE_SIZE]; fill_page(original, 0x11);
    write_page(TEST_DIR, "dirty_test", 1, original);

    BufferPool *p = make_pool(4);
    bp_load_frame(p, 0, "dirty_test", 1, original);
    fill_page(p->frames[0].data, 0xCC);  // modify in memory
    bp_unpin_frame(p, 0, 1);             // mark dirty
    bp_evict_frame(p, 0);                // should write 0xCC to disk

    char readback[PAGE_SIZE];
    load_page(TEST_DIR, "dirty_test", 1, readback);
    char expected[PAGE_SIZE]; fill_page(expected, 0xCC);
    ASSERT_MEM(expected, readback, PAGE_SIZE,
               "dirty frame written to disk on eviction");
    free(p);
    return 1;
}

static int test_evict_clean_does_not_write(void) {
    cleanup_files();
    char original[PAGE_SIZE]; fill_page(original, 0xAA);
    write_page(TEST_DIR, "clean_test", 1, original);

    BufferPool *p = make_pool(4);
    bp_load_frame(p, 0, "clean_test", 1, original);
    fill_page(p->frames[0].data, 0xFF);  // modify in memory
    bp_unpin_frame(p, 0, 0);             // NOT dirty
    bp_evict_frame(p, 0);                // must NOT write to disk

    char readback[PAGE_SIZE];
    load_page(TEST_DIR, "clean_test", 1, readback);
    ASSERT_MEM(original, readback, PAGE_SIZE,
               "clean eviction does not overwrite disk");
    free(p);
    return 1;
}


// ============================================================================
// BLOCK 9: state counters
// ============================================================================

static int test_count_states_empty(void) {
    BufferPool *p = make_pool(4);
    ASSERT_INT(4, bp_count_free(p),     "count_free == 4 on empty pool");
    ASSERT_INT(0, bp_count_occupied(p), "count_occupied == 0 on empty pool");
    ASSERT_INT(0, bp_count_pinned(p),   "count_pinned == 0 on empty pool");
    free(p);
    return 1;
}

static int test_count_states_after_load(void) {
    BufferPool *p = make_pool(4);
    char data[PAGE_SIZE]; fill_page(data, 0x11);
    bp_load_frame(p, 0, "t", 1, data);
    ASSERT_INT(3, bp_count_free(p),     "count_free == 3 after one load");
    ASSERT_INT(1, bp_count_occupied(p), "count_occupied == 1 after one load");
    ASSERT_INT(0, bp_count_pinned(p),   "count_pinned == 0 after one load");
    free(p);
    return 1;
}

static int test_count_states_after_pin(void) {
    BufferPool *p = make_pool(4);
    char data[PAGE_SIZE]; fill_page(data, 0x22);
    bp_load_frame(p, 0, "t", 1, data);
    bp_pin_frame(p, 0);
    ASSERT_INT(3, bp_count_free(p),     "count_free == 3 after pin");
    ASSERT_INT(0, bp_count_occupied(p), "count_occupied == 0 after pin");
    ASSERT_INT(1, bp_count_pinned(p),   "count_pinned == 1 after pin");
    free(p);
    return 1;
}

static int test_count_states_after_unpin(void) {
    BufferPool *p = make_pool(4);
    char data[PAGE_SIZE]; fill_page(data, 0x33);
    bp_load_frame(p, 0, "t", 1, data);
    bp_unpin_frame(p, 0, 0);
    ASSERT_INT(3, bp_count_free(p),     "count_free == 3 after unpin");
    ASSERT_INT(1, bp_count_occupied(p), "count_occupied == 1 after unpin");
    ASSERT_INT(0, bp_count_pinned(p),   "count_pinned == 0 after unpin");
    free(p);
    return 1;
}

static int test_count_states_after_evict(void) {
    BufferPool *p = make_pool(4);
    char data[PAGE_SIZE]; fill_page(data, 0x44);
    bp_load_frame(p, 0, "t", 1, data);
    bp_unpin_frame(p, 0, 0);
    bp_evict_frame(p, 0);
    ASSERT_INT(4, bp_count_free(p),     "count_free == 4 after evict");
    ASSERT_INT(0, bp_count_occupied(p), "count_occupied == 0 after evict");
    ASSERT_INT(0, bp_count_pinned(p),   "count_pinned == 0 after evict");
    free(p);
    return 1;
}

static int test_count_states_null(void) {
    ASSERT_INT(0, bp_count_free(NULL),     "count_free NULL returns 0");
    ASSERT_INT(0, bp_count_occupied(NULL), "count_occupied NULL returns 0");
    ASSERT_INT(0, bp_count_pinned(NULL),   "count_pinned NULL returns 0");
    return 1;
}


// ============================================================================
// BLOCK 10: metrics
// ============================================================================

static int test_metrics_hits_misses(void) {
    BufferPool *p = make_pool(4);
    char data[PAGE_SIZE]; fill_page(data, 0x55);
    bp_load_frame(p, 0, "t", 1, data);
    bp_load_frame(p, 1, "t", 2, data);
    ASSERT_INT(2, (int)p->misses, "2 misses after 2 loads");
    bp_pin_frame(p, 0);
    bp_pin_frame(p, 0);
    bp_pin_frame(p, 1);
    ASSERT_INT(3, (int)p->hits, "3 hits after 3 pins");
    free(p);
    return 1;
}

static int test_metrics_evictions(void) {
    BufferPool *p = make_pool(4);
    char data[PAGE_SIZE]; fill_page(data, 0x66);
    bp_load_frame(p, 0, "t", 1, data);
    bp_load_frame(p, 1, "t", 2, data);
    bp_unpin_frame(p, 0, 0);
    bp_unpin_frame(p, 1, 0);
    bp_evict_frame(p, 0);
    bp_evict_frame(p, 1);
    ASSERT_INT(2, (int)p->evictions, "2 evictions counted correctly");
    free(p);
    return 1;
}

static int test_metrics_access_clock_monotonic(void) {
    BufferPool *p = make_pool(8);
    char data[PAGE_SIZE]; fill_page(data, 0x77);
    for (int i = 0; i < 4; i++)
        bp_load_frame(p, i, "t", i+1, data);
    long long prev = 0;
    for (int i = 0; i < 4; i++) {
        ASSERT(p->frames[i].last_access > prev,
               "last_access is monotonically increasing");
        prev = p->frames[i].last_access;
    }
    free(p);
    return 1;
}


// ============================================================================
// BLOCK 11: stress
// ============================================================================

static int test_stress_fill_and_evict_all(void) {
    BufferPool *p = make_pool(16);
    char data[PAGE_SIZE]; fill_page(data, 0x88);
    for (int i = 0; i < 16; i++)
        bp_load_frame(p, i, "t", i+1, data);
    ASSERT_INT(0,  bp_count_free(p),     "0 free frames when pool full");
    ASSERT_INT(16, bp_count_occupied(p), "16 occupied when all loaded");
    for (int i = 0; i < 16; i++) {
        bp_unpin_frame(p, i, 0);
        bp_evict_frame(p, i);
    }
    ASSERT_INT(16, bp_count_free(p),     "16 free after evicting all");
    ASSERT_INT(0,  bp_count_occupied(p), "0 occupied after evicting all");
    ASSERT_INT(16, (int)p->evictions,    "16 evictions counted");
    free(p);
    return 1;
}

static int test_stress_load_find_evict_cycle(void) {
    BufferPool *p = make_pool(8);
    char data[PAGE_SIZE]; fill_page(data, 0x99);
    for (int round = 0; round < 10; round++) {
        for (int i = 0; i < 8; i++)
            bp_load_frame(p, i, "t", i + round*8 + 1, data);
        for (int i = 0; i < 8; i++) {
            int fid = bp_find_frame(p, "t", i + round*8 + 1);
            ASSERT_INT(i, fid, "find_frame correct in cycle");
        }
        for (int i = 0; i < 8; i++) {
            bp_unpin_frame(p, i, 0);
            bp_evict_frame(p, i);
        }
    }
    ASSERT_INT(8, bp_count_free(p), "all frames free after 10 cycles");
    free(p);
    return 1;
}

static int test_stress_two_tables_no_interference(void) {
    BufferPool *p = make_pool(8);
    char du[PAGE_SIZE]; fill_page(du, 0xAA);
    char dp[PAGE_SIZE]; fill_page(dp, 0xBB);
    for (int i = 0; i < 4; i++) {
        bp_load_frame(p, i,   "users",    i+1, du);
        bp_load_frame(p, i+4, "products", i+1, dp);
    }
    for (int i = 0; i < 4; i++) {
        ASSERT_MEM(du, p->frames[i].data,   PAGE_SIZE, "users data intact");
        ASSERT_MEM(dp, p->frames[i+4].data, PAGE_SIZE, "products data intact");
    }
    for (int i = 0; i < 4; i++) {
        ASSERT_INT(i,   bp_find_frame(p, "users",    i+1), "users frame correct");
        ASSERT_INT(i+4, bp_find_frame(p, "products", i+1), "products frame correct");
    }
    free(p);
    return 1;
}


// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    strcpy(TEST_DIR, "/tmp/test_bf_XXXXXX");
    if (!mkdtemp(TEST_DIR)) { perror("mkdtemp"); return 1; }

    printf("=== TEST BUFFER FRAME ===\n");
    printf("Temp directory: %s\n\n", TEST_DIR);

    printf("-- Block 1: bp_init --\n");
    RUN_TEST(test_init_valid);
    RUN_TEST(test_init_all_frames_free);
    RUN_TEST(test_init_null_pool);
    RUN_TEST(test_init_null_data_dir);
    RUN_TEST(test_init_zero_frames);
    RUN_TEST(test_init_negative_frames);
    RUN_TEST(test_init_max_frames);
    RUN_TEST(test_init_exceeds_max_frames);
    RUN_TEST(test_init_twice_resets);

    printf("\n-- Block 2: bp_reset_metrics --\n");
    RUN_TEST(test_reset_metrics);
    RUN_TEST(test_reset_metrics_null);

    printf("\n-- Block 3: bp_find_frame --\n");
    RUN_TEST(test_find_frame_miss);
    RUN_TEST(test_find_frame_hit);
    RUN_TEST(test_find_frame_wrong_table);
    RUN_TEST(test_find_frame_wrong_page);
    RUN_TEST(test_find_frame_multiple_frames);
    RUN_TEST(test_find_frame_null_params);
    RUN_TEST(test_find_frame_same_page_two_tables);
    RUN_TEST(test_find_frame_not_found_after_evict);

    printf("\n-- Block 4: bp_find_free_frame --\n");
    RUN_TEST(test_find_free_frame_empty_pool);
    RUN_TEST(test_find_free_frame_all_occupied);
    RUN_TEST(test_find_free_frame_after_evict);
    RUN_TEST(test_find_free_frame_null);

    printf("\n-- Block 5: bp_load_frame --\n");
    RUN_TEST(test_load_frame_basic);
    RUN_TEST(test_load_frame_increments_misses);
    RUN_TEST(test_load_frame_increments_access_clock);
    RUN_TEST(test_load_frame_invalid_frame_id);
    RUN_TEST(test_load_frame_null_params);
    RUN_TEST(test_load_frame_data_integrity);
    RUN_TEST(test_load_frame_null_bytes);
    RUN_TEST(test_load_frame_overwrites_previous);

    printf("\n-- Block 6: bp_pin_frame --\n");
    RUN_TEST(test_pin_frame_basic);
    RUN_TEST(test_pin_frame_increments_hits);
    RUN_TEST(test_pin_frame_free_frame_fails);
    RUN_TEST(test_pin_frame_invalid_id);
    RUN_TEST(test_pin_frame_null_pool);
    RUN_TEST(test_pin_frame_increments_access_clock);
    RUN_TEST(test_pin_frame_multiple_pins);

    printf("\n-- Block 7: bp_unpin_frame --\n");
    RUN_TEST(test_unpin_frame_basic);
    RUN_TEST(test_unpin_frame_stays_pinned);
    RUN_TEST(test_unpin_frame_sets_dirty);
    RUN_TEST(test_unpin_frame_dirty_not_cleared);
    RUN_TEST(test_unpin_frame_zero_pin_count_fails);
    RUN_TEST(test_unpin_frame_invalid_id);
    RUN_TEST(test_unpin_frame_null_pool);

    printf("\n-- Block 8: bp_evict_frame --\n");
    RUN_TEST(test_evict_frame_basic);
    RUN_TEST(test_evict_frame_pinned_fails);
    RUN_TEST(test_evict_frame_increments_evictions);
    RUN_TEST(test_evict_frame_invalid_id);
    RUN_TEST(test_evict_frame_null_pool);
    RUN_TEST(test_evict_frame_free_frame);
    RUN_TEST(test_evict_dirty_writes_to_disk);
    RUN_TEST(test_evict_clean_does_not_write);

    printf("\n-- Block 9: state counters --\n");
    RUN_TEST(test_count_states_empty);
    RUN_TEST(test_count_states_after_load);
    RUN_TEST(test_count_states_after_pin);
    RUN_TEST(test_count_states_after_unpin);
    RUN_TEST(test_count_states_after_evict);
    RUN_TEST(test_count_states_null);

    printf("\n-- Block 10: metrics --\n");
    RUN_TEST(test_metrics_hits_misses);
    RUN_TEST(test_metrics_evictions);
    RUN_TEST(test_metrics_access_clock_monotonic);

    printf("\n-- Block 11: stress --\n");
    RUN_TEST(test_stress_fill_and_evict_all);
    RUN_TEST(test_stress_load_find_evict_cycle);
    RUN_TEST(test_stress_two_tables_no_interference);

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", TEST_DIR);
    system(cmd);

    printf("\n=== RESULT: %d passed, %d failed, %d total ===\n",
           tests_passed, tests_failed, tests_passed + tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
