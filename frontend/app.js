const statusText = document.getElementById('statusText');
const profitText = document.getElementById('profitText');
const reservoirText = document.getElementById('reservoirText');
const batteryText = document.getElementById('batteryText');
const nextSignalText = document.getElementById('nextSignalText');
const netExposureText = document.getElementById('netExposureText');
const scenarioSummary = document.getElementById('scenarioSummary');
const constraintsSummary = document.getElementById('constraintsSummary');
const dispatchTable = document.getElementById('dispatchTable');
const traderTable = document.getElementById('traderTable');
const solverSelect = document.getElementById('solverSelect');
const priceCsvInput = document.getElementById('priceCsvInput');
const inflowCsvInput = document.getElementById('inflowCsvInput');

const constraintInputs = {
  timestepHours: document.getElementById('timestepHoursInput'),
  discountFactor: document.getElementById('discountFactorInput'),
  initialReservoir: document.getElementById('initialReservoirInput'),
  minReservoir: document.getElementById('minReservoirInput'),
  maxReservoir: document.getElementById('maxReservoirInput'),
  maxTurbineFlow: document.getElementById('maxTurbineFlowInput'),
  maxPumpFlow: document.getElementById('maxPumpFlowInput'),
  maxSpillFlow: document.getElementById('maxSpillFlowInput'),
  hydraulicHead: document.getElementById('hydraulicHeadInput'),
  turbineEfficiency: document.getElementById('turbineEfficiencyInput'),
  pumpEfficiency: document.getElementById('pumpEfficiencyInput'),
  turbineCost: document.getElementById('turbineCostInput'),
  pumpCost: document.getElementById('pumpCostInput'),
  spillPenalty: document.getElementById('spillPenaltyInput'),
  batteryEnabled: document.getElementById('batteryEnabledInput'),
  initialBattery: document.getElementById('initialBatteryInput'),
  batteryCapacity: document.getElementById('batteryCapacityInput'),
  maxBatteryCharge: document.getElementById('maxBatteryChargeInput'),
  maxBatteryDischarge: document.getElementById('maxBatteryDischargeInput'),
  batteryChargeEfficiency: document.getElementById('batteryChargeEfficiencyInput'),
  batteryDischargeEfficiency: document.getElementById('batteryDischargeEfficiencyInput'),
  batteryDegradationCost: document.getElementById('batteryDegradationCostInput'),
  terminalWaterValue: document.getElementById('terminalWaterValueInput'),
  terminalBatteryValue: document.getElementById('terminalBatteryValueInput'),
};

const defaultParameters = {
  timestep_hours: 1.0,
  terminal_water_value_eur_per_m3: 0.001,
  terminal_battery_value_eur_per_mwh: 5.0,
  hydro: {
    min_reservoir_volume_m3: 0.0,
    max_reservoir_volume_m3: 100000000.0,
    max_turbine_flow_m3_s: 150.0,
    max_pump_flow_m3_s: 75.0,
    max_spill_flow_m3_s: 260.0,
    hydraulic_head_m: 120.0,
    turbine_efficiency: 0.90,
    pump_efficiency: 0.85,
    turbine_cost_eur_per_mwh: 1.0,
    pump_cost_eur_per_mwh: 0.5,
    spill_penalty_eur_per_m3: 0.0,
  },
  battery: {
    enabled: true,
    capacity_mwh: 50.0,
    max_charge_mw: 25.0,
    max_discharge_mw: 25.0,
    charge_efficiency: 0.95,
    discharge_efficiency: 0.95,
    degradation_cost_eur_per_mwh: 1.0,
  },
};

const defaultInitialState = {
  reservoir_volume_m3: 50000000.0,
  battery_soc_mwh: 25.0,
};

let scenario = null;
let result = null;

function formatNumber(value, digits = 2) {
  return Number(value).toLocaleString(undefined, { maximumFractionDigits: digits });
}

function setStatus(text) {
  statusText.textContent = text;
}

function clone(value) {
  return JSON.parse(JSON.stringify(value));
}

function mergeParameters(parameters) {
  const merged = clone(defaultParameters);
  if (!parameters || typeof parameters !== 'object') {
    return merged;
  }
  Object.assign(merged, parameters);
  merged.hydro = { ...defaultParameters.hydro, ...(parameters.hydro ?? {}) };
  merged.battery = { ...defaultParameters.battery, ...(parameters.battery ?? {}) };
  return merged;
}

function extractSeries(steps, selector) {
  return steps.map((step, index) => ({ x: index, y: selector(step) }));
}

function drawChart(canvasId, seriesList) {
  const canvas = document.getElementById(canvasId);
  const ctx = canvas.getContext('2d');
  const width = canvas.width;
  const height = canvas.height;
  const pad = 42;

  ctx.clearRect(0, 0, width, height);
  ctx.lineWidth = 1;
  ctx.strokeStyle = '#d1d5db';
  ctx.strokeRect(pad, pad, width - 2 * pad, height - 2 * pad);

  const allPoints = seriesList.flatMap((series) => series.points);
  if (allPoints.length === 0) {
    return;
  }

  const minX = Math.min(...allPoints.map((point) => point.x));
  const maxX = Math.max(...allPoints.map((point) => point.x));
  let minY = Math.min(...allPoints.map((point) => point.y));
  let maxY = Math.max(...allPoints.map((point) => point.y));
  if (Math.abs(maxY - minY) < 1e-9) {
    minY -= 1;
    maxY += 1;
  }

  const toX = (x) => pad + ((x - minX) / Math.max(1, maxX - minX)) * (width - 2 * pad);
  const toY = (y) => height - pad - ((y - minY) / (maxY - minY)) * (height - 2 * pad);

  ctx.fillStyle = '#4b5563';
  ctx.font = '12px system-ui, sans-serif';
  ctx.fillText(formatNumber(maxY), 8, pad + 4);
  ctx.fillText(formatNumber(minY), 8, height - pad + 4);

  const palette = ['#111827', '#2563eb', '#dc2626', '#059669', '#7c3aed'];
  seriesList.forEach((series, index) => {
    ctx.beginPath();
    ctx.lineWidth = 2;
    ctx.strokeStyle = palette[index % palette.length];
    series.points.forEach((point, pointIndex) => {
      const x = toX(point.x);
      const y = toY(point.y);
      if (pointIndex === 0) {
        ctx.moveTo(x, y);
      } else {
        ctx.lineTo(x, y);
      }
    });
    ctx.stroke();

    ctx.fillStyle = palette[index % palette.length];
    ctx.fillText(series.label, pad + 8 + index * 150, 24);
  });
}

function splitCsvLine(line) {
  const values = [];
  let current = '';
  let inQuotes = false;

  for (let index = 0; index < line.length; ++index) {
    const character = line[index];
    if (character === '"') {
      if (inQuotes && line[index + 1] === '"') {
        current += '"';
        ++index;
      } else {
        inQuotes = !inQuotes;
      }
    } else if (character === ',' && !inQuotes) {
      values.push(current.trim());
      current = '';
    } else {
      current += character;
    }
  }

  values.push(current.trim());
  return values;
}

function parseCsv(text) {
  const lines = text.split(/\r?\n/).map((line) => line.trim()).filter((line) => line.length > 0);
  if (lines.length < 2) {
    throw new Error('CSV must contain a header and at least one data row');
  }

  const header = splitCsvLine(lines[0]);
  return lines.slice(1).map((line, rowIndex) => {
    const values = splitCsvLine(line);
    if (values.length !== header.length) {
      throw new Error(`CSV row ${rowIndex + 2} has ${values.length} columns, expected ${header.length}`);
    }
    const row = {};
    header.forEach((name, index) => {
      row[name] = values[index];
    });
    return row;
  });
}

function parseRequiredNumber(row, key, context) {
  if (!(key in row)) {
    throw new Error(`${context} is missing column ${key}`);
  }
  const value = Number(row[key]);
  if (!Number.isFinite(value)) {
    throw new Error(`${context}.${key} must be a finite number`);
  }
  return value;
}

function parseRequiredIndex(row, key, context) {
  const value = parseRequiredNumber(row, key, context);
  if (value < 0 || Math.floor(value) !== value) {
    throw new Error(`${context}.${key} must be a non-negative integer`);
  }
  return value;
}

function readFileAsText(file) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => resolve(String(reader.result ?? ''));
    reader.onerror = () => reject(new Error(`Could not read ${file.name}`));
    reader.readAsText(file);
  });
}

function requireContiguousTimeIndexes(indexes, context) {
  const sorted = [...indexes].sort((a, b) => a - b);
  sorted.forEach((value, expected) => {
    if (value !== expected) {
      throw new Error(`${context} time_index values must be contiguous from 0`);
    }
  });
}

function buildDeterministicScenario(priceText, inflowText) {
  const priceRows = parseCsv(priceText);
  const inflowRows = parseCsv(inflowText);
  const priceByTime = new Map();
  const inflowByTime = new Map();

  priceRows.forEach((row, index) => {
    const timeIndex = parseRequiredIndex(row, 'time_index', `price row ${index + 2}`);
    if (priceByTime.has(timeIndex)) {
      throw new Error(`duplicate price time_index ${timeIndex}`);
    }
    priceByTime.set(timeIndex, parseRequiredNumber(row, 'price_eur_per_mwh', `price row ${index + 2}`));
  });

  inflowRows.forEach((row, index) => {
    const timeIndex = parseRequiredIndex(row, 'time_index', `inflow row ${index + 2}`);
    const inflow = parseRequiredNumber(row, 'natural_inflow_m3_s', `inflow row ${index + 2}`);
    if (inflow < 0) {
      throw new Error(`inflow row ${index + 2}.natural_inflow_m3_s must be non-negative`);
    }
    if (inflowByTime.has(timeIndex)) {
      throw new Error(`duplicate inflow time_index ${timeIndex}`);
    }
    inflowByTime.set(timeIndex, inflow);
  });

  requireContiguousTimeIndexes(priceByTime.keys(), 'price CSV');
  requireContiguousTimeIndexes(inflowByTime.keys(), 'inflow CSV');

  if (priceByTime.size !== inflowByTime.size) {
    throw new Error('price and inflow CSV files must have the same horizon');
  }

  const exogenous = [];
  for (let timeIndex = 0; timeIndex < priceByTime.size; ++timeIndex) {
    if (!inflowByTime.has(timeIndex)) {
      throw new Error(`missing inflow row for time_index ${timeIndex}`);
    }
    exogenous.push({
      time_index: timeIndex,
      price_eur_per_mwh: priceByTime.get(timeIndex),
      natural_inflow_m3_s: inflowByTime.get(timeIndex),
    });
  }

  return {
    solver_kind: 'deterministic',
    scenario_name: 'frontend_csv_deterministic',
    exogenous,
  };
}

function stochasticKey(timeIndex, realizationIndex) {
  return `${timeIndex}:${realizationIndex}`;
}

function buildStochasticScenario(priceText, inflowText) {
  const priceRows = parseCsv(priceText);
  const inflowRows = parseCsv(inflowText);
  const priceByKey = new Map();
  const inflowByKey = new Map();
  const timeIndexes = new Set();
  const realizationsByTime = new Map();

  priceRows.forEach((row, index) => {
    const context = `price row ${index + 2}`;
    const timeIndex = parseRequiredIndex(row, 'time_index', context);
    const realizationIndex = parseRequiredIndex(row, 'realization_index', context);
    const probability = parseRequiredNumber(row, 'probability', context);
    if (probability < 0) {
      throw new Error(`${context}.probability must be non-negative`);
    }
    const key = stochasticKey(timeIndex, realizationIndex);
    if (priceByKey.has(key)) {
      throw new Error(`duplicate price realization ${key}`);
    }
    priceByKey.set(key, {
      timeIndex,
      realizationIndex,
      probability,
      price: parseRequiredNumber(row, 'price_eur_per_mwh', context),
    });
    timeIndexes.add(timeIndex);
    if (!realizationsByTime.has(timeIndex)) {
      realizationsByTime.set(timeIndex, new Set());
    }
    realizationsByTime.get(timeIndex).add(realizationIndex);
  });

  inflowRows.forEach((row, index) => {
    const context = `inflow row ${index + 2}`;
    const timeIndex = parseRequiredIndex(row, 'time_index', context);
    const realizationIndex = parseRequiredIndex(row, 'realization_index', context);
    const probability = parseRequiredNumber(row, 'probability', context);
    const inflow = parseRequiredNumber(row, 'natural_inflow_m3_s', context);
    if (probability < 0) {
      throw new Error(`${context}.probability must be non-negative`);
    }
    if (inflow < 0) {
      throw new Error(`${context}.natural_inflow_m3_s must be non-negative`);
    }
    const key = stochasticKey(timeIndex, realizationIndex);
    if (inflowByKey.has(key)) {
      throw new Error(`duplicate inflow realization ${key}`);
    }
    inflowByKey.set(key, {
      timeIndex,
      realizationIndex,
      probability,
      inflow,
    });
  });

  requireContiguousTimeIndexes(timeIndexes.keys(), 'stochastic price CSV');

  const stochasticProcess = [];
  for (let timeIndex = 0; timeIndex < timeIndexes.size; ++timeIndex) {
    const realizationIndexes = realizationsByTime.get(timeIndex);
    if (!realizationIndexes || realizationIndexes.size === 0) {
      throw new Error(`missing stochastic realizations for time_index ${timeIndex}`);
    }
    const sortedRealizations = [...realizationIndexes].sort((a, b) => a - b);
    sortedRealizations.forEach((realizationIndex, expected) => {
      if (realizationIndex !== expected) {
        throw new Error(`realization_index values at time_index ${timeIndex} must be contiguous from 0`);
      }
    });

    let probabilitySum = 0;
    const realizations = sortedRealizations.map((realizationIndex) => {
      const key = stochasticKey(timeIndex, realizationIndex);
      const price = priceByKey.get(key);
      const inflow = inflowByKey.get(key);
      if (!inflow) {
        throw new Error(`missing inflow realization ${key}`);
      }
      if (Math.abs(price.probability - inflow.probability) > 1e-9) {
        throw new Error(`probability mismatch for realization ${key}`);
      }
      probabilitySum += price.probability;
      return {
        realization_index: realizationIndex,
        probability: price.probability,
        price_eur_per_mwh: price.price,
        natural_inflow_m3_s: inflow.inflow,
      };
    });

    if (Math.abs(probabilitySum - 1.0) > 1e-9) {
      throw new Error(`probabilities at time_index ${timeIndex} must sum to 1.0`);
    }
    stochasticProcess.push({ time_index: timeIndex, realizations });
  }

  if (inflowByKey.size !== priceByKey.size) {
    throw new Error('price and inflow stochastic CSV files must contain the same realization keys');
  }

  return {
    solver_kind: 'stochastic',
    scenario_name: 'frontend_csv_stochastic',
    stochastic_process: stochasticProcess,
  };
}

function expectedPathFromStochasticProcess(process) {
  return process.map((stage) => {
    const expected = stage.realizations.reduce((accumulator, realization) => ({
      price_eur_per_mwh: accumulator.price_eur_per_mwh + realization.probability * realization.price_eur_per_mwh,
      natural_inflow_m3_s: accumulator.natural_inflow_m3_s + realization.probability * realization.natural_inflow_m3_s,
    }), { price_eur_per_mwh: 0, natural_inflow_m3_s: 0 });
    return {
      time_index: stage.time_index,
      price_eur_per_mwh: expected.price_eur_per_mwh,
      natural_inflow_m3_s: expected.natural_inflow_m3_s,
    };
  });
}

function scenarioPathForPlot() {
  if (!scenario) {
    return [];
  }
  if (Array.isArray(scenario.exogenous)) {
    return scenario.exogenous;
  }
  if (Array.isArray(scenario.stochastic_process)) {
    return expectedPathFromStochasticProcess(scenario.stochastic_process);
  }
  return [];
}

function renderScenario() {
  const path = scenarioPathForPlot();
  if (path.length === 0) {
    return;
  }

  drawChart('priceChart', [
    { label: 'price EUR/MWh', points: path.map((item) => ({ x: item.time_index, y: item.price_eur_per_mwh })) },
    { label: 'inflow m3/s', points: path.map((item) => ({ x: item.time_index, y: item.natural_inflow_m3_s })) },
  ]);
}

function readInputNumber(input, name) {
  const value = Number(input.value);
  if (!Number.isFinite(value)) {
    throw new Error(`${name} must be finite`);
  }
  return value;
}

function requireNonNegative(value, name) {
  if (value < 0) {
    throw new Error(`${name} must be non-negative`);
  }
}

function requirePositive(value, name) {
  if (value <= 0) {
    throw new Error(`${name} must be positive`);
  }
}

function requireEfficiency(value, name) {
  if (value <= 0 || value > 1) {
    throw new Error(`${name} must be in (0, 1]`);
  }
}

function readConstraintsFromForm() {
  const parameters = {
    timestep_hours: readInputNumber(constraintInputs.timestepHours, 'time step hours'),
    terminal_water_value_eur_per_m3: readInputNumber(constraintInputs.terminalWaterValue, 'terminal water value'),
    terminal_battery_value_eur_per_mwh: readInputNumber(constraintInputs.terminalBatteryValue, 'terminal battery value'),
    hydro: {
      min_reservoir_volume_m3: readInputNumber(constraintInputs.minReservoir, 'minimum reservoir volume'),
      max_reservoir_volume_m3: readInputNumber(constraintInputs.maxReservoir, 'maximum reservoir volume'),
      max_turbine_flow_m3_s: readInputNumber(constraintInputs.maxTurbineFlow, 'maximum turbine flow'),
      max_pump_flow_m3_s: readInputNumber(constraintInputs.maxPumpFlow, 'maximum pump flow'),
      max_spill_flow_m3_s: readInputNumber(constraintInputs.maxSpillFlow, 'maximum spill flow'),
      hydraulic_head_m: readInputNumber(constraintInputs.hydraulicHead, 'hydraulic head'),
      turbine_efficiency: readInputNumber(constraintInputs.turbineEfficiency, 'turbine efficiency'),
      pump_efficiency: readInputNumber(constraintInputs.pumpEfficiency, 'pump efficiency'),
      turbine_cost_eur_per_mwh: readInputNumber(constraintInputs.turbineCost, 'turbine cost'),
      pump_cost_eur_per_mwh: readInputNumber(constraintInputs.pumpCost, 'pump cost'),
      spill_penalty_eur_per_m3: readInputNumber(constraintInputs.spillPenalty, 'spill penalty'),
    },
    battery: {
      enabled: constraintInputs.batteryEnabled.checked,
      capacity_mwh: readInputNumber(constraintInputs.batteryCapacity, 'battery capacity'),
      max_charge_mw: readInputNumber(constraintInputs.maxBatteryCharge, 'maximum battery charge'),
      max_discharge_mw: readInputNumber(constraintInputs.maxBatteryDischarge, 'maximum battery discharge'),
      charge_efficiency: readInputNumber(constraintInputs.batteryChargeEfficiency, 'battery charge efficiency'),
      discharge_efficiency: readInputNumber(constraintInputs.batteryDischargeEfficiency, 'battery discharge efficiency'),
      degradation_cost_eur_per_mwh: readInputNumber(constraintInputs.batteryDegradationCost, 'battery degradation cost'),
    },
  };

  const initialState = {
    reservoir_volume_m3: readInputNumber(constraintInputs.initialReservoir, 'initial reservoir volume'),
    battery_soc_mwh: readInputNumber(constraintInputs.initialBattery, 'initial battery SOC'),
  };

  const optimizationConfig = {
    discount_factor: readInputNumber(constraintInputs.discountFactor, 'discount factor'),
  };

  validateConstraints(parameters, initialState, optimizationConfig);
  return { parameters, initialState, optimizationConfig };
}

function validateConstraints(parameters, initialState, optimizationConfig) {
  requirePositive(parameters.timestep_hours, 'time step hours');
  requireNonNegative(parameters.hydro.min_reservoir_volume_m3, 'minimum reservoir volume');
  requireNonNegative(parameters.hydro.max_reservoir_volume_m3, 'maximum reservoir volume');
  if (parameters.hydro.max_reservoir_volume_m3 <= parameters.hydro.min_reservoir_volume_m3) {
    throw new Error('maximum reservoir volume must exceed minimum reservoir volume');
  }
  requireNonNegative(parameters.hydro.max_turbine_flow_m3_s, 'maximum turbine flow');
  requireNonNegative(parameters.hydro.max_pump_flow_m3_s, 'maximum pump flow');
  requireNonNegative(parameters.hydro.max_spill_flow_m3_s, 'maximum spill flow');
  requirePositive(parameters.hydro.hydraulic_head_m, 'hydraulic head');
  requireEfficiency(parameters.hydro.turbine_efficiency, 'turbine efficiency');
  requireEfficiency(parameters.hydro.pump_efficiency, 'pump efficiency');
  requireNonNegative(parameters.hydro.turbine_cost_eur_per_mwh, 'turbine cost');
  requireNonNegative(parameters.hydro.pump_cost_eur_per_mwh, 'pump cost');
  requireNonNegative(parameters.hydro.spill_penalty_eur_per_m3, 'spill penalty');

  if (initialState.reservoir_volume_m3 < parameters.hydro.min_reservoir_volume_m3
      || initialState.reservoir_volume_m3 > parameters.hydro.max_reservoir_volume_m3) {
    throw new Error('initial reservoir volume must lie inside reservoir bounds');
  }

  requireNonNegative(parameters.battery.capacity_mwh, 'battery capacity');
  requireNonNegative(parameters.battery.max_charge_mw, 'maximum battery charge');
  requireNonNegative(parameters.battery.max_discharge_mw, 'maximum battery discharge');
  requireEfficiency(parameters.battery.charge_efficiency, 'battery charge efficiency');
  requireEfficiency(parameters.battery.discharge_efficiency, 'battery discharge efficiency');
  requireNonNegative(parameters.battery.degradation_cost_eur_per_mwh, 'battery degradation cost');

  if (parameters.battery.enabled) {
    requirePositive(parameters.battery.capacity_mwh, 'battery capacity');
    if (initialState.battery_soc_mwh < 0 || initialState.battery_soc_mwh > parameters.battery.capacity_mwh) {
      throw new Error('initial battery SOC must lie inside battery bounds');
    }
  } else if (Math.abs(initialState.battery_soc_mwh) > 1e-9) {
    throw new Error('initial battery SOC must be zero when the battery is disabled');
  }

  if (optimizationConfig.discount_factor < 0 || optimizationConfig.discount_factor > 1) {
    throw new Error('discount factor must be in [0, 1]');
  }
}

function setConstraintForm(parameters, initialState, optimizationConfig) {
  const merged = mergeParameters(parameters);
  const state = { ...defaultInitialState, ...(initialState ?? {}) };
  const config = { discount_factor: 1.0, ...(optimizationConfig ?? {}) };

  constraintInputs.timestepHours.value = merged.timestep_hours;
  constraintInputs.discountFactor.value = config.discount_factor;
  constraintInputs.initialReservoir.value = state.reservoir_volume_m3;
  constraintInputs.minReservoir.value = merged.hydro.min_reservoir_volume_m3;
  constraintInputs.maxReservoir.value = merged.hydro.max_reservoir_volume_m3;
  constraintInputs.maxTurbineFlow.value = merged.hydro.max_turbine_flow_m3_s;
  constraintInputs.maxPumpFlow.value = merged.hydro.max_pump_flow_m3_s;
  constraintInputs.maxSpillFlow.value = merged.hydro.max_spill_flow_m3_s;
  constraintInputs.hydraulicHead.value = merged.hydro.hydraulic_head_m;
  constraintInputs.turbineEfficiency.value = merged.hydro.turbine_efficiency;
  constraintInputs.pumpEfficiency.value = merged.hydro.pump_efficiency;
  constraintInputs.turbineCost.value = merged.hydro.turbine_cost_eur_per_mwh;
  constraintInputs.pumpCost.value = merged.hydro.pump_cost_eur_per_mwh;
  constraintInputs.spillPenalty.value = merged.hydro.spill_penalty_eur_per_m3;
  constraintInputs.batteryEnabled.checked = Boolean(merged.battery.enabled);
  constraintInputs.initialBattery.value = state.battery_soc_mwh;
  constraintInputs.batteryCapacity.value = merged.battery.capacity_mwh;
  constraintInputs.maxBatteryCharge.value = merged.battery.max_charge_mw;
  constraintInputs.maxBatteryDischarge.value = merged.battery.max_discharge_mw;
  constraintInputs.batteryChargeEfficiency.value = merged.battery.charge_efficiency;
  constraintInputs.batteryDischargeEfficiency.value = merged.battery.discharge_efficiency;
  constraintInputs.batteryDegradationCost.value = merged.battery.degradation_cost_eur_per_mwh;
  constraintInputs.terminalWaterValue.value = merged.terminal_water_value_eur_per_m3;
  constraintInputs.terminalBatteryValue.value = merged.terminal_battery_value_eur_per_mwh;
  renderConstraintsSummary();
}

function renderConstraintsSummary() {
  try {
    const { parameters, initialState, optimizationConfig } = readConstraintsFromForm();
    constraintsSummary.innerHTML = `
      <span>Reservoir ${formatNumber(parameters.hydro.min_reservoir_volume_m3, 0)} to ${formatNumber(parameters.hydro.max_reservoir_volume_m3, 0)} m3</span>
      <span>Initial reservoir ${formatNumber(initialState.reservoir_volume_m3, 0)} m3</span>
      <span>Turbine ${formatNumber(parameters.hydro.max_turbine_flow_m3_s)} m3/s</span>
      <span>Pump ${formatNumber(parameters.hydro.max_pump_flow_m3_s)} m3/s</span>
      <span>Spill ${formatNumber(parameters.hydro.max_spill_flow_m3_s)} m3/s</span>
      <span>Battery ${parameters.battery.enabled ? `${formatNumber(parameters.battery.capacity_mwh)} MWh` : 'disabled'}</span>
      <span>Charge/discharge ${formatNumber(parameters.battery.max_charge_mw)} / ${formatNumber(parameters.battery.max_discharge_mw)} MW</span>
      <span>Discount ${formatNumber(optimizationConfig.discount_factor, 4)}</span>`;
  } catch (error) {
    constraintsSummary.textContent = error.message;
  }
}

function attachConstraintsToScenario(baseScenario) {
  const { parameters, initialState, optimizationConfig } = readConstraintsFromForm();
  const request = clone(baseScenario);
  request.parameters = parameters;
  request.initial_state = initialState;
  request.optimization_config = optimizationConfig;
  request.timestep_hours = parameters.timestep_hours;
  return request;
}

function timestepHours() {
  if (scenario && scenario.parameters && Number.isFinite(Number(scenario.parameters.timestep_hours))) {
    return Number(scenario.parameters.timestep_hours);
  }
  if (scenario && Number.isFinite(Number(scenario.timestep_hours))) {
    return Number(scenario.timestep_hours);
  }
  return 1.0;
}

function dispatchSignal(step) {
  const netPower = step.outcome.net_power_mw;
  if (netPower > 1e-9) {
    return 'SELL';
  }
  if (netPower < -1e-9) {
    return 'BUY';
  }
  if (step.action.turbine_flow_m3_s > 1e-9) {
    return 'TURBINE';
  }
  if (step.action.pump_flow_m3_s > 1e-9) {
    return 'PUMP';
  }
  return 'WAIT';
}

function renderTraderSummary() {
  const hours = timestepHours();
  let netExposureMwh = 0;
  let firstActiveSignal = 'WAIT';

  traderTable.innerHTML = '';
  result.steps.forEach((step) => {
    const signal = dispatchSignal(step);
    const netMwh = step.outcome.net_power_mw * hours;
    netExposureMwh += netMwh;
    if (firstActiveSignal === 'WAIT' && signal !== 'WAIT') {
      firstActiveSignal = `${signal} t=${step.time_index}`;
    }

    const row = document.createElement('tr');
    row.innerHTML = `
      <td>${step.time_index}</td>
      <td>${signal}</td>
      <td>${formatNumber(step.exogenous.price_eur_per_mwh)}</td>
      <td>${formatNumber(step.outcome.net_power_mw)}</td>
      <td>${formatNumber(netMwh)}</td>
      <td>${formatNumber(step.outcome.reward_eur)}</td>
      <td>${formatNumber(step.cumulative_profit_eur)}</td>`;
    traderTable.appendChild(row);
  });

  nextSignalText.textContent = firstActiveSignal;
  netExposureText.textContent = `${formatNumber(netExposureMwh)} MWh`;
}

function renderResult() {
  if (!result) {
    return;
  }

  profitText.textContent = `${formatNumber(result.run.total_profit_eur)} EUR`;
  reservoirText.textContent = `${formatNumber(result.run.final_state.reservoir_volume_m3, 0)} m3`;
  batteryText.textContent = `${formatNumber(result.run.final_state.battery_soc_mwh)} MWh`;

  drawChart('reservoirChart', [
    { label: 'reservoir m3', points: extractSeries(result.steps, (step) => step.state.reservoir_volume_m3) },
    { label: 'SOC MWh', points: extractSeries(result.steps, (step) => step.state.battery_soc_mwh) },
  ]);

  drawChart('powerChart', [
    { label: 'net MW', points: extractSeries(result.steps, (step) => step.outcome.net_power_mw) },
    { label: 'turbine flow', points: extractSeries(result.steps, (step) => step.action.turbine_flow_m3_s) },
    { label: 'pump flow', points: extractSeries(result.steps, (step) => step.action.pump_flow_m3_s) },
  ]);

  drawChart('profitChart', [
    { label: 'cum. EUR', points: extractSeries(result.steps, (step) => step.cumulative_profit_eur) },
  ]);

  dispatchTable.innerHTML = '';
  result.steps.forEach((step) => {
    const row = document.createElement('tr');
    row.innerHTML = `
      <td>${step.time_index}</td>
      <td>${formatNumber(step.exogenous.price_eur_per_mwh)}</td>
      <td>${formatNumber(step.exogenous.natural_inflow_m3_s)}</td>
      <td>${formatNumber(step.state.reservoir_volume_m3, 0)}</td>
      <td>${formatNumber(step.state.battery_soc_mwh)}</td>
      <td>${formatNumber(step.action.turbine_flow_m3_s)}</td>
      <td>${formatNumber(step.action.pump_flow_m3_s)}</td>
      <td>${formatNumber(step.action.spill_flow_m3_s)}</td>
      <td>${formatNumber(step.action.battery_charge_mw)}</td>
      <td>${formatNumber(step.action.battery_discharge_mw)}</td>
      <td>${formatNumber(step.outcome.net_power_mw)}</td>
      <td>${formatNumber(step.outcome.reward_eur)}</td>
      <td>${formatNumber(step.cumulative_profit_eur)}</td>`;
    dispatchTable.appendChild(row);
  });

  renderTraderSummary();
}

async function loadScenario() {
  setStatus('Loading scenario');
  const response = await fetch('/api/scenarios/sample');
  if (!response.ok) {
    throw new Error(`Scenario request failed with HTTP ${response.status}`);
  }
  scenario = await response.json();
  scenario.solver_kind = 'deterministic';
  solverSelect.value = 'deterministic';
  setConstraintForm(scenario.parameters, scenario.initial_state, scenario.optimization_config);
  scenario = attachConstraintsToScenario(scenario);
  scenarioSummary.textContent = `Loaded deterministic sample with ${scenario.exogenous.length} time steps.`;
  renderScenario();
  setStatus('Scenario loaded');
}

async function loadCsvScenario() {
  if (!priceCsvInput.files || priceCsvInput.files.length !== 1) {
    throw new Error('Select one price CSV file');
  }
  if (!inflowCsvInput.files || inflowCsvInput.files.length !== 1) {
    throw new Error('Select one inflow CSV file');
  }

  setStatus('Reading CSV files');
  const [priceText, inflowText] = await Promise.all([
    readFileAsText(priceCsvInput.files[0]),
    readFileAsText(inflowCsvInput.files[0]),
  ]);

  if (solverSelect.value === 'stochastic') {
    scenario = buildStochasticScenario(priceText, inflowText);
    scenario = attachConstraintsToScenario(scenario);
    scenarioSummary.textContent = `Loaded stochastic CSV scenario with ${scenario.stochastic_process.length} time steps.`;
  } else {
    scenario = buildDeterministicScenario(priceText, inflowText);
    scenario = attachConstraintsToScenario(scenario);
    scenarioSummary.textContent = `Loaded deterministic CSV scenario with ${scenario.exogenous.length} time steps.`;
  }

  result = null;
  renderScenario();
  setStatus('CSV scenario loaded');
}

async function runOptimization() {
  if (!scenario) {
    throw new Error('Load a sample scenario or CSV scenario first');
  }

  const request = attachConstraintsToScenario(scenario);
  scenario = request;

  setStatus('Running optimization');
  const response = await fetch('/api/optimizations', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(request),
  });
  if (!response.ok) {
    const errorText = await response.text();
    throw new Error(`Optimization failed with HTTP ${response.status}: ${errorText}`);
  }
  result = await response.json();
  renderResult();
  setStatus('Optimization complete');
}

document.getElementById('loadScenarioButton').addEventListener('click', () => {
  loadScenario().catch((error) => setStatus(error.message));
});

document.getElementById('loadCsvButton').addEventListener('click', () => {
  loadCsvScenario().catch((error) => setStatus(error.message));
});

document.getElementById('runOptimizationButton').addEventListener('click', () => {
  runOptimization().catch((error) => setStatus(error.message));
});

Object.values(constraintInputs).forEach((input) => {
  input.addEventListener('input', renderConstraintsSummary);
  input.addEventListener('change', renderConstraintsSummary);
});

setConstraintForm(defaultParameters, defaultInitialState, { discount_factor: 1.0 });
loadScenario().catch((error) => setStatus(error.message));
