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
  panelToken: required("PANEL_TOKEN"),
  host: process.env.HOST || "0.0.0.0",
  port: Number(process.env.PORT || 3737),
};
