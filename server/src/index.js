const http = require("http");
const fs = require("fs");
const path = require("path");
const { execFile } = require("child_process");
const env = require("./env");
const { fetchQuota } = require("./glm");
const { fetchQuota: fetchCursorQuota } = require("./cursor");
const { createPet } = require("./pet");
const { panelRoundtrip, listCandidates } = require("./panel-link");
const { EVENT_SET } = require("./lib/events");

const pet = createPet();
const cache = {
  glm: { ok: false },
  cursor: { ok: false },
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
    glm: cache.glm,
    pet: pet.snapshot(),
  };
}

async function refreshGlm() {
  try {
    cache.glm = await fetchQuota(env.zaiKey);
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
    json(res, 200, { ok: true, pet: pet.snapshot() });
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

server.listen(env.port, env.host, async () => {
  console.log(`[listen] http://${env.host}:${env.port}`);
  console.log("[token] X-Panel-Token is set (see .env). Do not commit .env.");
  tryFirewall();
  await refreshGlm();
  await refreshCursor();
  setInterval(refreshGlm, 5 * 60 * 1000).unref();
  setInterval(refreshCursor, 5 * 60 * 1000).unref();
});
