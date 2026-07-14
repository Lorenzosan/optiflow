import {
  SCENARIO_FILE_LIMIT_BYTES,
  SCENARIO_PARAMETER_GROUPS,
  SERIES_FILE_LIMIT_BYTES,
  buildScenarioCsv,
  validateSeriesPair,
} from "./scenario.mjs";
import { provenanceItems } from "./run_presentation.mjs";
import {
  DISPATCH_CHART_TIME_ZONE,
  buildDispatchChartModel,
  renderDispatchCharts,
} from "./dispatch_charts.mjs";
import {
  PEAK_END_HOUR,
  PEAK_START_HOUR,
  TRADER_TIME_ZONE,
  buildTraderRows,
} from "./trader.mjs";

const API_BASE = "/api";
const PAGE_SIZE = 10;
const ALLOWED_STATUSES = new Set(["pending", "running", "succeeded", "failed"]);

const state = {
  scenarios: [],
  runs: [],
  total: 0,
  offset: 0,
  selectedRunId: null,
  traderRequestId: 0,
};

const elements = {
  runForm: document.querySelector("#run-form"),
  scenarioSelect: document.querySelector("#scenario-select"),
  scenarioDescription: document.querySelector("#scenario-description"),
  runButton: document.querySelector("#run-button"),
  operationMessage: document.querySelector("#operation-message"),
  scenarioEditor: document.querySelector("#scenario-editor"),
  scenarioForm: document.querySelector("#scenario-form"),
  scenarioName: document.querySelector("#scenario-name"),
  scenarioDescriptionInput: document.querySelector("#scenario-description-input"),
  scenarioFields: document.querySelector("#scenario-fields"),
  pricesFile: document.querySelector("#prices-file"),
  inflowsFile: document.querySelector("#inflows-file"),
  seriesPreview: document.querySelector("#series-preview"),
  saveScenarioButton: document.querySelector("#save-scenario-button"),
  scenarioSaveMessage: document.querySelector("#scenario-save-message"),
  refreshButton: document.querySelector("#refresh-button"),
  filterScenario: document.querySelector("#filter-scenario"),
  filterStatus: document.querySelector("#filter-status"),
  runsBody: document.querySelector("#runs-body"),
  previousButton: document.querySelector("#previous-button"),
  nextButton: document.querySelector("#next-button"),
  pageSummary: document.querySelector("#page-summary"),
  detailsEmpty: document.querySelector("#details-empty"),
  detailsContent: document.querySelector("#details-content"),
  runTitle: document.querySelector("#run-title"),
  runStatus: document.querySelector("#run-status"),
  runStarted: document.querySelector("#run-started"),
  runCompleted: document.querySelector("#run-completed"),
  runError: document.querySelector("#run-error"),
  summaryGrid: document.querySelector("#summary-grid"),
  runProvenance: document.querySelector("#run-provenance"),
  provenanceGrid: document.querySelector("#provenance-grid"),
  dispatchCharts: document.querySelector("#dispatch-charts"),
  dispatchChartsMessage: document.querySelector("#dispatch-charts-message"),
  traderCaption: document.querySelector("#trader-caption"),
  traderMessage: document.querySelector("#trader-message"),
  traderBody: document.querySelector("#trader-body"),
  downloadLink: document.querySelector("#download-link"),
};

class ApiError extends Error {
  constructor(message, status) {
    super(message);
    this.name = "ApiError";
    this.status = status;
  }
}

async function apiRequest(path, options = {}) {
  const response = await fetch(`${API_BASE}${path}`, {
    ...options,
    headers: {
      Accept: "application/json",
      ...options.headers,
    },
  });

  const contentType = response.headers.get("content-type") ?? "";
  const payload = contentType.includes("application/json")
    ? await response.json()
    : await response.text();

  if (!response.ok) {
    throw new ApiError(extractErrorMessage(payload), response.status);
  }

  return payload;
}

function extractErrorMessage(payload) {
  if (typeof payload === "string" && payload.trim()) {
    return payload.trim();
  }
  if (payload && typeof payload.detail === "string") {
    return payload.detail;
  }
  if (Array.isArray(payload?.detail)) {
    return payload.detail
      .map((item) => {
        const location = Array.isArray(item.loc) ? item.loc.join(".") : "request";
        return `${location}: ${item.msg ?? "invalid value"}`;
      })
      .join("; ");
  }
  if (payload?.detail?.message) {
    const runSuffix = payload.detail.run_id ? ` (run ${payload.detail.run_id})` : "";
    const validationSuffix = payload.detail.error ? `: ${payload.detail.error}` : "";
    return `${payload.detail.message}${runSuffix}${validationSuffix}`;
  }
  return "The API request failed.";
}

function setOperationMessage(message, isError = false) {
  elements.operationMessage.textContent = message;
  elements.operationMessage.classList.toggle("error", isError);
}

function setScenarioMessage(message, isError = false) {
  elements.scenarioSaveMessage.textContent = message;
  elements.scenarioSaveMessage.classList.toggle("error", isError);
  elements.scenarioSaveMessage.classList.toggle("success", Boolean(message) && !isError);
}

function formatDate(value) {
  if (!value) {
    return "—";
  }
  const hasTimezone = /(?:Z|[+-]\d{2}:\d{2})$/.test(value);
  const date = new Date(hasTimezone ? value : `${value}Z`);
  return Number.isNaN(date.valueOf())
    ? value
    : new Intl.DateTimeFormat(undefined, {
        dateStyle: "medium",
        timeStyle: "short",
      }).format(date);
}

function formatNumber(value, maximumFractionDigits = 2) {
  if (value === null || value === undefined || !Number.isFinite(Number(value))) {
    return "—";
  }
  return new Intl.NumberFormat(undefined, {
    maximumFractionDigits,
  }).format(Number(value));
}

function formatRuntime(summary) {
  if (!summary) {
    return "—";
  }
  return `${formatNumber(summary.solve_seconds + summary.simulation_seconds, 3)} s`;
}

function createOption(value, label, disabled = false) {
  const option = document.createElement("option");
  option.value = String(value);
  option.textContent = label;
  option.disabled = disabled;
  return option;
}

function renderScenarioControls(preferredScenarioId = null) {
  const selectedScenarioId = preferredScenarioId ?? (Number(elements.scenarioSelect.value) || null);
  const filteredScenarioId = Number(elements.filterScenario.value) || null;

  elements.scenarioSelect.replaceChildren(createOption("", "Select a scenario"));
  elements.filterScenario.replaceChildren(createOption("", "All scenarios"));

  for (const scenario of state.scenarios) {
    const suffix = scenario.available ? "" : " — files unavailable";
    elements.scenarioSelect.append(
      createOption(scenario.id, `${scenario.name}${suffix}`, !scenario.available),
    );
    elements.filterScenario.append(createOption(scenario.id, scenario.name));
  }

  if (selectedScenarioId && state.scenarios.some((item) => item.id === selectedScenarioId)) {
    elements.scenarioSelect.value = String(selectedScenarioId);
  }
  if (filteredScenarioId && state.scenarios.some((item) => item.id === filteredScenarioId)) {
    elements.filterScenario.value = String(filteredScenarioId);
  }

  elements.scenarioSelect.disabled = false;
  elements.runButton.disabled = !elements.scenarioSelect.value;
  updateScenarioDescription();
}

function updateScenarioDescription() {
  const selectedId = Number(elements.scenarioSelect.value);
  const scenario = state.scenarios.find((item) => item.id === selectedId);
  elements.scenarioDescription.textContent = scenario
    ? scenario.description
    : "Choose a bundled or custom scenario to run the optimizer.";
  elements.runButton.disabled = !scenario || !scenario.available;
}

async function loadScenarios({ selectedScenarioId = null } = {}) {
  try {
    state.scenarios = await apiRequest("/scenarios");
    renderScenarioControls(selectedScenarioId);
  } catch (error) {
    elements.scenarioSelect.replaceChildren(createOption("", "Scenarios unavailable"));
    elements.scenarioSelect.disabled = true;
    elements.runButton.disabled = true;
    setOperationMessage(error.message, true);
  }
}

function createParameterInput(field) {
  const wrapper = document.createElement("div");
  wrapper.className = "form-field parameter-field";

  const label = document.createElement("label");
  const inputId = `scenario-${field.key}`;
  label.htmlFor = inputId;
  label.textContent = field.label;

  const input = document.createElement("input");
  input.id = inputId;
  input.name = field.key;
  input.type = "number";
  input.step = String(field.step ?? (field.integer ? 1 : "any"));
  input.required = true;
  input.value = String(field.defaultValue);
  input.defaultValue = String(field.defaultValue);
  if (field.min !== undefined) {
    input.min = String(field.min);
  }
  if (field.max !== undefined) {
    input.max = String(field.max);
  }

  wrapper.append(label, input);
  if (field.help) {
    const help = document.createElement("small");
    help.textContent = field.help;
    wrapper.append(help);
  }
  return wrapper;
}

function renderScenarioFields() {
  const groups = SCENARIO_PARAMETER_GROUPS.map((group) => {
    const fieldset = document.createElement("fieldset");
    fieldset.className = "parameter-group";

    const legend = document.createElement("legend");
    legend.textContent = group.title;

    const grid = document.createElement("div");
    grid.className = "parameter-grid";
    grid.append(...group.fields.map(createParameterInput));

    if (group.note) {
      const note = document.createElement("p");
      note.className = "group-note";
      note.textContent = group.note;
      fieldset.append(legend, note, grid);
    } else {
      fieldset.append(legend, grid);
    }
    return fieldset;
  });

  elements.scenarioFields.replaceChildren(...groups);
}

function requireSeriesFile(input, label) {
  const file = input.files?.[0];
  if (!file) {
    throw new Error(`${label} CSV is required.`);
  }
  if (!file.name.toLowerCase().endsWith(".csv")) {
    throw new Error(`${label} must be a .csv file.`);
  }
  if (file.size > SERIES_FILE_LIMIT_BYTES) {
    throw new Error(`${label} exceeds the 8 MiB upload limit.`);
  }
  return file;
}

async function validateSelectedSeries() {
  const pricesFile = requireSeriesFile(elements.pricesFile, "Prices");
  const inflowsFile = requireSeriesFile(elements.inflowsFile, "Inflows");
  const [pricesText, inflowsText] = await Promise.all([
    pricesFile.text(),
    inflowsFile.text(),
  ]);
  const timeStepInput = elements.scenarioForm.elements.namedItem("time_step_hours");
  const summary = validateSeriesPair(pricesText, inflowsText, timeStepInput?.value);
  return { pricesFile, inflowsFile, summary };
}

async function updateSeriesPreview() {
  if (!elements.pricesFile.files?.[0] || !elements.inflowsFile.files?.[0]) {
    elements.seriesPreview.textContent = "Select both series files to validate their headers and horizon.";
    elements.seriesPreview.classList.remove("error", "success");
    return;
  }

  try {
    const { summary } = await validateSelectedSeries();
    elements.seriesPreview.textContent = `${summary.rowCount.toLocaleString()} matching timestamped steps detected.`;
    elements.seriesPreview.classList.remove("error");
    elements.seriesPreview.classList.add("success");
  } catch (error) {
    elements.seriesPreview.textContent = error.message;
    elements.seriesPreview.classList.add("error");
    elements.seriesPreview.classList.remove("success");
  }
}

async function createScenario(event) {
  event.preventDefault();
  setScenarioMessage("");
  elements.saveScenarioButton.disabled = true;

  try {
    const values = Object.fromEntries(new FormData(elements.scenarioForm).entries());
    const scenarioCsv = buildScenarioCsv(elements.scenarioName.value, values);
    const scenarioFile = new File([scenarioCsv], "scenario.csv", { type: "text/csv" });
    if (scenarioFile.size > SCENARIO_FILE_LIMIT_BYTES) {
      throw new Error("Generated scenario CSV exceeds the 256 KiB upload limit.");
    }

    const { pricesFile, inflowsFile, summary } = await validateSelectedSeries();
    const payload = new FormData();
    payload.append("description", elements.scenarioDescriptionInput.value.trim());
    payload.append("scenario", scenarioFile);
    payload.append("prices", pricesFile, pricesFile.name);
    payload.append("inflows", inflowsFile, inflowsFile.name);

    const scenario = await apiRequest("/scenarios", {
      method: "POST",
      body: payload,
    });

    await loadScenarios({ selectedScenarioId: scenario.id });
    setScenarioMessage(
      `Saved ${scenario.name} with ${summary.rowCount.toLocaleString()} time steps. It is selected for optimization.`,
    );
    setOperationMessage(`Custom scenario ${scenario.name} is ready to run.`);
  } catch (error) {
    setScenarioMessage(error.message, true);
  } finally {
    elements.saveScenarioButton.disabled = false;
  }
}

function buildRunQuery() {
  const params = new URLSearchParams({
    limit: String(PAGE_SIZE),
    offset: String(state.offset),
  });
  if (elements.filterScenario.value) {
    params.set("scenario_id", elements.filterScenario.value);
  }
  if (elements.filterStatus.value) {
    params.set("status", elements.filterStatus.value);
  }
  return params.toString();
}

async function loadRuns({ selectFirst = false } = {}) {
  elements.refreshButton.disabled = true;
  try {
    const page = await apiRequest(`/runs?${buildRunQuery()}`);
    state.runs = page.items;
    state.total = page.total;
    state.offset = page.offset;
    renderRunTable();
    renderPagination(page);

    const selected = state.runs.find((run) => run.id === state.selectedRunId);
    const newestSucceeded = state.runs.find((run) => run.status === "succeeded");
    if (selected) {
      await renderRunDetails(selected);
    } else if ((selectFirst || state.selectedRunId === null) && state.runs.length > 0) {
      await renderRunDetails(newestSucceeded ?? state.runs[0]);
    } else if (state.runs.length === 0) {
      clearRunDetails();
    }
  } catch (error) {
    renderTableMessage(error.message);
    setOperationMessage(error.message, true);
  } finally {
    elements.refreshButton.disabled = false;
  }
}

function renderTableMessage(message) {
  const row = document.createElement("tr");
  const cell = document.createElement("td");
  cell.colSpan = 7;
  cell.className = "empty-cell";
  cell.textContent = message;
  row.append(cell);
  elements.runsBody.replaceChildren(row);
}

function renderRunTable() {
  if (state.runs.length === 0) {
    renderTableMessage("No runs match the current filters.");
    return;
  }

  const rows = state.runs.map((run) => {
    const row = document.createElement("tr");

    const idCell = document.createElement("td");
    const selectButton = document.createElement("button");
    selectButton.type = "button";
    selectButton.className = "run-button";
    selectButton.textContent = `#${run.id}`;
    selectButton.setAttribute("aria-label", `Inspect run ${run.id}`);
    selectButton.addEventListener("click", () => {
      void renderRunDetails(run);
    });
    idCell.append(selectButton);

    const scenarioCell = document.createElement("td");
    scenarioCell.textContent = run.scenario_name;

    const startedCell = document.createElement("td");
    startedCell.textContent = formatDate(run.started_at);

    const statusCell = document.createElement("td");
    statusCell.append(createStatusChip(run.status));

    const profitCell = document.createElement("td");
    profitCell.className = "numeric";
    profitCell.textContent = run.summary
      ? formatNumber(run.summary.cumulative_profit)
      : "—";

    const runtimeCell = document.createElement("td");
    runtimeCell.className = "numeric";
    runtimeCell.textContent = formatRuntime(run.summary);

    const artifactCell = document.createElement("td");
    if (run.status === "succeeded" && run.output_dispatch_path) {
      const link = document.createElement("a");
      link.className = "artifact-link";
      link.href = `${API_BASE}/runs/${run.id}/dispatch.csv`;
      link.textContent = "CSV";
      artifactCell.append(link);
    } else {
      artifactCell.textContent = "—";
    }

    row.append(
      idCell,
      scenarioCell,
      startedCell,
      statusCell,
      profitCell,
      runtimeCell,
      artifactCell,
    );
    return row;
  });

  elements.runsBody.replaceChildren(...rows);
}

function createStatusChip(status) {
  const chip = document.createElement("span");
  chip.className = "status-chip";
  if (ALLOWED_STATUSES.has(status)) {
    chip.classList.add(status);
  }
  chip.textContent = status;
  return chip;
}

function updateStatusChip(chip, status) {
  chip.className = "status-chip";
  if (ALLOWED_STATUSES.has(status)) {
    chip.classList.add(status);
  }
  chip.textContent = status;
}

function renderPagination(page) {
  const first = page.total === 0 ? 0 : page.offset + 1;
  const last = Math.min(page.offset + page.items.length, page.total);
  elements.pageSummary.textContent = `${first}–${last} of ${page.total} runs`;
  elements.previousButton.disabled = page.offset === 0;
  elements.nextButton.disabled = page.offset + page.limit >= page.total;
}

function clearDispatchCharts(message = "") {
  elements.dispatchCharts.replaceChildren();
  elements.dispatchCharts.hidden = true;
  elements.dispatchChartsMessage.textContent = message;
  elements.dispatchChartsMessage.classList.remove("error");
}

function clearTraderView(message = "") {
  elements.traderBody.replaceChildren();
  elements.traderMessage.textContent = message;
  elements.traderMessage.classList.remove("error");
  elements.traderCaption.textContent = "";
}

function clearRunDetails() {
  state.selectedRunId = null;
  state.traderRequestId += 1;
  elements.detailsEmpty.hidden = false;
  elements.detailsContent.hidden = true;
  renderProvenance(null);
  clearDispatchCharts();
  clearTraderView();
}

function renderTraderTable(rows) {
  const tableRows = rows.map((item) => {
    const row = document.createElement("tr");
    const values = [
      item.period,
      item.baseload.averageMw === null ? "—" : formatNumber(item.baseload.averageMw, 2),
      formatNumber(item.baseload.energyMwh, 2),
      formatNumber(item.baseload.pnl, 2),
      item.peak.averageMw === null ? "—" : formatNumber(item.peak.averageMw, 2),
      formatNumber(item.peak.energyMwh, 2),
      formatNumber(item.peak.pnl, 2),
      item.offPeak.averageMw === null ? "—" : formatNumber(item.offPeak.averageMw, 2),
      formatNumber(item.offPeak.energyMwh, 2),
      formatNumber(item.offPeak.pnl, 2),
    ];
    values.forEach((value, index) => {
      const cell = document.createElement("td");
      cell.textContent = value;
      if (index >= 1) {
        cell.className = "numeric";
      }
      row.append(cell);
    });
    return row;
  });
  elements.traderBody.replaceChildren(...tableRows);
}

async function renderRunDetails(run) {
  state.selectedRunId = run.id;
  elements.detailsEmpty.hidden = true;
  elements.detailsContent.hidden = false;
  elements.runTitle.textContent = `Run #${run.id} · ${run.scenario_name}`;
  updateStatusChip(elements.runStatus, run.status);
  elements.runStarted.textContent = formatDate(run.started_at);
  elements.runCompleted.textContent = formatDate(run.completed_at);

  elements.runError.hidden = !run.error_message;
  elements.runError.textContent = run.error_message ?? "";

  renderProvenance(run.provenance);
  renderSummary(run.summary);

  const canDownload = run.status === "succeeded" && Boolean(run.output_dispatch_path);
  elements.downloadLink.hidden = !canDownload;
  elements.downloadLink.href = canDownload
    ? `${API_BASE}/runs/${run.id}/dispatch.csv`
    : "#";

  const requestId = ++state.traderRequestId;
  if (!canDownload) {
    clearDispatchCharts("Dispatch charts are available only for succeeded runs with a dispatch artifact.");
    clearTraderView("Trader view is available only for succeeded runs with a dispatch artifact.");
    return;
  }

  const scenario = state.scenarios.find((item) => item.id === run.scenario_id);
  if (!Number.isFinite(Number(scenario?.time_step_hours))) {
    clearDispatchCharts("Dispatch charts unavailable: the scenario time step could not be read.");
    clearTraderView("Trader view unavailable: the scenario time step could not be read.");
    return;
  }

  clearDispatchCharts("Loading selected dispatch charts…");
  clearTraderView("Loading the selected dispatch…");
  try {
    const dispatchText = await apiRequest(`/runs/${run.id}/dispatch.csv`);
    const chartModel = buildDispatchChartModel(dispatchText, scenario.time_step_hours);
    const rows = buildTraderRows(dispatchText, scenario.time_step_hours);
    if (requestId !== state.traderRequestId) {
      return;
    }
    renderDispatchCharts(elements.dispatchCharts, chartModel);
    elements.dispatchCharts.hidden = false;
    elements.dispatchChartsMessage.textContent =
      `Times are shown in ${DISPATCH_CHART_TIME_ZONE}. Hydraulic inflow and controls are positive MW hydraulic magnitudes. Hover for interval values.`;
    renderTraderTable(rows);
    elements.traderMessage.textContent = "";
    elements.traderCaption.textContent =
      `Run #${run.id}. Baseload contains all intervals. Peak is Monday–Friday `
      + `${String(PEAK_START_HOUR).padStart(2, "0")}:00–`
      + `${String(PEAK_END_HOUR).padStart(2, "0")}:00 `
      + `in ${TRADER_TIME_ZONE}; the end hour is exclusive. `
      + "The first 12 calendar months are monthly, then results are quarterly.";
  } catch (error) {
    if (requestId !== state.traderRequestId) {
      return;
    }
    clearDispatchCharts(error.message);
    elements.dispatchChartsMessage.classList.add("error");
    clearTraderView(error.message);
    elements.traderMessage.classList.add("error");
  }
}

function renderProvenance(provenance) {
  const items = provenanceItems(provenance);
  elements.runProvenance.hidden = items.length === 0;
  if (items.length === 0) {
    elements.provenanceGrid.replaceChildren();
    return;
  }

  const rows = items.map((item) => {
    const wrapper = document.createElement("div");
    const term = document.createElement("dt");
    term.textContent = item.label;
    const description = document.createElement("dd");
    description.textContent = item.value;
    if (item.fullValue) {
      description.title = item.fullValue;
      description.setAttribute("aria-label", `${item.label}: ${item.fullValue}`);
      description.className = "hash-value";
    }
    wrapper.append(term, description);
    return wrapper;
  });
  elements.provenanceGrid.replaceChildren(...rows);
}

function renderSummary(summary) {
  if (!summary) {
    const card = createSummaryCard("Summary", "Not available");
    elements.summaryGrid.replaceChildren(card);
    return;
  }

  const metrics = [
    ["Cumulative profit [€]", formatNumber(summary.cumulative_profit)],
    ["Export energy [MWh]", formatNumber(summary.export_energy_mwh)],
    ["Import energy [MWh]", formatNumber(summary.import_energy_mwh)],
    ["Final storage content [MWh hydraulic]", formatNumber(summary.final_reservoir_volume)],
    ["Solve time [s]", formatNumber(summary.solve_seconds, 3)],
    ["Simulation time [s]", formatNumber(summary.simulation_seconds, 3)],
    ["Turbine steps [count]", formatNumber(summary.turbine_steps, 0)],
    ["Pump steps [count]", formatNumber(summary.pump_steps, 0)],
    ["Spill steps [count]", formatNumber(summary.spill_steps, 0)],
    ["Wait steps [count]", formatNumber(summary.wait_steps, 0)],
  ];

  elements.summaryGrid.replaceChildren(
    ...metrics.map(([label, value]) => createSummaryCard(label, value)),
  );
}

function createSummaryCard(label, value) {
  const card = document.createElement("div");
  card.className = "summary-card";
  const labelElement = document.createElement("span");
  labelElement.className = "summary-label";
  labelElement.textContent = label;
  const valueElement = document.createElement("span");
  valueElement.className = "summary-value";
  valueElement.textContent = value;
  card.append(labelElement, valueElement);
  return card;
}

async function createRun(event) {
  event.preventDefault();
  const scenarioId = Number(elements.scenarioSelect.value);
  if (!scenarioId) {
    return;
  }

  elements.runButton.disabled = true;
  setOperationMessage("Optimization is running. This request may take several minutes.");

  try {
    const run = await apiRequest("/runs", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ scenario_id: scenarioId }),
    });
    await renderRunDetails(run);
    state.offset = 0;
    await loadRuns();
    setOperationMessage(`Run #${run.id} completed with status ${run.status}.`);
  } catch (error) {
    setOperationMessage(error.message, true);
    await loadRuns();
  } finally {
    updateScenarioDescription();
  }
}

function resetAndLoadRuns() {
  state.offset = 0;
  clearRunDetails();
  loadRuns({ selectFirst: true });
}

renderScenarioFields();

const timeStepInput = elements.scenarioForm.elements.namedItem("time_step_hours");
timeStepInput?.addEventListener("change", () => updateSeriesPreview());

elements.scenarioForm.addEventListener("submit", createScenario);
elements.scenarioForm.addEventListener("reset", () => {
  setScenarioMessage("");
  window.setTimeout(() => updateSeriesPreview(), 0);
});
elements.pricesFile.addEventListener("change", () => updateSeriesPreview());
elements.inflowsFile.addEventListener("change", () => updateSeriesPreview());
elements.runForm.addEventListener("submit", createRun);
elements.scenarioSelect.addEventListener("change", updateScenarioDescription);
elements.refreshButton.addEventListener("click", () => loadRuns());
elements.filterScenario.addEventListener("change", resetAndLoadRuns);
elements.filterStatus.addEventListener("change", resetAndLoadRuns);
elements.previousButton.addEventListener("click", () => {
  state.offset = Math.max(0, state.offset - PAGE_SIZE);
  loadRuns();
});
elements.nextButton.addEventListener("click", () => {
  state.offset += PAGE_SIZE;
  loadRuns();
});

await loadScenarios();
await loadRuns({ selectFirst: true });
