# Synthetic yearly example

This directory contains deterministic one-year pumped-storage hydro inputs for long-horizon smoke testing. The 8760 hourly rows represent one non-leap year. The data is synthetic and is not calibrated to a real asset or market.

Files:

- `scenario.csv`: base hydro plant with pumping, turbining, and spilling.
- `scenario_no_pumping.csv`: pumping disabled while generation remains available.
- `scenario_high_operating_cost.csv`: the same physical plant with expensive hydraulic throughput.
- `prices.csv`: hourly synthetic market prices.
- `inflows.csv`: hourly synthetic natural inflows.

The upper-reservoir volume is the only storage state. Pumping consumes electricity and moves water into that reservoir. Turbining releases water and produces electricity. Spilling releases water without generation.

## Terminal inventory

The base scenario uses:

- target reservoir volume: `250` volume units;
- hard terminal band: `187.5` to `312.5` volume units;
- a soft quadratic penalty for deviation from the target.

The wider hard band avoids requiring exact grid alignment while still controlling end-of-horizon inventory.

## Run and validate

From the repository root:

```bash
./build/apps/solve_cli/optiflow_solve \
  --scenario examples/yearly/scenario.csv \
  --prices examples/yearly/prices.csv \
  --inflows examples/yearly/inflows.csv \
  --output build/yearly_dispatch.csv \
  --summary-output build/yearly_summary.json

python3 tools/validate_dispatch.py \
  --scenario examples/yearly/scenario.csv \
  --prices examples/yearly/prices.csv \
  --inflows examples/yearly/inflows.csv \
  --dispatch build/yearly_dispatch.csv
```

The dispatch should contain one header plus 8760 data rows.

## Explain the result

```bash
python3 tools/summarize_dispatch.py \
  --scenario examples/yearly/scenario.csv \
  --prices examples/yearly/prices.csv \
  --inflows examples/yearly/inflows.csv \
  --dispatch build/yearly_dispatch.csv
```

The summary reports market cashflow, operating cost, export/import energy, weighted operating prices, action counts, and terminal reservoir deviation.

## Compare sensitivities

```bash
python3 tools/compare_scenarios.py \
  --solve ./build/apps/solve_cli/optiflow_solve \
  --prices examples/yearly/prices.csv \
  --inflows examples/yearly/inflows.csv \
  --output-dir build/yearly-comparison \
  --scenario examples/yearly/scenario.csv \
  --scenario examples/yearly/scenario_no_pumping.csv \
  --scenario examples/yearly/scenario_high_operating_cost.csv \
  --summary-output build/yearly-comparison.csv
```

These variants separate a physical operating restriction from an economic sensitivity without introducing another storage technology.
