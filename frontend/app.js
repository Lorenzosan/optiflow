/**
 * @file
 * @brief Browser orchestration for scenarios, optimization runs, and persisted results.
 *
 * This module owns the page-level state, communicates with the same-origin FastAPI
 * service through `/api`, and delegates numerical parsing and chart aggregation to
 * the smaller frontend modules.
 */

import {
  SCENARIO_FILE_LIMIT_BYTES,
  SCENARIO_PARAMETER_GROUPS,
  SERIES_FILE_LIMIT_BYTES,
  buildScenarioCsv,
  parseScenarioCsv,
  suggestScenarioCopyName,
  validateSeriesPair,
} from "./scenario.mjs";
import { formatNumber } from "./number_format.mjs";
import {
  buildDispatchChartModel,
  renderDispatchCharts,
} from "./dispatch_charts.mjs";
import { buildTraderCsv, buildTraderRows } from "./trader.mjs";

/**
 * @brief Same-origin prefix used for every backend request.
 */
const API_BASE = "/api";
/**
 * @brief Number of optimization runs requested per history page.
 */
const PAGE_SIZE = 10;
/**
 * @brief Run statuses that receive dedicated status-chip styling.
 */
const ALLOWED_STATUSES = new Set(["pending", "running", "succeeded", "failed"]);

/**
 * @brief Mutable browser state shared by event handlers and render functions.
 */
const state = {
  scenarios: [],
  runs: [],
  total: 0,
  offset: 0,
  selectedRunId: null,
  traderRequestId: 0,
  traderCsvUrl: null,
  editorSource: null,
};

/**
 * @brief Cached DOM elements used by the page controller.
 */
const elements = {
  runForm: document.querySelector("#run-form"),
  scenarioSelect: document.querySelector("#scenario-select"),
  scenarioDescription: document.querySelector("#scenario-description"),
  editScenarioButton: document.querySelector("#edit-scenario-button"),
  runButton: document.querySelector("#run-button"),
  operationMessage: document.querySelector("#operation-message"),
  scenarioEditor: document.querySelector("#scenario-editor"),
  scenarioEditorTitle: document.querySelector("#scenario-editor-title"),
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
  dispatchCharts: document.querySelector("#dispatch-charts"),
  dispatchChartsMessage: document.querySelector("#dispatch-charts-message"),
  traderMessage: document.querySelector("#trader-message"),
  traderBody: document.querySelector("#trader-body"),
  downloadLink: document.querySelector("#download-link"),
  traderDownloadLink: document.querySelector("#trader-download-link"),
};

/**
 * @brief HTTP error carrying the response status returned by FastAPI.
 */
class ApiError extends Error {
  /**
   * @brief Constructs an API error.
   * @param message Human-readable response error.
   * @param status HTTP status code.
   */
  constructor(message, status) {
    super(message);
    this.name = "ApiError";
    this.status = status;
  }
}

/**
 * @brief Sends a request to the OptiFlow API and parses its response.
 * @param path Path relative to the `/api` prefix.
 * @param options Optional Fetch API request options.
 * @return A promise resolving to the decoded JSON value or response text.
 * @throws ApiError when the response status is not successful.
 */
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

/**
 * @brief Normalizes text and FastAPI error payloads into one display message.
 * @param payload Decoded response body.
 * @return A user-facing error string.
 */
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

/**
 * @brief Updates the page-level operation status message.
 * @param message Message to display.
 * @param isError Whether to apply error styling.
 */
function setOperationMessage(message, isError = false) {
  elements.operationMessage.textContent = message;
  elements.operationMessage.classList.toggle("error", isError);
}

/**
 * @brief Updates the custom-scenario editor status message.
 * @param message Message to display.
 * @param isError Whether to apply error styling.
 */
function setScenarioMessage(message, isError = false) {
  elements.scenarioSaveMessage.textContent = message;
  elements.scenarioSaveMessage.classList.toggle("error", isError);
  elements.scenarioSaveMessage.classList.toggle("success", Boolean(message) && !isError);
}

/**
 * @brief Formats an API timestamp for the browser locale.
 * @param value ISO timestamp, with naive values interpreted as UTC.
 * @return A localized date-time string or an em dash when absent.
 */
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

/**
 * @brief Formats the combined solve and simulation duration.
 * @param summary Persisted run summary or null.
 * @return A duration in seconds or an em dash.
 */
function formatRuntime(summary) {
  if (!summary) {
    return "—";
  }
  return `${formatNumber(summary.solve_seconds + summary.simulation_seconds, 3)} s`;
}

/**
 * @brief Creates one option for a scenario or filter selector.
 * @param value Option value.
 * @param label Visible option label.
 * @param disabled Whether the option is unavailable.
 * @return The configured option element.
 */
function createOption(value, label, disabled = false) {
  const option = document.createElement("option");
  option.value = String(value);
  option.textContent = label;
  option.disabled = disabled;
  return option;
}

/**
 * @brief Rebuilds scenario selectors while preserving valid selections.
 * @param preferredScenarioId Scenario to select after a refresh, when supplied.
 */
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

/**
 * @brief Synchronizes scenario description and action-button availability.
 */
function updateScenarioDescription() {
  const selectedId = Number(elements.scenarioSelect.value);
  const scenario = state.scenarios.find((item) => item.id === selectedId);
  elements.scenarioDescription.textContent = scenario
    ? scenario.description
    : "Choose a bundled or custom scenario to run the optimizer.";
  elements.runButton.disabled = !scenario || !scenario.available;
  elements.editScenarioButton.disabled = !scenario || !scenario.available;
}

/**
 * @brief Loads scenario metadata and refreshes scenario controls.
 * @param selectedScenarioId Scenario to select after loading.
 * @return A promise that settles after the controls are updated.
 */
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

/**
 * @brief Builds one numeric editor control from a scenario field definition.
 * @param field Field metadata exported by `scenario.mjs`.
 * @return A wrapper containing the label, input, and optional help text.
 */
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

/**
 * @brief Builds all grouped scalar scenario controls.
 */
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

/**
 * @brief Checks a selected series file before reading its contents.
 * @param file Browser File object.
 * @param label Human-readable series name.
 * @throws Error when the extension or size is invalid.
 */
function validateSeriesFile(file, label) {
  if (!file.name.toLowerCase().endsWith(".csv")) {
    throw new Error(`${label} must be a .csv file.`);
  }
  if (file.size > SERIES_FILE_LIMIT_BYTES) {
    throw new Error(`${label} exceeds the 8 MiB upload limit.`);
  }
}

/**
 * @brief Reads a newly selected series or reuses the editor-loaded text.
 * @param input File input element.
 * @param label Human-readable series name.
 * @param sourceKey Key in the retained editor source.
 * @param filename Synthetic filename used when reusing retained text.
 * @return A promise resolving to the File, text, and retained-source flag.
 * @throws Error when neither source is available.
 */
async function readSeriesInput(input, label, sourceKey, filename) {
  const selected = input.files?.[0];
  if (selected) {
    validateSeriesFile(selected, label);
    return { file: selected, text: await selected.text(), loaded: false };
  }

  const loadedText = state.editorSource?.[sourceKey];
  if (loadedText !== null && loadedText !== undefined) {
    const file = new File([loadedText], filename, { type: "text/csv" });
    return { file, text: loadedText, loaded: true };
  }

  throw new Error(`${label} CSV is required.`);
}

/**
 * @brief Reads and jointly validates the selected price and inflow series.
 * @return A promise resolving to both files, their text, source flags, and validation summary.
 */
async function validateSelectedSeries() {
  const [prices, inflows] = await Promise.all([
    readSeriesInput(elements.pricesFile, "Prices", "pricesText", "prices.csv"),
    readSeriesInput(elements.inflowsFile, "Inflows", "inflowsText", "inflows.csv"),
  ]);
  const summary = validateSeriesPair(prices.text, inflows.text);
  return {
    pricesFile: prices.file,
    inflowsFile: inflows.file,
    pricesText: prices.text,
    inflowsText: inflows.text,
    loadedPrices: prices.loaded,
    loadedInflows: inflows.loaded,
    summary,
  };
}

/**
 * @brief Checks whether a series is available from a file input or retained source.
 * @param input File input element.
 * @param sourceKey Key in the retained editor source.
 * @return True when a series source is available.
 */
function hasSeriesInput(input, sourceKey) {
  return Boolean(input.files?.[0]) || state.editorSource?.[sourceKey] !== undefined;
}

/**
 * @brief Validates available series and renders their horizon summary.
 */
async function updateSeriesPreview() {
  if (!hasSeriesInput(elements.pricesFile, "pricesText")
      || !hasSeriesInput(elements.inflowsFile, "inflowsText")) {
    elements.seriesPreview.textContent = "Select both series files to validate their headers and horizon.";
    elements.seriesPreview.classList.remove("error", "success");
    return;
  }

  try {
    const { summary, loadedPrices, loadedInflows } = await validateSelectedSeries();
    const sourceNote = loadedPrices || loadedInflows
      ? ` Loaded values are retained unless replacement files are selected.`
      : "";
    elements.seriesPreview.textContent = `${summary.rowCount.toLocaleString()} matching timestamped steps detected; time step ${formatNumber(summary.timeStepHours, 6)} h.${sourceNote}`;
    elements.seriesPreview.classList.remove("error");
    elements.seriesPreview.classList.add("success");
  } catch (error) {
    elements.seriesPreview.textContent = error.message;
    elements.seriesPreview.classList.add("error");
    elements.seriesPreview.classList.remove("success");
  }
}

/**
 * @brief Writes parsed scalar scenario values into the editor inputs.
 * @param values Map of editor field keys to values.
 */
function setScenarioFieldValues(values) {
  for (const group of SCENARIO_PARAMETER_GROUPS) {
    for (const field of group.fields) {
      const input = elements.scenarioForm.elements.namedItem(field.key);
      if (input) {
        input.value = String(values[field.key]);
      }
    }
  }
}

/**
 * @brief Loads the selected scenario inputs and hydrates the custom editor.
 * @return A promise that settles after editor hydration or error display.
 */
async function openSelectedScenarioInEditor() {
  const scenarioId = Number(elements.scenarioSelect.value);
  const selectedScenario = state.scenarios.find((scenario) => scenario.id === scenarioId);
  if (!selectedScenario) {
    return;
  }

  elements.editScenarioButton.disabled = true;
  setOperationMessage(`Loading ${selectedScenario.name} into the editor.`);
  try {
    const inputs = await apiRequest(`/scenarios/${scenarioId}/inputs`);
    const parsed = parseScenarioCsv(inputs.scenario_csv);
    const seriesSummary = validateSeriesPair(inputs.prices_csv, inputs.inflows_csv);
    if (inputs.id !== scenarioId || inputs.name !== parsed.name) {
      throw new Error("Stored scenario metadata does not match its scenario CSV.");
    }
    if (Math.abs(parsed.timeStepHours - seriesSummary.timeStepHours) > 1e-9) {
      throw new Error("Stored scenario time step does not match its series timestamps.");
    }

    elements.scenarioForm.reset();
    // Retain uploaded text so subsequent edits do not require reselecting files.
    state.editorSource = {
      scenarioId: inputs.id,
      sourceName: inputs.name,
      editable: inputs.editable,
      pricesText: inputs.prices_csv,
      inflowsText: inputs.inflows_csv,
    };
    elements.scenarioName.value = inputs.editable
      ? parsed.name
      : suggestScenarioCopyName(parsed.name, state.scenarios.map((scenario) => scenario.name));
    elements.scenarioDescriptionInput.value = inputs.description;
    setScenarioFieldValues(parsed.values);
    elements.pricesFile.value = "";
    elements.inflowsFile.value = "";
    elements.scenarioEditorTitle.textContent = inputs.editable
      ? `Edit ${inputs.name}`
      : `Create from ${inputs.name}`;
    elements.scenarioEditor.open = true;
    await updateSeriesPreview();

    const mode = inputs.editable
      ? "Saving with the same name will ask before replacing it."
      : `Bundled inputs were opened as ${elements.scenarioName.value}.`;
    setScenarioMessage(`${inputs.name} loaded. ${mode}`);
    setOperationMessage(`${inputs.name} is open in the scenario editor.`);
    elements.scenarioEditor.scrollIntoView({ behavior: "smooth", block: "start" });
  } catch (error) {
    setOperationMessage(error.message, true);
  } finally {
    updateScenarioDescription();
  }
}

/**
 * @brief Validates and uploads a new or replacement custom scenario.
 * @param event Scenario form submission event.
 * @return A promise that settles after upload and UI refresh.
 */
async function createScenario(event) {
  event.preventDefault();
  setScenarioMessage("");
  elements.saveScenarioButton.disabled = true;

  try {
    const {
      pricesFile,
      inflowsFile,
      pricesText,
      inflowsText,
      summary,
    } = await validateSelectedSeries();
    const values = Object.fromEntries(new FormData(elements.scenarioForm).entries());
    const scenarioName = elements.scenarioName.value.trim();
    const scenarioCsv = buildScenarioCsv(scenarioName, values, summary.timeStepHours);
    const scenarioFile = new File([scenarioCsv], "scenario.csv", { type: "text/csv" });
    if (scenarioFile.size > SCENARIO_FILE_LIMIT_BYTES) {
      throw new Error("Generated scenario CSV exceeds the 256 KiB upload limit.");
    }

    const existingScenario = state.scenarios.find((scenario) => scenario.name === scenarioName);
    let replacing = false;
    if (existingScenario) {
      if (!existingScenario.editable) {
        throw new Error(`Bundled scenario ${scenarioName} is read-only. Choose another name.`);
      }
      replacing = window.confirm(
        `Replace ${scenarioName}? This deletes its previous runs and dispatch artifacts.`,
      );
      if (!replacing) {
        setScenarioMessage("Scenario replacement cancelled.");
        return;
      }
    }

    const payload = new FormData();
    payload.append("description", elements.scenarioDescriptionInput.value.trim());
    payload.append("scenario", scenarioFile);
    payload.append("prices", pricesFile, pricesFile.name);
    payload.append("inflows", inflowsFile, inflowsFile.name);
    payload.append("overwrite", String(replacing));

    const scenario = await apiRequest("/scenarios", {
      method: "POST",
      body: payload,
    });

    await loadScenarios({ selectedScenarioId: scenario.id });
    state.editorSource = {
      scenarioId: scenario.id,
      sourceName: scenario.name,
      editable: true,
      pricesText,
      inflowsText,
    };
    elements.pricesFile.value = "";
    elements.inflowsFile.value = "";
    elements.scenarioEditorTitle.textContent = `Edit ${scenario.name}`;
    await updateSeriesPreview();
    setScenarioMessage(
      `${replacing ? "Replaced" : "Saved"} ${scenario.name} with ${summary.rowCount.toLocaleString()} time steps at ${formatNumber(summary.timeStepHours, 6)} h. It is selected for optimization.`,
    );
    setOperationMessage(`Custom scenario ${scenario.name} is ready to run.`);
  } catch (error) {
    setScenarioMessage(error.message, true);
  } finally {
    elements.saveScenarioButton.disabled = false;
  }
}

/**
 * @brief Builds the bounded run-history query from pagination and filters.
 * @return A URL-encoded query string.
 */
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

/**
 * @brief Loads one page of run history and refreshes the selected result.
 * @param selectFirst Whether to select the newest successful run when possible.
 * @return A promise that settles after rendering.
 */
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

/**
 * @brief Replaces the run-history body with a single status row.
 * @param message Message shown across all table columns.
 */
function renderTableMessage(message) {
  const row = document.createElement("tr");
  const cell = document.createElement("td");
  cell.colSpan = 7;
  cell.className = "empty-cell";
  cell.textContent = message;
  row.append(cell);
  elements.runsBody.replaceChildren(row);
}

/**
 * @brief Renders the current run-history page.
 */
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

    const cashflowCell = document.createElement("td");
    cashflowCell.className = "numeric";
    cashflowCell.textContent = run.summary
      ? formatNumber(run.summary.net_operating_cashflow)
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
      cashflowCell,
      runtimeCell,
      artifactCell,
    );
    return row;
  });

  elements.runsBody.replaceChildren(...rows);
}

/**
 * @brief Creates a styled run-status element.
 * @param status Backend run status.
 * @return The configured status element.
 */
function createStatusChip(status) {
  const chip = document.createElement("span");
  chip.className = "status-chip";
  if (ALLOWED_STATUSES.has(status)) {
    chip.classList.add(status);
  }
  chip.textContent = status;
  return chip;
}

/**
 * @brief Updates an existing run-status element.
 * @param chip Status element to update.
 * @param status Backend run status.
 */
function updateStatusChip(chip, status) {
  chip.className = "status-chip";
  if (ALLOWED_STATUSES.has(status)) {
    chip.classList.add(status);
  }
  chip.textContent = status;
}

/**
 * @brief Updates run-history range text and pagination controls.
 * @param page Paginated API response.
 */
function renderPagination(page) {
  const first = page.total === 0 ? 0 : page.offset + 1;
  const last = Math.min(page.offset + page.items.length, page.total);
  elements.pageSummary.textContent = `${first}–${last} of ${page.total} runs`;
  elements.previousButton.disabled = page.offset === 0;
  elements.nextButton.disabled = page.offset + page.limit >= page.total;
}

/**
 * @brief Clears chart content and optionally shows a status message.
 * @param message Loading, unavailable, or error message.
 */
function clearDispatchCharts(message = "") {
  elements.dispatchCharts.replaceChildren();
  elements.dispatchCharts.hidden = true;
  elements.dispatchChartsMessage.textContent = message;
  elements.dispatchChartsMessage.classList.remove("error");
}

/**
 * @brief Revokes and hides the generated product-summary CSV download.
 */
function clearTraderCsvDownload() {
  if (state.traderCsvUrl !== null) {
    URL.revokeObjectURL(state.traderCsvUrl);
    state.traderCsvUrl = null;
  }
  elements.traderDownloadLink.hidden = true;
  elements.traderDownloadLink.href = "#";
  elements.traderDownloadLink.removeAttribute("download");
}

/**
 * @brief Clears trader aggregation rows and optionally shows a status message.
 * @param message Loading, unavailable, or error message.
 */
function clearTraderView(message = "") {
  clearTraderCsvDownload();
  elements.traderBody.replaceChildren();
  elements.traderMessage.textContent = message;
  elements.traderMessage.classList.remove("error");
}

/**
 * @brief Resets the selected-run panel and invalidates pending dispatch requests.
 */
function clearRunDetails() {
  state.selectedRunId = null;
  state.traderRequestId += 1;
  elements.detailsEmpty.hidden = false;
  elements.detailsContent.hidden = true;
  clearDispatchCharts();
  clearTraderView();
}

/**
 * @brief Renders period-level Baseload, Peak, and Off-peak aggregates.
 * @param rows Trader rows produced by `buildTraderRows`.
 */
function renderTraderTable(rows) {
  const tableRows = rows.map((item) => {
    const row = document.createElement("tr");
    const values = [
      item.period,
      item.baseload.averageMw === null ? "—" : formatNumber(item.baseload.averageMw, 2),
      formatNumber(item.baseload.energyMwh, 2),
      formatNumber(item.baseload.cashflow, 2),
      item.peak.averageMw === null ? "—" : formatNumber(item.peak.averageMw, 2),
      formatNumber(item.peak.energyMwh, 2),
      formatNumber(item.peak.cashflow, 2),
      item.offPeak.averageMw === null ? "—" : formatNumber(item.offPeak.averageMw, 2),
      formatNumber(item.offPeak.energyMwh, 2),
      formatNumber(item.offPeak.cashflow, 2),
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

/**
 * @brief Creates the downloadable long-format product-summary CSV.
 * @param rows Trader rows produced by `buildTraderRows`.
 * @param run Selected persisted run.
 */
function renderTraderCsvDownload(rows, run) {
  clearTraderCsvDownload();
  const csv = buildTraderCsv(rows, {
    runId: run.id,
    scenarioName: run.scenario_name,
  });
  // A UTF-8 byte-order mark improves non-ASCII scenario names in spreadsheet software.
  const blob = new Blob(["\uFEFF", csv], { type: "text/csv;charset=utf-8" });
  state.traderCsvUrl = URL.createObjectURL(blob);
  elements.traderDownloadLink.href = state.traderCsvUrl;
  elements.traderDownloadLink.download = `optiflow-run-${run.id}-product-summary.csv`;
  elements.traderDownloadLink.hidden = false;
}

/**
 * @brief Renders one run and loads its dispatch artifact when available.
 * @param run Run detail returned by the backend.
 * @return A promise that settles after charts and trader rows are rendered.
 */
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

  renderSummary(run.summary);

  const canDownload = run.status === "succeeded" && Boolean(run.output_dispatch_path);
  elements.downloadLink.hidden = !canDownload;
  elements.downloadLink.href = canDownload
    ? `${API_BASE}/runs/${run.id}/dispatch.csv`
    : "#";

  // Ignore dispatch responses that arrive after the user selects another run.
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
    elements.dispatchChartsMessage.textContent = "";
    renderTraderTable(rows);
    renderTraderCsvDownload(rows, run);
    elements.traderMessage.textContent = "";
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

/**
 * @brief Renders persisted optimization summary metrics.
 * @param summary Run summary or null.
 */
function renderSummary(summary) {
  if (!summary) {
    const card = createSummaryCard("Summary", "Not available");
    elements.summaryGrid.replaceChildren(card);
    return;
  }

  const metrics = [
    ["Net operating cashflow [€]", formatNumber(summary.net_operating_cashflow)],
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

/**
 * @brief Creates one metric card for the selected-run summary.
 * @param label Metric label including units.
 * @param value Formatted metric value.
 * @return The configured summary-card element.
 */
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

/**
 * @brief Submits an optimization run for the selected scenario.
 * @param event Run form submission event.
 * @return A promise that settles after the run and history views are refreshed.
 */
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

/**
 * @brief Resets pagination and reloads the first run-history page.
 */
function resetAndLoadRuns() {
  state.offset = 0;
  clearRunDetails();
  loadRuns({ selectFirst: true });
}

renderScenarioFields();
elements.scenarioForm.addEventListener("submit", createScenario);
elements.scenarioForm.addEventListener("reset", () => {
  state.editorSource = null;
  elements.scenarioEditorTitle.textContent = "Create a custom scenario";
  setScenarioMessage("");
  window.setTimeout(() => updateSeriesPreview(), 0);
});
elements.pricesFile.addEventListener("change", () => updateSeriesPreview());
elements.inflowsFile.addEventListener("change", () => updateSeriesPreview());
elements.runForm.addEventListener("submit", createRun);
elements.editScenarioButton.addEventListener("click", openSelectedScenarioInEditor);
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
