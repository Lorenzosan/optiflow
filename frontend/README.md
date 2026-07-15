# OptiFlow frontend

The frontend is a dependency-free browser application served by NGINX. It uses plain HTML, CSS, and ES modules and sends same-origin requests through `/api/`.

The custom-scenario editor generates the hydraulic-energy `key,value` optimizer schema, validates separate timestamped price and inflow files, derives `time_step_hours` from their constant timestamp spacing, and submits managed inputs for server-side C++ validation. Storage content is presented in `MWh hydraulic`, hydraulic inflow and controls in `MW hydraulic`, and efficiencies as percentages; the generated scenario CSV converts efficiencies back to optimizer fractions. Existing custom scenarios can be replaced by name, which deletes their prior runs and dispatch artifacts. Bundled scenarios remain read-only.

The selected-result panel loads the newest succeeded run by default, or the historical run selected by the user. It shows the optimization summary, synchronized dispatch charts, and one row per reporting period with Baseload, Peak, and Off-peak columns for average power, energy, and P&L. Peak is fixed to Monday–Friday 09:00–20:00 in Europe/Zurich. The first twelve calendar months are monthly and later periods are quarterly. Units are shown on scenario inputs, summaries, and trader outputs.

Run frontend checks from the repository root:

```bash
node --check frontend/app.js
node --check frontend/scenario.mjs
node --check frontend/trader.mjs
node --check frontend/dispatch_charts.mjs
node --test frontend/scenario.test.mjs frontend/trader.test.mjs frontend/dispatch_charts.test.mjs frontend/container_assets.test.mjs
```

Run the stack with:

```bash
docker compose up --build
```

Then open `http://127.0.0.1:8080`.

The selected-run panel also renders synchronized dependency-free SVG charts for price, natural inflow, turbine/pump/spill controls, storage content, and cumulative profit. Chart timestamps are shown in Europe/Zurich, while the dispatch artifact remains UTC.
