from collections.abc import Generator

from sqlalchemy import create_engine
from sqlalchemy.orm import Session, sessionmaker

from backend.app.config import database_url


def engine_kwargs(url: str) -> dict[str, object]:
    if url.startswith("sqlite:"):
        return {"connect_args": {"check_same_thread": False}}
    return {}


engine = create_engine(database_url(), **engine_kwargs(database_url()))

SessionLocal = sessionmaker(
    bind=engine,
    autoflush=False,
    autocommit=False,
    expire_on_commit=False,
)


def get_db() -> Generator[Session, None, None]:
    with SessionLocal() as db:
        yield db
