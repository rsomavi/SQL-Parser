#ifndef BUFFER_FRAME_H
#define BUFFER_FRAME_H

#include "disk.h"

// ============================================================================
// Constants
// ============================================================================

#define BUFFER_POOL_MAX_FRAMES 1024
#define MAX_TABLE_NAME_LEN     64
#define FRAME_INVALID_PAGE     -1

// ============================================================================
// Types
// ============================================================================

typedef enum {
    FRAME_FREE,       // empty, available
    FRAME_OCCUPIED,   // has a page, no active readers
    FRAME_PINNED      // has a page, active readers — cannot be evicted
} FrameState;

typedef struct {
    // identity
    char       table_name[MAX_TABLE_NAME_LEN];
    int        page_id;
    // state
    FrameState state;
    int        pin_count;
    int        dirty;
    // metadata for replacement policies
    int        ref_bit;        // Clock: was this frame accessed recently?
    long long  last_access;    // LRU: timestamp of last access
    // content
    char       data[PAGE_SIZE];
} BufferFrame;

typedef struct {
    BufferFrame frames[BUFFER_POOL_MAX_FRAMES];
    int         num_frames;
    char        data_dir[256];
    // internal clock for LRU timestamps
    long long   access_clock;
    // metrics
    long long   hits;
    long long   misses;
    long long   evictions;
} BufferPool;

// ============================================================================
// API
// ============================================================================

// Initializes the pool with num_frames frames. Returns 0 or -1 if invalid.
int bp_init(BufferPool *pool, int num_frames, const char *data_dir);

// Resets all metric counters to 0.
void bp_reset_metrics(BufferPool *pool);

// Searches for a frame containing (table_name, page_id). Returns frame_id or -1.
int  bp_find_frame(BufferPool *pool, const char *table_name, int page_id);

// Returns the first FREE frame, or -1 if none available.
int  bp_find_free_frame(BufferPool *pool);

// Loads data (PAGE_SIZE bytes) into the given frame.
// Sets state=OCCUPIED, pin_count=1, dirty=0, ref_bit=1.
// Increments misses. Returns 0 or -1.
int  bp_load_frame(BufferPool *pool, int frame_id,
                   const char *table_name, int page_id,
                   const char *data);

// Pins the frame: pin_count++, state=PINNED, ref_bit=1.
// Increments hits. Returns 0 or -1.
int  bp_pin_frame(BufferPool *pool, int frame_id);

// Unpins the frame: pin_count--. If it reaches 0, state=OCCUPIED.
// If dirty==1, sets frame->dirty=1. Returns 0 or -1.
int  bp_unpin_frame(BufferPool *pool, int frame_id, int dirty);

// Evicts the frame: clears all fields, state=FRAME_FREE.
// Fails with -1 if state==FRAME_PINNED. Increments evictions.
int  bp_evict_frame(BufferPool *pool, int frame_id);

// State counters
int  bp_count_free(BufferPool *pool);
int  bp_count_occupied(BufferPool *pool);
int  bp_count_pinned(BufferPool *pool);

// Debug
void bp_print_state(BufferPool *pool);
void bp_print_metrics(BufferPool *pool);

#endif