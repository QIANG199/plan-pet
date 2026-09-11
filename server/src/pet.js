const { EVENT_SET } = require("./lib/events");

const HAPPY_MS = 4_000;
const SETTLE_MS = 10_000;
const SLEEP_MS = 15 * 60_000;

/* 三个代理各占一条 lane，互不干扰；数组顺序同时是桌宠动画的优先顺序——
 * 同时 busy 时跟 zcode（GLM），claude 插在中间（它是 clawd 的原生目标）。 */
const SOURCES = ["zcode", "claude", "cursor"];

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
  const lanes = Object.fromEntries(SOURCES.map((s) => [s, createLane()]));
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
    if (!EVENT_SET.has(event)) return false;
    if (!SOURCES.includes(source)) return false;

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
    const ordered = SOURCES.map((name) => ({ name, lane: lanes[name] }));
    const of = (it) => ({ agent: it.name, state: it.lane.state, since: it.lane.since });
    /* busy 优先，其次限时的 happy/error；同一档里按 SOURCES 的顺序定胜负 */
    for (const it of ordered) if (busy(it.lane.state)) return of(it);
    for (const it of ordered) if (live(it.lane.state)) return of(it);
    /* 都不活跃：跟最后收到事件的那条 lane（并列时靠前的赢，即 zcode） */
    const newest = ordered.reduce((a, b) =>
      b.lane.lastEventAt > a.lane.lastEventAt ? b : a
    );
    const agent = newest.lane.lastEventAt ? newest.name : null;
    return { agent, state: newest.lane.state, since: newest.lane.since };
  }

  function tick() {
    if (sleeping) return;
    if (Date.now() - lastEventAt >= SLEEP_MS) sleeping = true;
  }

  function snapshot() {
    const p = pick();
    const dots = Object.fromEntries(
      SOURCES.map((s) => [s, !sleeping && live(lanes[s].state)])
    );
    if (sleeping) return { agent: p.agent, state: "sleeping", since: p.since, dots };
    return { ...p, dots };
  }

  setInterval(tick, 30_000).unref();
  return { apply, snapshot };
}

/* OpenCode 运行时在场（进程扫描，无 hooks）叠到快照上：呼吸灯跟 hooks 绿点
 * 一样在 sleeping 时熄；动画只在 idle 上兜底成 typing——hooks 的 busy /
 * happy / error 不被打断，15 分钟睡眠计时也不由它重置（挂着 ≠ 活动）。 */
function withRuntime(snap, running) {
  const dots = { ...snap.dots, opencode: running && snap.state !== "sleeping" };
  if (running && snap.state === "idle") {
    return { ...snap, agent: "opencode", state: "typing", dots };
  }
  return { ...snap, dots };
}

module.exports = { createPet, withRuntime };
