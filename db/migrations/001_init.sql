CREATE TABLE IF NOT EXISTS scenarios (
  id BIGSERIAL PRIMARY KEY,
  name TEXT NOT NULL UNIQUE,
  description TEXT NOT NULL DEFAULT '',
  timestep_hours DOUBLE PRECISION NOT NULL,
  initial_reservoir_volume_m3 DOUBLE PRECISION NOT NULL,
  initial_battery_soc_mwh DOUBLE PRECISION NOT NULL,
  created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS scenario_time_series (
  id BIGSERIAL PRIMARY KEY,
  scenario_id BIGINT NOT NULL REFERENCES scenarios(id) ON DELETE CASCADE,
  time_index INTEGER NOT NULL,
  price_eur_per_mwh DOUBLE PRECISION NOT NULL,
  natural_inflow_m3_s DOUBLE PRECISION NOT NULL,
  UNIQUE (scenario_id, time_index)
);

CREATE TABLE IF NOT EXISTS optimization_runs (
  id BIGSERIAL PRIMARY KEY,
  scenario_id BIGINT REFERENCES scenarios(id) ON DELETE SET NULL,
  status TEXT NOT NULL,
  total_profit_eur DOUBLE PRECISION,
  final_reservoir_volume_m3 DOUBLE PRECISION,
  final_battery_soc_mwh DOUBLE PRECISION,
  started_at TIMESTAMPTZ NOT NULL DEFAULT now(),
  completed_at TIMESTAMPTZ
);

CREATE TABLE IF NOT EXISTS dispatch_steps (
  id BIGSERIAL PRIMARY KEY,
  run_id BIGINT NOT NULL REFERENCES optimization_runs(id) ON DELETE CASCADE,
  time_index INTEGER NOT NULL,
  price_eur_per_mwh DOUBLE PRECISION NOT NULL,
  natural_inflow_m3_s DOUBLE PRECISION NOT NULL,
  reservoir_volume_m3 DOUBLE PRECISION NOT NULL,
  battery_soc_mwh DOUBLE PRECISION NOT NULL,
  turbine_flow_m3_s DOUBLE PRECISION NOT NULL,
  spill_flow_m3_s DOUBLE PRECISION NOT NULL,
  pump_flow_m3_s DOUBLE PRECISION NOT NULL,
  battery_charge_mw DOUBLE PRECISION NOT NULL,
  battery_discharge_mw DOUBLE PRECISION NOT NULL,
  net_power_mw DOUBLE PRECISION NOT NULL,
  reward_eur DOUBLE PRECISION NOT NULL,
  cumulative_profit_eur DOUBLE PRECISION NOT NULL,
  UNIQUE (run_id, time_index)
);

CREATE INDEX IF NOT EXISTS idx_dispatch_steps_run_id_time_index
  ON dispatch_steps(run_id, time_index);
