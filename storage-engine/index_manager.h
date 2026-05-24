#ifndef INDEX_MANAGER_H
#define INDEX_MANAGER_H

#include "btree.h"

#define MAX_INDEXED_TABLES 64

typedef struct {
    char      table_name[64];
    int       fd;
    BTreeMeta meta;
} IndexHandle;

typedef struct {
    IndexHandle handles[MAX_INDEXED_TABLES];
    int         n_handles;
    char        data_dir[256];
} IndexManager;

void index_manager_init(IndexManager *im, const char *data_dir);
int index_manager_open(IndexManager *im, const char *table_name);
IndexHandle *index_manager_get(IndexManager *im, const char *table_name);
void index_manager_close_all(IndexManager *im);

#endif /* INDEX_MANAGER_H */
