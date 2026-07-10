"""Create the initial API schema.

Revision ID: 20260710_0001
Revises:
Create Date: 2026-07-10

"""
from collections.abc import Sequence

from alembic import op
import sqlalchemy as sa


revision: str = "20260710_0001"
down_revision: str | None = None
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    op.create_table(
        "scenarios",
        sa.Column("id", sa.Integer(), nullable=False),
        sa.Column("name", sa.String(length=128), nullable=False),
        sa.Column("description", sa.Text(), nullable=False),
        sa.Column("scenario_path", sa.String(length=512), nullable=False),
        sa.Column("prices_path", sa.String(length=512), nullable=False),
        sa.Column("inflows_path", sa.String(length=512), nullable=False),
        sa.Column("created_at", sa.DateTime(), nullable=False),
        sa.PrimaryKeyConstraint("id"),
    )
    op.create_index("ix_scenarios_name", "scenarios", ["name"], unique=True)

    op.create_table(
        "optimization_runs",
        sa.Column("id", sa.Integer(), nullable=False),
        sa.Column("scenario_id", sa.Integer(), nullable=False),
        sa.Column("status", sa.String(length=32), nullable=False),
        sa.Column("started_at", sa.DateTime(), nullable=False),
        sa.Column("completed_at", sa.DateTime(), nullable=True),
        sa.Column("output_dispatch_path", sa.String(length=512), nullable=True),
        sa.Column("error_message", sa.Text(), nullable=True),
        sa.ForeignKeyConstraint(["scenario_id"], ["scenarios.id"]),
        sa.PrimaryKeyConstraint("id"),
    )
    op.create_index(
        "ix_optimization_runs_scenario_id",
        "optimization_runs",
        ["scenario_id"],
        unique=False,
    )
    op.create_index(
        "ix_optimization_runs_status",
        "optimization_runs",
        ["status"],
        unique=False,
    )


def downgrade() -> None:
    op.drop_index("ix_optimization_runs_status", table_name="optimization_runs")
    op.drop_index("ix_optimization_runs_scenario_id", table_name="optimization_runs")
    op.drop_table("optimization_runs")
    op.drop_index("ix_scenarios_name", table_name="scenarios")
    op.drop_table("scenarios")
