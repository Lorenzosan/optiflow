"""@file
@brief Relational persistence models for scenarios, optimization runs, and summaries.

Database timestamps are stored as naive UTC values and converted to aware UTC
values only when API responses are serialized.
"""

from __future__ import annotations

from datetime import datetime

from sqlalchemy import DateTime, Float, ForeignKey, Integer, String, Text
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column, relationship

from backend.app.timestamps import utc_now_naive


class Base(DeclarativeBase):
    """Declarative base for all OptiFlow SQLAlchemy models."""
    pass


class Scenario(Base):
    """Persist one bundled or uploaded optimizer scenario.

    The three stored paths identify the scalar scenario CSV, price series, and
    natural-inflow series. Deleting a scenario cascades to its optimization runs.
    """
    __tablename__ = "scenarios"

    id: Mapped[int] = mapped_column(primary_key=True)
    name: Mapped[str] = mapped_column(String(128), unique=True, index=True, nullable=False)
    description: Mapped[str] = mapped_column(Text, nullable=False)
    scenario_path: Mapped[str] = mapped_column(String(512), nullable=False)
    prices_path: Mapped[str] = mapped_column(String(512), nullable=False)
    inflows_path: Mapped[str] = mapped_column(String(512), nullable=False)
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=False),
        default=utc_now_naive,
        nullable=False,
    )

    runs: Mapped[list[OptimizationRun]] = relationship(
        back_populates="scenario",
        cascade="all, delete-orphan",
    )


class OptimizationRun(Base):
    """Persist one execution of the C++ optimizer.

    A run records lifecycle state, timestamps, an optional dispatch artifact path,
    and an optional one-to-one summary. Dispatch trajectories remain CSV artifacts
    rather than relational rows.
    """
    __tablename__ = "optimization_runs"

    id: Mapped[int] = mapped_column(primary_key=True)
    scenario_id: Mapped[int] = mapped_column(
        ForeignKey("scenarios.id"),
        index=True,
        nullable=False,
    )
    status: Mapped[str] = mapped_column(
        String(32),
        default="pending",
        index=True,
        nullable=False,
    )
    started_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=False),
        default=utc_now_naive,
        nullable=False,
    )
    completed_at: Mapped[datetime | None] = mapped_column(
        DateTime(timezone=False),
        nullable=True,
    )
    output_dispatch_path: Mapped[str | None] = mapped_column(
        String(512),
        nullable=True,
    )
    error_message: Mapped[str | None] = mapped_column(Text, nullable=True)

    scenario: Mapped[Scenario] = relationship(back_populates="runs")
    summary: Mapped[RunSummary | None] = relationship(
        back_populates="run",
        cascade="all, delete-orphan",
        uselist=False,
    )


class RunSummary(Base):
    """Persist scalar output metrics for one successful optimization run.

    The primary key is also a foreign key to `OptimizationRun`, enforcing at most
    one summary per run.
    """
    __tablename__ = "run_summaries"

    run_id: Mapped[int] = mapped_column(
        ForeignKey("optimization_runs.id", ondelete="CASCADE"),
        primary_key=True,
    )
    net_operating_cashflow: Mapped[float] = mapped_column(Float, nullable=False)
    export_energy_mwh: Mapped[float] = mapped_column(Float, nullable=False)
    import_energy_mwh: Mapped[float] = mapped_column(Float, nullable=False)
    final_reservoir_volume: Mapped[float] = mapped_column(Float, nullable=False)
    solve_seconds: Mapped[float] = mapped_column(Float, nullable=False)
    simulation_seconds: Mapped[float] = mapped_column(Float, nullable=False)
    turbine_steps: Mapped[int] = mapped_column(Integer, nullable=False)
    pump_steps: Mapped[int] = mapped_column(Integer, nullable=False)
    spill_steps: Mapped[int] = mapped_column(Integer, nullable=False)
    wait_steps: Mapped[int] = mapped_column(Integer, nullable=False)

    run: Mapped[OptimizationRun] = relationship(back_populates="summary")
