#!/usr/bin/env node
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

function installCursor() {
  // The idempotency checks below match the REPO FOLDER PATH (…/desktop-pet/hooks/…),
  // i.e. the pre-rename project folder, NOT the current project name.
  // Do not "modernise" these strings or reinstall detection breaks.
  const file = path.join(os.homedir(), ".cursor", "hooks.json");
  const data = readJson(file, { version: 1, hooks: {} });
  if (!data.hooks || typeof data.hooks !== "object") data.hooks = {};
  let added = 0;
  let updated = 0;
  for (const event of CURSOR_EVENTS) {
    if (!Array.isArray(data.hooks[event])) data.hooks[event] = [];
    const cmd = cursorCommand(event);
    const idx = data.hooks[event].findIndex(
      (item) => item && typeof item.command === "string" && item.command.includes("desktop-pet/hooks/cursor-hook.js")
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
  writeJson(file, data);
  console.log(`[cursor] ${file}  (+${added} events, ~${updated} updated)`);
}

function installZcode() {
  const file = path.join(os.homedir(), ".zcode", "cli", "config.json");
  const data = readJson(file, {});
  if (!data.hooks || typeof data.hooks !== "object") data.hooks = {};
  data.hooks.enabled = true;
  if (!data.hooks.events || typeof data.hooks.events !== "object") data.hooks.events = {};
  let added = 0;
  for (const event of ZCODE_EVENTS) {
    if (!Array.isArray(data.hooks.events[event])) data.hooks.events[event] = [];
    const already = data.hooks.events[event].some((matcher) =>
      (matcher.hooks || []).some(
        (h) =>
          (h.command && String(h.command).includes("desktop-pet/hooks/zcode-hook.js")) ||
          (Array.isArray(h.args) && h.args.some((a) => String(a).includes("desktop-pet/hooks/zcode-hook.js")))
      )
    );
    if (already) continue;
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
  writeJson(file, data);
  console.log(`[zcode]  ${file}  (+${added} events, hooks.enabled=true)`);
}

installCursor();
installZcode();
console.log("hooks installed. Restart Cursor / ZCode sessions so they reload.");
console.log("Keep `cd server && npm start` running so POST /api/event can land.");
