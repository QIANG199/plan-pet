const http = require("http");
const fs = require("fs");
const path = require("path");
const { execFile } = require("child_process");
const env = require("./env");
const { fetchQuota, isPeakNow } = require("./glm");
const { fetchQuota: fetchCursorQuota } = require("./cursor");
const { fetchQuota: fetchOpenCodeQuota } = require("./opencode");
const { createRuntimeWatch } = require("./opencode-runtime");
const { createPet, withRuntime } = require("./pet");
const { otaInfo, otaBinPath, armPush, clearPush, pushState } = require("./ota");
const { panelRoundtrip, listCandidates } = require("./panel-link");
const { EVENT_SET } = require("./lib/events");

const pet = createPet();
const ocRuntime = createRuntimeWatch();

function petView() {
  return withRuntime(pet.snapshot(), ocRuntime.running());
}
const cache = {
  glm: { ok: false },
  cursor: { ok: false },
  opencodeGo: { ok: false },
};

function json(res, status, body) {
  const data = JSON.stringify(body);
  res.writeHead(status, {
    "Content-Type": "application/json; charset=utf-8",
    "Content-Length": Buffer.byteLength(data),
  });
  res.end(data);
}

function tokenOk(req, url) {
  const header = req.headers["x-panel-token"];
  if (header && header === env.panelToken) return true;
  // Local browser check only: ?token= 方便本机打开验收，局域网仍走请求头
  if (url.searchParams.get("token") === env.panelToken) {
    const ip = req.socket.remoteAddress || "";
    return ip === "127.0.0.1" || ip === "::1" || ip === "::ffff:127.0.0.1";
  }
  return false;
}

function dashboard() {
  return {
    ts: Math.floor(Date.now() / 1000),
    cursor: cache.cursor,
    /* peak 在响应时点算，不受 5 分钟刷新间隔影响，14/18 点边界即时翻转 */
    glm: { ...cache.glm, peak: isPeakNow() },
    opencodeGo: cache.opencodeGo,
    /* 指令推：非 null 时面板下一次轮询（约 1 秒）就开始下载固件 */
    ota: { forceAt: pushState() },
    pet: petView(),
  };
}

async function refreshGlm() {
  try {
    cache.glm = await fetchQuota(env.zaiKey, env.zaiApiBase);
    const week = cache.glm.week ? `${cache.glm.week.pct}%` : "none";
    console.log(`[glm] ok  5h=${cache.glm.h5.pct}%  7d=${week}`);
  } catch (err) {
    cache.glm = { ok: false };
    console.error("[glm] fail", err.message);
  }
}

async function refreshCursor() {
  try {
    cache.cursor = await fetchCursorQuota();
    console.log(
      `[cursor] ok  auto=${cache.cursor.auto.pct}%  api=${cache.cursor.api.pct}%`
    );
  } catch (err) {
    cache.cursor = { ok: false };
    console.error("[cursor] fail", err.message);
  }
}

async function refreshOpenCode() {
  if (!env.opencodeGoKey) {
    cache.opencodeGo = { ok: false };
    return;
  }
  try {
    cache.opencodeGo = await fetchOpenCodeQuota(env.opencodeGoKey);
    const week = cache.opencodeGo.week ? `${cache.opencodeGo.week.pct}%` : "none";
    const month = cache.opencodeGo.month ? `${cache.opencodeGo.month.pct}%` : "none";
    console.log(
      `[opencode] ok  5h=${cache.opencodeGo.h5 ? cache.opencodeGo.h5.pct : "none"}%  week=${week}  month=${month}`
    );
  } catch (err) {
    cache.opencodeGo = { ok: false };
    console.error("[opencode] fail", err.message);
  }
}

function readBody(req) {
  return new Promise((resolve, reject) => {
    const chunks = [];
    let n = 0;
    req.on("data", (c) => {
      n += c.length;
      if (n > 8192) {
        reject(new Error("body too large"));
        req.destroy();
        return;
      }
      chunks.push(c);
    });
    req.on("end", () => resolve(Buffer.concat(chunks).toString("utf8")));
    req.on("error", reject);
  });
}

const server = http.createServer(async (req, res) => {
  const url = new URL(req.url, "http://127.0.0.1");

  if (req.method === "GET" && url.pathname === "/health") {
    json(res, 200, { ok: true });
    return;
  }

  /* The setup page itself is public; its API calls carry the panel token. */
  if (req.method === "GET" && (url.pathname === "/setup" || url.pathname === "/setup.html")) {
    fs.readFile(path.join(__dirname, "..", "public", "setup.html"), (err, data) => {
      if (err) {
        json(res, 500, { ok: false, error: "setup page missing" });
        return;
      }
      res.writeHead(200, { "Content-Type": "text/html; charset=utf-8" });
      res.end(data);
    });
    return;
  }

  if (!tokenOk(req, url)) {
    json(res, 401, { ok: false, error: "unauthorized" });
    return;
  }

  if (req.method === "GET" && url.pathname === "/api/panel/ports") {
    try {
      json(res, 200, { ports: await listCandidates() });
    } catch (err) {
      json(res, 500, { ok: false, error: err.message });
    }
    return;
  }

  if (req.method === "POST" && url.pathname === "/api/panel/setup") {
    let body;
    try {
      body = JSON.parse((await readBody(req)) || "{}");
    } catch {
      json(res, 400, { ok: false, error: "invalid json" });
      return;
    }
    /* Order matters: token/host/port never reconnect, then PASS, and WIFI
     * last so the panel reconnects once with the complete credentials. */
    const commands = [];
    if (body.token) commands.push(`TOKEN ${body.token}`);
    if (body.host) commands.push(`HOST ${body.host}`);
    if (body.port) commands.push(`PORT ${body.port}`);
    if (body.pass) commands.push(`PASS ${body.pass}`);
    if (body.ssid) commands.push(`WIFI ${body.ssid}`);
    try {
      const replies = await panelRoundtrip({ com: body.com, commands });
      const config = {};
      for (const line of replies) {
        let m;
        if ((m = line.match(/^ssid=(.*)$/))) config.ssid = m[1];
        else if ((m = line.match(/^host=(.*):(\d+)$/))) {
          config.host = m[1];
          config.port = m[2];
        } else if ((m = line.match(/^token=\((set|empty)\)$/))) config.tokenSet = m[1] === "set";
        else if ((m = line.match(/^bright=(\d+)$/))) config.bright = m[1];
      }
      json(res, 200, { ok: true, replies, config });
    } catch (err) {
      console.error("[panel] setup failed", err.message);
      json(res, 500, { ok: false, error: err.message });
    }
    return;
  }

  if (req.method === "GET" && url.pathname === "/api/ota/info") {
    const info = otaInfo();
    /* 面板的自检（开机 +2min、每 6h、收到指令推）都打这；调推时看这一行即可
     * 确认设备真的来问了 */
    console.log(
      `[ota] info asked by ${req.socket.remoteAddress} -> ${info.ok ? "build " + info.build : "not built"}`
    );
    json(res, info.ok ? 200 : 404, info);
    return;
  }

  /* 指令推：置位后由快照带给面板（面板不在线/息屏时保持置位，等它下次醒来） */
  if (req.method === "POST" && url.pathname === "/api/ota/push") {
    const info = otaInfo();
    if (!info.ok) {
      json(res, 404, { ok: false, error: "firmware.bin not built" });
      return;
    }
    const forceAt = armPush();
    console.log(`[ota] push armed build=${info.build} size=${info.size}`);
    json(res, 200, { ok: true, forceAt, build: info.build, size: info.size });
    return;
  }

  if (req.method === "GET" && url.pathname === "/api/ota/bin") {
    const info = otaInfo();
    if (!info.ok) {
      json(res, 404, { ok: false, error: "firmware.bin not built" });
      return;
    }
    fs.readFile(otaBinPath(), (err, data) => {
      if (err) {
        json(res, 500, { ok: false, error: err.message });
        return;
      }
      console.log(`[ota] serve build=${info.build} ${data.length}B`);
      /* 取走即清零：一次置位只对应一次下载，面板重启后不会重复触发 */
      clearPush();
      res.writeHead(200, {
        "Content-Type": "application/octet-stream",
        "Content-Length": data.length,
      });
      res.end(data);
    });
    return;
  }

  if (req.method === "GET" && url.pathname === "/api/dashboard") {
    json(res, 200, dashboard());
    return;
  }

  if (req.method === "POST" && url.pathname === "/api/event") {
    let body;
    try {
      body = JSON.parse((await readBody(req)) || "{}");
    } catch {
      json(res, 400, { ok: false, error: "invalid json" });
      return;
    }
    if (!EVENT_SET.has(body.event)) {
      res.writeHead(204);
      res.end();
      return;
    }
    pet.apply(body);
    json(res, 200, { ok: true, pet: petView() });
    return;
  }

  json(res, 404, { ok: false, error: "not found" });
});

function tryFirewall() {
  if (process.platform !== "win32") return;
  execFile(
    "netsh",
    [
      "advfirewall",
      "firewall",
      "add",
      "rule",
      "name=plan-pet-" + String(env.port),
      "dir=in",
      "action=allow",
      "protocol=TCP",
      "localport=" + String(env.port),
    ],
    { windowsHide: true },
    (err) => {
      if (err) {
        console.log(
          `[fw] inbound TCP ${env.port} not added (need admin). Add it in Windows Firewall if the panel cannot connect.`
        );
      } else {
        console.log(`[fw] inbound TCP ${env.port} allowed`);
      }
    }
  );
}

server.on("error", (err) => {
  if (err.code === "EADDRINUSE") {
    console.error(
      `[listen] port ${env.port} is already in use. Close the other node process, then npm start again.`
    );
    process.exit(1);
  }
  throw err;
});

server.listen(env.port, env.host, () => {
  console.log(`[listen] http://${env.host}:${env.port}`);
  console.log("[token] X-Panel-Token is set (see .env). Do not commit .env.");
  tryFirewall();
  /* 三份额度互不依赖：一个来源慢/挂不能拖着另外两个的首拉 */
  refreshGlm();
  refreshCursor();
  if (!env.opencodeGoKey) {
    console.log(
      "[opencode] OPENCODE_GO_KEY unset in .env; the Open Code page will show fail"
    );
  } else {
    refreshOpenCode();
  }
  ocRuntime.start();
  setInterval(refreshGlm, 5 * 60 * 1000).unref();
  setInterval(refreshCursor, 5 * 60 * 1000).unref();
  setInterval(refreshOpenCode, 5 * 60 * 1000).unref();
});
