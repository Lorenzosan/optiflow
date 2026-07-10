import os


DATABASE_URL_ENV = "OPTIFLOW_DATABASE_URL"
DEFAULT_DATABASE_URL = "sqlite:///./optiflow_api.db"


def database_url() -> str:
    return os.environ.get(DATABASE_URL_ENV, DEFAULT_DATABASE_URL)
