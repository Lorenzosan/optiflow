# Synthetic yearly example

This directory contains a deterministic one-year OptiFlow example for long-horizon smoke testing.

The files are:

- `scenario.csv`: model, grid, terminal, and solver parameters.
- `prices.csv`: hourly synthetic market prices.
- `inflows.csv`: hourly synthetic natural inflows.

The example contains 8760 hourly rows, corresponding to one non-leap year. The data is synthetic and intentionally simple. It is not calibrated to a real market or asset.

## Purpose

This example is intended to check that the optimizer can solve a longer horizon and that the resulting dispatch remains physically and economically consistent.

It is useful for:

- testing long-horizon CLI execution;
- validating dispatch state transitions;
- checking cumulative-profit accounting;
- inspecting yearly energy and cost decomposition;
- demonstrating deterministic optimizer diagnostics.

It should not be interpreted as a forecast or benchmark for a real pumped-storage plant.

## Terminal inventory

The yearly scenario uses a terminal inventory band with target penalties, rather than exact cyclic equality.

The target terminal state is:

- reservoir volume: `250`
- battery SOC: `50`

The hard terminal band is wider:

- reservoir volume: `187.5` to `312.5`
- battery SOC: `25` to `75`

This avoids infeasibility caused by exact terminal equality in a model where the dynamic program is solved on a grid but the forward simulation evolves continuous states.

The terminal penalties encourage the optimizer to finish near the target inventory without requiring exact equality.

## Run the yearly example

From the repository root:

    ./build/apps/solve_cli/optiflow_solve \
      --scenario examples/yearly/scenario.csv \
      --prices examples/yearly/prices.csv \
      --inflows examples/yearly/inflows.csv \
      --output build/yearly_dispatch.csv

The expected output has 8760 dispatch rows plus one CSV header row:

    wc -l build/yearly_dispatch.csv

Expected:

    8761 build/yearly_dispatch.csv

## Validate the dispatch

After running the optimizer, validate the generated dispatch:

    python3 tools/validate_dispatch.py \
      --scenario examples/yearly/scenario.csv \
      --prices examples/yearly/prices.csv \
      --inflows examples/yearly/inflows.csv \
      --dispatch build/yearly_dispatch.csv

If stdout diagnostics were captured, validate them too:

    ./build/apps/solve_cli/optiflow_solve \
      --scenario examples/yearly/scenario.csv \
      --prices examples/yearly/prices.csv \
      --inflows examples/yearly/inflows.csv \
      --output build/yearly_dispatch.csv > build/yearly_stdout.txt

    python3 tools/validate_dispatch.py \
      --scenario examples/yearly/scenario.csv \
      --prices examples/yearly/prices.csv \
      --inflows examples/yearly/inflows.csv \
      --dispatch build/yearly_dispatch.csv \
      --stdout build/yearly_stdout.txt

The validator checks row counts, time-index alignment, state continuity, physical bounds, action-grid membership, transition equations, reward accounting, cumulative profit, terminal bounds, and optional stdout diagnostic counters.

## Summarize the dispatch economics

Use the summary tool to decompose the yearly result:

    python3 tools/summarize_dispatch.py \
      --scenario examples/yearly/scenario.csv \
      --prices examples/yearly/prices.csv \
      --inflows examples/yearly/inflows.csv \
      --dispatch build/yearly_dispatch.csv

The summary reports:

- export revenue;
- import cost;
- net market cashflow;
- operating cost;
- battery degradation cost;
- recomputed reward;
- reported cumulative profit;
- export and import energy;
- weighted average operating prices;
- action counts;
- initial and final inventory;
- terminal target deviations.

This is the preferred way to explain why the yearly cumulative profit has a given magnitude.

## Compare yearly scenarios

The directory also includes `scenario_no_battery.csv`. It uses the same prices and inflows as the base yearly scenario, but disables the battery by setting the battery state range and battery power limits to zero.

Compare the base and no-battery cases with:

    python3 tools/compare_scenarios.py \
      --solve ./build/apps/solve_cli/optiflow_solve \
      --prices examples/yearly/prices.csv \
      --inflows examples/yearly/inflows.csv \
      --output-dir build/yearly-comparison \
      --scenario examples/yearly/scenario.csv \
      --scenario examples/yearly/scenario_no_battery.csv \
      --summary-output build/yearly-comparison.csv

The comparison tool runs the CLI once per scenario and writes one dispatch CSV per scenario under the selected output directory. The comparison CSV reports cumulative profit, energy import/export totals, final inventory, action counts, and solver diagnostics.

## Additional yearly variants

The yearly directory includes additional scenarios for sensitivity checks:

- `scenario_no_battery.csv`: battery physically disabled.
- `scenario_high_battery_degradation.csv`: battery available, but with high throughput cost.

Use these variants with `tools/compare_scenarios.py` to separate physical battery availability from economic battery use under the synthetic price spread.
