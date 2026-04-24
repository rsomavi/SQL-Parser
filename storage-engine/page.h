#ifndef PAGE_H
#define PAGE_H

#include <stdint.h>
#include "disk.h"

// ============================================================================
// Slot directory entry
//
// Inspired by PostgreSQL's ItemId (storage/itemid.h): rather than encoding
// row status as a sentinel offset value (-1 for deleted), each entry carries
// an explicit state field. This separates "where is the row" from "what is
// the status of this slot", and reserves room for REDIRECT (HOT update chains)
// without repurposing the offset field.
// ============================================================================

#define SLOT_NORMAL   0   // slot holds a live row
#define SLOT_DELETED  1   // row deleted; slot may be reused
#define SLOT_REDIRECT 2   // offset is the target slot (HOT chain, not yet implemented)

typedef struct {
    int     offset;  // byte offset within page where row data starts (NORMAL/REDIRECT)
    uint8_t state;   // SLOT_NORMAL, SLOT_DELETED, or SLOT_REDIRECT
} SlotEntry;

// ============================================================================
// Page header
// ============================================================================

typedef struct {
    int num_slots;        // number of slots allocated (includes deleted)
    int free_space_start; // byte offset where the slot directory ends
    int free_space_end;   // byte offset where row data starts (grows downward)
} PageHeader;

void  init_page(char *page);
int   insert_row(char *page, const void *data, int data_size);
void  delete_row(char *page, int slot_id);
char *read_row(char *page, int slot_id);
int   get_row_size(char *page, int slot_id);
void  print_page(char *page);

#endif /* PAGE_H */
