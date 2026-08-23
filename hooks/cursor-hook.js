#!/usr/bin/env node
const { runHook } = require("./common");
const { CURSOR_TO_EVENT } = require("../server/src/lib/events");

runHook({
  stdoutLine(payload) {
    const name = hookName(payload);
    if (name === "beforeSubmitPrompt") return JSON.stringify({ continue: true });
    return "{}";
  },
  mapPayload(payload) {
    const name = hookName(payload);
    const event = CURSOR_TO_EVENT[name];
    if (!event) return null;
    const body = {
      source: "cursor",
      event,
      session: payload.conversation_id || payload.session_id || "",
      tool: payload.tool_name || "",
    };
    if (event === "Stop" && payload.status) body.status = payload.status;
    return body;
  },
});

function hookName(payload) {
  return process.argv[2] || (payload && payload.hook_event_name) || "";
}
