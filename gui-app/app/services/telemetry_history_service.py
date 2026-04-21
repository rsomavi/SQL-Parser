from __future__ import annotations

from copy import deepcopy
from datetime import datetime

from app.models.query_models import QueryExecutionResult, QueryMetrics
from app.models.telemetry_event import TelemetryEvent


class TelemetryHistoryService:
    """Owns telemetry history and app-level reset semantics outside the UI."""

    def __init__(self):
        self._events: list[TelemetryEvent] = []
        self._current_total = QueryMetrics()

    def record_query(self, result: QueryExecutionResult) -> QueryMetrics:
        self._current_total.hits += result.metrics.hits
        self._current_total.misses += result.metrics.misses
        self._current_total.evictions += result.metrics.evictions
        self._current_total.elapsed_time += result.metrics.elapsed_time

        total_accesses = self._current_total.hits + self._current_total.misses
        self._current_total.hit_rate = (
            self._current_total.hits / total_accesses if total_accesses else 0.0
        )

        event = TelemetryEvent(
            timestamp=self._timestamp(),
            kind="query",
            query=result.query.strip(),
            delta_metrics=deepcopy(result.metrics),
            total_metrics=deepcopy(self._current_total),
            note="ok" if not result.error else f"error: {result.error}",
        )
        self._events.append(event)
        return deepcopy(self._current_total)

    def reset(self) -> QueryMetrics:
        event = TelemetryEvent(
            timestamp=self._timestamp(),
            kind="reset",
            query="",
            delta_metrics=QueryMetrics(),
            total_metrics=QueryMetrics(),
            note="telemetry reset",
        )
        self._events.append(event)
        self._current_total = QueryMetrics()
        return deepcopy(self._current_total)

    def clear_for_new_server(self) -> QueryMetrics:
        event = TelemetryEvent(
            timestamp=self._timestamp(),
            kind="server_start",
            query="",
            delta_metrics=QueryMetrics(),
            total_metrics=QueryMetrics(),
            note="new server session",
        )
        self._events.append(event)
        self._current_total = QueryMetrics()
        return deepcopy(self._current_total)

    def current_total(self) -> QueryMetrics:
        return deepcopy(self._current_total)

    def history_rows(self) -> list[dict[str, str]]:
        rows = []
        for event in reversed(self._events):
            rows.append(
                {
                    "time": event.timestamp,
                    "kind": event.kind,
                    "query": event.query or "-",
                    "d_hits": str(event.delta_metrics.hits),
                    "d_miss": str(event.delta_metrics.misses),
                    "d_evict": str(event.delta_metrics.evictions),
                    "total_hits": str(event.total_metrics.hits),
                    "total_miss": str(event.total_metrics.misses),
                    "total_evict": str(event.total_metrics.evictions),
                    "elapsed": f"{event.delta_metrics.elapsed_time:.4f}s",
                    "note": event.note,
                }
            )
        return rows

    def clear_history(self) -> list[dict[str, str]]:
        self._events = []
        return self.history_rows()

    @staticmethod
    def _timestamp() -> str:
        return datetime.now().strftime("%H:%M:%S")
