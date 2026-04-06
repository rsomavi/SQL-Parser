#ifndef DISK_H
#define DISK_H

#define PAGE_SIZE 4096

int read_page(const char *data_dir, const char *table, int page_id);
int write_page(const char *data_dir, const char *table, int page_id, char *page);
int load_page(const char *data_dir, const char *table, int page_id, char *page);
int get_num_pages(const char *data_dir, const char *table);

#endif /* DISK_H */
