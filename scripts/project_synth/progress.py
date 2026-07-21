from __future__ import annotations

import sys
import time


class ProgressReporter:
    def __init__(self, total_steps: int, *, phase: str, update_interval_seconds: float = 1.0):
        self.total_steps = max(0, total_steps)
        self.phase = phase
        self.completed_steps = 0
        self.last_reported_units = -1
        self.last_detail = ""
        self.last_reported_at = 0.0
        self.bar_width = 28
        self.update_interval_seconds = update_interval_seconds

    def _render(self, *, detail: str = "", force: bool = False) -> None:
        if self.total_steps <= 0:
            return

        progress_units = int((self.completed_steps * 1000) / self.total_steps)
        now = time.monotonic()
        should_report = (
            force
            or progress_units != self.last_reported_units
            or (now - self.last_reported_at) >= self.update_interval_seconds
        )
        if not should_report:
            return

        self.last_reported_units = progress_units
        self.last_reported_at = now
        self.last_detail = detail
        percent = progress_units / 10.0
        filled = int((self.completed_steps * self.bar_width) / self.total_steps)
        bar = "#" * filled + "-" * (self.bar_width - filled)
        icon = "✅" if progress_units == 1000 else "🧪"
        line = (
            f"\r{icon} [{self.phase}] [{bar}] {percent:>5.1f}% "
            f"{self.completed_steps}/{self.total_steps}"
        )
        if detail:
            line += f" {detail}"

        print(
            f"\r\x1b[2K{line}",
            file=sys.stderr,
            end="" if progress_units < 1000 else "\n",
            flush=True,
        )

    def advance(self, count: int = 1, *, detail: str | None = None, force: bool = False) -> None:
        if self.total_steps <= 0:
            return
        self.completed_steps = min(self.total_steps, self.completed_steps + max(0, count))
        self._render(detail=detail or self.last_detail, force=force)

    def status(self, detail: str, *, force: bool = False) -> None:
        self._render(detail=detail, force=force)
