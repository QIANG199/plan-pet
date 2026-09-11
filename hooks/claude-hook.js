#!/usr/bin/env node
/* Claude Code hook adapter: Claude Code already speaks the canonical event
 * names, so this only tags the lane and forwards (see server/src/lib/events.js).
 * Registered in ~/.claude/settings.json by hooks/install.js. */
const { runHook } = require("./common");
const { EVENT_SET } = require("../server/src/lib/events");

runHook({
  // Claude Code treats hook stdout as a decision object; stay silent like the
  // ZCode adapter so the default behaviour is untouched.
  stdoutLine() {
    return "";
  },
  mapPayload(payload) {
    const event = payload.hook_event_name || payload.event || "";
    if (!EVENT_SET.has(event)) return null;
    const body = {
      source: "claude",
      event,
      session: payload.session_id || "",
      tool: payload.tool_name || "",
    };
    if (event === "Stop" && payload.status) body.status = payload.status;
    return body;
  },
});
