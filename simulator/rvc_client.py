"""TCP client for the C++ RVC controller bridge."""

from __future__ import annotations

import json
import socket
from typing import Any


class RvcClient:
    def __init__(self, host: str = "127.0.0.1", port: int = 5050) -> None:
        self.host = host
        self.port = port
        self._socket: socket.socket | None = None
        self._file = None

    def connect(self) -> None:
        if self._socket is not None:
            return
        self._socket = socket.create_connection((self.host, self.port), timeout=3)
        self._file = self._socket.makefile("r", encoding="utf-8")

    def close(self) -> None:
        if self._file is not None:
            self._file.close()
            self._file = None
        if self._socket is not None:
            self._socket.close()
            self._socket = None

    def send(self, op: str, **payload: Any) -> dict[str, Any]:
        self.connect()
        assert self._socket is not None
        assert self._file is not None

        message = {"op": op, **payload}
        self._socket.sendall((json.dumps(message) + "\n").encode("utf-8"))
        line = self._file.readline()
        if not line:
            raise ConnectionError("RVC controller bridge closed the connection")
        return json.loads(line)

    def __enter__(self) -> "RvcClient":
        self.connect()
        return self

    def __exit__(self, *_: object) -> None:
        self.close()
