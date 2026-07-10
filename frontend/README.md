# OptiFlow frontend

The frontend is a dependency-free browser application served by NGINX. It uses plain HTML, CSS, and ES modules and sends same-origin requests through `/api/`.

The custom-scenario editor generates the reservoir-only `key,value` optimizer schema, validates separate timestamped price and inflow files, and submits immutable inputs for server-side C++ validation.

The selected-result panel loads the newest succeeded run by default, or the historical run selected by the user. It shows one row per reporting period, with Baseload, Peak, and Off-peak columns for average power, energy, and P&L. Peak is fixed to Monday–Friday 09:00–20:00 in Europe/Zurich. The first twelve calendar months are monthly and later periods are quarterly. Units are shown on scenario inputs, summaries, and trader outputs.

Run frontend checks from the repository root:

```bash
node --check frontend/app.js
node --check frontend/scenario.mjs
node --check frontend/trader.mjs
node --test frontend/scenario.test.mjs frontend/trader.test.mjs
```

Run the stack with:

```bash
docker compose up --build
```

Then open `http://127.0.0.1:8080`.
