# OptiFlow frontend

The frontend is a dependency-free browser application served by NGINX. It uses plain HTML, CSS, and ES modules and sends same-origin requests through `/api/`.

The custom scenario editor:

* starts from the bundled synthetic-year scalar parameter template;
* generates the `key,value` scenario CSV in the browser;
* validates price and inflow CSV headers, indices, row counts, numeric values, and nonnegative inflows;
* submits the three files to `POST /scenarios` as multipart form data;
* refreshes and selects the saved immutable scenario after server-side C++ validation.

The browser checks are for fast feedback only. `CsvScenarioReader` and the C++ parameter constructors remain authoritative for input and cross-field validation.

Run the dependency-free JavaScript checks from the repository root:

```bash
node --check frontend/app.js
node --check frontend/scenario.mjs
node --test frontend/scenario.test.mjs
```

Run the full stack with:

```bash
docker compose up --build
```

Then open `http://127.0.0.1:8080`.
