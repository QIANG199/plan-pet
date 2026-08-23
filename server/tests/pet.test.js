const test = require("node:test");
const assert = require("node:assert");
const { createPet } = require("../src/pet");

function lane(pet, agent) {
  return pet.snapshot().agent === agent ? pet.snapshot() : null;
}

test("events drive the lane through thinking -> typing -> happy", () => {
  const pet = createPet();
  assert.equal(pet.apply({ source: "zcode", event: "UserPromptSubmit" }), true);
  assert.equal(pet.snapshot().state, "thinking");

  assert.equal(pet.apply({ source: "zcode", event: "PostToolUse", tool: "Edit" }), true);
  assert.equal(pet.snapshot().state, "typing");

  assert.equal(pet.apply({ source: "zcode", event: "Stop" }), true);
  assert.equal(pet.snapshot().state, "happy");
  assert.equal(pet.snapshot().agent, "zcode");
});

test("PostToolUseFailure pins error, Stop with error status keeps it", () => {
  const pet = createPet();
  pet.apply({ source: "cursor", event: "PreToolUse" });
  pet.apply({ source: "cursor", event: "PostToolUseFailure" });
  assert.equal(pet.snapshot().state, "error");
  pet.apply({ source: "cursor", event: "Stop", status: "error" });
  assert.equal(pet.snapshot().state, "error");
});

test("unknown events and unknown sources are rejected", () => {
  const pet = createPet();
  assert.equal(pet.apply({ source: "zcode", event: "Whatever" }), false);
  assert.equal(pet.apply({ source: "vscode", event: "Stop" }), false);
  assert.equal(pet.snapshot().state, "idle");
});

test("busy lanes win arbitration over live lanes", () => {
  const pet = createPet();
  pet.apply({ source: "cursor", event: "Stop" }); /* cursor: happy */
  pet.apply({ source: "zcode", event: "PreToolUse" }); /* zcode: thinking */
  assert.equal(pet.snapshot().agent, "zcode");
  assert.equal(pet.snapshot().state, "thinking");
  /* dots follow the same priority */
  assert.deepEqual(pet.snapshot().dots, { zcode: true, cursor: true });
});

test("SessionStart refreshes liveness without changing state", () => {
  const pet = createPet();
  pet.apply({ source: "zcode", event: "Stop" });
  const before = pet.snapshot().state;
  pet.apply({ source: "zcode", event: "SessionStart" });
  assert.equal(pet.snapshot().state, before);
});

test("snapshot stays idle with agent null before any event", () => {
  const pet = createPet();
  const snap = pet.snapshot();
  assert.equal(snap.agent, null);
  assert.equal(snap.state, "idle");
  assert.deepEqual(snap.dots, { zcode: false, cursor: false });
});
