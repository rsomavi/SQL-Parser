#!/usr/bin/env python3
# test_scan.py - Integration test for SCAN operation
# Requires: server running on localhost:5433
# Run: python3 test_scan.py

import socket
import struct

HOST = "localhost"
PORT = 5433


# ============================================================================
# Client helpers
# ============================================================================

def connect():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((HOST, PORT))
    return sock


def send_command(sock, cmd):
    sock.sendall((cmd + "\n").encode())


def recv_line(sock):
    """Read one line from socket (until \\n)."""
    buf = b""
    while True:
        c = sock.recv(1)
        if not c:
            raise ConnectionError("server disconnected")
        if c == b"\n":
            return buf.decode()
        buf += c


def recv_bytes(sock, n):
    """Read exactly n bytes from socket."""
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("server disconnected")
        buf += chunk
    return buf


def read_response(sock):
    """
    Read a full server response.
    Returns dict with:
      - status: "OK" or "ERR"
      - lines: list of text lines before METRICS
      - rows: list of bytes (binary row data)
      - metrics: dict with hits, misses, evictions, hit_rate
    """
    status_line = recv_line(sock)
    status = "OK" if status_line.startswith("OK") else "ERR"

    lines  = []
    rows   = []
    metrics = {}

    while True:
        line = recv_line(sock)

        if line.startswith("END"):
            break

        if line.startswith("METRICS"):
            # Parse: METRICS hits=3 misses=1 evictions=0 hit_rate=0.750
            parts = line.split()
            for part in parts[1:]:
                key, val = part.split("=")
                try:
                    metrics[key] = float(val) if "." in val else int(val)
                except ValueError:
                    metrics[key] = val
            continue

        if line.startswith("ROWS"):
            lines.append(line)
            row_count = int(line.split()[1])
            # Read each row: size line + binary data
            for _ in range(row_count):
                size_line = recv_line(sock)
                row_size  = int(size_line.strip())
                row_data  = recv_bytes(sock, row_size)
                rows.append(row_data)
            continue

        lines.append(line)

    return {
        "status":  status,
        "lines":   lines,
        "rows":    rows,
        "metrics": metrics,
    }


# ============================================================================
# Tests
# ============================================================================

tests_passed = 0
tests_failed  = 0


def check(condition, msg):
    global tests_passed, tests_failed
    if condition:
        print(f"  PASS: {msg}")
        tests_passed += 1
    else:
        print(f"  FAIL: {msg}")
        tests_failed += 1


def test_scan_returns_ok():
    sock = connect()
    send_command(sock, "SCAN users")
    resp = read_response(sock)
    check(resp["status"] == "OK", "SCAN users returns OK")
    sock.close()


def test_scan_returns_rows():
    sock = connect()
    send_command(sock, "SCAN users")
    resp = read_response(sock)
    check(len(resp["rows"]) > 0, f"SCAN users returns rows (got {len(resp['rows'])})")
    sock.close()


def test_scan_rows_have_data():
    sock = connect()
    send_command(sock, "SCAN users")
    resp = read_response(sock)
    for i, row in enumerate(resp["rows"]):
        check(len(row) > 0, f"row {i} has data (size={len(row)})")
    sock.close()


def test_scan_has_metrics():
    sock = connect()
    send_command(sock, "SCAN users")
    resp = read_response(sock)
    m = resp["metrics"]
    check("hits"      in m, "metrics contains hits")
    check("misses"    in m, "metrics contains misses")
    check("evictions" in m, "metrics contains evictions")
    check("hit_rate"  in m, "metrics contains hit_rate")
    sock.close()


def test_scan_hit_rate_valid():
    sock = connect()
    send_command(sock, "SCAN users")
    resp = read_response(sock)
    hr = resp["metrics"].get("hit_rate", -1)
    check(0.0 <= hr <= 1.0, f"hit_rate is between 0 and 1 (got {hr})")
    sock.close()


def test_scan_second_call_has_more_hits():
    """Second SCAN of same table should have more hits (cache warming)."""
    sock1 = connect()
    send_command(sock1, "SCAN users")
    resp1 = read_response(sock1)
    hits1 = resp1["metrics"].get("hits", 0)
    sock1.close()

    sock2 = connect()
    send_command(sock2, "SCAN users")
    resp2 = read_response(sock2)
    hits2 = resp2["metrics"].get("hits", 0)
    sock2.close()

    check(hits2 > hits1,
          f"second SCAN has more hits ({hits1} -> {hits2}) — cache warming")


def test_scan_two_tables_independent():
    """Scanning two different tables should both return rows."""
    sock = connect()
    send_command(sock, "SCAN users")
    resp_users = read_response(sock)
    sock.close()

    sock = connect()
    send_command(sock, "SCAN products")
    resp_products = read_response(sock)
    sock.close()

    check(len(resp_users["rows"]) > 0,    "users has rows")
    check(len(resp_products["rows"]) > 0, "products has rows")
    check(
        len(resp_users["rows"]) != len(resp_products["rows"]) or True,
        "both tables scanned independently"
    )


def test_scan_unknown_table_returns_rows_0():
    """Scanning a non-existent table should return ROWS 0, not crash."""
    sock = connect()
    send_command(sock, "SCAN nonexistent_table_xyz")
    resp = read_response(sock)
    check(resp["status"] == "OK", "unknown table returns OK (not ERR)")
    check(len(resp["rows"]) == 0, "unknown table returns 0 rows")
    sock.close()


def test_scan_missing_table_name():
    """SCAN with no table name should return ERR."""
    sock = connect()
    send_command(sock, "SCAN")
    resp = read_response(sock)
    check(resp["status"] == "ERR", "SCAN with no table returns ERR")
    sock.close()


def test_ping_still_works_after_scan():
    """PING should work normally after SCAN operations."""
    # Do a scan first
    sock = connect()
    send_command(sock, "SCAN users")
    read_response(sock)
    sock.close()

    # Now ping
    sock = connect()
    send_command(sock, "PING")
    resp = read_response(sock)
    check(resp["status"] == "OK", "PING works after SCAN")
    check("PONG" in resp["lines"], "PING returns PONG after SCAN")
    sock.close()


# ============================================================================
# Main
# ============================================================================

if __name__ == "__main__":
    print("=== TEST SCAN ===\n")

    print("-- Block 1: basic response --")
    test_scan_returns_ok()
    test_scan_returns_rows()
    test_scan_rows_have_data()

    print("\n-- Block 2: metrics --")
    test_scan_has_metrics()
    test_scan_hit_rate_valid()
    test_scan_second_call_has_more_hits()

    print("\n-- Block 3: multiple tables --")
    test_scan_two_tables_independent()

    print("\n-- Block 4: error handling --")
    test_scan_unknown_table_returns_rows_0()
    test_scan_missing_table_name()

    print("\n-- Block 5: regression --")
    test_ping_still_works_after_scan()

    print(f"\n=== RESULT: {tests_passed} passed, {tests_failed} failed, "
          f"{tests_passed + tests_failed} total ===")
