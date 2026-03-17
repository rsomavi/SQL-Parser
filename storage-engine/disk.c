#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 1024
#define PAGE_SIZE 4096

// ============================================================================
// Page Structures
// ============================================================================

typedef struct {
    int num_slots;        // number of rows
    int free_space_start; // where slot directory ends
    int free_space_end;   // where row data starts
} PageHeader;

// ============================================================================
// Table Functions (existing)
// ============================================================================

void read_table(const char *table) {
    char filename[256];
    sprintf(filename, "../data/%s.tbl", table);
    
    FILE *file = fopen(filename, "r");
    
    if (!file) {
        fprintf(stderr, "ERROR: table not found\n");
        exit(1);
    }
    
    char line[MAX_LINE];
    
    while (fgets(line, MAX_LINE, file)) {
        printf("%s", line);
    }
    
    fclose(file);
}

void read_page(const char *table, int page_id) {
    char filename[256];
    sprintf(filename, "../data/%s.db", table);
    
    FILE *file = fopen(filename, "rb");
    
    if (!file) {
        fprintf(stderr, "ERROR: file not found\n");
        exit(1);
    }
    
    char buffer[PAGE_SIZE];
    
    fseek(file, page_id * PAGE_SIZE, SEEK_SET);
    
    size_t bytes_read = fread(buffer, 1, PAGE_SIZE, file);
    
    if (bytes_read == 0) {
        fprintf(stderr, "ERROR: empty page or out of bounds\n");
        fclose(file);
        exit(1);
    }
    
    fwrite(buffer, 1, bytes_read, stdout);
    
    fclose(file);
}

void write_page(const char *table, int page_id, char *page) {
    char filename[256];
    sprintf(filename, "../data/%s.db", table);
    
    // Open in read+binary mode, create if doesn't exist
    FILE *file = fopen(filename, "rb+");
    if (!file) {
        file = fopen(filename, "wb");
        if (!file) {
            fprintf(stderr, "ERROR: cannot create file\n");
            exit(1);
        }
        fclose(file);
        file = fopen(filename, "rb+");
    }
    
    fseek(file, page_id * PAGE_SIZE, SEEK_SET);
    
    fwrite(page, 1, PAGE_SIZE, file);
    
    fclose(file);
}

void load_page(const char *table, int page_id, char *page) {
    char filename[256];
    sprintf(filename, "../data/%s.db", table);
    
    FILE *file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "ERROR: file not found\n");
        exit(1);
    }
    
    fseek(file, page_id * PAGE_SIZE, SEEK_SET);
    
    size_t bytes_read = fread(page, 1, PAGE_SIZE, file);
    
    if (bytes_read != PAGE_SIZE) {
        fprintf(stderr, "ERROR: incomplete read (%zu bytes)\n", bytes_read);
        fclose(file);
        exit(1);
    }
    
    fclose(file);
}

// ============================================================================
// Slotted Page Functions
// ============================================================================

void init_page(char *page) {
    memset(page, 0, PAGE_SIZE);
    PageHeader *header = (PageHeader *)page;
    header->num_slots = 0;
    header->free_space_start = sizeof(PageHeader);
    header->free_space_end = PAGE_SIZE;
}

int insert_row(char *page, const void *data, int data_size) {
    PageHeader *header = (PageHeader *)page;
    
    int total_size = sizeof(int) + data_size;  // size header + data
    
    // Calculate available space
    int available = header->free_space_end - header->free_space_start;
    int required = total_size + sizeof(int);  // row + slot entry
    
    if (available < required) {
        return -1;  // Not enough space
    }
    
    // Check for deleted slot to reuse
    int *slot_dir = (int *)(page + sizeof(PageHeader));
    int slot_index = -1;
    
    for (int i = 0; i < header->num_slots; i++) {
        if (slot_dir[i] == -1) {
            slot_index = i;
            break;
        }
    }
    
    // If no deleted slot found, append new one
    if (slot_index == -1) {
        slot_index = header->num_slots;
    }
    
    // Insert row: decrease free_space_end by total_size
    header->free_space_end -= total_size;
    
    // Write data_size at the start of the row using memcpy
    memcpy(page + header->free_space_end, &data_size, sizeof(int));
    
    // Write row_data immediately after the size using memcpy
    memcpy(page + header->free_space_end + sizeof(int), data, data_size);
    
    // Add slot: store offset at free_space_start
    slot_dir[slot_index] = header->free_space_end;
    
    // Update header (only if appending new slot)
    if (slot_index == header->num_slots) {
        header->num_slots++;
        header->free_space_start += sizeof(int);
    }
    
    return slot_index;
}

void delete_row(char *page, int slot_id) {
    PageHeader *header = (PageHeader *)page;
    
    // Validate slot_id
    if (slot_id >= header->num_slots || slot_id < 0) {
        return;  // Invalid slot
    }
    
    // Access slot directory and set to -1
    int *slot_dir = (int *)(page + sizeof(PageHeader));
    slot_dir[slot_id] = -1;
}

char* read_row(char *page, int slot_id) {
    PageHeader *header = (PageHeader *)page;
    
    if (slot_id >= header->num_slots || slot_id < 0) {
        return NULL;  // Invalid slot
    }
    
    int *slot_dir = (int *)(page + sizeof(PageHeader));
    int offset = slot_dir[slot_id];
    
    // Check if slot is deleted
    if (offset == -1) {
        return NULL;
    }
    
    // Return pointer to row_data (skip size header)
    return page + offset + sizeof(int);
}

int get_row_size(char *page, int slot_id) {
    PageHeader *header = (PageHeader *)page;
    
    if (slot_id >= header->num_slots || slot_id < 0) {
        return -1;  // Invalid slot
    }
    
    int *slot_dir = (int *)(page + sizeof(PageHeader));
    int offset = slot_dir[slot_id];
    
    // Check if slot is deleted
    if (offset == -1) {
        return -1;
    }
    
    // Read row_size from the beginning of the row using memcpy
    int size;
    memcpy(&size, page + offset, sizeof(int));
    return size;
}

void print_page(char *page) {
    PageHeader *header = (PageHeader *)page;
    
    printf("=== Page Header ===\n");
    printf("num_slots: %d\n", header->num_slots);
    printf("free_space_start: %d\n", header->free_space_start);
    printf("free_space_end: %d\n", header->free_space_end);
    printf("free_space: %d bytes\n\n", header->free_space_end - header->free_space_start);
    
    printf("=== Rows ===\n");
    for (int i = 0; i < header->num_slots; i++) {
        int *slot_dir = (int *)(page + sizeof(PageHeader));
        int offset = slot_dir[i];
        
        if (offset == -1) {
            printf("slot %d → DELETED\n", i);
        } else {
            int row_size = get_row_size(page, i);
            char *row = read_row(page, i);
            if (row) {
                printf("slot %d → size=%d data=%s\n", i, row_size, row);
            }
        }
    }
}

void test_disk_page() {
    char page[PAGE_SIZE];
    
    // init page
    init_page(page);
    
    // insert rows
    insert_row(page, "row1", 5);
    insert_row(page, "hello world", 12);
    insert_row(page, "abc", 4);
    
    // delete slot 1
    delete_row(page, 1);
    
    // write page to disk
    write_page("users", 0, page);
    
    printf("Page written to disk\n");
    
    // clear memory
    memset(page, 0, PAGE_SIZE);
    
    // load page from disk
    load_page("users", 0, page);
    
    printf("Page loaded from disk\n\n");
    
    // print page structure
    print_page(page);
}

void test_page() {
    char page[PAGE_SIZE];
    
    // Initialize page
    init_page(page);
    printf("=== Testing Slotted Page ===\n\n");
    
    // Insert rows (binary-safe: null terminator NOT included in data_size)
    insert_row(page, "row1", 4);
    insert_row(page, "hello world", 12);
    insert_row(page, "abc", 4);
    
    printf("=== Before Delete ===\n");
    print_page(page);
    
    printf("\n=== Delete slot 1 ===\n");
    delete_row(page, 1);
    print_page(page);
    
    printf("\n=== Reuse deleted slot ===\n");
    insert_row(page, "newrow", 7);
    print_page(page);
    
    printf("\n=== Reading individual rows ===\n");
    for (int i = 0; i < 4; i++) {
        char *row = read_row(page, i);
        if (row) {
            printf("read_row(page, %d) = %s\n", i, row);
        } else {
            printf("read_row(page, %d) = NULL (deleted)\n", i);
        }
    }
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char *argv[]) {
    
    if (argc < 2) {
        fprintf(stderr, "Usage: disk read <table>\n");
        fprintf(stderr, "       disk read_page <table> <page_id>\n");
        fprintf(stderr, "       disk write_page <table> <page_id> <data>\n");
        fprintf(stderr, "       disk test_page\n");
        fprintf(stderr, "       disk test_disk_page\n");
        return 1;
    }
    
    if (strcmp(argv[1], "test_page") == 0) {
        test_page();
    }
    else if (strcmp(argv[1], "test_disk_page") == 0) {
        test_disk_page();
    }
    else if (argc < 3) {
        fprintf(stderr, "Error: missing table name\n");
        return 1;
    }
    else if (strcmp(argv[1], "read") == 0) {
        read_table(argv[2]);
    }
    else if (strcmp(argv[1], "read_page") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: disk read_page <table> <page_id>\n");
            return 1;
        }
        int page_id = atoi(argv[3]);
        read_page(argv[2], page_id);
    }
    else if (strcmp(argv[1], "write_page") == 0) {
        if (argc < 5) {
            fprintf(stderr, "Usage: disk write_page <table> <page_id> <data>\n");
            return 1;
        }
        int page_id = atoi(argv[3]);
        write_page(argv[2], page_id, argv[4]);
    }
    
    return 0;
}
