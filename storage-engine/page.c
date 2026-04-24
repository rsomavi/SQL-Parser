#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "page.h"

// ============================================================================
// Slotted Page Functions
// ============================================================================

void init_page(char *page) {
    memset(page, 0, PAGE_SIZE);
    PageHeader *header = (PageHeader *)page;
    header->num_slots        = 0;
    header->free_space_start = sizeof(PageHeader);
    header->free_space_end   = PAGE_SIZE;

    SlotEntry *slot_dir = (SlotEntry *)(page + sizeof(PageHeader));
    int max_slots = (PAGE_SIZE - (int)sizeof(PageHeader)) / (int)sizeof(SlotEntry);
    for (int i = 0; i < max_slots; i++) {
        slot_dir[i].state  = SLOT_DELETED;
        slot_dir[i].offset = 0;
    }
}

int insert_row(char *page, const void *data, int data_size) {
    PageHeader *header = (PageHeader *)page;

    int total_size = (int)sizeof(int) + data_size;  // size prefix + row data

    // Conservative: always reserve space for a new slot entry in case no
    // deleted slot is available for reuse.
    int available = header->free_space_end - header->free_space_start;
    int required  = total_size + (int)sizeof(SlotEntry);

    if (available < required) {
        return -1;
    }

    SlotEntry *slot_dir = (SlotEntry *)(page + sizeof(PageHeader));
    int slot_index = -1;

    for (int i = 0; i < header->num_slots; i++) {
        if (slot_dir[i].state == SLOT_DELETED) {
            slot_index = i;
            break;
        }
    }

    if (slot_index == -1) {
        slot_index = header->num_slots;
    }

    header->free_space_end -= total_size;

    memcpy(page + header->free_space_end, &data_size, sizeof(int));
    memcpy(page + header->free_space_end + sizeof(int), data, data_size);

    slot_dir[slot_index].offset = header->free_space_end;
    slot_dir[slot_index].state  = SLOT_NORMAL;

    if (slot_index == header->num_slots) {
        header->num_slots++;
        header->free_space_start += (int)sizeof(SlotEntry);
    }

    return slot_index;
}

void delete_row(char *page, int slot_id) {
    PageHeader *header = (PageHeader *)page;

    if (slot_id < 0 || slot_id >= header->num_slots) {
        return;
    }

    SlotEntry *slot_dir = (SlotEntry *)(page + sizeof(PageHeader));
    slot_dir[slot_id].state = SLOT_DELETED;
}

char *read_row(char *page, int slot_id) {
    PageHeader *header = (PageHeader *)page;

    if (slot_id < 0 || slot_id >= header->num_slots) {
        return NULL;
    }

    SlotEntry *slot_dir = (SlotEntry *)(page + sizeof(PageHeader));
    if (slot_dir[slot_id].state == SLOT_DELETED) {
        return NULL;
    }

    int offset = slot_dir[slot_id].offset;
    return page + offset + sizeof(int);
}

int get_row_size(char *page, int slot_id) {
    PageHeader *header = (PageHeader *)page;

    if (slot_id < 0 || slot_id >= header->num_slots) {
        return -1;
    }

    SlotEntry *slot_dir = (SlotEntry *)(page + sizeof(PageHeader));
    if (slot_dir[slot_id].state == SLOT_DELETED) {
        return -1;
    }

    int offset = slot_dir[slot_id].offset;
    int size;
    memcpy(&size, page + offset, sizeof(int));
    return size;
}

void print_page(char *page) {
    PageHeader *header   = (PageHeader *)page;
    SlotEntry  *slot_dir = (SlotEntry *)(page + sizeof(PageHeader));

    printf("=== Page Header ===\n");
    printf("num_slots: %d\n", header->num_slots);
    printf("free_space_start: %d\n", header->free_space_start);
    printf("free_space_end: %d\n", header->free_space_end);
    printf("free_space: %d bytes\n\n", header->free_space_end - header->free_space_start);

    printf("=== Rows ===\n");
    for (int i = 0; i < header->num_slots; i++) {
        if (slot_dir[i].state == SLOT_DELETED) {
            printf("slot %d → DELETED\n", i);
        } else {
            int   row_size = get_row_size(page, i);
            char *row      = read_row(page, i);
            if (row) {
                printf("slot %d → size=%d data=", i, row_size);
                for (int j = 0; j < row_size; j++)
                    printf("%c", row[j]);
                printf("\n");
            }
        }
    }
}
