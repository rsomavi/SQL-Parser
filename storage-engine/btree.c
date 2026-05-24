#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "btree.h"

#define BTREE_KEY_TYPE_INT 0

static void leaf_insert_sorted(BTreeNode *leaf, int key, int row_id) {
    int index;

    index = leaf->n_keys;
    while (index > 0 && leaf->keys[index - 1] > key) {
        leaf->keys[index] = leaf->keys[index - 1];
        leaf->row_ids[index] = leaf->row_ids[index - 1];
        index--;
    }

    leaf->keys[index] = key;
    leaf->row_ids[index] = row_id;
    leaf->n_keys++;
}

static void internal_insert_sorted(BTreeNode *node, int key, int right_child) {
    int index;

    index = 0;
    while (index < node->n_keys && node->keys[index] < key) {
        index++;
    }

    for (int i = node->n_keys; i > index; i--) {
        node->keys[i] = node->keys[i - 1];
    }

    for (int i = node->n_keys + 1; i > index + 1; i--) {
        node->children[i] = node->children[i - 1];
    }

    node->keys[index] = key;
    node->children[index + 1] = right_child;
    node->n_keys++;
}

static int btree_page_offset(int page_id, off_t *offset) {
    if (page_id < 0 || offset == NULL) {
        return -1;
    }

    *offset = (off_t)page_id * PAGE_SIZE;
    return 0;
}

static int btree_write_page(int fd, int page_id, const char *buffer) {
    off_t offset;
    ssize_t bytes_written;

    if (fd < 0 || buffer == NULL || btree_page_offset(page_id, &offset) != 0) {
        return -1;
    }

    bytes_written = pwrite(fd, buffer, PAGE_SIZE, offset);
    if (bytes_written != PAGE_SIZE) {
        return -1;
    }

    return 0;
}

static int btree_read_page(int fd, int page_id, char *buffer) {
    off_t offset;
    ssize_t bytes_read;

    if (fd < 0 || buffer == NULL || btree_page_offset(page_id, &offset) != 0) {
        return -1;
    }

    bytes_read = pread(fd, buffer, PAGE_SIZE, offset);
    if (bytes_read != PAGE_SIZE) {
        return -1;
    }

    return 0;
}

static int btree_build_path(char *path, size_t path_size, const char *data_dir, const char *table_name) {
    int written;

    if (path == NULL || data_dir == NULL || table_name == NULL) {
        return -1;
    }

    written = snprintf(path, path_size, "%s/%s.idx", data_dir, table_name);
    if (written < 0 || (size_t)written >= path_size) {
        return -1;
    }

    return 0;
}

int btree_open(const char *data_dir, const char *table_name) {
    char path[PATH_MAX];

    if (btree_build_path(path, sizeof(path), data_dir, table_name) != 0) {
        return -1;
    }

    return open(path, O_RDWR | O_CREAT, 0644);
}

int btree_read_meta(int fd, BTreeMeta *meta) {
    char buffer[PAGE_SIZE];
    size_t offset = 0;

    if (meta == NULL || btree_read_page(fd, 0, buffer) != 0) {
        return -1;
    }

    memcpy(&meta->root_page_id, buffer + offset, sizeof(int));
    offset += sizeof(int);
    memcpy(&meta->n_entries, buffer + offset, sizeof(int));
    offset += sizeof(int);
    memcpy(&meta->height, buffer + offset, sizeof(int));
    offset += sizeof(int);
    memcpy(&meta->key_type, buffer + offset, sizeof(int));
    offset += sizeof(int);
    memcpy(&meta->next_free_page, buffer + offset, sizeof(int));

    return 0;
}

int btree_write_meta(int fd, const BTreeMeta *meta) {
    char buffer[PAGE_SIZE];
    size_t offset = 0;

    if (fd < 0 || meta == NULL) {
        return -1;
    }

    memset(buffer, 0, sizeof(buffer));

    memcpy(buffer + offset, &meta->root_page_id, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, &meta->n_entries, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, &meta->height, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, &meta->key_type, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, &meta->next_free_page, sizeof(int));

    return btree_write_page(fd, 0, buffer);
}

int btree_read_node(int fd, int page_id, BTreeNode *node) {
    char buffer[PAGE_SIZE];
    size_t offset = 0;

    if (node == NULL || btree_read_page(fd, page_id, buffer) != 0) {
        return -1;
    }

    memcpy(&node->type, buffer + offset, sizeof(int));
    offset += sizeof(int);
    memcpy(&node->n_keys, buffer + offset, sizeof(int));
    offset += sizeof(int);
    memcpy(node->keys, buffer + offset, sizeof(node->keys));
    offset += sizeof(node->keys);
    memcpy(node->children, buffer + offset, sizeof(node->children));
    offset += sizeof(node->children);
    memcpy(node->row_ids, buffer + offset, sizeof(node->row_ids));
    offset += sizeof(node->row_ids);
    memcpy(&node->next_leaf, buffer + offset, sizeof(int));

    return 0;
}

int btree_write_node(int fd, int page_id, const BTreeNode *node) {
    char buffer[PAGE_SIZE];
    size_t offset = 0;

    if (fd < 0 || page_id <= 0 || node == NULL) {
        return -1;
    }

    memset(buffer, 0, sizeof(buffer));

    memcpy(buffer + offset, &node->type, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, &node->n_keys, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, node->keys, sizeof(node->keys));
    offset += sizeof(node->keys);
    memcpy(buffer + offset, node->children, sizeof(node->children));
    offset += sizeof(node->children);
    memcpy(buffer + offset, node->row_ids, sizeof(node->row_ids));
    offset += sizeof(node->row_ids);
    memcpy(buffer + offset, &node->next_leaf, sizeof(int));

    return btree_write_page(fd, page_id, buffer);
}

int btree_alloc_page(int fd, BTreeMeta *meta) {
    char buffer[PAGE_SIZE];
    int page_id;

    if (fd < 0 || meta == NULL || meta->next_free_page <= 0) {
        return -1;
    }

    page_id = meta->next_free_page;
    meta->next_free_page++;

    memset(buffer, 0, sizeof(buffer));
    if (btree_write_page(fd, page_id, buffer) != 0) {
        meta->next_free_page--;
        return -1;
    }

    if (btree_write_meta(fd, meta) != 0) {
        meta->next_free_page--;
        return -1;
    }

    return page_id;
}

int btree_init(const char *data_dir, const char *table_name) {
    char path[PATH_MAX];
    BTreeMeta meta;
    BTreeNode root;
    int fd;

    if (btree_build_path(path, sizeof(path), data_dir, table_name) != 0) {
        return -1;
    }

    fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return -1;
    }

    meta.root_page_id = 1;
    meta.n_entries = 0;
    meta.height = 1;
    meta.key_type = BTREE_KEY_TYPE_INT;
    meta.next_free_page = 2;

    memset(&root, 0, sizeof(root));
    root.type = BTREE_LEAF;
    root.n_keys = 0;
    root.next_leaf = -1;

    if (btree_write_meta(fd, &meta) != 0 || btree_write_node(fd, meta.root_page_id, &root) != 0) {
        close(fd);
        return -1;
    }

    if (close(fd) != 0) {
        return -1;
    }

    return 0;
}

int btree_search(int fd, const BTreeMeta *meta, int key) {
    BTreeNode node;
    int current_page_id;

    if (fd < 0 || meta == NULL || meta->root_page_id <= 0 || meta->key_type != BTREE_KEY_TYPE_INT) {
        return -1;
    }

    current_page_id = meta->root_page_id;

    while (1) {
        int index;

        if (btree_read_node(fd, current_page_id, &node) != 0) {
            return -1;
        }

        if (node.type == BTREE_LEAF) {
            for (index = 0; index < node.n_keys; index++) {
                if (node.keys[index] == key) {
                    return node.row_ids[index];
                }
            }
            return -1;
        }

        if (node.type != BTREE_INTERNAL) {
            return -1;
        }

        index = 0;
        while (index < node.n_keys && key >= node.keys[index]) {
            index++;
        }

        current_page_id = node.children[index];
        if (current_page_id <= 0) {
            return -1;
        }
    }
}

int btree_insert(int fd, BTreeMeta *meta, int key, int row_id) {
    BTreeNode node;
    int ancestor_stack[BTREE_MAX_HEIGHT];
    int ancestor_size = 0;
    int current_page_id;
    int left_page_id;
    int right_page_id;
    int separator_key;

    if (fd < 0 || meta == NULL || meta->root_page_id <= 0 || meta->key_type != BTREE_KEY_TYPE_INT) {
        return -1;
    }

    current_page_id = meta->root_page_id;

    while (1) {
        int index;

        if (btree_read_node(fd, current_page_id, &node) != 0) {
            return -1;
        }

        if (node.type == BTREE_LEAF) {
            break;
        }

        if (node.type != BTREE_INTERNAL || ancestor_size >= BTREE_MAX_HEIGHT) {
            return -1;
        }

        ancestor_stack[ancestor_size++] = current_page_id;

        index = 0;
        while (index < node.n_keys && key >= node.keys[index]) {
            index++;
        }

        current_page_id = node.children[index];
        if (current_page_id <= 0) {
            return -1;
        }
    }

    for (int i = 0; i < node.n_keys; i++) {
        if (node.keys[i] == key) {
            return 1;
        }
    }

    if (node.n_keys < BTREE_ORDER) {
        leaf_insert_sorted(&node, key, row_id);
        meta->n_entries++;
        if (btree_write_node(fd, current_page_id, &node) != 0 || btree_write_meta(fd, meta) != 0) {
            return -1;
        }
        return 0;
    }

    {
        BTreeNode right;
        int temp_keys[BTREE_ORDER + 1];
        int temp_row_ids[BTREE_ORDER + 1];
        int insert_index;
        int right_leaf_page_id;
        int mid = BTREE_ORDER / 2;

        memset(&right, 0, sizeof(right));
        right.type = BTREE_LEAF;
        right.next_leaf = -1;

        insert_index = 0;
        while (insert_index < node.n_keys && node.keys[insert_index] < key) {
            insert_index++;
        }

        for (int i = 0, j = 0; i < BTREE_ORDER + 1; i++) {
            if (i == insert_index) {
                temp_keys[i] = key;
                temp_row_ids[i] = row_id;
            } else {
                temp_keys[i] = node.keys[j];
                temp_row_ids[i] = node.row_ids[j];
                j++;
            }
        }

        memset(node.keys, 0, sizeof(node.keys));
        memset(node.row_ids, 0, sizeof(node.row_ids));
        node.n_keys = mid;
        for (int i = 0; i < mid; i++) {
            node.keys[i] = temp_keys[i];
            node.row_ids[i] = temp_row_ids[i];
        }

        right.n_keys = BTREE_ORDER + 1 - mid;
        for (int i = 0; i < right.n_keys; i++) {
            right.keys[i] = temp_keys[mid + i];
            right.row_ids[i] = temp_row_ids[mid + i];
        }

        right_leaf_page_id = btree_alloc_page(fd, meta);
        if (right_leaf_page_id < 0) {
            return -1;
        }

        right.next_leaf = node.next_leaf;
        node.next_leaf = right_leaf_page_id;

        if (btree_write_node(fd, current_page_id, &node) != 0 ||
            btree_write_node(fd, right_leaf_page_id, &right) != 0) {
            return -1;
        }

        left_page_id = current_page_id;
        right_page_id = right_leaf_page_id;
        separator_key = right.keys[0];
    }

    while (1) {
        BTreeNode parent;

        if (ancestor_size == 0) {
            BTreeNode root;
            int new_root_page_id;

            memset(&root, 0, sizeof(root));
            root.type = BTREE_INTERNAL;
            root.n_keys = 1;
            root.keys[0] = separator_key;
            root.children[0] = left_page_id;
            root.children[1] = right_page_id;
            root.next_leaf = -1;

            new_root_page_id = btree_alloc_page(fd, meta);
            if (new_root_page_id < 0) {
                return -1;
            }

            if (btree_write_node(fd, new_root_page_id, &root) != 0) {
                return -1;
            }

            meta->root_page_id = new_root_page_id;
            meta->height++;
            break;
        }

        current_page_id = ancestor_stack[--ancestor_size];
        if (btree_read_node(fd, current_page_id, &parent) != 0 || parent.type != BTREE_INTERNAL) {
            return -1;
        }

        if (parent.n_keys < BTREE_ORDER) {
            internal_insert_sorted(&parent, separator_key, right_page_id);
            if (btree_write_node(fd, current_page_id, &parent) != 0) {
                return -1;
            }
            break;
        }

        {
            BTreeNode right;
            int temp_keys[BTREE_ORDER + 1];
            int temp_children[BTREE_ORDER + 2];
            int insert_index;
            int right_internal_page_id;
            int mid = BTREE_ORDER / 2;

            memset(&right, 0, sizeof(right));
            right.type = BTREE_INTERNAL;
            right.next_leaf = -1;

            insert_index = 0;
            while (insert_index < parent.n_keys && parent.keys[insert_index] < separator_key) {
                insert_index++;
            }

            for (int i = 0, j = 0; i < BTREE_ORDER + 1; i++) {
                if (i == insert_index) {
                    temp_keys[i] = separator_key;
                } else {
                    temp_keys[i] = parent.keys[j];
                    j++;
                }
            }

            for (int i = 0; i <= BTREE_ORDER + 1; i++) {
                if (i <= insert_index) {
                    temp_children[i] = parent.children[i];
                } else if (i == insert_index + 1) {
                    temp_children[i] = right_page_id;
                } else {
                    temp_children[i] = parent.children[i - 1];
                }
            }

            memset(parent.keys, 0, sizeof(parent.keys));
            memset(parent.children, 0, sizeof(parent.children));
            parent.n_keys = mid;
            for (int i = 0; i < mid; i++) {
                parent.keys[i] = temp_keys[i];
            }
            for (int i = 0; i <= mid; i++) {
                parent.children[i] = temp_children[i];
            }

            separator_key = temp_keys[mid];

            right.n_keys = BTREE_ORDER - mid;
            for (int i = 0; i < right.n_keys; i++) {
                right.keys[i] = temp_keys[mid + 1 + i];
            }
            for (int i = 0; i <= right.n_keys; i++) {
                right.children[i] = temp_children[mid + 1 + i];
            }

            right_internal_page_id = btree_alloc_page(fd, meta);
            if (right_internal_page_id < 0) {
                return -1;
            }

            if (btree_write_node(fd, current_page_id, &parent) != 0 ||
                btree_write_node(fd, right_internal_page_id, &right) != 0) {
                return -1;
            }

            left_page_id = current_page_id;
            right_page_id = right_internal_page_id;
        }
    }

    meta->n_entries++;
    if (btree_write_meta(fd, meta) != 0) {
        return -1;
    }

    return 0;
}

int btree_delete(int fd, BTreeMeta *meta, int key) {
    BTreeNode node;
    int current_page_id;

    if (fd < 0 || meta == NULL || meta->root_page_id <= 0 || meta->key_type != BTREE_KEY_TYPE_INT) {
        return -1;
    }

    current_page_id = meta->root_page_id;

    while (1) {
        int index;

        if (btree_read_node(fd, current_page_id, &node) != 0) {
            return -1;
        }

        if (node.type == BTREE_LEAF) {
            break;
        }

        if (node.type != BTREE_INTERNAL) {
            return -1;
        }

        index = 0;
        while (index < node.n_keys && key >= node.keys[index]) {
            index++;
        }

        current_page_id = node.children[index];
        if (current_page_id <= 0) {
            return -1;
        }
    }

    for (int i = 0; i < node.n_keys; i++) {
        if (node.keys[i] == key) {
            for (int j = i; j < node.n_keys - 1; j++) {
                node.keys[j] = node.keys[j + 1];
                node.row_ids[j] = node.row_ids[j + 1];
            }

            node.keys[node.n_keys - 1] = 0;
            node.row_ids[node.n_keys - 1] = 0;
            node.n_keys--;

            if (meta->n_entries > 0) {
                meta->n_entries--;
            }

            if (btree_write_node(fd, current_page_id, &node) != 0 || btree_write_meta(fd, meta) != 0) {
                return -1;
            }

            return 0;
        }
    }

    return -1;
}
