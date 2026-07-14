from datetime import UTC, datetime


def utc_now_naive() -> datetime:
    """Return the current UTC time for naive UTC database columns."""
    return datetime.now(UTC).replace(tzinfo=None)


def as_utc(value: datetime) -> datetime:
    """Return an aware UTC datetime for API serialization."""
    if value.tzinfo is None:
        return value.replace(tzinfo=UTC)
    return value.astimezone(UTC)
