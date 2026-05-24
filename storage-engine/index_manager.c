#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "index_manager.h"

static int index_build_path(char *path, size_t path_size, const char *data_dir, const char *table_name) {
    int written;

    if (!path || !data_dir || !table_name) {
        return -1;
    }

    written = snprintf(path, path_size, "%s/%s.idx", data_dir, table_name);
    if (written < 0 || (size_t)written >= path_size) {
        return -1;
    }

    return 0;
}

void index_manager_init(IndexManager *im, const char *data_dir) {
    if (!im) {
        return;
    }

    memset(im, 0, sizeof(*im));
    if (data_dir) {
        strncpy(im->data_dir, data_dir, sizeof(im->data_dir) - 1);
        im->data_dir[sizeof(im->data_dir) - 1] = '\0';
    }
}

IndexHandle *index_manager_get(IndexManager *im, const char *table_name) {
    if (!im || !table_name) {
        return NULL;
    }

    for (int i = 0; i < im->n_handles; i++) {
        if (strcmp(im->handles[i].table_name, table_name) == 0) {
            return &im->handles[i];
        }
    }

    return NULL;
}

int index_manager_open(IndexManager *im, const char *table_name) {
    IndexHandle *handle;
    char path[512];
    int fd;

    if (!im || !table_name || im->data_dir[0] == '\0') {
        return -1;
    }

    handle = index_manager_get(im, table_name);
    if (handle) {
        return 0;
    }

    if (im->n_handles >= MAX_INDEXED_TABLES) {
        return -1;
    }

    if (index_build_path(path, sizeof(path), im->data_dir, table_name) != 0) {
        return -1;
    }

    if (access(path, F_OK) != 0 && btree_init(im->data_dir, table_name) != 0) {
        return -1;
    }

    fd = btree_open(im->data_dir, table_name);
    if (fd < 0) {
        return -1;
    }

    handle = &im->handles[im->n_handles];
    memset(handle, 0, sizeof(*handle));

    if (btree_read_meta(fd, &handle->meta) != 0) {
        close(fd);
        return -1;
    }

    strncpy(handle->table_name, table_name, sizeof(handle->table_name) - 1);
    handle->table_name[sizeof(handle->table_name) - 1] = '\0';
    handle->fd = fd;
    im->n_handles++;

    return 0;
}

void index_manager_close_all(IndexManager *im) {
    if (!im) {
        return;
    }

    for (int i = 0; i < im->n_handles; i++) {
        if (im->handles[i].fd >= 0) {
            close(im->handles[i].fd);
            im->handles[i].fd = -1;
        }
    }

    im->n_handles = 0;
}
