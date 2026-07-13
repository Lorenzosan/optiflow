import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";

const appSource = readFileSync(new URL("./app.js", import.meta.url), "utf8");
const dockerfile = readFileSync(new URL("./Dockerfile", import.meta.url), "utf8");

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
