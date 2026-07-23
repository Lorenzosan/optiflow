"""@file
@brief Convert between naive UTC database timestamps and aware API timestamps.
"""

from datetime import UTC, datetime


def utc_now_naive() -> datetime:
    """Return the current time as naive UTC for database columns.

    @return Current UTC datetime with `tzinfo` removed.
    """
    return datetime.now(UTC).replace(tzinfo=None)


def as_utc(value: datetime) -> datetime:
    """Normalize a database datetime for API serialization.

    Naive values are interpreted as UTC by database convention; aware values are
    converted to UTC.

    @param value Naive or timezone-aware datetime.
    @return Timezone-aware UTC datetime.
    """
    if value.tzinfo is None:
        return value.replace(tzinfo=UTC)
    return value.astimezone(UTC)
