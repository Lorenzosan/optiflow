from hashlib import sha256
from pathlib import Path

import pytest

from backend.app.models import Scenario
from backend.app.runner import run_solver


SUMMARY_JSON = """{
  "cumulative_profit": 123.5,
  "export_energy_mwh": 44.0,
  "import_energy_mwh": 12.0,
  "final_reservoir_volume": 51.0,
  "solve_seconds": 1.2,
  "simulation_seconds": 0.1,
  "turbine_steps": 5,
  "pump_steps": 2,
  "spill_steps": 0,
  "wait_steps": 3
}
"""

SCENARIO_CSV = """key,value
scenario_name,runner_test
time_step_hours,1
reservoir_volume_grid_points,21
turbine_flow_steps,4
spill_flow_steps,2
pump_flow_steps,3
"""

PRICES_CSV = """timestamp_utc,price
2027-01-01T00:00:00Z,10
2027-01-01T01:00:00Z,20
"""

INFLOWS_CSV = """timestamp_utc,natural_inflow
2027-01-01T00:00:00Z,1
2027-01-01T01:00:00Z,2
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
) -> tuple[Path, Path]:
    examples = root / "examples"
    examples.mkdir(parents=True)
    (examples / "scenario.csv").write_text(SCENARIO_CSV)
    (examples / "prices.csv").write_text(PRICES_CSV)
    (examples / "inflows.csv").write_text(INFLOWS_CSV)

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
    return output_dir, solver


def test_run_solver_reads_machine_summary_and_provenance(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    output_dir, solver = configure_fake_solver(tmp_path, monkeypatch, SUMMARY_JSON)
    result = run_solver(tmp_path, make_scenario(), 7)

    assert result.status == "succeeded"
    assert result.output_dispatch_path == "artifacts/run_000007_dispatch.csv"
    assert result.error_message is None
    assert result.summary is not None
    assert result.summary.cumulative_profit == 123.5
    assert result.summary.export_energy_mwh == 44.0
    assert result.summary.final_reservoir_volume == 51.0
    assert result.summary.wait_steps == 3
    assert result.provenance is not None
    assert result.provenance.result_schema_version == 1
    assert result.provenance.scenario_sha256 == sha256(SCENARIO_CSV.encode()).hexdigest()
    assert result.provenance.prices_sha256 == sha256(PRICES_CSV.encode()).hexdigest()
    assert result.provenance.inflows_sha256 == sha256(INFLOWS_CSV.encode()).hexdigest()
    assert result.provenance.solver_sha256 == sha256(solver.read_bytes()).hexdigest()
    assert result.provenance.dispatch_sha256 == sha256(b"dispatch\n").hexdigest()
    assert result.provenance.horizon_steps == 2
    assert result.provenance.reservoir_volume_grid_points == 21
    assert result.provenance.turbine_flow_steps == 4
    assert result.provenance.pump_flow_steps == 3
    assert result.provenance.spill_flow_steps == 2
    assert not (output_dir / "run_000007_summary.json").exists()


def test_run_solver_rejects_incomplete_summary(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    output_dir, _ = configure_fake_solver(
        tmp_path,
        monkeypatch,
        '{"cumulative_profit": 123.5}\n',
    )
    result = run_solver(tmp_path, make_scenario(), 8)

    assert result.status == "failed"
    assert result.output_dispatch_path is None
    assert result.summary is None
    assert result.provenance is not None
    assert result.provenance.dispatch_sha256 is None
    assert result.error_message is not None
    assert result.error_message.startswith("Solver summary JSON is invalid:")
    assert "export_energy_mwh" in result.error_message
    assert "Field required" in result.error_message
    assert not (output_dir / "run_000008_dispatch.csv").exists()
    assert not (output_dir / "run_000008_summary.json").exists()


def test_run_solver_rejects_unexpected_summary_fields(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    unexpected_summary = SUMMARY_JSON.rstrip()[:-1] + ',\n  "unexpected_metric": 1\n}\n'
    output_dir, _ = configure_fake_solver(
        tmp_path,
        monkeypatch,
        unexpected_summary,
    )
    result = run_solver(tmp_path, make_scenario(), 9)

    assert result.status == "failed"
    assert result.output_dispatch_path is None
    assert result.summary is None
    assert result.provenance is not None
    assert result.provenance.dispatch_sha256 is None
    assert result.error_message is not None
    assert result.error_message.startswith("Solver summary JSON is invalid:")
    assert "unexpected_metric" in result.error_message
    assert "Extra inputs are not permitted" in result.error_message
    assert not (output_dir / "run_000009_dispatch.csv").exists()
    assert not (output_dir / "run_000009_summary.json").exists()


def test_run_solver_fails_cleanly_when_provenance_cannot_be_collected(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    _, solver = configure_fake_solver(tmp_path, monkeypatch, SUMMARY_JSON)
    solver.unlink()

    result = run_solver(tmp_path, make_scenario(), 10)

    assert result.status == "failed"
    assert result.output_dispatch_path is None
    assert result.summary is None
    assert result.provenance is None
    assert result.error_message is not None
    assert result.error_message.startswith("Run provenance is unavailable:")


def test_run_solver_rejects_mismatched_series_before_execution(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    output_dir, _ = configure_fake_solver(tmp_path, monkeypatch, SUMMARY_JSON)
    (tmp_path / "examples" / "inflows.csv").write_text(
        "timestamp_utc,natural_inflow\n2027-01-01T00:00:00Z,1\n"
    )

    result = run_solver(tmp_path, make_scenario(), 11)

    assert result.status == "failed"
    assert result.output_dispatch_path is None
    assert result.summary is None
    assert result.provenance is None
    assert result.error_message is not None
    assert "different row counts" in result.error_message
    assert list(output_dir.iterdir()) == []
