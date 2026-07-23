# OptiFlow frontend

The frontend is a dependency-free browser application served by NGINX. It uses plain HTML, CSS, and ES modules and sends same-origin requests through `/api/`.

The custom-scenario editor generates the hydraulic-energy `key,value` optimizer schema, validates separate timestamped price and inflow files, derives `time_step_hours` from their constant timestamp spacing, and submits managed inputs for server-side C++ validation. Storage content is presented in `MWh hydraulic`, hydraulic inflow and controls in `MW hydraulic`, and efficiencies as percentages; the generated scenario CSV converts efficiencies back to optimizer fractions. Storage resolution is entered as interval count and converted to optimizer points by adding one. The selected scenario can be opened in the editor. Existing custom scenarios retain their name and can be replaced after confirmation, which deletes their prior runs and dispatch artifacts. Bundled scenarios open under a unique copy name. Loaded price and inflow text is retained in browser state and reused unless replacement files are selected; browser file inputs remain empty.

The selected-result panel loads the newest succeeded run by default, or the historical run selected by the user. It shows the optimization summary, synchronized dispatch charts, and one row per reporting period with Baseload, Peak, and Off-peak columns for average power, energy, and net operating cashflow. Units are shown on scenario inputs, summaries, and trader outputs. Successful runs expose the original interval dispatch CSV and a client-generated product-summary CSV.

## Trader aggregation

Dispatch is classified in `Europe/Zurich`. Multi-hour optimizer intervals are split at hourly boundaries before aggregation. Baseload contains all interval segments. Peak contains Monday–Friday segments from `09:00` inclusive to `20:00` exclusive. Off-peak is the complement of Peak. Average power is signed net electrical energy divided by included hours, and energy is the signed sum of net power multiplied by segment duration.

Cashflow is the sum of interval model reward, prorated when an optimizer interval is split. Model reward is market settlement minus modeled operating cost; it is not mark-to-market or accounting P&L, and it excludes terminal target penalties. The month containing the first dispatch timestamp and the next eleven calendar months are reported individually; later dispatch is grouped into calendar quarters.

## Product-summary CSV

The browser exports the already rendered trader aggregation without another API request. The long format contains one row per reporting period and product:

```csv
run_id,scenario_name,period,product,average_net_power_mw,energy_mwh,net_operating_cashflow_eur
42,multistep_inflow_pulse,January 2027,baseload,11.25,135,202.5
42,multistep_inflow_pulse,January 2027,peak,11.25,45,67.5
42,multistep_inflow_pulse,January 2027,off_peak,11.25,90,135
```

Numbers are exported at JavaScript precision rather than with the two-decimal table formatting. An empty `average_net_power_mw` cell means that the product contains no hours in that reporting period. Product keys are `baseload`, `peak`, and `off_peak`. The generated filename is `optiflow-run-<run_id>-product-summary.csv`; changing the selected run revokes the previous in-memory download URL.

## Source documentation

Production JavaScript modules use Doxygen-compatible Javadoc blocks. Public functions and nontrivial private helpers document their contracts; inline comments are reserved for state synchronization, unit conversion, time segmentation, and numerical edge cases rather than restating obvious statements. Test names serve as the documentation for test bodies.

The root `Doxyfile` maps both `.js` and `.mjs` to Doxygen's JavaScript parser and includes the production frontend modules alongside the C++ public headers. Generate the combined documentation from the repository root:

```bash
doxygen Doxyfile
```

The generated HTML entry point is `docs/html/index.html`.

Run frontend checks from the repository root:

```bash
node --check frontend/app.js
node --check frontend/scenario.mjs
node --check frontend/number_format.mjs
node --check frontend/trader.mjs
node --check frontend/dispatch_charts.mjs
node --test frontend/scenario.test.mjs frontend/number_format.test.mjs frontend/trader.test.mjs frontend/dispatch_charts.test.mjs frontend/container_assets.test.mjs
```

Run the stack with:

```bash
docker compose up --build
```

Then open `http://127.0.0.1:8080`.

The selected-run panel also renders synchronized dependency-free SVG charts for price, natural inflow, turbine/pump/spill controls, storage content, net operating cashflow, and operating cost. Cashflow and cost use separate interval panels so zero-cost runs do not hide overlapping series; gross market settlement remains available in the tooltip. Chart timestamps are shown in Europe/Zurich, while the dispatch artifact remains UTC.
