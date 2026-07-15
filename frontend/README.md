# OptiFlow frontend

The frontend is a dependency-free browser application served by NGINX. It uses plain HTML, CSS, and ES modules and sends same-origin requests through `/api/`.

The custom-scenario editor generates the hydraulic-energy `key,value` optimizer schema, validates separate timestamped price and inflow files, derives `time_step_hours` from their constant timestamp spacing, and submits managed inputs for server-side C++ validation. Storage content is presented in `MWh hydraulic`, hydraulic inflow and controls in `MW hydraulic`, and efficiencies as percentages; the generated scenario CSV converts efficiencies back to optimizer fractions. Storage resolution is entered as interval count and converted to optimizer points by adding one. The editor reports grid spacing, action-grid alignment, terminal-penalty interpolation scale, and the possibility of costless zero-price cycling. The selected scenario can be opened in the editor. Existing custom scenarios retain their name and can be replaced after confirmation, which deletes their prior runs and dispatch artifacts. Bundled scenarios open under a unique copy name. Loaded price and inflow text is retained in browser state and reused unless replacement files are selected; browser file inputs remain empty.

The selected-result panel loads the newest succeeded run by default, or the historical run selected by the user. It shows the optimization summary, synchronized dispatch charts, and one row per reporting period with Baseload, Peak, and Off-peak columns for average power, energy, and net operating cashflow. Cashflow is market settlement minus modeled operating cost; it is not mark-to-market or accounting P&L, and it excludes terminal target penalties. Peak is fixed to Monday–Friday 09:00–20:00 in Europe/Zurich. The first twelve calendar months are monthly and later periods are quarterly. Units are shown on scenario inputs, summaries, and trader outputs.

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
