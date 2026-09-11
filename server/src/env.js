const path = require("path");
const { loadEnvFile } = require("./lib/dotenv");

loadEnvFile(path.join(__dirname, "..", "..", ".env"));

function required(name) {
  const v = process.env[name];
  if (!v) throw new Error(`Missing ${name} in .env`);
  return v;
}

module.exports = {
  zaiKey: required("ZAI_API_KEY"),
  /* GLM 套餐区域：国际版 api.z.ai（默认）；国内 bigmodel 设 https://open.bigmodel.cn */
  zaiApiBase: process.env.ZAI_API_BASE || "https://api.z.ai",
  /* OTA 固件路径：默认用 ota.js 里的 pio 构建产物路径，OTA_BIN 可覆盖 */
  otaBinPath: process.env.OTA_BIN || "",
  /* OpenCode Go 套餐是可选第三份额度：缺 key 时该计划页显示 fail，服务照跑 */
  opencodeGoKey: process.env.OPENCODE_GO_KEY || "",
  panelToken: required("PANEL_TOKEN"),
  host: process.env.HOST || "0.0.0.0",
  port: Number(process.env.PORT || 3737),
};
