const API_BASE = "/api";
const PAGE_SIZE = 10;
const ALLOWED_STATUSES = new Set(["pending", "running", "succeeded", "failed"]);

const state = {
  scenarios: [],
  runs: [],
  total: 0,
  offset: 0,
  selectedRunId: null,
};

const elements = {
  runForm: document.querySelector("#run-form"),
  scenarioSelect: document.querySelector("#scenario-select"),
  scenarioDescription: document.querySelector("#scenario-description"),
  runButton: document.querySelector("#run-button"),
  operationMessage: document.querySelector("#operation-message"),
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
  if (payload?.detail?.message) {
    const runSuffix = payload.detail.run_id ? ` (run ${payload.detail.run_id})` : "";
    return `${payload.detail.message}${runSuffix}`;
  }
  return "The API request failed.";
}

function setOperationMessage(message, isError = false) {
  elements.operationMessage.textContent = message;
  elements.operationMessage.classList.toggle("error", isError);
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

function renderScenarioControls() {
  elements.scenarioSelect.replaceChildren(createOption("", "Select a scenario"));
  elements.filterScenario.replaceChildren(createOption("", "All scenarios"));

  for (const scenario of state.scenarios) {
    const suffix = scenario.available ? "" : " — files unavailable";
    elements.scenarioSelect.append(
      createOption(scenario.id, `${scenario.name}${suffix}`, !scenario.available),
    );
    elements.filterScenario.append(createOption(scenario.id, scenario.name));
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
    : "Choose one of the seeded scenarios to run the optimizer.";
  elements.runButton.disabled = !scenario || !scenario.available;
}

async function loadScenarios() {
  try {
    state.scenarios = await apiRequest("/scenarios");
    renderScenarioControls();
  } catch (error) {
    elements.scenarioSelect.replaceChildren(createOption("", "Scenarios unavailable"));
    elements.scenarioSelect.disabled = true;
    elements.runButton.disabled = true;
    setOperationMessage(error.message, true);
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
    if (selected) {
      renderRunDetails(selected);
    } else if (selectFirst && state.runs.length > 0) {
      renderRunDetails(state.runs[0]);
    } else if (state.selectedRunId === null && state.runs.length > 0) {
      renderRunDetails(state.runs[0]);
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
    selectButton.addEventListener("click", () => renderRunDetails(run));
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

function clearRunDetails() {
  state.selectedRunId = null;
  elements.detailsEmpty.hidden = false;
  elements.detailsContent.hidden = true;
}

function renderRunDetails(run) {
  state.selectedRunId = run.id;
  elements.detailsEmpty.hidden = true;
  elements.detailsContent.hidden = false;
  elements.runTitle.textContent = `Run #${run.id} · ${run.scenario_name}`;
  updateStatusChip(elements.runStatus, run.status);
  elements.runStarted.textContent = formatDate(run.started_at);
  elements.runCompleted.textContent = formatDate(run.completed_at);

  elements.runError.hidden = !run.error_message;
  elements.runError.textContent = run.error_message ?? "";

  renderSummary(run.summary);

  const canDownload = run.status === "succeeded" && Boolean(run.output_dispatch_path);
  elements.downloadLink.hidden = !canDownload;
  elements.downloadLink.href = canDownload
    ? `${API_BASE}/runs/${run.id}/dispatch.csv`
    : "#";
}

function renderSummary(summary) {
  if (!summary) {
    const card = createSummaryCard("Summary", "Not available");
    elements.summaryGrid.replaceChildren(card);
    return;
  }

  const metrics = [
    ["Cumulative profit", formatNumber(summary.cumulative_profit)],
    ["Export energy", `${formatNumber(summary.export_energy_mwh)} MWh`],
    ["Import energy", `${formatNumber(summary.import_energy_mwh)} MWh`],
    ["Final reservoir", formatNumber(summary.final_reservoir_volume)],
    ["Final battery SOC", formatNumber(summary.final_battery_soc)],
    ["Solve time", `${formatNumber(summary.solve_seconds, 3)} s`],
    ["Simulation time", `${formatNumber(summary.simulation_seconds, 3)} s`],
    ["Turbine steps", formatNumber(summary.turbine_steps, 0)],
    ["Pump steps", formatNumber(summary.pump_steps, 0)],
    ["Spill steps", formatNumber(summary.spill_steps, 0)],
    ["Battery charge steps", formatNumber(summary.battery_charge_steps, 0)],
    ["Battery discharge steps", formatNumber(summary.battery_discharge_steps, 0)],
    ["Wait steps", formatNumber(summary.wait_steps, 0)],
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
    renderRunDetails(run);
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

await Promise.all([loadScenarios(), loadRuns({ selectFirst: true })]);
