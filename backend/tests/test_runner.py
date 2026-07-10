from pathlib import Path

import pytest

from backend.app.models import Scenario
from backend.app.runner import run_solver


SUMMARY_JSON = """{
  "cumulative_profit": 123.5,
  "export_energy_mwh": 44.0,
  "import_energy_mwh": 12.0,
  "final_reservoir_volume": 51.0,
  "final_battery_soc": 3.0,
  "solve_seconds": 1.2,
  "simulation_seconds": 0.1,
  "turbine_steps": 5,
  "pump_steps": 2,
  "spill_steps": 0,
  "battery_charge_steps": 1,
  "battery_discharge_steps": 1,
  "wait_steps": 3
}
"""


def make_scenario() -> Scenario:
    return Scenario(
        name="runner_test",
        description="Runner test scenario",
        scenario_path="examples/scenario.csv",
        prices_path="examples/prices.csv",
        inflows_path="examples/inflows.csv",
    )


def configure_fake_solver(
    root: Path,
    monkeypatch: pytest.MonkeyPatch,
    summary_contents: str,
) -> Path:
    examples = root / "examples"
    examples.mkdir(parents=True)
    for filename in ("scenario.csv", "prices.csv", "inflows.csv"):
        (examples / filename).write_text("test\n")

    solver = root / "fake_solver.py"
    solver.write_text(
        "#!/usr/bin/env python3\n"
        "from pathlib import Path\n"
        "import sys\n"
        "arguments = dict(zip(sys.argv[1::2], sys.argv[2::2]))\n"
        "Path(arguments['--output']).write_text('dispatch\\n')\n"
        f"Path(arguments['--summary-output']).write_text({summary_contents!r})\n"
    )
    solver.chmod(0o755)

    output_dir = root / "artifacts"
    monkeypatch.setenv("OPTIFLOW_SOLVE_BIN", str(solver))
    monkeypatch.setenv("OPTIFLOW_RUN_OUTPUT_DIR", str(output_dir))
    return output_dir


def test_run_solver_reads_machine_summary(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    output_dir = configure_fake_solver(tmp_path, monkeypatch, SUMMARY_JSON)

    result = run_solver(tmp_path, make_scenario(), 7)

    assert result.status == "succeeded"
    assert result.output_dispatch_path == "artifacts/run_000007_dispatch.csv"
    assert result.error_message is None
    assert result.summary is not None
    assert result.summary.cumulative_profit == 123.5
    assert result.summary.export_energy_mwh == 44.0
    assert result.summary.final_reservoir_volume == 51.0
    assert result.summary.wait_steps == 3
    assert not (output_dir / "run_000007_summary.json").exists()


def test_run_solver_rejects_incomplete_summary(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    output_dir = configure_fake_solver(
        tmp_path,
        monkeypatch,
        '{"cumulative_profit": 123.5}\n',
    )

    result = run_solver(tmp_path, make_scenario(), 8)

    assert result.status == "failed"
    assert result.output_dispatch_path is None
    assert result.summary is None
    assert result.error_message is not None
    assert result.error_message.startswith("Solver summary JSON is invalid:")
    assert "export_energy_mwh: Field required" in result.error_message
    assert not (output_dir / "run_000008_dispatch.csv").exists()
    assert not (output_dir / "run_000008_summary.json").exists()
