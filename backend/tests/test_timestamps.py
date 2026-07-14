from datetime import UTC, datetime, timedelta, timezone

from backend.app.timestamps import as_utc, utc_now_naive


def test_utc_now_naive_returns_current_naive_utc() -> None:
    before = datetime.now(UTC).replace(tzinfo=None)
    value = utc_now_naive()
    after = datetime.now(UTC).replace(tzinfo=None)

    assert value.tzinfo is None
    assert before <= value <= after


def test_as_utc_marks_naive_database_value_as_utc() -> None:
    value = datetime(2026, 7, 14, 12, 30, 0)

    assert as_utc(value) == datetime(2026, 7, 14, 12, 30, 0, tzinfo=UTC)


def test_as_utc_converts_aware_value_to_utc() -> None:
    source_timezone = timezone(timedelta(hours=2))
    value = datetime(2026, 7, 14, 14, 30, 0, tzinfo=source_timezone)

    assert as_utc(value) == datetime(2026, 7, 14, 12, 30, 0, tzinfo=UTC)
