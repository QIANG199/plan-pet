const http = require("http");
const { execFile } = require("child_process");
const { Bonjour } = require("bonjour-service");
const env = require("./env");
const { fetchQuota } = require("./glm");
const { createPet } = require("./pet");

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

  if (!tokenOk(req, url)) {
    json(res, 401, { ok: false, error: "unauthorized" });
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
    if (!pet.EVENTS.has(body.event)) {
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

function advertiseMdns() {
  try {
    const bonjour = new Bonjour();
    bonjour.publish({
      name: "desktop-pet",
      host: "desktop-pet.local",
      type: "http",
      port: env.port,
      disableIPv6: true,
      txt: { path: "/api/dashboard" },
    });
    console.log("[mdns] advertised desktop-pet.local");
  } catch (err) {
    console.error("[mdns] skip", err.message);
  }
}

function tryFirewall() {
  if (process.platform !== "win32") return;
  execFile(
    "netsh",
    [
      "advfirewall",
      "firewall",
      "add",
      "rule",
      "name=desktop-pet-3737",
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
  advertiseMdns();
  await refreshGlm();
  setInterval(refreshGlm, 5 * 60 * 1000).unref();
});
