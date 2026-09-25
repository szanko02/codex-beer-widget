import { readFileSync } from 'node:fs';
import assert from 'node:assert/strict';
import test from 'node:test';
import Ajv from 'ajv/dist/2020.js';

const read = path => JSON.parse(readFileSync(new URL(path, import.meta.url), 'utf8'));
const validate = new Ajv({ strict: false }).compile(read('./quota-state-v1.json'));
const fixtures = read('./fixtures/normalization.json');
for (const fixture of fixtures) {
  test(fixture.name, () => assert.equal(validate(fixture.expected), true, JSON.stringify(validate.errors)));
}
for (const [name, mutate] of Object.entries({
  credentials: s => { s.token = 'must-not-cross-boundary'; },
  diagnostics: s => { s.groups.codex.error = 'private-path'; },
  percentage: s => { s.groups.codex.windows[0].remaining = 101; },
  duplicate: s => { s.groups.codex.windows[1].slot = 'primary'; },
  revision: s => { s.revision = 0; },
  timestamp: s => { s.sourceUpdatedAt = -1; },
  version: s => { s.version = 2; },
  missing: s => { delete s.groups.codex.windows[0].remaining; },
})) {
  test(`reject ${name}`, () => {
    const snapshot = structuredClone(fixtures[0].expected);
    mutate(snapshot);
    assert.equal(validate(snapshot), false);
  });
}
