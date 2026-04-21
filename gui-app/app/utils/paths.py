from pathlib import Path

from app.config.constants import DATA_DIR, RESULTS_DIR, SERVER_BIN, SERVER_DIR


def ensure_runtime_paths() -> None:
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)


def server_binary_path() -> Path:
    return SERVER_BIN


def server_workdir() -> Path:
    return SERVER_DIR


def data_dir_path() -> Path:
    return DATA_DIR


def results_dir_path() -> Path:
    return RESULTS_DIR
