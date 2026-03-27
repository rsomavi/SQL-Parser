#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "handlers.h"
#include "../storage-engine/page.h"
#include "../storage-engine/disk.h"

// ============================================================================
// Internal helper: append metrics footer to response
// ============================================================================

static void append_metrics(ResponseBuf *rb, Server *srv) {
    char metrics[256];
    snprintf(metrics, sizeof(metrics),
             "METRICS hits=%lld misses=%lld evictions=%lld hit_rate=%.3f\n",
             bm_get_hits(&srv->bm),
             bm_get_misses(&srv->bm),
             bm_get_evictions(&srv->bm),
             bm_get_hit_rate(&srv->bm));
    protocol_response_append(rb, metrics);
}

// ============================================================================
// handler_dispatch
// ============================================================================

void handler_dispatch(Server *srv, int client_fd, Request *req) {
    switch (req->op) {
        case OP_PING:
            handler_ping(srv, client_fd);
            break;
        case OP_SCAN:
            handler_scan(srv, client_fd, req);
            break;
        case OP_UNKNOWN:
        default: {
            ResponseBuf rb;
            protocol_response_init(&rb);
            protocol_response_append(&rb, "ERR INVALID_OP unknown operation\n");
            append_metrics(&rb, srv);
            protocol_response_append(&rb, "END\n");
            protocol_response_send(&rb, client_fd);
            protocol_response_free(&rb);
            break;
        }
    }
}

// ============================================================================
// handler_ping
// ============================================================================

void handler_ping(Server *srv, int client_fd) {
    ResponseBuf rb;
    protocol_response_init(&rb);

    protocol_response_append(&rb, "OK\n");
    protocol_response_append(&rb, "PONG\n");
    append_metrics(&rb, srv);
    protocol_response_append(&rb, "END\n");

    protocol_response_send(&rb, client_fd);
    protocol_response_free(&rb);
}

void handler_scan(Server *srv, int client_fd, Request *req) {
    ResponseBuf rb;
    protocol_response_init(&rb);

    // Validate table name
    if (req->table_name[0] == '\0') {
        protocol_response_append(&rb, "ERR INVALID_ARGS missing table name\n");
        append_metrics(&rb, srv);
        protocol_response_append(&rb, "END\n");
        protocol_response_send(&rb, client_fd);
        protocol_response_free(&rb);
        return;
    }

    // Get number of pages for this table
    int num_pages = get_num_pages(srv->data_dir, req->table_name);
    if (num_pages <= 1) {
        // No data pages (page 0 is schema only)
        protocol_response_append(&rb, "OK\n");
        protocol_response_append(&rb, "ROWS 0\n");
        append_metrics(&rb, srv);
        protocol_response_append(&rb, "END\n");
        protocol_response_send(&rb, client_fd);
        protocol_response_free(&rb);
        return;
    }

    // First pass: count rows to send ROWS <count> header
    int total_rows = 0;
    for (int page_id = 1; page_id < num_pages; page_id++) {
        char *page = bm_fetch_page(&srv->bm, req->table_name, page_id);
        if (!page) continue;

        PageHeader *header   = (PageHeader *)page;
        int        *slot_dir = (int *)(page + sizeof(PageHeader));

        for (int slot_id = 0; slot_id < header->num_slots; slot_id++) {
            if (slot_dir[slot_id] != -1)
                total_rows++;
        }
        bm_unpin_page(&srv->bm, req->table_name, page_id, 0);
    }

    // Send header
    protocol_response_append(&rb, "OK\n");

    char rows_line[64];
    snprintf(rows_line, sizeof(rows_line), "ROWS %d\n", total_rows);
    protocol_response_append(&rb, rows_line);
    protocol_response_send(&rb, client_fd);
    protocol_response_free(&rb);

    // Second pass: send rows via buffer pool
    for (int page_id = 1; page_id < num_pages; page_id++) {
        char *page = bm_fetch_page(&srv->bm, req->table_name, page_id);
        if (!page) continue;

        PageHeader *header   = (PageHeader *)page;
        int        *slot_dir = (int *)(page + sizeof(PageHeader));

        for (int slot_id = 0; slot_id < header->num_slots; slot_id++) {
            if (slot_dir[slot_id] == -1) continue;  // deleted slot

            int   row_size = get_row_size(page, slot_id);
            char *row      = read_row(page, slot_id);

            if (!row || row_size <= 0) continue;

            // Send: "<row_size>\n<binary row data>"
            ResponseBuf row_rb;
            protocol_response_init(&row_rb);

            char size_line[32];
            snprintf(size_line, sizeof(size_line), "%d\n", row_size);
            protocol_response_append(&row_rb, size_line);
            protocol_response_append_binary(&row_rb, row, row_size);

            protocol_response_send(&row_rb, client_fd);
            protocol_response_free(&row_rb);
        }

        bm_unpin_page(&srv->bm, req->table_name, page_id, 0);
    }

    // Send metrics and END
    ResponseBuf end_rb;
    protocol_response_init(&end_rb);
    append_metrics(&end_rb, srv);
    protocol_response_append(&end_rb, "END\n");
    protocol_response_send(&end_rb, client_fd);
    protocol_response_free(&end_rb);
}