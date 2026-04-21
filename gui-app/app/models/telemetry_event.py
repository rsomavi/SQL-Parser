from dataclasses import dataclass

from app.models.query_models import QueryMetrics


@dataclass(slots=True)
class TelemetryEvent:
    timestamp: str
    kind: str
    query: str
    delta_metrics: QueryMetrics
    total_metrics: QueryMetrics
    note: str = ""

