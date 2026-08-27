const DEFAULT_BASE = "https://api.z.ai";
const QUOTA_PATH = "/api/monitor/usage/quota/limit";
const { toUnixSec } = require("./lib/text");

/* GLM 计费高峰：北京时间每天 14:00–18:00 用量按 3 倍计。 */
const PEAK_START_HOUR = 14;
const PEAK_END_HOUR = 18;

function isPeakNow(now = Date.now()) {
  const bjHour = Math.floor((now / 1000 + 8 * 3600) % 86400 / 3600);
  return bjHour >= PEAK_START_HOUR && bjHour < PEAK_END_HOUR;
}

function monitorUrl(base) {
  return `${String(base || DEFAULT_BASE).replace(/\/+$/, "")}${QUOTA_PATH}`;
}

/* z.ai (global) takes the bare key; bigmodel (CN) gateways expect Bearer. */
function authHeader(apiKey, bearer) {
  return bearer ? `Bearer ${apiKey}` : apiKey;
}

function wantBearerRetry(status) {
  return status === 401 || status === 403;
}

function bar(limit) {
  if (!limit) return null;
  return {
    pct: Math.round(Number(limit.percentage) || 0),
    nextResetAt: toUnixSec(limit.nextResetTime),
  };
}

function pickWindows(limits) {
  const tokens = (limits || []).filter((l) => l && l.type === "TOKENS_LIMIT");
  const byReset = [...tokens].sort(
    (a, b) => (a.nextResetTime || 0) - (b.nextResetTime || 0)
  );
  const h5 =
    tokens.find((l) => l.unit === 3 && l.number === 5) || byReset[0] || null;
  let week = tokens.find((l) => l.unit === 6 && l.number === 7) || null;
  if (!week && byReset.length > 1) {
    week = byReset.find((l) => l !== h5) || null;
  }
  if (week === h5) week = null;
  return { h5, week };
}

function mapQuota(payload) {
  if (!payload || payload.success === false) {
    throw new Error(payload && payload.msg ? payload.msg : "glm request failed");
  }
  const data = payload.data;
  if (!data) throw new Error("glm response missing data");
  const { h5, week } = pickWindows(data.limits);
  if (!h5) throw new Error("glm response has no TOKENS_LIMIT");
  return {
    ok: true,
    level: data.level || null,
    h5: bar(h5),
    week: bar(week),
  };
}

async function callQuota(url, apiKey, bearer) {
  const res = await fetch(url, {
    headers: {
      Authorization: authHeader(apiKey, bearer),
      "Content-Type": "application/json",
    },
  });
  const text = await res.text();
  let json;
  try {
    json = JSON.parse(text);
  } catch {
    json = null;
  }
  if (!res.ok) {
    const msg = (json && json.msg) || text.slice(0, 120);
    const err = new Error(`glm HTTP ${res.status}: ${msg}`);
    err.status = res.status;
    throw err;
  }
  if (json == null) throw new Error(`glm HTTP ${res.status}: not JSON`);
  return mapQuota(json);
}

async function fetchQuota(apiKey, base) {
  const url = monitorUrl(base);
  try {
    return await callQuota(url, apiKey, false);
  } catch (err) {
    if (!wantBearerRetry(err.status)) throw err;
  }
  /* Bare-key rejection means the host wants Bearer; retry once. */
  return await callQuota(url, apiKey, true);
}

module.exports = { fetchQuota, mapQuota, pickWindows, monitorUrl, authHeader, wantBearerRetry, isPeakNow };
