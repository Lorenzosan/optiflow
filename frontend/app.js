const statusText = document.getElementById('statusText');
const profitText = document.getElementById('profitText');
const reservoirText = document.getElementById('reservoirText');
const batteryText = document.getElementById('batteryText');
const dispatchTable = document.getElementById('dispatchTable');

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
    ctx.fillText(series.label, pad + 8 + index * 130, 24);
  });
}

function renderScenario() {
  if (!scenario) {
    return;
  }

  const steps = scenario.exogenous.map((item) => ({
    x: item.time_index,
    y: item.price_eur_per_mwh,
  }));
  drawChart('priceChart', [{ label: 'EUR/MWh', points: steps }]);
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
      <td>${formatNumber(step.state.reservoir_volume_m3, 0)}</td>
      <td>${formatNumber(step.state.battery_soc_mwh)}</td>
      <td>${formatNumber(step.action.turbine_flow_m3_s)}</td>
      <td>${formatNumber(step.action.pump_flow_m3_s)}</td>
      <td>${formatNumber(step.outcome.net_power_mw)}</td>
      <td>${formatNumber(step.outcome.reward_eur)}</td>
      <td>${formatNumber(step.cumulative_profit_eur)}</td>`;
    dispatchTable.appendChild(row);
  });
}

async function loadScenario() {
  setStatus('Loading scenario');
  const response = await fetch('/api/scenarios/sample');
  if (!response.ok) {
    throw new Error(`Scenario request failed with HTTP ${response.status}`);
  }
  scenario = await response.json();
  renderScenario();
  setStatus('Scenario loaded');
}

async function runOptimization() {
  setStatus('Running optimization');
  const response = await fetch('/api/optimizations', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(scenario ?? {}),
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

document.getElementById('runOptimizationButton').addEventListener('click', () => {
  runOptimization().catch((error) => setStatus(error.message));
});

loadScenario().catch((error) => setStatus(error.message));
