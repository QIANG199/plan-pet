const test = require("node:test");
const assert = require("node:assert");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");
const { createRuntimeWatch, createActivityScan } = require("../src/opencode-runtime");

test("runtime watch follows scan results", async () => {
  let on = true;
  const w = createRuntimeWatch({ scanActivity: async () => on });
  await w.refresh();
  assert.equal(w.running(), true);
  on = false;
  await w.refresh();
  assert.equal(w.running(), false);
});

test("runtime watch keeps the last result when a scan fails", async () => {
  let fail = false;
  const w = createRuntimeWatch({
    scanActivity: async () => {
      if (fail) throw new Error("scan exploded");
      return true;
    },
  });
  await w.refresh();
  assert.equal(w.running(), true);
  fail = true;
  await w.refresh();
  assert.equal(w.running(), true); /* 瞬时失败不熄灯 */
});

test("activity scan requires a fresh session-db write", async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "opencode-rt-"));
  const scan = createActivityScan(dir);
  assert.equal(await scan(), false); /* 库文件缺失 = 没装 / 没用过 */

  const wal = path.join(dir, "opencode.db-wal");
  fs.writeFileSync(wal, "x");
  assert.equal(await scan(), true); /* 刚写入 = 在用 */

  const stale = new Date(Date.now() - 6 * 60 * 1000);
  fs.utimesSync(wal, stale, stale);
  assert.equal(await scan(), false); /* 写入太久远 = 挂着不干活 */
});
