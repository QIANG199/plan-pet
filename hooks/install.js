#!/usr/bin/env node
/* Append PlanPet hooks into ~/.cursor/hooks.json and ~/.zcode/cli/config.json.
 * Idempotent: safe to re-run after a Cursor update wipes hooks.json.
 *
 *   node hooks/install.js           # install / repair
 *   node hooks/install.js status    # check only (exit 1 if incomplete)
 *   hooks\install.cmd               # same, double-clickable on Windows
 */
const fs = require("fs");
const os = require("os");
const path = require("path");
const { EVENTS, CURSOR_TO_EVENT } = require("../server/src/lib/events");

const ROOT = path.join(__dirname);
const cursorHook = path.join(ROOT, "cursor-hook.js").replace(/\\/g, "/");
const zcodeHook = path.join(ROOT, "zcode-hook.js").replace(/\\/g, "/");
const nodeBin = process.execPath.replace(/\\/g, "/");

const CURSOR_EVENTS = Object.keys(CURSOR_TO_EVENT);
const ZCODE_EVENTS = EVENTS;
const CURSOR_FILE = path.join(os.homedir(), ".cursor", "hooks.json");
const ZCODE_FILE = path.join(os.homedir(), ".zcode", "cli", "config.json");

function readJson(file, fallback) {
  if (!fs.existsSync(file)) return fallback;
  try {
    return JSON.parse(fs.readFileSync(file, "utf8"));
  } catch (err) {
    throw new Error(`cannot parse ${file}: ${err.message}`);
  }
}

function writeJson(file, data) {
  fs.mkdirSync(path.dirname(file), { recursive: true });
  fs.writeFileSync(file, JSON.stringify(data, null, 2) + "\n", "utf8");
}

function cursorCommand(event) {
  // Cursor on Windows often delivers empty stdin; the event name in argv is enough
  // for the pet state machine. Direct node.exe (no nested cmd).
  return `"${nodeBin}" "${cursorHook}" ${event}`;
}

function isOurCursorHook(command) {
  if (typeof command !== "string") return false;
  // Legacy folder name before the rename, plus the live absolute path.
  return (
    command.includes("desktop-pet/hooks/cursor-hook.js") ||
    command.includes(cursorHook)
  );
}

function isOurZcodeHook(h) {
  const hit = (s) =>
    String(s).includes("desktop-pet/hooks/zcode-hook.js") ||
    String(s).includes(zcodeHook);
  if (h.command && hit(h.command)) return true;
  if (Array.isArray(h.args) && h.args.some(hit)) return true;
  return false;
}

function cursorPresent(data, event) {
  const list = data.hooks && data.hooks[event];
  if (!Array.isArray(list)) return false;
  return list.some((item) => item && isOurCursorHook(item.command));
}

function zcodePresent(data, event) {
  const list = data.hooks && data.hooks.events && data.hooks.events[event];
  if (!Array.isArray(list)) return false;
  return list.some((matcher) =>
    (matcher.hooks || []).some((h) => h && isOurZcodeHook(h))
  );
}

function installCursor() {
  const data = readJson(CURSOR_FILE, { version: 1, hooks: {} });
  if (!data.hooks || typeof data.hooks !== "object") data.hooks = {};
  let added = 0;
  let updated = 0;
  for (const event of CURSOR_EVENTS) {
    if (!Array.isArray(data.hooks[event])) data.hooks[event] = [];
    const cmd = cursorCommand(event);
    const idx = data.hooks[event].findIndex(
      (item) => item && isOurCursorHook(item.command)
    );
    if (idx >= 0) {
      if (data.hooks[event][idx].command !== cmd) {
        data.hooks[event][idx] = { command: cmd };
        updated++;
      }
      continue;
    }
    data.hooks[event].push({ command: cmd });
    added++;
  }
  if (data.version == null) data.version = 1;
  writeJson(CURSOR_FILE, data);
  console.log(`[cursor] ${CURSOR_FILE}  (+${added} events, ~${updated} updated)`);
}

function installZcode() {
  const data = readJson(ZCODE_FILE, {});
  if (!data.hooks || typeof data.hooks !== "object") data.hooks = {};
  data.hooks.enabled = true;
  if (!data.hooks.events || typeof data.hooks.events !== "object") data.hooks.events = {};
  let added = 0;
  for (const event of ZCODE_EVENTS) {
    if (!Array.isArray(data.hooks.events[event])) data.hooks.events[event] = [];
    if (zcodePresent(data, event)) continue;
    data.hooks.events[event].push({
      hooks: [
        {
          type: "process",
          command: nodeBin,
          args: [zcodeHook],
          timeoutMs: 800,
        },
      ],
    });
    added++;
  }
  writeJson(ZCODE_FILE, data);
  console.log(`[zcode]  ${ZCODE_FILE}  (+${added} events, hooks.enabled=true)`);
}

function statusReport() {
  const cursorData = readJson(CURSOR_FILE, { version: 1, hooks: {} });
  const zcodeData = readJson(ZCODE_FILE, {});
  const cursorMissing = CURSOR_EVENTS.filter((e) => !cursorPresent(cursorData, e));
  const zcodeMissing = ZCODE_EVENTS.filter((e) => !zcodePresent(zcodeData, e));
  const cursorOk = cursorMissing.length === 0;
  const zcodeOk = zcodeMissing.length === 0;
  const zcodeEnabled = !!(zcodeData.hooks && zcodeData.hooks.enabled);

  console.log(`[status] cursor  ${cursorOk ? "ok" : "INCOMPLETE"}  ${CURSOR_FILE}`);
  if (!cursorOk) console.log(`         missing: ${cursorMissing.join(", ")}`);
  console.log(
    `[status] zcode   ${zcodeOk && zcodeEnabled ? "ok" : "INCOMPLETE"}  ${ZCODE_FILE}`
  );
  if (!zcodeEnabled) console.log("         hooks.enabled is not true");
  if (!zcodeOk) console.log(`         missing: ${zcodeMissing.join(", ")}`);

  return cursorOk && zcodeOk && zcodeEnabled;
}

function usage() {
  console.log("usage: node hooks/install.js [install|status|help]");
  console.log("  install (default)  append / repair PlanPet hooks");
  console.log("  status             check only; exit 1 if incomplete");
}

function main() {
  const cmd = (process.argv[2] || "install").toLowerCase();
  if (cmd === "help" || cmd === "-h" || cmd === "--help") {
    usage();
    return;
  }
  if (cmd === "status" || cmd === "check") {
    const ok = statusReport();
    if (!ok) {
      console.log("hint: run  node hooks/install.js   (or hooks\\install.cmd)");
      process.exit(1);
    }
    return;
  }
  if (cmd !== "install" && cmd !== "repair" && cmd !== "fix") {
    usage();
    process.exit(2);
  }
  installCursor();
  installZcode();
  statusReport();
  console.log("hooks installed. Restart Cursor / ZCode sessions so they reload.");
  console.log("Keep `cd server && npm start` running so POST /api/event can land.");
}

main();
