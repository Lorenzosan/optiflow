# Synthetic yearly example

This directory contains a deterministic 8760-hour CSV scenario for manual long-horizon runs.

The files are synthetic and are not calibrated to a real market or hydrology data set. They are intended to exercise CSV loading, dynamic-programming solve size diagnostics, and long dispatch CSV generation with a still-small discretization.

Files:

```text
scenario.csv   Scalar model, terminal, and solver parameters.
prices.csv     Hourly synthetic electricity prices for one non-leap year.
inflows.csv    Hourly synthetic natural inflows for one non-leap year.
```

The scenario uses 9 reservoir grid points, 5 battery grid points, and 72 generated actions. This keeps the example suitable for local smoke testing while still covering a one-year horizon.


Run and validate the example from the repository root:

```bash
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
```

The validation helper checks that the generated trajectory is internally consistent with the configured model, action grid, and exogenous inputs. It is a consistency check, not a calibration claim.

The yearly scenario uses a terminal inventory band with target penalties, rather than exact cyclic equality, because the forward simulation evolves continuous states while the dynamic program is solved on a grid.
