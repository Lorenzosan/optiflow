import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";

const appSource = readFileSync(new URL("./app.js", import.meta.url), "utf8");
const dockerfile = readFileSync(new URL("./Dockerfile", import.meta.url), "utf8");
const indexHtml = readFileSync(new URL("./index.html", import.meta.url), "utf8");

function importedLocalModules(source) {
  return [...source.matchAll(/from\s+["']\.\/([^"']+)["']/g)].map((match) => match[1]);
}

test("frontend image includes every local module imported by app.js", () => {
  const modules = importedLocalModules(appSource);
  assert.ok(modules.length > 0, "app.js should import at least one local module");

  for (const moduleName of modules) {
    assert.match(
      dockerfile,
      new RegExp(`(?:^|\\s)frontend/${moduleName.replace(/[.*+?^${}()|[\\]\\]/g, "\\$&")}(?:\\s|$)`, "m"),
      `frontend/Dockerfile must copy frontend/${moduleName}`,
    );
  }
});


test("scenario overwrite uses confirmation without a checkbox", () => {
  assert.doesNotMatch(indexHtml, /overwrite-scenario/);
  assert.match(indexHtml, />Save scenario<\/button>/);
  assert.match(appSource, /window\.confirm\(/);
  assert.match(appSource, /payload\.append\("overwrite", String\(replacing\)\)/);
});


test("scenario description is optional", () => {
  assert.match(indexHtml, /<label for="scenario-description-input">Description \(optional\)<\/label>/);
  assert.match(indexHtml, /<textarea id="scenario-description-input" name="description" maxlength="2000"><\/textarea>/);
});


test("selected scenarios can be opened with retained CSV inputs", () => {
  assert.match(indexHtml, /id="edit-scenario-button"[^>]*>Open in editor<\/button>/);
  assert.doesNotMatch(indexHtml, /id="prices-file"[^>]*required/);
  assert.doesNotMatch(indexHtml, /id="inflows-file"[^>]*required/);
  assert.match(appSource, /\/scenarios\/\$\{scenarioId\}\/inputs/);
  assert.match(appSource, /state\.editorSource/);
  assert.match(appSource, /suggestScenarioCopyName/);
});


test("dispatch economics use cashflow terminology rather than P&L or MTM", () => {
  assert.doesNotMatch(indexHtml, /P&amp;L|Profit \[€\]/);
  assert.match(indexHtml, /Cashflow \[€\]/);
  assert.match(appSource, /not mark-to-market/);
  assert.match(appSource, /Net operating cashflow \[€\]/);
});
