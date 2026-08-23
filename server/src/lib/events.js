/* Canonical hook-event vocabulary shared by the pet state machine, both hook
 * adapters and the installer. The panel firmware also mirrors these states —
 * change here and every consumer follows in one place. */

const EVENTS = [
  "SessionStart",
  "UserPromptSubmit",
  "PreToolUse",
  "PostToolUse",
  "PostToolUseFailure",
  "Stop",
];

const EVENT_SET = new Set(EVENTS);

/* Cursor's hook event names -> canonical internal names. */
const CURSOR_TO_EVENT = {
  sessionStart: "SessionStart",
  beforeSubmitPrompt: "UserPromptSubmit",
  preToolUse: "PreToolUse",
  postToolUse: "PostToolUse",
  postToolUseFailure: "PostToolUseFailure",
  stop: "Stop",
};

module.exports = { EVENTS, EVENT_SET, CURSOR_TO_EVENT };
