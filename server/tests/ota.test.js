const test = require("node:test");
const assert = require("node:assert");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");
const { otaInfo, armPush, clearPush, pushState } = require("../src/ota");

test("otaInfo maps mtime to build seconds and reports size", () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "ota-"));
  const bin = path.join(dir, "firmware.bin");
  fs.writeFileSync(bin, Buffer.alloc(1024));
  const past = new Date(Date.now() - 60_000);
  fs.utimesSync(bin, past, past);
  const info = otaInfo(bin);
  assert.equal(info.ok, true);
  assert.equal(info.size, 1024);
  assert.equal(info.build, Math.floor(past.getTime() / 1000));
  fs.rmSync(dir, { recursive: true, force: true });
});

test("otaInfo reports ok=false for missing files and directories", () => {
  assert.deepEqual(otaInfo(path.join(os.tmpdir(), "ota-missing-" + process.pid)), {
    ok: false,
  });
  assert.deepEqual(otaInfo(os.tmpdir()), { ok: false });
});

test("push flag is armed on demand and survives until the bin is served", () => {
  clearPush();
  assert.equal(pushState(), null);
  const at = armPush();
  assert.equal(typeof at, "number");
  assert.equal(pushState(), at);
  /* 再次置位取新时刻：固件按「值变化」只触发一次 */
  const later = armPush();
  assert.equal(pushState(), later);
  clearPush();
  assert.equal(pushState(), null);
});
