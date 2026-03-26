// test_eviction_policy.c - Exhaustive test suite for eviction_policy module
// Compile: gcc -Wall -Wextra -g -o test_eviction_policy test_eviction_policy.c
//          ../eviction_policy.c ../buffer_frame.c ../disk.c ../page.c ../heap.c ../schema.c
// Run:     ./test_eviction_policy

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../eviction_policy.h"

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

#define RUN_TEST(fn) do { \
    printf("  %-65s", #fn); \
    fflush(stdout); \
    int _r = fn(); \
    if (_r) { printf("PASS\n"); tests_passed++; } \
    else    { printf("(see above)\n"); } \
} while(0)

// Helper: allocate pool on heap
static BufferPool *make_pool(int num_frames) {
    BufferPool *p = malloc(sizeof(BufferPool));
    bp_init(p, num_frames, TEST_DIR);
    return p;
}

// Helper: load a frame and immediately unpin it (OCCUPIED, not PINNED)
static void load_occupied(BufferPool *p, int frame_id,
                           const char *table, int page_id, char val) {
    char data[PAGE_SIZE];
    memset(data, val, PAGE_SIZE);
    bp_load_frame(p, frame_id, table, page_id, data);
    bp_unpin_frame(p, frame_id, 0);  // pin_count -> 0, state -> OCCUPIED
}


// ============================================================================
// BLOCK 1: policy creation and destruction
// ============================================================================

static int test_nocache_create_destroy(void) {
    EvictionPolicy *p = policy_nocache_create();
    ASSERT(p != NULL,        "policy_nocache_create returns non-NULL");
    ASSERT(p->evict != NULL, "evict function pointer set");
    POLICY_DESTROY(p);
    free(p);
    return 1;
}

static int test_clock_create_destroy(void) {
    EvictionPolicy *p = policy_clock_create(16);
    ASSERT(p != NULL,        "policy_clock_create returns non-NULL");
    ASSERT(p->evict != NULL, "evict function pointer set");
    ASSERT(p->data  != NULL, "clock has private data");
    POLICY_DESTROY(p);
    free(p);
    return 1;
}

static int test_clock_create_invalid(void) {
    ASSERT(policy_clock_create(0)  == NULL, "clock create 0 frames returns NULL");
    ASSERT(policy_clock_create(-1) == NULL, "clock create -1 frames returns NULL");
    return 1;
}

static int test_lru_create_destroy(void) {
    EvictionPolicy *p = policy_lru_create();
    ASSERT(p != NULL,        "policy_lru_create returns non-NULL");
    ASSERT(p->evict != NULL, "evict function pointer set");
    POLICY_DESTROY(p);
    free(p);
    return 1;
}

static int test_opt_create_destroy(void) {
    OPTAccess trace[] = {
        {"users", 1}, {"users", 2}, {"users", 1}
    };
    EvictionPolicy *p = policy_opt_create(trace, 3);
    ASSERT(p != NULL,        "policy_opt_create returns non-NULL");
    ASSERT(p->evict != NULL, "evict function pointer set");
    ASSERT(p->data  != NULL, "OPT has private data");
    POLICY_DESTROY(p);
    free(p);
    return 1;
}

static int test_opt_create_invalid(void) {
    OPTAccess trace[] = {{"users", 1}};
    ASSERT(policy_opt_create(NULL,  1) == NULL, "opt create NULL trace returns NULL");
    ASSERT(policy_opt_create(trace, 0) == NULL, "opt create 0 len returns NULL");
    ASSERT(policy_opt_create(trace,-1) == NULL, "opt create -1 len returns NULL");
    return 1;
}

static int test_macros_null_policy(void) {
    // Macros must not crash with NULL policy
    POLICY_ON_PIN(NULL, 0);
    POLICY_ON_UNPIN(NULL, 0);
    ASSERT_INT(-1, POLICY_EVICT(NULL, NULL), "POLICY_EVICT NULL returns -1");
    POLICY_DESTROY(NULL);
    return 1;
}


// ============================================================================
// BLOCK 2: NoCache
// ============================================================================

static int test_nocache_evicts_first_occupied(void) {
    BufferPool *pool = make_pool(4);
    EvictionPolicy *p = policy_nocache_create();

    load_occupied(pool, 0, "t", 1, 0xAA);
    load_occupied(pool, 1, "t", 2, 0xBB);
    load_occupied(pool, 2, "t", 3, 0xCC);

    int victim = POLICY_EVICT(p, pool);
    ASSERT_INT(0, victim, "NoCache evicts first OCCUPIED frame (frame 0)");

    POLICY_DESTROY(p); free(p); free(pool);
    return 1;
}

static int test_nocache_skips_pinned(void) {
    BufferPool *pool = make_pool(4);
    EvictionPolicy *p = policy_nocache_create();

    char data[PAGE_SIZE]; memset(data, 0x11, PAGE_SIZE);
    // frame 0: PINNED (pin_count=1 after load, then pin again)
    bp_load_frame(pool, 0, "t", 1, data);
    bp_pin_frame(pool, 0);   // pin_count=2, PINNED
    // frame 1: OCCUPIED
    load_occupied(pool, 1, "t", 2, 0x22);

    int victim = POLICY_EVICT(p, pool);
    ASSERT_INT(1, victim, "NoCache skips PINNED frames, evicts first OCCUPIED");

    POLICY_DESTROY(p); free(p); free(pool);
    return 1;
}

static int test_nocache_all_pinned_returns_minus1(void) {
    BufferPool *pool = make_pool(3);
    EvictionPolicy *p = policy_nocache_create();

    char data[PAGE_SIZE]; memset(data, 0x33, PAGE_SIZE);
    // All frames PINNED
    bp_load_frame(pool, 0, "t", 1, data);
    bp_load_frame(pool, 1, "t", 2, data);
    bp_load_frame(pool, 2, "t", 3, data);
    bp_pin_frame(pool, 0);
    bp_pin_frame(pool, 1);
    bp_pin_frame(pool, 2);

    ASSERT_INT(-1, POLICY_EVICT(p, pool),
               "NoCache returns -1 when all frames PINNED");

    POLICY_DESTROY(p); free(p); free(pool);
    return 1;
}

static int test_nocache_null_pool(void) {
    EvictionPolicy *p = policy_nocache_create();
    ASSERT_INT(-1, POLICY_EVICT(p, NULL), "NoCache returns -1 for NULL pool");
    POLICY_DESTROY(p); free(p);
    return 1;
}

static int test_nocache_on_pin_unpin_no_crash(void) {
    EvictionPolicy *p = policy_nocache_create();
    POLICY_ON_PIN(p, 0);
    POLICY_ON_UNPIN(p, 0);
    POLICY_ON_PIN(p, 5);
    POLICY_ON_UNPIN(p, 5);
    POLICY_DESTROY(p); free(p);
    return 1;
}


// ============================================================================
// BLOCK 3: Clock
// ============================================================================

static int test_clock_evicts_zero_refbit(void) {
    BufferPool *pool = make_pool(4);
    EvictionPolicy *p = policy_clock_create(4);

    load_occupied(pool, 0, "t", 1, 0xAA);
    load_occupied(pool, 1, "t", 2, 0xBB);
    load_occupied(pool, 2, "t", 3, 0xCC);

    // Manually clear ref_bit on frame 1 to force it as victim
    pool->frames[1].ref_bit = 0;
    pool->frames[0].ref_bit = 1;
    pool->frames[2].ref_bit = 1;

    int victim = POLICY_EVICT(p, pool);
    ASSERT_INT(1, victim, "Clock evicts frame with ref_bit=0");

    POLICY_DESTROY(p); free(p); free(pool);
    return 1;
}

static int test_clock_gives_second_chance(void) {
    BufferPool *pool = make_pool(4);
    EvictionPolicy *p = policy_clock_create(4);

    // All frames OCCUPIED with ref_bit=1
    load_occupied(pool, 0, "t", 1, 0x11);
    load_occupied(pool, 1, "t", 2, 0x22);
    load_occupied(pool, 2, "t", 3, 0x33);

    // All have ref_bit=1 after load — clock must clear them first
    // then on second pass find the first one with ref_bit=0
    int victim = POLICY_EVICT(p, pool);
    ASSERT(victim >= 0 && victim <= 2,
           "Clock finds victim after clearing ref_bits on second pass");

    // Victim's ref_bit must have been cleared
    ASSERT_INT(0, pool->frames[victim].ref_bit,
               "Victim had ref_bit=0 when chosen");

    POLICY_DESTROY(p); free(p); free(pool);
    return 1;
}

static int test_clock_skips_pinned(void) {
    BufferPool *pool = make_pool(4);
    EvictionPolicy *p = policy_clock_create(4);

    char data[PAGE_SIZE]; memset(data, 0x55, PAGE_SIZE);
    // frames 0,1,2: PINNED
    bp_load_frame(pool, 0, "t", 1, data);
    bp_load_frame(pool, 1, "t", 2, data);
    bp_load_frame(pool, 2, "t", 3, data);
    bp_pin_frame(pool, 0);
    bp_pin_frame(pool, 1);
    bp_pin_frame(pool, 2);
    // frame 3: OCCUPIED with ref_bit=0
    load_occupied(pool, 3, "t", 4, 0x66);
    pool->frames[3].ref_bit = 0;

    int victim = POLICY_EVICT(p, pool);
    ASSERT_INT(3, victim, "Clock skips PINNED frames, evicts frame 3");

    POLICY_DESTROY(p); free(p); free(pool);
    return 1;
}

static int test_clock_hand_advances(void) {
    BufferPool *pool = make_pool(4);
    EvictionPolicy *p = policy_clock_create(4);

    // Load 4 frames, all with ref_bit=0
    for (int i = 0; i < 4; i++) {
        load_occupied(pool, i, "t", i+1, (char)(0x10 * (i+1)));
        pool->frames[i].ref_bit = 0;
    }

    // First eviction should pick frame 0 (hand starts at 0)
    int v1 = POLICY_EVICT(p, pool);
    ASSERT_INT(0, v1, "first eviction picks frame 0");

    // Reload frame 0, ref_bit=0
    load_occupied(pool, 0, "t", 1, 0x11);
    pool->frames[0].ref_bit = 0;

    // Second eviction: hand advanced past 0, should pick frame 1
    int v2 = POLICY_EVICT(p, pool);
    ASSERT_INT(1, v2, "second eviction picks frame 1 (hand advanced)");

    POLICY_DESTROY(p); free(p); free(pool);
    return 1;
}

static int test_clock_all_pinned_returns_minus1(void) {
    BufferPool *pool = make_pool(3);
    EvictionPolicy *p = policy_clock_create(3);

    char data[PAGE_SIZE]; memset(data, 0x77, PAGE_SIZE);
    bp_load_frame(pool, 0, "t", 1, data);
    bp_load_frame(pool, 1, "t", 2, data);
    bp_load_frame(pool, 2, "t", 3, data);
    bp_pin_frame(pool, 0);
    bp_pin_frame(pool, 1);
    bp_pin_frame(pool, 2);

    ASSERT_INT(-1, POLICY_EVICT(p, pool),
               "Clock returns -1 when all frames PINNED");

    POLICY_DESTROY(p); free(p); free(pool);
    return 1;
}

static int test_clock_null_pool(void) {
    EvictionPolicy *p = policy_clock_create(4);
    ASSERT_INT(-1, POLICY_EVICT(p, NULL), "Clock returns -1 for NULL pool");
    POLICY_DESTROY(p); free(p);
    return 1;
}


// ============================================================================
// BLOCK 4: LRU
// ============================================================================

static int test_lru_evicts_oldest(void) {
    BufferPool *pool = make_pool(4);
    EvictionPolicy *p = policy_lru_create();

    char data[PAGE_SIZE]; memset(data, 0x11, PAGE_SIZE);

    // Load frames in order — last_access will be 1,2,3
    load_occupied(pool, 0, "t", 1, 0x11);  // last_access=1 (oldest)
    load_occupied(pool, 1, "t", 2, 0x22);  // last_access=2
    load_occupied(pool, 2, "t", 3, 0x33);  // last_access=3 (newest)

    int victim = POLICY_EVICT(p, pool);
    ASSERT_INT(0, victim, "LRU evicts frame 0 (oldest last_access)");

    POLICY_DESTROY(p); free(p); free(pool);
    return 1;
}

static int test_lru_updates_on_pin(void) {
    BufferPool *pool = make_pool(4);
    EvictionPolicy *p = policy_lru_create();

    // Load 3 frames
    load_occupied(pool, 0, "t", 1, 0x11);  // last_access=1
    load_occupied(pool, 1, "t", 2, 0x22);  // last_access=2
    load_occupied(pool, 2, "t", 3, 0x33);  // last_access=3

    // Re-pin frame 0 to update its last_access to most recent
    bp_pin_frame(pool, 0);   // last_access now = 4 (most recent)
    bp_unpin_frame(pool, 0, 0);

    // Now frame 1 is oldest
    int victim = POLICY_EVICT(p, pool);
    ASSERT_INT(1, victim, "LRU evicts frame 1 after frame 0 was re-accessed");

    POLICY_DESTROY(p); free(p); free(pool);
    return 1;
}

static int test_lru_skips_pinned(void) {
    BufferPool *pool = make_pool(4);
    EvictionPolicy *p = policy_lru_create();

    char data[PAGE_SIZE]; memset(data, 0x44, PAGE_SIZE);

    // frame 0: oldest but PINNED
    bp_load_frame(pool, 0, "t", 1, data);
    bp_pin_frame(pool, 0);   // PINNED, pin_count=2

    // frame 1: second oldest, OCCUPIED
    load_occupied(pool, 1, "t", 2, 0x55);

    // frame 2: newest, OCCUPIED
    load_occupied(pool, 2, "t", 3, 0x66);

    int victim = POLICY_EVICT(p, pool);
    ASSERT_INT(1, victim, "LRU skips PINNED frame 0, evicts frame 1 (oldest OCCUPIED)");

    POLICY_DESTROY(p); free(p); free(pool);
    return 1;
}

static int test_lru_all_pinned_returns_minus1(void) {
    BufferPool *pool = make_pool(3);
    EvictionPolicy *p = policy_lru_create();

    char data[PAGE_SIZE]; memset(data, 0x77, PAGE_SIZE);
    bp_load_frame(pool, 0, "t", 1, data);
    bp_load_frame(pool, 1, "t", 2, data);
    bp_load_frame(pool, 2, "t", 3, data);
    bp_pin_frame(pool, 0);
    bp_pin_frame(pool, 1);
    bp_pin_frame(pool, 2);

    ASSERT_INT(-1, POLICY_EVICT(p, pool),
               "LRU returns -1 when all frames PINNED");

    POLICY_DESTROY(p); free(p); free(pool);
    return 1;
}

static int test_lru_null_pool(void) {
    EvictionPolicy *p = policy_lru_create();
    ASSERT_INT(-1, POLICY_EVICT(p, NULL), "LRU returns -1 for NULL pool");
    POLICY_DESTROY(p); free(p);
    return 1;
}

static int test_lru_tie_picks_lowest_frame(void) {
    // If two frames have the same last_access, LRU picks the first one found
    BufferPool *pool = make_pool(4);
    EvictionPolicy *p = policy_lru_create();

    load_occupied(pool, 0, "t", 1, 0x11);
    load_occupied(pool, 1, "t", 2, 0x22);

    // Force same last_access on both
    pool->frames[0].last_access = 5;
    pool->frames[1].last_access = 5;

    int victim = POLICY_EVICT(p, pool);
    ASSERT(victim == 0 || victim == 1,
           "LRU picks one of the tied frames (0 or 1)");

    POLICY_DESTROY(p); free(p); free(pool);
    return 1;
}


// ============================================================================
// BLOCK 5: OPT
// ============================================================================

static int test_opt_evicts_never_used_again(void) {
    // Trace: users/1, users/2, users/1
    // Both pages in pool. OPT should evict users/2 (never used again after pos 1)
    OPTAccess trace[] = {
        {"users", 1},
        {"users", 2},
        {"users", 1}
    };
    EvictionPolicy *p = policy_opt_create(trace, 3);

    BufferPool *pool = make_pool(4);
    load_occupied(pool, 0, "users", 1, 0xAA);
    load_occupied(pool, 1, "users", 2, 0xBB);

    // Advance to position 2 (about to access users/1 again)
    policy_opt_advance(p);
    policy_opt_advance(p);

    int victim = POLICY_EVICT(p, pool);
    ASSERT_INT(1, victim,
               "OPT evicts users/2 (not used again) over users/1 (used at pos 2)");

    POLICY_DESTROY(p); free(p); free(pool);
    return 1;
}

static int test_opt_evicts_farthest_next_use(void) {
    // Trace: t/1, t/2, t/3, t/1, t/3, t/2
    // Pool has t/1, t/2, t/3. At pos=3:
    //   t/1 next use at pos 3 (soon)
    //   t/2 next use at pos 5 (later)
    //   t/3 next use at pos 4 (middle)
    // OPT should evict t/2 (farthest next use)
    OPTAccess trace[] = {
        {"t", 1}, {"t", 2}, {"t", 3},
        {"t", 1}, {"t", 3}, {"t", 2}
    };
    EvictionPolicy *p = policy_opt_create(trace, 6);

    BufferPool *pool = make_pool(4);
    load_occupied(pool, 0, "t", 1, 0x11);
    load_occupied(pool, 1, "t", 2, 0x22);
    load_occupied(pool, 2, "t", 3, 0x33);

    // Advance to position 3
    policy_opt_advance(p);
    policy_opt_advance(p);
    policy_opt_advance(p);

    int victim = POLICY_EVICT(p, pool);
    ASSERT_INT(1, victim, "OPT evicts t/2 (farthest next use at pos 5)");

    POLICY_DESTROY(p); free(p); free(pool);
    return 1;
}

static int test_opt_advance_moves_position(void) {
    OPTAccess trace[] = {{"t", 1}, {"t", 2}, {"t", 3}};
    EvictionPolicy *p = policy_opt_create(trace, 3);

    ASSERT_INT(0, policy_opt_get_pos(p), "initial position is 0");
    policy_opt_advance(p);
    ASSERT_INT(1, policy_opt_get_pos(p), "position is 1 after one advance");
    policy_opt_advance(p);
    ASSERT_INT(2, policy_opt_get_pos(p), "position is 2 after two advances");

    POLICY_DESTROY(p); free(p);
    return 1;
}

static int test_opt_advance_does_not_exceed_trace_len(void) {
    OPTAccess trace[] = {{"t", 1}};
    EvictionPolicy *p = policy_opt_create(trace, 1);

    policy_opt_advance(p);
    ASSERT_INT(1, policy_opt_get_pos(p), "position reaches trace_len");
    policy_opt_advance(p);  // must not go beyond
    ASSERT_INT(1, policy_opt_get_pos(p), "position does not exceed trace_len");

    POLICY_DESTROY(p); free(p);
    return 1;
}

static int test_opt_null_pool(void) {
    OPTAccess trace[] = {{"t", 1}, {"t", 2}};
    EvictionPolicy *p = policy_opt_create(trace, 2);
    ASSERT_INT(-1, POLICY_EVICT(p, NULL), "OPT returns -1 for NULL pool");
    POLICY_DESTROY(p); free(p);
    return 1;
}

static int test_opt_all_pages_never_used_again(void) {
    // Trace is empty from current position — all pages never used again
    // OPT should still pick a victim (the one scanned last or first)
    OPTAccess trace[] = {{"t", 1}, {"t", 2}};
    EvictionPolicy *p = policy_opt_create(trace, 2);

    BufferPool *pool = make_pool(4);
    load_occupied(pool, 0, "t", 3, 0x33);
    load_occupied(pool, 1, "t", 4, 0x44);

    // Advance past end of trace
    policy_opt_advance(p);
    policy_opt_advance(p);

    int victim = POLICY_EVICT(p, pool);
    ASSERT(victim >= 0, "OPT picks a victim even when no future accesses exist");

    POLICY_DESTROY(p); free(p); free(pool);
    return 1;
}


// ============================================================================
// BLOCK 6: comparative behavior
// ============================================================================

static int test_nocache_vs_lru_different_victims(void) {
    // With frames 0,1,2 loaded in order, LRU and NoCache may differ
    // NoCache always picks frame 0 (first OCCUPIED)
    // LRU picks frame 0 (oldest last_access) — same here, but via different logic
    BufferPool *pool1 = make_pool(4);
    BufferPool *pool2 = make_pool(4);

    load_occupied(pool1, 0, "t", 1, 0x11);
    load_occupied(pool1, 1, "t", 2, 0x22);
    load_occupied(pool1, 2, "t", 3, 0x33);

    load_occupied(pool2, 0, "t", 1, 0x11);
    load_occupied(pool2, 1, "t", 2, 0x22);
    load_occupied(pool2, 2, "t", 3, 0x33);

    // Re-access frame 0 to make it most recent in pool2
    bp_pin_frame(pool2, 0);
    bp_unpin_frame(pool2, 0, 0);

    EvictionPolicy *nocache = policy_nocache_create();
    EvictionPolicy *lru     = policy_lru_create();

    int v_nocache = POLICY_EVICT(nocache, pool2);
    int v_lru     = POLICY_EVICT(lru,     pool2);

    // NoCache picks frame 0 (first OCCUPIED regardless of access time)
    // LRU picks frame 1 (oldest last_access after frame 0 was re-accessed)
    ASSERT_INT(0, v_nocache, "NoCache still picks frame 0 (first OCCUPIED)");
    ASSERT_INT(1, v_lru,     "LRU picks frame 1 (oldest after frame 0 re-accessed)");
    ASSERT(v_nocache != v_lru, "NoCache and LRU chose different victims");

    POLICY_DESTROY(nocache); free(nocache);
    POLICY_DESTROY(lru);     free(lru);
    free(pool1); free(pool2);
    return 1;
}

static int test_all_policies_skip_pinned(void) {
    // All 4 policies must skip PINNED frames
    BufferPool *pool = make_pool(4);

    char data[PAGE_SIZE]; memset(data, 0x55, PAGE_SIZE);
    // frames 0,1: PINNED
    bp_load_frame(pool, 0, "t", 1, data);
    bp_load_frame(pool, 1, "t", 2, data);
    bp_pin_frame(pool, 0);
    bp_pin_frame(pool, 1);
    // frame 2: OCCUPIED
    load_occupied(pool, 2, "t", 3, 0x66);
    pool->frames[2].ref_bit = 0;

    OPTAccess trace[] = {{"t", 1}, {"t", 2}, {"t", 3}, {"t", 4}};

    EvictionPolicy *nocache = policy_nocache_create();
    EvictionPolicy *clock   = policy_clock_create(4);
    EvictionPolicy *lru     = policy_lru_create();
    EvictionPolicy *opt     = policy_opt_create(trace, 4);
    policy_opt_advance(opt);
    policy_opt_advance(opt);
    policy_opt_advance(opt);

    ASSERT_INT(2, POLICY_EVICT(nocache, pool), "NoCache skips pinned, picks 2");
    ASSERT_INT(2, POLICY_EVICT(clock,   pool), "Clock  skips pinned, picks 2");
    ASSERT_INT(2, POLICY_EVICT(lru,     pool), "LRU    skips pinned, picks 2");
    ASSERT(POLICY_EVICT(opt, pool) == 2,       "OPT    skips pinned, picks 2");

    POLICY_DESTROY(nocache); free(nocache);
    POLICY_DESTROY(clock);   free(clock);
    POLICY_DESTROY(lru);     free(lru);
    POLICY_DESTROY(opt);     free(opt);
    free(pool);
    return 1;
}

static int test_all_policies_return_minus1_empty_pool(void) {
    BufferPool *pool = make_pool(4);
    // Pool is entirely FREE — no OCCUPIED frames
    OPTAccess trace[] = {{"t", 1}};

    EvictionPolicy *nocache = policy_nocache_create();
    EvictionPolicy *clock   = policy_clock_create(4);
    EvictionPolicy *lru     = policy_lru_create();
    EvictionPolicy *opt     = policy_opt_create(trace, 1);

    ASSERT_INT(-1, POLICY_EVICT(nocache, pool), "NoCache -1 on empty pool");
    ASSERT_INT(-1, POLICY_EVICT(clock,   pool), "Clock   -1 on empty pool");
    ASSERT_INT(-1, POLICY_EVICT(lru,     pool), "LRU     -1 on empty pool");
    ASSERT_INT(-1, POLICY_EVICT(opt,     pool), "OPT     -1 on empty pool");

    POLICY_DESTROY(nocache); free(nocache);
    POLICY_DESTROY(clock);   free(clock);
    POLICY_DESTROY(lru);     free(lru);
    POLICY_DESTROY(opt);     free(opt);
    free(pool);
    return 1;
}


// ============================================================================
// BLOCK 7: stress
// ============================================================================

static int test_clock_stress_many_evictions(void) {
    BufferPool *pool = make_pool(8);
    EvictionPolicy *p = policy_clock_create(8);

    // Fill pool
    for (int i = 0; i < 8; i++)
        load_occupied(pool, i, "t", i+1, (char)(i+1));

    // Evict and reload 50 times
    for (int round = 0; round < 50; round++) {
        // Clear all ref_bits to force eviction
        for (int i = 0; i < 8; i++)
            pool->frames[i].ref_bit = 0;

        int victim = POLICY_EVICT(p, pool);
        ASSERT(victim >= 0 && victim < 8,
               "Clock returns valid frame in stress loop");

        // Reload the evicted frame
        bp_evict_frame(pool, victim);
        load_occupied(pool, victim, "t", round + 100, (char)(round % 256));
    }

    POLICY_DESTROY(p); free(p); free(pool);
    return 1;
}

static int test_lru_stress_access_pattern(void) {
    BufferPool *pool = make_pool(8);
    EvictionPolicy *p = policy_lru_create();

    for (int i = 0; i < 8; i++)
        load_occupied(pool, i, "t", i+1, (char)(i+1));

    // Access frames in reverse order: 7,6,5,...,0
    // Frame 7 accessed first -> oldest last_access
    // Frame 0 accessed last  -> newest last_access
    for (int i = 7; i >= 0; i--) {
        bp_pin_frame(pool, i);
        bp_unpin_frame(pool, i, 0);
    }

    // LRU victim must be frame 7 (accessed first = oldest last_access)
    int victim = POLICY_EVICT(p, pool);
    ASSERT_INT(7, victim,
               "LRU picks frame 7 after reverse access pattern");

    POLICY_DESTROY(p); free(p); free(pool);
    return 1;
}

static int test_opt_stress_long_trace(void) {
    // Build a long trace with a repeating pattern
    int trace_len = 200;
    OPTAccess *trace = malloc(sizeof(OPTAccess) * trace_len);
    for (int i = 0; i < trace_len; i++) {
        strcpy(trace[i].table_name, "t");
        trace[i].page_id = (i % 5) + 1;  // pages 1-5 in rotation
    }

    EvictionPolicy *p = policy_opt_create(trace, trace_len);
    BufferPool *pool  = make_pool(8);

    // Load pages 1-4
    for (int i = 0; i < 4; i++)
        load_occupied(pool, i, "t", i+1, (char)(i+1));

    // Advance 50 steps and evict — must not crash
    for (int i = 0; i < 50; i++)
        policy_opt_advance(p);

    int victim = POLICY_EVICT(p, pool);
    ASSERT(victim >= 0 && victim < 4, "OPT returns valid victim in long trace");

    POLICY_DESTROY(p); free(p); free(pool); free(trace);
    return 1;
}


// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    strcpy(TEST_DIR, "/tmp/test_ep_XXXXXX");
    if (!mkdtemp(TEST_DIR)) { perror("mkdtemp"); return 1; }

    printf("=== TEST EVICTION POLICY ===\n\n");

    printf("-- Block 1: creation and destruction --\n");
    RUN_TEST(test_nocache_create_destroy);
    RUN_TEST(test_clock_create_destroy);
    RUN_TEST(test_clock_create_invalid);
    RUN_TEST(test_lru_create_destroy);
    RUN_TEST(test_opt_create_destroy);
    RUN_TEST(test_opt_create_invalid);
    RUN_TEST(test_macros_null_policy);

    printf("\n-- Block 2: NoCache --\n");
    RUN_TEST(test_nocache_evicts_first_occupied);
    RUN_TEST(test_nocache_skips_pinned);
    RUN_TEST(test_nocache_all_pinned_returns_minus1);
    RUN_TEST(test_nocache_null_pool);
    RUN_TEST(test_nocache_on_pin_unpin_no_crash);

    printf("\n-- Block 3: Clock --\n");
    RUN_TEST(test_clock_evicts_zero_refbit);
    RUN_TEST(test_clock_gives_second_chance);
    RUN_TEST(test_clock_skips_pinned);
    RUN_TEST(test_clock_hand_advances);
    RUN_TEST(test_clock_all_pinned_returns_minus1);
    RUN_TEST(test_clock_null_pool);

    printf("\n-- Block 4: LRU --\n");
    RUN_TEST(test_lru_evicts_oldest);
    RUN_TEST(test_lru_updates_on_pin);
    RUN_TEST(test_lru_skips_pinned);
    RUN_TEST(test_lru_all_pinned_returns_minus1);
    RUN_TEST(test_lru_null_pool);
    RUN_TEST(test_lru_tie_picks_lowest_frame);

    printf("\n-- Block 5: OPT --\n");
    RUN_TEST(test_opt_evicts_never_used_again);
    RUN_TEST(test_opt_evicts_farthest_next_use);
    RUN_TEST(test_opt_advance_moves_position);
    RUN_TEST(test_opt_advance_does_not_exceed_trace_len);
    RUN_TEST(test_opt_null_pool);
    RUN_TEST(test_opt_all_pages_never_used_again);

    printf("\n-- Block 6: comparative behavior --\n");
    RUN_TEST(test_nocache_vs_lru_different_victims);
    RUN_TEST(test_all_policies_skip_pinned);
    RUN_TEST(test_all_policies_return_minus1_empty_pool);

    printf("\n-- Block 7: stress --\n");
    RUN_TEST(test_clock_stress_many_evictions);
    RUN_TEST(test_lru_stress_access_pattern);
    RUN_TEST(test_opt_stress_long_trace);

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", TEST_DIR);
    system(cmd);

    printf("\n=== RESULT: %d passed, %d failed, %d total ===\n",
           tests_passed, tests_failed, tests_passed + tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
