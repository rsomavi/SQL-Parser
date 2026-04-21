import subprocess
import time
import socket

from app.models.server_config import ServerConfig
from app.utils.paths import data_dir_path, server_binary_path, server_workdir


class ServerService:
    """Controls the lifecycle of the C server process."""

    def __init__(self):
        self._process: subprocess.Popen | None = None

    def start(self, config: ServerConfig) -> None:
        if self.is_running():
            raise RuntimeError("Server is already running.")

        binary = server_binary_path()
        if not binary.exists():
            raise RuntimeError(
                f"Server binary not found at {binary}. Build it first with `cd server && make`."
            )

        command = [
            str(binary),
            str(data_dir_path()),
            str(config.frames),
            config.policy,
        ]

        self._process = subprocess.Popen(
            command,
            cwd=str(server_workdir()),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        self._wait_until_ready(config.host, config.port)
        time.sleep(0.1)
        if self._process and self._process.poll() is not None:
            stdout, stderr = self.read_recent_logs()
            self._process = None
            raise RuntimeError(
                "Server exited just after startup.\n"
                f"stdout:\n{stdout}\n"
                f"stderr:\n{stderr}"
            )

    def stop(self) -> None:
        if not self._process:
            return

        process = self._process
        self._process = None

        if process.poll() is not None:
            return

        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=5)

    def is_running(self) -> bool:
        return self._process is not None and self._process.poll() is None

    def is_available(self, host: str, port: int) -> bool:
        if self.is_running():
            return True
        try:
            with socket.create_connection((host, port), timeout=0.25):
                return True
        except OSError:
            return False

    def read_recent_logs(self) -> tuple[str, str]:
        if not self._process:
            return "", ""
        stdout = ""
        stderr = ""
        if self._process.stdout:
            stdout = self._process.stdout.read() or ""
        if self._process.stderr:
            stderr = self._process.stderr.read() or ""
        return stdout, stderr

    def _wait_until_ready(self, host: str, port: int) -> None:
        deadline = time.time() + 5
        while time.time() < deadline:
            if self._process and self._process.poll() is not None:
                stdout, stderr = self.read_recent_logs()
                raise RuntimeError(
                    "Server exited during startup.\n"
                    f"stdout:\n{stdout}\n"
                    f"stderr:\n{stderr}"
                )
            try:
                with socket.create_connection((host, port), timeout=0.25):
                    return
            except OSError:
                time.sleep(0.15)
        raise RuntimeError("Server did not become reachable on port 5433.")
