"""Add persisted optimization run provenance.

Revision ID: 20260714_0003
Revises: 20260710_0002
Create Date: 2026-07-14

"""
from collections.abc import Sequence

from alembic import op
import sqlalchemy as sa


revision: str = "20260714_0003"
down_revision: str | None = "20260710_0002"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    op.create_table(
        "run_provenance",
        sa.Column("run_id", sa.Integer(), nullable=False),
        sa.Column("result_schema_version", sa.Integer(), nullable=False),
        sa.Column("scenario_sha256", sa.String(length=64), nullable=False),
        sa.Column("prices_sha256", sa.String(length=64), nullable=False),
        sa.Column("inflows_sha256", sa.String(length=64), nullable=False),
        sa.Column("solver_sha256", sa.String(length=64), nullable=False),
        sa.Column("dispatch_sha256", sa.String(length=64), nullable=True),
        sa.Column("horizon_steps", sa.Integer(), nullable=False),
        sa.Column("reservoir_volume_grid_points", sa.Integer(), nullable=False),
        sa.Column("turbine_flow_steps", sa.Integer(), nullable=False),
        sa.Column("pump_flow_steps", sa.Integer(), nullable=False),
        sa.Column("spill_flow_steps", sa.Integer(), nullable=False),
        sa.ForeignKeyConstraint(
            ["run_id"],
            ["optimization_runs.id"],
            ondelete="CASCADE",
        ),
        sa.PrimaryKeyConstraint("run_id"),
    )


def downgrade() -> None:
    op.drop_table("run_provenance")
