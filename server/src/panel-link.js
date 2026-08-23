/* USB serial bridge to the panel over the firmware's line protocol
 * (WIFI/PASS/TOKEN/HOST/PORT/SHOW). The port opens on demand and closes
 * after an idle window so `pio device monitor` can use it in between. */

const { SerialPort } = require("serialport");

/* Espressif native USB, SiLabs CP210x, WCH CH34x, FTDI. */
const CANDIDATE_VIDS = [0x303a, 0x10c4, 0x1a86, 0x0403];
const IDLE_CLOSE_MS = 20000;

let port = null;
let portPath = null;
let idleTimer = null;
let rxBuf = "";
let rxLines = [];
let chain = Promise.resolve();

function vid(p) {
  return p.vendorId ? parseInt(p.vendorId, 16) : null;
}

async function listCandidates() {
  const ports = await SerialPort.list();
  return ports
    .filter((p) => CANDIDATE_VIDS.includes(vid(p)))
    .map((p) => ({ path: p.path, id: `${p.vendorId || ""}:${p.productId || ""}` }));
}

function armIdleTimer() {
  if (idleTimer) clearTimeout(idleTimer);
  idleTimer = setTimeout(() => {
    if (port) {
      try {
        port.close();
      } catch {
        /* already closed */
      }
    }
    port = null;
    portPath = null;
    console.log("[panel] serial closed (idle)");
  }, IDLE_CLOSE_MS);
}

function openPort(path) {
  return new Promise((resolve, reject) => {
    const p = new SerialPort({ path, baudRate: 115200, autoOpen: false });
    p.open((err) => (err ? reject(err) : resolve(p)));
  });
}

/* Drain replies for a while after pushing commands through. */
async function exchange({ com, commands }) {
  const lines = [...(commands || []), "SHOW"];

  let target = com || portPath;
  if (!target) {
    const cands = await listCandidates();
    if (cands.length === 0) throw new Error("no panel serial port found (plug the panel in via USB)");
    target = cands[0].path;
  }
  if (port && portPath !== target) {
    try {
      port.close();
    } catch {
      /* ignore */
    }
    port = null;
  }
  if (!port) {
    port = await openPort(target);
    portPath = target;
    rxBuf = "";
    rxLines = [];
    port.on("data", (chunk) => {
      rxBuf += chunk.toString("utf8");
      let nl;
      while ((nl = rxBuf.indexOf("\n")) >= 0) {
        const line = rxBuf.slice(0, nl).replace(/\r$/, "");
        rxBuf = rxBuf.slice(nl + 1);
        if (line.trim()) rxLines.push(line);
      }
    });
    port.on("error", () => {
      port = null;
      portPath = null;
    });
    console.log(`[panel] serial open ${target}`);
    await new Promise((r) => setTimeout(r, 200));
  }
  armIdleTimer();

  rxLines = [];
  for (const line of lines) {
    await new Promise((resolve) => port.write(line + "\n", resolve));
    await new Promise((r) => setTimeout(r, 150));
  }
  await new Promise((r) => setTimeout(r, 700));
  return rxLines.slice(-40);
}

/* Serialize access so concurrent HTTP requests don't interleave writes. */
function panelRoundtrip(opts) {
  const run = chain.then(() => exchange(opts));
  chain = run.catch(() => {});
  return run;
}

module.exports = { panelRoundtrip, listCandidates };
