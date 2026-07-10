# OptiFlow frontend

The frontend is deliberately build-free: one HTML file, one stylesheet, and one ES module served by NGINX. It uses only browser APIs and the existing FastAPI contract.

NGINX serves the static files and forwards `/api/` requests to the `api` Compose service. The proxy timeout is longer than the backend's default solver timeout because `POST /runs` is synchronous.

Run the full stack from the repository root:

```bash
docker compose up --build
```

Open `http://localhost:8080`.

The UI supports scenario discovery, synchronous run launch, filtered and paginated run history, persisted summary inspection, failure display, and dispatch CSV download. Charts and client-side routing are intentionally omitted from this first slice.
