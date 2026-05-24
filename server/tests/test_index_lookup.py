#!/usr/bin/env python3

import os
import shutil
import socket
import struct
import subprocess
import tempfile
import time
from pathlib import Path

HOST = "127.0.0.1"
PORT = 5433

ROOT_DIR = Path(__file__).resolve().parents[2]
SERVER_DIR = ROOT_DIR / "server"
SERVER_BIN = SERVER_DIR / "minidbms-server"

passed = 0
failed = 0


def check(condition, msg):
    global passed, failed
    if condition:
        print(f"  PASS: {msg}")
        passed += 1
    else:
        print(f"  FAIL: {msg}")
        failed += 1


def recv_line(sock):
    buf = b""
    while True:
        c = sock.recv(1)
        if not c:
            raise ConnectionError("server disconnected")
        if c == b"\n":
            return buf.decode("utf-8", errors="replace")
        buf += c


def recv_bytes(sock, n):
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("server disconnected")
        buf += chunk
    return buf


def connect():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((HOST, PORT))
    return sock


def read_response(sock):
    status_line = recv_line(sock)
    status = "OK" if status_line.startswith("OK") else "ERR"
    lines = []

    if not status_line.startswith("OK"):
        lines.append(status_line)

    while True:
        line = recv_line(sock)
        if line == "END":
            break
        if line.startswith("METRICS "):
            continue
        lines.append(line)

    return {"status": status, "lines": lines}


def send_text_command(cmd):
    sock = connect()
    sock.sendall((cmd + "\n").encode())
    resp = read_response(sock)
    sock.close()
    return resp


def send_insert(table, row_bytes):
    sock = connect()
    header = f"INSERT {table} {len(row_bytes)}\n".encode()
    sock.sendall(header + row_bytes)
    resp = read_response(sock)
    sock.close()
    return resp


def serialize_row(row_id, name):
    name_bytes = name.encode("utf-8")
    return b"\x00" + struct.pack("<i", row_id) + struct.pack("<H", len(name_bytes)) + name_bytes


def parse_row_id(resp):
    for line in resp["lines"]:
        if line.startswith("ROW_ID "):
            return int(line.split()[1])
    return None


def build_server():
    subprocess.run(["make"], cwd=SERVER_DIR, check=True)


def start_server():
    data_dir = tempfile.mkdtemp(prefix="minidbms_index_")
    proc = subprocess.Popen(
        [str(SERVER_BIN), data_dir, "64", "lru"],
        cwd=SERVER_DIR,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    deadline = time.time() + 10
    while time.time() < deadline:
        try:
            resp = send_text_command("PING")
            if resp["status"] == "OK":
                return proc, data_dir
        except OSError:
            time.sleep(0.1)

    proc.terminate()
    proc.wait(timeout=5)
    shutil.rmtree(data_dir, ignore_errors=True)
    raise RuntimeError("server did not start in time")


def stop_server(proc, data_dir):
    if proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=5)
    shutil.rmtree(data_dir, ignore_errors=True)


def test_create_insert_lookup():
    resp = send_text_command("CREATE users id:INT:4:0:1 name:VARCHAR:32:1:0")
    check(resp["status"] == "OK", "CREATE TABLE with PK returns OK")

    resp = send_insert("users", serialize_row(1, "ana"))
    row_id = parse_row_id(resp)
    check(resp["status"] == "OK" and row_id is not None, "INSERT returns ROW_ID")

    resp = send_text_command("INDEX_LOOKUP users 1")
    check(resp["status"] == "OK" and parse_row_id(resp) == row_id, "INDEX_LOOKUP finds inserted row")


def test_lookup_non_existent_key():
    send_text_command("CREATE miss id:INT:4:0:1 name:VARCHAR:32:1:0")
    send_insert("miss", serialize_row(1, "ana"))
    resp = send_text_command("INDEX_LOOKUP miss 99")
    check(resp["status"] == "OK" and "NOT_FOUND" in resp["lines"], "INDEX_LOOKUP returns NOT_FOUND for missing key")


def test_lookup_without_index_returns_error():
    send_text_command("CREATE noidx id:INT:4:0:0 name:VARCHAR:32:1:0")
    resp = send_text_command("INDEX_LOOKUP noidx 1")
    check(resp["status"] == "ERR" and any("no index for table noidx" in line for line in resp["lines"]),
          "INDEX_LOOKUP on non-indexed table returns error")


def test_duplicate_pk_returns_error():
    send_text_command("CREATE dup id:INT:4:0:1 name:VARCHAR:32:1:0")
    send_insert("dup", serialize_row(1, "ana"))
    resp = send_insert("dup", serialize_row(1, "bea"))
    check(resp["status"] == "ERR", "duplicate PK insert returns error")


def test_lookup_multiple_rows():
    send_text_command("CREATE many id:INT:4:0:1 name:VARCHAR:32:1:0")
    expected = {}
    for i, name in [(1, "ana"), (2, "bea"), (3, "carl")]:
        resp = send_insert("many", serialize_row(i, name))
        expected[i] = parse_row_id(resp)

    for key, row_id in expected.items():
        resp = send_text_command(f"INDEX_LOOKUP many {key}")
        check(resp["status"] == "OK" and parse_row_id(resp) == row_id,
              f"INDEX_LOOKUP finds row for key {key}")


def test_delete_then_lookup_not_found():
    send_text_command("CREATE gone id:INT:4:0:1 name:VARCHAR:32:1:0")
    send_insert("gone", serialize_row(7, "ana"))
    resp = send_text_command("DELETE gone WHERE id = 7")
    check(resp["status"] == "OK", "DELETE returns OK")

    resp = send_text_command("INDEX_LOOKUP gone 7")
    check(resp["status"] == "OK" and "NOT_FOUND" in resp["lines"], "INDEX_LOOKUP returns NOT_FOUND after DELETE")


def test_insert_after_delete_same_pk_succeeds():
    send_text_command("CREATE recycle id:INT:4:0:1 name:VARCHAR:32:1:0")
    send_insert("recycle", serialize_row(7, "ana"))
    send_text_command("DELETE recycle WHERE id = 7")

    resp = send_insert("recycle", serialize_row(7, "bea"))
    check(resp["status"] == "OK", "INSERT after DELETE with same PK succeeds")

    row_id = parse_row_id(resp)
    resp = send_text_command("INDEX_LOOKUP recycle 7")
    check(resp["status"] == "OK" and parse_row_id(resp) == row_id,
          "INDEX_LOOKUP finds reinserted key after DELETE")


def main():
    print("=== TEST INDEX_LOOKUP ===")
    build_server()
    proc, data_dir = start_server()

    try:
        test_create_insert_lookup()
        test_lookup_non_existent_key()
        test_lookup_without_index_returns_error()
        test_duplicate_pk_returns_error()
        test_lookup_multiple_rows()
        test_delete_then_lookup_not_found()
        test_insert_after_delete_same_pk_succeeds()
    finally:
        stop_server(proc, data_dir)

    print(f"\n=== RESULT: {passed} passed, {failed} failed, {passed + failed} total ===")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
