#!/usr/bin/env node
const fs = require("fs");
const os = require("os");
const path = require("path");

const ROOT = path.join(__dirname);
const cursorHook = path.join(ROOT, "cursor-hook.js").replace(/\\/g, "/");
const zcodeHook = path.join(ROOT, "zcode-hook.js").replace(/\\/g, "/");
const nodeBin = process.execPath.replace(/\\/g, "/");

const CURSOR_EVENTS = [
  "sessionStart",
  "beforeSubmitPrompt",
  "preToolUse",
  "postToolUse",
  "postToolUseFailure",
  "stop",
];

const ZCODE_EVENTS = [
  "SessionStart",
  "UserPromptSubmit",
  "PreToolUse",
  "PostToolUse",
  "PostToolUseFailure",
  "Stop",
];

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

function cursorCommand() {
  return `cmd /d /s /c ""${nodeBin}" "${cursorHook}""`;
}

function installCursor() {
  const file = path.join(os.homedir(), ".cursor", "hooks.json");
  const data = readJson(file, { version: 1, hooks: {} });
  if (!data.hooks || typeof data.hooks !== "object") data.hooks = {};
  const cmd = cursorCommand();
  let added = 0;
  for (const event of CURSOR_EVENTS) {
    if (!Array.isArray(data.hooks[event])) data.hooks[event] = [];
    const already = data.hooks[event].some(
      (item) => item && typeof item.command === "string" && item.command.includes("desktop-pet/hooks/cursor-hook.js")
    );
    if (already) continue;
    data.hooks[event].push({ command: cmd });
    added++;
  }
  if (data.version == null) data.version = 1;
  writeJson(file, data);
  console.log(`[cursor] ${file}  (+${added} events)`);
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
