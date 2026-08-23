const fs = require("fs");
const http = require("http");
const path = require("path");

const ROOT = path.join(__dirname, "..");

function loadEnv() {
  const file = path.join(ROOT, ".env");
  if (!fs.existsSync(file)) return;
  for (const raw of fs.readFileSync(file, "utf8").split(/\r?\n/)) {
    const line = raw.trim();
    if (!line || line.startsWith("#")) continue;
    const eq = line.indexOf("=");
    if (eq < 1) continue;
    const key = line.slice(0, eq).trim();
    let value = line.slice(eq + 1).trim();
    if (
      (value.startsWith('"') && value.endsWith('"')) ||
      (value.startsWith("'") && value.endsWith("'"))
    ) {
      value = value.slice(1, -1);
    }
    if (process.env[key] == null) process.env[key] = value;
  }
}

function readStdinJson() {
  return new Promise((resolve) => {
    const chunks = [];
    process.stdin.setEncoding("utf8");
    process.stdin.on("data", (c) => chunks.push(c));
    process.stdin.on("end", () => {
      try {
        resolve(JSON.parse(chunks.join("") || "{}"));
      } catch {
        resolve({});
      }
    });
    process.stdin.on("error", () => resolve({}));
  });
}

function postEvent(body) {
  loadEnv();
  const token = process.env.PANEL_TOKEN;
  if (!token) return Promise.resolve();
  const port = Number(process.env.PORT || 3737);
  const payload = JSON.stringify({
    ts: Math.floor(Date.now() / 1000),
    ...body,
  });
  return new Promise((resolve) => {
    const req = http.request(
      {
        hostname: "127.0.0.1",
        port,
        path: "/api/event",
        method: "POST",
        headers: {
          "Content-Type": "application/json",
          "Content-Length": Buffer.byteLength(payload),
          "X-Panel-Token": token,
        },
        timeout: 400,
      },
      (res) => {
        res.resume();
        res.on("end", resolve);
      }
    );
    req.on("timeout", () => {
      req.destroy();
      resolve();
    });
    req.on("error", () => resolve());
    req.end(payload);
  });
}

function runHook({ stdoutLine, mapPayload }) {
  const SAFETY_MS = 800;
  let wrote = false;
  let exited = false;

  function writeOut(line) {
    if (wrote) return;
    wrote = true;
    if (line != null && line !== "") process.stdout.write(line + "\n");
  }

  function finish(line) {
    writeOut(line);
    if (exited) return;
    exited = true;
    process.exit(0);
  }

  const safety = setTimeout(() => finish(stdoutLine({})), SAFETY_MS);

  readStdinJson()
    .then(async (payload) => {
      const mapped = mapPayload(payload);
      writeOut(stdoutLine(payload));
      if (mapped) await postEvent(mapped);
      clearTimeout(safety);
      finish(null);
    })
    .catch(() => {
      clearTimeout(safety);
      finish(stdoutLine({}));
    });
}

module.exports = { ROOT, runHook, postEvent, loadEnv };
