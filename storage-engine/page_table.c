#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "page_table.h"

// ============================================================================
// Internal: hash function
// ============================================================================

// Combines table_name and page_id into a bucket index.
// Uses DJB2 for the string part, then mixes in page_id.
static unsigned int pt_hash(const char *table_name, int page_id) {
    unsigned int hash = 5381;
    const unsigned char *s = (const unsigned char *)table_name;
    while (*s)
        hash = ((hash << 5) + hash) ^ *s++;   // hash * 33 ^ c
    hash ^= (unsigned int)page_id * 2654435761u;  // Knuth multiplicative hash
    return hash & (PT_NUM_BUCKETS - 1);            // fast modulo (power of 2)
}

// ============================================================================
// pt_init
// ============================================================================

int pt_init(PageTable *pt) {
    if (!pt) return -1;
    memset(pt, 0, sizeof(PageTable));
    return 0;
}

// ============================================================================
// pt_insert
// ============================================================================

int pt_insert(PageTable *pt, const char *table_name, int page_id, int frame_id) {
    if (!pt || !table_name) return -1;

    unsigned int idx = pt_hash(table_name, page_id);
    PageTableEntry *entry = pt->buckets[idx].head;

    // Check if entry already exists — update frame_id if so
    while (entry) {
        if (entry->page_id == page_id &&
            strcmp(entry->table_name, table_name) == 0) {
            entry->frame_id = frame_id;
            return 0;
        }
        entry = entry->next;
    }

    // New entry — allocate and prepend to bucket list
    PageTableEntry *new_entry = malloc(sizeof(PageTableEntry));
    if (!new_entry) return -1;

    strncpy(new_entry->table_name, table_name, MAX_TABLE_NAME_LEN - 1);
    new_entry->table_name[MAX_TABLE_NAME_LEN - 1] = '\0';
    new_entry->page_id  = page_id;
    new_entry->frame_id = frame_id;
    new_entry->next     = pt->buckets[idx].head;

    pt->buckets[idx].head = new_entry;
    pt->num_entries++;
    return 0;
}

// ============================================================================
// pt_lookup
// ============================================================================

int pt_lookup(PageTable *pt, const char *table_name, int page_id) {
    if (!pt || !table_name) return -1;

    unsigned int idx = pt_hash(table_name, page_id);
    PageTableEntry *entry = pt->buckets[idx].head;

    while (entry) {
        if (entry->page_id == page_id &&
            strcmp(entry->table_name, table_name) == 0)
            return entry->frame_id;
        entry = entry->next;
    }
    return -1;
}

// ============================================================================
// pt_remove
// ============================================================================

int pt_remove(PageTable *pt, const char *table_name, int page_id) {
    if (!pt || !table_name) return -1;

    unsigned int idx = pt_hash(table_name, page_id);
    PageTableEntry *entry = pt->buckets[idx].head;
    PageTableEntry *prev  = NULL;

    while (entry) {
        if (entry->page_id == page_id &&
            strcmp(entry->table_name, table_name) == 0) {
            if (prev)
                prev->next = entry->next;
            else
                pt->buckets[idx].head = entry->next;
            free(entry);
            pt->num_entries--;
            return 0;
        }
        prev  = entry;
        entry = entry->next;
    }
    return -1;  // not found
}

// ============================================================================
// pt_clear
// ============================================================================

int pt_clear(PageTable *pt) {
    if (!pt) return -1;

    for (int i = 0; i < PT_NUM_BUCKETS; i++) {
        PageTableEntry *entry = pt->buckets[i].head;
        while (entry) {
            PageTableEntry *next = entry->next;
            free(entry);
            entry = next;
        }
        pt->buckets[i].head = NULL;
    }
    pt->num_entries = 0;
    return 0;
}

// ============================================================================
// pt_size
// ============================================================================

int pt_size(PageTable *pt) {
    if (!pt) return 0;
    return pt->num_entries;
}

// ============================================================================
// pt_print
// ============================================================================

void pt_print(PageTable *pt) {
    if (!pt) return;

    printf("=== Page Table (%d entries) ===\n", pt->num_entries);
    for (int i = 0; i < PT_NUM_BUCKETS; i++) {
        if (!pt->buckets[i].head) continue;
        printf("  bucket %4d: ", i);
        PageTableEntry *e = pt->buckets[i].head;
        while (e) {
            printf("(%s, page=%d) -> frame=%d",
                   e->table_name, e->page_id, e->frame_id);
            if (e->next) printf(" | ");
            e = e->next;
        }
        printf("\n");
    }
}
