#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buffer_frame.h"

// ============================================================================
// Initialization
// ============================================================================

int bp_init(BufferPool *pool, int num_frames, const char *data_dir) {
    if (!pool || num_frames <= 0 || num_frames > BUFFER_POOL_MAX_FRAMES)
        return -1;
    if (!data_dir)
        return -1;
        
    memset(pool, 0, sizeof(BufferPool));
    pool->num_frames = num_frames;
    strncpy(pool->data_dir, data_dir, 255);
    pool->data_dir[255] = '\0';

    for (int i = 0; i < num_frames; i++) {
        pool->frames[i].state   = FRAME_FREE;
        pool->frames[i].page_id = FRAME_INVALID_PAGE;
        pool->frames[i].table_name[0] = '\0';
    }

    return 0;
}

// ============================================================================
// Metrics
// ============================================================================

void bp_reset_metrics(BufferPool *pool) {
    if (!pool) return;
    pool->hits      = 0;
    pool->misses    = 0;
    pool->evictions = 0;
    pool->access_clock = 0;
}

// ============================================================================
// Frame lookup
// ============================================================================

int bp_find_frame(BufferPool *pool, const char *table_name, int page_id) {
    if (!pool || !table_name) return -1;

    for (int i = 0; i < pool->num_frames; i++) {
        BufferFrame *f = &pool->frames[i];
        if (f->state != FRAME_FREE &&
            f->page_id == page_id &&
            strcmp(f->table_name, table_name) == 0)
            return i;
    }
    return -1;
}

int bp_find_free_frame(BufferPool *pool) {
    if (!pool) return -1;

    for (int i = 0; i < pool->num_frames; i++) {
        if (pool->frames[i].state == FRAME_FREE)
            return i;
    }
    return -1;
}

// ============================================================================
// Core operations
// ============================================================================

int bp_load_frame(BufferPool *pool, int frame_id,
                  const char *table_name, int page_id,
                  const char *data) {
    if (!pool || !table_name || !data) return -1;
    if (frame_id < 0 || frame_id >= pool->num_frames) return -1;

    BufferFrame *f = &pool->frames[frame_id];

    memcpy(f->data, data, PAGE_SIZE);
    strncpy(f->table_name, table_name, MAX_TABLE_NAME_LEN - 1);
    f->table_name[MAX_TABLE_NAME_LEN - 1] = '\0';

    f->page_id     = page_id;
    f->state       = FRAME_OCCUPIED;
    f->pin_count   = 1;
    f->dirty       = 0;
    f->ref_bit     = 1;
    f->last_access = ++pool->access_clock;

    pool->misses++;
    return 0;
}

int bp_pin_frame(BufferPool *pool, int frame_id) {
    if (!pool) return -1;
    if (frame_id < 0 || frame_id >= pool->num_frames) return -1;

    BufferFrame *f = &pool->frames[frame_id];
    if (f->state == FRAME_FREE) return -1;

    f->pin_count++;
    f->state       = FRAME_PINNED;
    f->ref_bit     = 1;
    f->last_access = ++pool->access_clock;

    pool->hits++;
    return 0;
}

int bp_unpin_frame(BufferPool *pool, int frame_id, int dirty) {
    if (!pool) return -1;
    if (frame_id < 0 || frame_id >= pool->num_frames) return -1;

    BufferFrame *f = &pool->frames[frame_id];
    if (f->pin_count <= 0) return -1;

    if (dirty) f->dirty = 1;

    f->pin_count--;
    if (f->pin_count == 0)
        f->state = FRAME_OCCUPIED;

    return 0;
}

int bp_evict_frame(BufferPool *pool, int frame_id) {
    if (!pool) return -1;
    if (frame_id < 0 || frame_id >= pool->num_frames) return -1;

    BufferFrame *f = &pool->frames[frame_id];
    if (f->state == FRAME_PINNED) return -1;

    // write-back: if dirty, flush to disk before evicting
    if (f->dirty)
        write_page(pool->data_dir, f->table_name, f->page_id, f->data);

    memset(f, 0, sizeof(BufferFrame));
    f->state   = FRAME_FREE;
    f->page_id = FRAME_INVALID_PAGE;

    pool->evictions++;
    return 0;
}

// ============================================================================
// State counters
// ============================================================================

int bp_count_free(BufferPool *pool) {
    if (!pool) return 0;
    int count = 0;
    for (int i = 0; i < pool->num_frames; i++)
        if (pool->frames[i].state == FRAME_FREE) count++;
    return count;
}

int bp_count_occupied(BufferPool *pool) {
    if (!pool) return 0;
    int count = 0;
    for (int i = 0; i < pool->num_frames; i++)
        if (pool->frames[i].state == FRAME_OCCUPIED) count++;
    return count;
}

int bp_count_pinned(BufferPool *pool) {
    if (!pool) return 0;
    int count = 0;
    for (int i = 0; i < pool->num_frames; i++)
        if (pool->frames[i].state == FRAME_PINNED) count++;
    return count;
}

// ============================================================================
// Debug
// ============================================================================

void bp_print_state(BufferPool *pool) {
    if (!pool) return;

    printf("=== Buffer Pool State (%d frames) ===\n", pool->num_frames);
    for (int i = 0; i < pool->num_frames; i++) {
        BufferFrame *f = &pool->frames[i];
        if (f->state == FRAME_FREE) {
            printf("  frame %3d: FREE\n", i);
        } else {
            const char *state_str =
                (f->state == FRAME_PINNED) ? "PINNED  " : "OCCUPIED";
            printf("  frame %3d: %s  table=%-16s  page=%d"
                   "  pin=%d  dirty=%d  ref=%d  last=%lld\n",
                   i, state_str, f->table_name, f->page_id,
                   f->pin_count, f->dirty, f->ref_bit, f->last_access);
        }
    }
}

void bp_print_metrics(BufferPool *pool) {
    if (!pool) return;

    long long total = pool->hits + pool->misses;
    double hit_rate = (total > 0) ? (100.0 * pool->hits / total) : 0.0;

    printf("=== Buffer Pool Metrics ===\n");
    printf("  hits:      %lld\n", pool->hits);
    printf("  misses:    %lld\n", pool->misses);
    printf("  evictions: %lld\n", pool->evictions);
    printf("  hit rate:  %.1f%%\n", hit_rate);
}
