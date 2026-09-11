const test = require("node:test");
const assert = require("node:assert");
const { createPet, withRuntime } = require("../src/pet");

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
  assert.deepEqual(pet.snapshot().dots, { zcode: true, claude: false, cursor: true });
});

test("claude code events drive their own lane", () => {
  const pet = createPet();
  assert.equal(pet.apply({ source: "claude", event: "UserPromptSubmit" }), true);
  assert.equal(pet.snapshot().agent, "claude");
  assert.equal(pet.snapshot().state, "thinking");
  assert.equal(pet.apply({ source: "claude", event: "PostToolUse", tool: "Bash" }), true);
  assert.equal(pet.snapshot().state, "typing");
  assert.equal(pet.apply({ source: "claude", event: "Stop" }), true);
  assert.equal(pet.snapshot().state, "happy");
  assert.deepEqual(pet.snapshot().dots, { zcode: false, claude: true, cursor: false });
});

test("busy arbitration runs zcode, then claude, then cursor", () => {
  const pet = createPet();
  pet.apply({ source: "cursor", event: "PreToolUse" });
  pet.apply({ source: "claude", event: "PreToolUse" });
  assert.equal(pet.snapshot().agent, "claude"); /* claude outranks cursor */
  pet.apply({ source: "zcode", event: "PreToolUse" });
  assert.equal(pet.snapshot().agent, "zcode"); /* zcode outranks claude */
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
  assert.deepEqual(snap.dots, { zcode: false, claude: false, cursor: false });
});

/* opencode 运行时在场：只兜底 idle，不碰 hooks 状态与睡眠 */
test("opencode runtime overlays typing on idle and lights its dot", () => {
  const pet = createPet();
  const v = withRuntime(pet.snapshot(), true);
  assert.equal(v.agent, "opencode");
  assert.equal(v.state, "typing");
  assert.deepEqual(v.dots, {
    zcode: false,
    claude: false,
    cursor: false,
    opencode: true,
  });
});

test("opencode runtime keeps hook states and sleeps alone", () => {
  const pet = createPet();
  pet.apply({ source: "zcode", event: "UserPromptSubmit" });
  let v = withRuntime(pet.snapshot(), true);
  assert.equal(v.state, "thinking"); /* busy 不被打断 */
  assert.equal(v.dots.opencode, true);

  const sleeping = { agent: null, state: "sleeping", since: 0, dots: { zcode: false, claude: false, cursor: false } };
  v = withRuntime(sleeping, true);
  assert.equal(v.state, "sleeping"); /* 桌宠睡了就不兜底 */
  assert.equal(v.dots.opencode, false); /* 呼吸灯随睡眠熄灭 */

  const off = withRuntime(pet.snapshot(), false);
  assert.equal(off.state, "thinking");
  assert.equal(off.dots.opencode, false);
});
