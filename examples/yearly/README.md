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
