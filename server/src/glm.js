const GLM_URL = "https://api.z.ai/api/monitor/usage/quota/limit";

function toUnixSec(value) {
  if (value == null || value === "") return null;
  const n = Number(value);
  if (!Number.isFinite(n) || n <= 0) return null;
  return n > 1e12 ? Math.floor(n / 1000) : Math.floor(n);
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

async function fetchQuota(apiKey) {
  const res = await fetch(GLM_URL, {
    headers: {
      Authorization: apiKey,
      "Content-Type": "application/json",
    },
  });
  const text = await res.text();
  let json;
  try {
    json = JSON.parse(text);
  } catch {
    throw new Error(`glm HTTP ${res.status}: not JSON`);
  }
  if (!res.ok) {
    throw new Error(`glm HTTP ${res.status}: ${json.msg || text.slice(0, 120)}`);
  }
  return mapQuota(json);
}

module.exports = { fetchQuota, mapQuota, pickWindows, toUnixSec };
