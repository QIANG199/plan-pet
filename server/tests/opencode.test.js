const test = require("node:test");
const assert = require("node:assert");
const { mapUsage, isoToSec, USAGE_URL } = require("../src/opencode");

/* 真实响应（2026-09-10 本机实测 opencode.ai/zen/go/v1/usage） */
function realBody() {
  return {
    usage: {
      rolling: { status: "ok", percent: 11, resetsAt: "2026-09-10T13:16:39.121Z" },
      weekly: { status: "ok", percent: 4, resetsAt: "2026-09-14T00:00:00.121Z" },
      monthly: { status: "ok", percent: 2, resetsAt: "2026-10-10T08:05:59.121Z" },
    },
  };
}

test("usage endpoint is the Go plan one", () => {
  assert.equal(USAGE_URL, "https://opencode.ai/zen/go/v1/usage");
});

test("mapUsage maps the three windows to h5/week/month", () => {
  const out = mapUsage(realBody());
  assert.equal(out.ok, true);
  assert.equal(out.h5.pct, 11);
  assert.equal(out.h5.nextResetAt, 1789046199);
  assert.equal(out.week.pct, 4);
  assert.equal(out.month.pct, 2);
});

test("rate-limited windows keep their 100% and reset time", () => {
  const body = realBody();
  body.usage.monthly = {
    status: "rate-limited",
    percent: 100,
    resetsAt: "2026-10-10T08:05:59.121Z",
  };
  const out = mapUsage(body);
  assert.equal(out.month.pct, 100);
  assert.ok(out.month.nextResetAt > 0);
});

test("zero percent drops the placeholder reset time", () => {
  /* percent==0 时上游 resetsAt 是 now+窗口时长的占位值，不能当真实重置时刻 */
  const body = realBody();
  body.usage.rolling = { status: "ok", percent: 0, resetsAt: "2026-09-10T18:00:00.000Z" };
  const out = mapUsage(body);
  assert.equal(out.h5.pct, 0);
  assert.equal(out.h5.nextResetAt, null);
});

test("a malformed window is skipped, the rest survive", () => {
  const body = realBody();
  body.usage.rolling = { status: "ok" };
  body.usage.monthly = null;
  const out = mapUsage(body);
  assert.equal(out.h5, null);
  assert.equal(out.month, null);
  assert.equal(out.week.pct, 4);
});

test("percent accepts string numbers", () => {
  const body = realBody();
  body.usage.weekly = { status: "ok", percent: "12", resetsAt: "2026-09-14T00:00:00.121Z" };
  assert.equal(mapUsage(body).week.pct, 12);
});

test("an unknown shape throws instead of rendering an empty page", () => {
  assert.throws(() => mapUsage({}), /missing usage/);
  assert.throws(() => mapUsage({ usage: {} }), /unexpected/);
});

test("isoToSec normalises ISO timestamps to unix seconds", () => {
  assert.equal(isoToSec("2026-09-10T13:16:39.121Z"), 1789046199);
  assert.equal(isoToSec(""), null);
  assert.equal(isoToSec("not a date"), null);
});
