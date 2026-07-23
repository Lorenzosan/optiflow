"""@file
@brief Environment-backed configuration for the OptiFlow API.
"""

import os


## @brief Environment variable overriding the SQLAlchemy database URL.
DATABASE_URL_ENV = "OPTIFLOW_DATABASE_URL"

## @brief Local-development fallback database URL.
DEFAULT_DATABASE_URL = "sqlite:///./optiflow_api.db"


def database_url() -> str:
    """Return the SQLAlchemy database URL used by the API.

    @return The value of `OPTIFLOW_DATABASE_URL`, or the local SQLite default when
    the environment variable is unset.
    """
    return os.environ.get(DATABASE_URL_ENV, DEFAULT_DATABASE_URL)
