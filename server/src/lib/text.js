/* Small string/time helpers shared across the server and the hook adapters. */

function stripQuotes(value) {
  const s = String(value || "");
  if (s.length >= 2 &&
      ((s.startsWith('"') && s.endsWith('"')) ||
       (s.startsWith("'") && s.endsWith("'")))) {
    return s.slice(1, -1);
  }
  return s;
}

/* Accept seconds or milliseconds timestamps and normalise to unix seconds. */
function toUnixSec(value) {
  if (value == null || value === "") return null;
  const n = Number(value);
  if (!Number.isFinite(n) || n <= 0) return null;
  return n > 1e12 ? Math.floor(n / 1000) : Math.floor(n);
}

module.exports = { stripQuotes, toUnixSec };
