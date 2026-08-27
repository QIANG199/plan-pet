const test = require("node:test");
const assert = require("node:assert");
const {
  mapQuota,
  pickWindows,
  monitorUrl,
  authHeader,
  wantBearerRetry,
  isPeakNow,
} = require("../src/glm");

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

test("monitorUrl joins region base with the quota path", () => {
  assert.equal(monitorUrl(), "https://api.z.ai/api/monitor/usage/quota/limit");
  assert.equal(
    monitorUrl("https://open.bigmodel.cn"),
    "https://open.bigmodel.cn/api/monitor/usage/quota/limit"
  );
  assert.equal(monitorUrl("https://open.bigmodel.cn/"), "https://open.bigmodel.cn/api/monitor/usage/quota/limit");
});

test("authHeader sends bare key by default and Bearer on retry", () => {
  assert.equal(authHeader("k-123", false), "k-123");
  assert.equal(authHeader("k-123", true), "Bearer k-123");
});

test("wantBearerRetry triggers only on 401/403", () => {
  assert.equal(wantBearerRetry(401), true);
  assert.equal(wantBearerRetry(403), true);
  assert.equal(wantBearerRetry(404), false);
  assert.equal(wantBearerRetry(500), false);
});

test("isPeakNow marks Beijing 14:00-18:00 (no DST, UTC+8 is exact)", () => {
  const at = (utcH, utcM) => Date.UTC(2026, 7, 27, utcH, utcM, 0);
  assert.equal(isPeakNow(at(6, 0)), true); /* 北京 14:00 整 */
  assert.equal(isPeakNow(at(9, 59)), true); /* 北京 17:59 */
  assert.equal(isPeakNow(at(10, 0)), false); /* 北京 18:00 整，结束 */
  assert.equal(isPeakNow(at(5, 59)), false); /* 北京 13:59 */
  assert.equal(isPeakNow(at(16, 0)), false); /* 北京次日 0 点仍不在窗口 */
});
