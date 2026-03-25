#ifndef PAGE_TABLE_H
#define PAGE_TABLE_H

#include "buffer_frame.h"

// ============================================================================
// Constants
// ============================================================================

#define PT_NUM_BUCKETS 2048   // must be power of 2 for fast modulo

// ============================================================================
// Types
// ============================================================================

// Single entry in a bucket's linked list
typedef struct PageTableEntry {
    char                  table_name[MAX_TABLE_NAME_LEN];
    int                   page_id;
    int                   frame_id;
    struct PageTableEntry *next;
} PageTableEntry;

// One bucket = head of a linked list of entries
typedef struct {
    PageTableEntry *head;
} PageTableBucket;

// The full page table
typedef struct {
    PageTableBucket buckets[PT_NUM_BUCKETS];
    int             num_entries;   // total entries currently stored
} PageTable;

// ============================================================================
// API
// ============================================================================

// Initializes the page table. All buckets set to empty. Returns 0 or -1.
int  pt_init(PageTable *pt);

// Inserts or updates the mapping (table_name, page_id) -> frame_id.
// If the entry already exists, updates frame_id. Returns 0 or -1.
int  pt_insert(PageTable *pt, const char *table_name, int page_id, int frame_id);

// Looks up (table_name, page_id). Returns frame_id or -1 if not found.
int  pt_lookup(PageTable *pt, const char *table_name, int page_id);

// Removes the entry for (table_name, page_id). Returns 0 or -1 if not found.
int  pt_remove(PageTable *pt, const char *table_name, int page_id);

// Removes all entries and frees all allocated memory. Returns 0 or -1.
int  pt_clear(PageTable *pt);

// Returns the number of entries currently in the table.
int  pt_size(PageTable *pt);

// Debug: prints all non-empty buckets and their entries.
void pt_print(PageTable *pt);

#endif
