#!/usr/bin/env node
const { runHook } = require("./common");

const EVENTS = new Set([
  "SessionStart",
  "UserPromptSubmit",
  "PreToolUse",
  "PostToolUse",
  "PostToolUseFailure",
  "Stop",
]);

runHook({
  stdoutLine() {
    return "";
  },
  mapPayload(payload) {
    const event = payload.hook_event_name || payload.event || "";
    if (!EVENTS.has(event)) return null;
    const body = {
      source: "zcode",
      event,
      session: payload.session_id || "",
      tool: payload.tool_name || "",
    };
    if (event === "Stop" && payload.status) body.status = payload.status;
    return body;
  },
});
