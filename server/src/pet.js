const EVENTS = new Set([
  "SessionStart",
  "UserPromptSubmit",
  "PreToolUse",
  "PostToolUse",
  "PostToolUseFailure",
  "Stop",
]);

const HAPPY_MS = 60_000;
const SLEEP_MS = 15 * 60_000;

function nowSec() {
  return Math.floor(Date.now() / 1000);
}

function createPet() {
  const state = {
    agent: null,
    state: "idle",
    since: nowSec(),
    lastEventAt: Date.now(),
  };
  let holdTimer = null;

  function setState(next, agent) {
    if (holdTimer) {
      clearTimeout(holdTimer);
      holdTimer = null;
    }
    state.state = next;
    if (agent !== undefined) state.agent = agent;
    state.since = nowSec();
  }

  function holdThenIdle() {
    holdTimer = setTimeout(() => {
      holdTimer = null;
      if (state.state === "happy" || state.state === "error") {
        state.state = "idle";
        state.since = nowSec();
      }
    }, HAPPY_MS);
  }

  function apply({ source, event, status }) {
    if (!EVENTS.has(event)) return false;
    if (source !== "cursor" && source !== "zcode") return false;

    state.lastEventAt = Date.now();

    if (event === "SessionStart") {
      if (state.state === "sleeping") setState("idle", source);
      return true;
    }

    if (event === "UserPromptSubmit" || event === "PreToolUse") {
      setState("thinking", source);
      return true;
    }
    if (event === "PostToolUse") {
      setState("typing", source);
      return true;
    }
    if (event === "PostToolUseFailure") {
      setState("error", source);
      holdThenIdle();
      return true;
    }
    if (event === "Stop") {
      if (status === "error") setState("error", source);
      else setState("happy", source);
      holdThenIdle();
      return true;
    }
    return false;
  }

  function tick() {
    if (state.state === "sleeping") return;
    if (Date.now() - state.lastEventAt >= SLEEP_MS) {
      setState("sleeping", state.agent);
    }
  }

  function snapshot() {
    return { agent: state.agent, state: state.state, since: state.since };
  }

  setInterval(tick, 30_000).unref();
  return { apply, snapshot, EVENTS };
}

module.exports = { createPet, EVENTS };
