"""Add persisted optimization run summaries.

Revision ID: 20260710_0002
Revises: 20260710_0001
Create Date: 2026-07-10

"""
from collections.abc import Sequence

from alembic import op
import sqlalchemy as sa


revision: str = "20260710_0002"
down_revision: str | None = "20260710_0001"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    op.create_table(
        "run_summaries",
        sa.Column("run_id", sa.Integer(), nullable=False),
        sa.Column("cumulative_profit", sa.Float(), nullable=False),
        sa.Column("export_energy_mwh", sa.Float(), nullable=False),
        sa.Column("import_energy_mwh", sa.Float(), nullable=False),
        sa.Column("final_reservoir_volume", sa.Float(), nullable=False),
        sa.Column("solve_seconds", sa.Float(), nullable=False),
        sa.Column("simulation_seconds", sa.Float(), nullable=False),
        sa.Column("turbine_steps", sa.Integer(), nullable=False),
        sa.Column("pump_steps", sa.Integer(), nullable=False),
        sa.Column("spill_steps", sa.Integer(), nullable=False),
        sa.Column("wait_steps", sa.Integer(), nullable=False),
        sa.ForeignKeyConstraint(
            ["run_id"],
            ["optimization_runs.id"],
            ondelete="CASCADE",
        ),
        sa.PrimaryKeyConstraint("run_id"),
    )


def downgrade() -> None:
    op.drop_table("run_summaries")
