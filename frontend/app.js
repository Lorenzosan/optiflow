const statusText = document.getElementById('statusText');
const profitText = document.getElementById('profitText');
const reservoirText = document.getElementById('reservoirText');
const batteryText = document.getElementById('batteryText');
const nextSignalText = document.getElementById('nextSignalText');
const netExposureText = document.getElementById('netExposureText');
const scenarioSummary = document.getElementById('scenarioSummary');
const dispatchTable = document.getElementById('dispatchTable');
const traderTable = document.getElementById('traderTable');
const solverSelect = document.getElementById('solverSelect');
const priceCsvInput = document.getElementById('priceCsvInput');
const inflowCsvInput = document.getElementById('inflowCsvInput');

let scenario = null;
let result = null;

function formatNumber(value, digits = 2) {
  return Number(value).toLocaleString(undefined, { maximumFractionDigits: digits });
}

function setStatus(text) {
  statusText.textContent = text;
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
    if (inflowByTime.has(timeIndex)) {
      throw new Error(`duplicate inflow time_index ${timeIndex}`);
    }
    inflowByTime.set(timeIndex, parseRequiredNumber(row, 'natural_inflow_m3_s', `inflow row ${index + 2}`));
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
    if (probability < 0) {
      throw new Error(`${context}.probability must be non-negative`);
    }
    const key = stochasticKey(timeIndex, realizationIndex);
    if (inflowByKey.has(key)) {
      throw new Error(`duplicate inflow realization ${key}`);
    }
    inflowByKey.set(key, {
      timeIndex,
      realizationIndex,
      probability,
      inflow: parseRequiredNumber(row, 'natural_inflow_m3_s', context),
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

function timestepHours() {
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
    scenarioSummary.textContent = `Loaded stochastic CSV scenario with ${scenario.stochastic_process.length} time steps.`;
  } else {
    scenario = buildDeterministicScenario(priceText, inflowText);
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

  setStatus('Running optimization');
  const response = await fetch('/api/optimizations', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(scenario),
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

loadScenario().catch((error) => setStatus(error.message));
