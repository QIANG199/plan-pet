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
    let settled = false;
    function done(value) {
      if (settled) return;
      settled = true;
      resolve(value);
    }
    process.stdin.setEncoding("utf8");
    process.stdin.on("data", (c) => {
      chunks.push(c);
      try {
        done(JSON.parse(chunks.join("")));
      } catch {
        // wait for a complete JSON object; Cursor may keep stdin open
      }
    });
    process.stdin.on("end", () => {
      try {
        done(JSON.parse(chunks.join("") || "{}"));
      } catch {
        done({});
      }
    });
    process.stdin.on("error", () => done({}));
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
  const HARD_MS = 5000;
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

  let posted = false;
  async function emit(payload) {
    const mapped = mapPayload(payload);
    writeOut(stdoutLine(payload));
    if (mapped && !posted) {
      posted = true;
      await postEvent(mapped);
    }
  }

  // argv (Cursor) is enough to POST even when Windows delivers empty stdin.
  const early = emit({});

  const safety = setTimeout(() => writeOut(stdoutLine({})), SAFETY_MS);
  const hard = setTimeout(() => finish(stdoutLine({})), HARD_MS);

  readStdinJson()
    .then(async (payload) => {
      await early;
      await emit(payload);
      clearTimeout(safety);
      clearTimeout(hard);
      finish(null);
    })
    .catch(async () => {
      await early;
      clearTimeout(safety);
      clearTimeout(hard);
      finish(stdoutLine({}));
    });
}

module.exports = { ROOT, runHook, postEvent, loadEnv };
