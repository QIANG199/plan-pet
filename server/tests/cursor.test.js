const test = require("node:test");
const assert = require("node:assert");
const { mapSummary, isoToSec } = require("../src/cursor");

test("isoToSec accepts ISO strings and raw numbers", () => {
  assert.equal(isoToSec("2026-08-23T00:00:00Z"), Date.parse("2026-08-23T00:00:00Z") / 1000);
  assert.equal(isoToSec(1755900000), 1755900000);
  assert.equal(isoToSec(1755900000123), 1755900000);
  assert.equal(isoToSec(null), null);
  assert.equal(isoToSec("nonsense"), null);
});

test("mapSummary rounds plan percents and keeps the billing cycle", () => {
  const out = mapSummary({
    membershipType: "pro",
    billingCycleStart: "2026-08-01T00:00:00Z",
    billingCycleEnd: "2026-09-01T00:00:00Z",
    individualUsage: { plan: { autoPercentUsed: 12.4, apiPercentUsed: 80 } },
  });
  assert.equal(out.ok, true);
  assert.equal(out.level, "pro");
  assert.equal(out.auto.pct, 12);
  assert.equal(out.api.pct, 80);
  assert.equal(out.cycleEnd, Date.parse("2026-09-01T00:00:00Z") / 1000);
});

test("mapSummary throws when plan percents are missing", () => {
  assert.throws(() => mapSummary({ individualUsage: {} }), /plan percents/);
  assert.throws(() => mapSummary({}), /plan percents/);
});
