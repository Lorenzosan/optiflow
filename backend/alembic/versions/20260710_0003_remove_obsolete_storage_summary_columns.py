"""Remove obsolete electrochemical-storage summary columns.

Revision ID: 20260710_0003
Revises: 20260710_0002
"""

from collections.abc import Sequence

from alembic import op
import sqlalchemy as sa


revision: str = "20260710_0003"
down_revision: str | None = "20260710_0002"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    with op.batch_alter_table("run_summaries") as batch_op:
        batch_op.drop_column("final_battery_soc")
        batch_op.drop_column("battery_charge_steps")
        batch_op.drop_column("battery_discharge_steps")


def downgrade() -> None:
    with op.batch_alter_table("run_summaries") as batch_op:
        batch_op.add_column(
            sa.Column("final_battery_soc", sa.Float(), nullable=False, server_default=sa.text("0"))
        )
        batch_op.add_column(
            sa.Column("battery_charge_steps", sa.Integer(), nullable=False, server_default=sa.text("0"))
        )
        batch_op.add_column(
            sa.Column("battery_discharge_steps", sa.Integer(), nullable=False, server_default=sa.text("0"))
        )
