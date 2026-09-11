const { toUnixSec } = require("./lib/text");

/* OpenCode Go（opencode.ai/zen/go）的用量端点：三个美元额度窗口
 * rolling($12/5h) / weekly($30/周) / monthly($60/月)，响应只给百分比。
 * 未文档化的一手端点，形态参考 cc-switch 的 opencode_go 探测逻辑。 */
const USAGE_URL = "https://opencode.ai/zen/go/v1/usage";

/* 上游窗口键 -> 快照键（面板 `5h` / `week` / `month` 三行） */
const WINDOWS = [
  ["rolling", "h5"],
  ["weekly", "week"],
  ["monthly", "month"],
];

function isoToSec(value) {
  if (value == null || value === "") return null;
  const t = Date.parse(String(value));
  if (!Number.isFinite(t)) return null;
  return toUnixSec(t);
}

function bar(win) {
  if (!win || win.percent == null) return null;
  const pct = Number(win.percent);
  if (!Number.isFinite(pct)) return null;
  return {
    pct: Math.round(pct),
    /* 已用 0% 时上游的 resetsAt 是「now+窗口时长」的占位值（整窗早已清零），
     * 当作真实重置时刻会显示假倒计时，丢弃。*/
    nextResetAt: pct > 0 ? isoToSec(win.resetsAt) : null,
  };
}

function mapUsage(payload) {
  const usage = payload && payload.usage;
  if (!usage) throw new Error("opencode usage response missing usage");
  const out = { ok: true };
  let found = false;
  for (const [key, name] of WINDOWS) {
    out[name] = bar(usage[key]);
    if (out[name]) found = true;
  }
  /* 三个窗口一个都没解析出来 = 响应形态不认识（端点未文档化，会变形） */
  if (!found) throw new Error("unexpected opencode usage response shape");
  return out;
}

async function fetchQuota(apiKey) {
  const res = await fetch(USAGE_URL, {
    headers: {
      /* 用量端点只认 Bearer；与推理侧 /messages 的 x-api-key 不能互换 */
      Authorization: `Bearer ${apiKey}`,
      Accept: "application/json",
    },
  });
  const text = await res.text();
  let json;
  try {
    json = JSON.parse(text);
  } catch {
    json = null;
  }
  if (res.status === 403) {
    /* EntitlementError：key 本身有效（Zen 与 Go 共用一把 workspace key），
     * 但该 workspace 没有 Go 订阅——与 401 认证失败分开说。*/
    throw new Error("opencode HTTP 403: key has no OpenCode Go subscription");
  }
  if (!res.ok) {
    const msg = (json && json.error && json.error.message) || text.slice(0, 120);
    throw new Error(`opencode HTTP ${res.status}: ${msg}`);
  }
  if (json == null) throw new Error(`opencode HTTP ${res.status}: not JSON`);
  return mapUsage(json);
}

module.exports = { fetchQuota, mapUsage, isoToSec, USAGE_URL };
