const EVENTS = new Set([
  "SessionStart",
  "UserPromptSubmit",
  "PreToolUse",
  "PostToolUse",
  "PostToolUseFailure",
  "Stop",
]);

const HAPPY_MS = 4_000;
const SETTLE_MS = 10_000;
const SLEEP_MS = 15 * 60_000;

function nowSec() {
  return Math.floor(Date.now() / 1000);
}

function busy(state) {
  return state === "thinking" || state === "typing";
}

function live(state) {
  return busy(state) || state === "happy" || state === "error";
}

function createLane() {
  return {
    state: "idle",
    since: nowSec(),
    lastEventAt: 0,
    holdTimer: null,
  };
}

function createPet() {
  const lanes = { cursor: createLane(), zcode: createLane() };
  let lastEventAt = Date.now();
  let sleeping = false;

  function clearHold(lane) {
    if (lane.holdTimer) {
      clearTimeout(lane.holdTimer);
      lane.holdTimer = null;
    }
  }

  function setLane(lane, next) {
    clearHold(lane);
    lane.state = next;
    lane.since = nowSec();
  }

  function holdThenIdle(lane) {
    lane.holdTimer = setTimeout(() => {
      lane.holdTimer = null;
      if (lane.state === "happy" || lane.state === "error") {
        lane.state = "idle";
        lane.since = nowSec();
      }
    }, HAPPY_MS);
  }

  // Cursor may omit a final Stop, or fire Stop then more tools.
  // After the last PostToolUse, settle to happy if nothing else arrives.
  function settleTyping(lane) {
    lane.holdTimer = setTimeout(() => {
      lane.holdTimer = null;
      if (lane.state === "typing") {
        setLane(lane, "happy");
        holdThenIdle(lane);
      }
    }, SETTLE_MS);
  }

  function apply({ source, event, status }) {
    if (!EVENTS.has(event)) return false;
    if (source !== "cursor" && source !== "zcode") return false;

    lastEventAt = Date.now();
    sleeping = false;
    const lane = lanes[source];
    lane.lastEventAt = lastEventAt;

    if (event === "SessionStart") {
      return true;
    }

    if (event === "UserPromptSubmit" || event === "PreToolUse") {
      setLane(lane, "thinking");
      return true;
    }
    if (event === "PostToolUse") {
      // Cursor often flushes postToolUse after Stop; those must not pin typing.
      if (lane.state === "thinking" || lane.state === "typing") {
        setLane(lane, "typing");
        settleTyping(lane);
      }
      return true;
    }
    if (event === "PostToolUseFailure") {
      setLane(lane, "error");
      holdThenIdle(lane);
      return true;
    }
    if (event === "Stop") {
      if (status === "error") setLane(lane, "error");
      else setLane(lane, "happy");
      holdThenIdle(lane);
      return true;
    }
    return false;
  }

  function pick() {
    const z = lanes.zcode;
    const c = lanes.cursor;
    if (busy(z.state)) return { agent: "zcode", state: z.state, since: z.since };
    if (busy(c.state)) return { agent: "cursor", state: c.state, since: c.since };
    if (live(z.state)) return { agent: "zcode", state: z.state, since: z.since };
    if (live(c.state)) return { agent: "cursor", state: c.state, since: c.since };
    const zNewer = z.lastEventAt >= c.lastEventAt;
    const latest = zNewer ? z : c;
    const agent = z.lastEventAt || c.lastEventAt ? (zNewer ? "zcode" : "cursor") : null;
    return { agent, state: latest.state, since: latest.since };
  }

  function tick() {
    if (sleeping) return;
    if (Date.now() - lastEventAt >= SLEEP_MS) sleeping = true;
  }

  function snapshot() {
    const p = pick();
    const dots = {
      zcode: !sleeping && live(lanes.zcode.state),
      cursor: !sleeping && live(lanes.cursor.state),
    };
    if (sleeping) return { agent: p.agent, state: "sleeping", since: p.since, dots };
    return { ...p, dots };
  }

  setInterval(tick, 30_000).unref();
  return { apply, snapshot, EVENTS };
}

module.exports = { createPet, EVENTS };
