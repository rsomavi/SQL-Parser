from dataclasses import dataclass

from app.config.constants import (
    DEFAULT_FRAMES,
    DEFAULT_POLICY,
    DEFAULT_SERVER_HOST,
    DEFAULT_SERVER_PORT,
)


@dataclass(slots=True)
class ServerConfig:
    policy: str = DEFAULT_POLICY
    frames: int = DEFAULT_FRAMES
    host: str = DEFAULT_SERVER_HOST
    port: int = DEFAULT_SERVER_PORT

