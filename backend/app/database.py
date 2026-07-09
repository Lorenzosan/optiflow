import os
from collections.abc import Generator

from sqlalchemy import create_engine
from sqlalchemy.orm import Session, sessionmaker

from backend.app.models import Base


DATABASE_URL_ENV = "OPTIFLOW_DATABASE_URL"
DEFAULT_DATABASE_URL = "sqlite:///./optiflow_api.db"


def database_url() -> str:
    return os.environ.get(DATABASE_URL_ENV, DEFAULT_DATABASE_URL)


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


def create_schema() -> None:
    Base.metadata.create_all(bind=engine)


def get_db() -> Generator[Session, None, None]:
    with SessionLocal() as db:
        yield db
