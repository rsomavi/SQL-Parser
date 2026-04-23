# tests/test_reset_metrics.py
# Requires: server running on localhost:5433
# Run from sql-parse/: python3 server/tests/test_reset_metrics.py

import socket

HOST = "localhost"
PORT = 5433

passed = 0
failed = 0


def send_command(cmd: str) -> str:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((HOST, PORT))
    sock.sendall((cmd + "\n").encode())
    response = b""
    while True:
        chunk = sock.recv(4096)
        response += chunk
        if b"END\n" in response:
            break
    sock.close()
    return response.decode(errors="replace")


def parse_metrics(response: str) -> dict:
    for line in response.splitlines():
        if line.startswith("METRICS "):
            parts = line.split()
            metrics = {}
            for part in parts[1:]:
                key, _, val = part.partition("=")
                try:
                    metrics[key] = float(val)
                except ValueError:
                    metrics[key] = val
            return metrics
    return {}


def check(description: str, condition: bool):
    global passed, failed
    if condition:
        print(f"  PASS: {description}")
        passed += 1
    else:
        print(f"  FAIL: {description}")
        failed += 1


print("=== TEST RESET_METRICS ===\n")

print("-- Block 1: RESET_METRICS response format --")
resp = send_command("RESET_METRICS")
check("response contains OK",      "OK\n" in resp)
check("response contains METRICS", "METRICS" in resp)
check("response contains END",     "END\n" in resp)

print("\n-- Block 2: metrics are zero after reset --")
resp = send_command("RESET_METRICS")
m = parse_metrics(resp)
check("hits=0 after reset",      m.get("hits", -1) == 0.0)
check("misses=0 after reset",    m.get("misses", -1) == 0.0)
check("evictions=0 after reset", m.get("evictions", -1) == 0.0)
check("hit_rate=0 after reset",  m.get("hit_rate", -1) == 0.0)

print("\n-- Block 3: metrics accumulate after reset --")
send_command("RESET_METRICS")
send_command("SCAN users")
resp = send_command("PING")
m = parse_metrics(resp)
check("hits > 0 after SCAN",   m.get("hits", 0) > 0)

print("\n-- Block 4: reset clears accumulated metrics --")
send_command("SCAN users")
send_command("SCAN users")
resp = send_command("RESET_METRICS")
m = parse_metrics(resp)
check("hits=0 after second reset",   m.get("hits", -1) == 0.0)
check("misses=0 after second reset", m.get("misses", -1) == 0.0)

print("\n-- Block 5: metrics accumulate again after second reset --")
send_command("SCAN users")
resp = send_command("PING")
m = parse_metrics(resp)
check("hits > 0 after SCAN post-reset", m.get("hits", 0) > 0)

print(f"\n=== RESULT: {passed} passed, {failed} failed, {passed + failed} total ===")
