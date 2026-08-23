const fs = require("fs");
const os = require("os");
const path = require("path");
const { DatabaseSync } = require("node:sqlite");
const { stripQuotes, toUnixSec } = require("./lib/text");

const USAGE_URL = "https://cursor.com/api/usage-summary";

function vscdbPath() {
  if (process.platform === "win32") {
    return path.join(process.env.APPDATA || "", "Cursor", "User", "globalStorage", "state.vscdb");
  }
  if (process.platform === "darwin") {
    return path.join(os.homedir(), "Library", "Application Support", "Cursor", "User", "globalStorage", "state.vscdb");
  }
  return path.join(os.homedir(), ".config", "Cursor", "User", "globalStorage", "state.vscdb");
}

function readSession() {
  const src = vscdbPath();
  if (!fs.existsSync(src)) throw new Error("Cursor state.vscdb not found (sign in to Cursor)");
  const tmp = path.join(os.tmpdir(), "plan-pet-state.vscdb");
  fs.copyFileSync(src, tmp);
  const db = new DatabaseSync(tmp, { readOnly: true });
  try {
    const stmt = db.prepare("SELECT value FROM ItemTable WHERE key = ?");
    const idRow = stmt.get("adminSettings.cachedAuthId");
    const tokenRow = stmt.get("cursorAuth/accessToken");
    const cachedAuthId = stripQuotes(idRow && idRow.value);
    const accessToken = stripQuotes(tokenRow && tokenRow.value);
    if (!accessToken) throw new Error("cursorAuth/accessToken missing");
    if (!cachedAuthId) throw new Error("adminSettings.cachedAuthId missing");
    return { cachedAuthId, accessToken };
  } finally {
    db.close();
  }
}

function isoToSec(value) {
  if (value == null || value === "") return null;
  if (typeof value === "number") return toUnixSec(value);
  const t = Date.parse(String(value));
  if (!Number.isFinite(t)) return null;
  return Math.floor(t / 1000);
}

function mapSummary(payload) {
  const plan = payload && payload.individualUsage && payload.individualUsage.plan;
  if (!plan || plan.autoPercentUsed == null || plan.apiPercentUsed == null) {
    throw new Error("usage-summary missing plan percents");
  }
  return {
    ok: true,
    level: payload.membershipType || null,
    cycleStart: isoToSec(payload.billingCycleStart),
    cycleEnd: isoToSec(payload.billingCycleEnd),
    auto: { pct: Math.round(Number(plan.autoPercentUsed) || 0) },
    api: { pct: Math.round(Number(plan.apiPercentUsed) || 0) },
  };
}

async function fetchQuota() {
  const { cachedAuthId, accessToken } = readSession();
  const res = await fetch(USAGE_URL, {
    headers: {
      Cookie: `WorkosCursorSessionToken=${cachedAuthId}::${accessToken}`,
      Accept: "application/json",
      Origin: "https://cursor.com",
      Referer: "https://cursor.com/dashboard?tab=usage",
    },
  });
  const text = await res.text();
  let json;
  try {
    json = JSON.parse(text);
  } catch {
    throw new Error(`cursor HTTP ${res.status}: not JSON`);
  }
  if (!res.ok) {
    throw new Error(`cursor HTTP ${res.status}: ${json.error || text.slice(0, 80)}`);
  }
  return mapSummary(json);
}

module.exports = { fetchQuota, mapSummary, isoToSec, vscdbPath };
