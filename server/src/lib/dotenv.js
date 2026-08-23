/* Single .env parser for the repo root file, shared by server and hooks. */

const fs = require("fs");
const { stripQuotes } = require("./text");

function loadEnvFile(file) {
  if (!fs.existsSync(file)) return;
  for (const raw of fs.readFileSync(file, "utf8").split(/\r?\n/)) {
    const line = raw.trim();
    if (!line || line.startsWith("#")) continue;
    const eq = line.indexOf("=");
    if (eq < 1) continue;
    const key = line.slice(0, eq).trim();
    const value = stripQuotes(line.slice(eq + 1).trim());
    if (process.env[key] == null) process.env[key] = value;
  }
}

module.exports = { loadEnvFile };
