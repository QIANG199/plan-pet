const test = require("node:test");
const assert = require("node:assert");
const { mapQuota, pickWindows } = require("../src/glm");

function limit(type, unit, number, resetMs, percentage) {
  return { type, unit, number, nextResetTime: resetMs, percentage };
}

test("pickWindows selects the 5h and 7d TOKENS_LIMIT windows", () => {
  const limits = [
    limit("TOKENS_LIMIT", 6, 7, 200, 44),
    limit("TOKENS_LIMIT", 3, 5, 100, 12),
    limit("TIME_LIMIT", 1, 1, 999, 80), /* monthly MCP: never picked */
  ];
  const { h5, week } = pickWindows(limits);
  assert.equal(h5.unit, 3);
  assert.equal(h5.number, 5);
  assert.equal(week.unit, 6);
  assert.equal(week.number, 7);
});

test("pickWindows falls back by reset order when units are missing", () => {
  const limits = [
    limit("TOKENS_LIMIT", 0, 0, 300, 10),
    limit("TOKENS_LIMIT", 0, 0, 100, 20),
  ];
  const { h5, week } = pickWindows(limits);
  assert.equal(h5.nextResetTime, 100); /* earliest reset is the h5 fallback */
  assert.equal(week.nextResetTime, 300);
});

test("pickWindows ignores non-TOKENS_LIMIT entries entirely", () => {
  const { h5, week } = pickWindows([limit("TIME_LIMIT", 3, 5, 100, 12)]);
  assert.equal(h5, null);
  assert.equal(week, null);
});

test("mapQuota maps percentages and reset times to unix seconds", () => {
  const out = mapQuota({
    success: true,
    data: {
      level: "team",
      limits: [
        limit("TOKENS_LIMIT", 3, 5, 1755900000123, 12.4),
        limit("TOKENS_LIMIT", 6, 7, 1756800000123, 44.5),
      ],
    },
  });
  assert.equal(out.ok, true);
  assert.equal(out.level, "team");
  assert.equal(out.h5.pct, 12);
  assert.equal(out.h5.nextResetAt, 1755900000);
  assert.equal(out.week.pct, 45);
  assert.equal(out.week.nextResetAt, 1756800000);
});

test("mapQuota throws on failure payloads and missing windows", () => {
  assert.throws(() => mapQuota({ success: false, msg: "bad key" }), /bad key/);
  assert.throws(() => mapQuota({ success: true, data: { limits: [] } }), /TOKENS_LIMIT/);
  assert.throws(() => mapQuota(null), /glm request failed/);
});
