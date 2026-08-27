const fs = require("fs");
const path = require("path");
const env = require("./env");

const DEFAULT_BIN = path.join(
  __dirname,
  "..",
  "..",
  "firmware",
  ".pio",
  "build",
  "desktop-pet",
  "firmware.bin"
);

/* OTA 固件信息：build 取文件 mtime（秒），与固件编译时刻（__DATE__/__TIME__）
 * 的 epoch 比较，重编即出新版本号，无需手工维护版本。 */
function otaBinPath() {
  return env.otaBinPath || DEFAULT_BIN;
}

function otaInfo(binPath = otaBinPath()) {
  let st;
  try {
    st = fs.statSync(binPath);
  } catch {
    return { ok: false };
  }
  if (!st.isFile()) return { ok: false };
  return { ok: true, build: Math.floor(st.mtimeMs / 1000), size: st.size };
}

module.exports = { otaInfo, otaBinPath, DEFAULT_BIN };
