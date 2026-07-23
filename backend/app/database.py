"""@file
@brief SQLAlchemy engine, session factory, and FastAPI database dependency.
"""

from collections.abc import Generator

from sqlalchemy import create_engine
from sqlalchemy.orm import Session, sessionmaker

from backend.app.config import database_url


def engine_kwargs(url: str) -> dict[str, object]:
    """Build dialect-specific SQLAlchemy engine arguments.

    @param url SQLAlchemy database URL.
    @return SQLite thread-safety arguments for SQLite URLs, otherwise an empty
    mapping.
    """
    if url.startswith("sqlite:"):
        return {"connect_args": {"check_same_thread": False}}
    return {}


## @brief Shared SQLAlchemy engine configured at module import time.
engine = create_engine(database_url(), **engine_kwargs(database_url()))

## @brief Factory for explicit request and startup database sessions.
SessionLocal = sessionmaker(
    bind=engine,
    autoflush=False,
    autocommit=False,
    expire_on_commit=False,
)


def get_db() -> Generator[Session, None, None]:
    """Yield one request-scoped SQLAlchemy session.

    The context manager closes the session after the FastAPI dependency finishes.
    Transaction boundaries remain the responsibility of the route or service.

    @return A generator yielding one active SQLAlchemy session.
    """
    with SessionLocal() as db:
        yield db
