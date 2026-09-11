const fs = require("fs");
const path = require("path");
const os = require("os");

/* OpenCode 运行时在场：opencode 侧没有 hooks 适配器，中转服务自己判——
 * 「运行中」= 会话库最近 ACTIVITY_WINDOW_MS 内有写入，而非进程存在：
 * 桌面版是 Electron 常驻应用，进程/窗口挂着 ≠ 在用，opencode.log 更是
 * 持续心跳；只有会话库 opencode.db（含 -wal，桌面版与 TUI 共用）在有
 * 会话活动时才落笔。它是最近活动不是事件：只供 OpenCode 页呼吸灯与
 * 桌宠 idle 兜底动画（见 pet.js 的 withRuntime），不喂 15 分钟睡眠
 * 计时——挂着不干活不算活动，息屏节奏不受它影响。 */

const POLL_MS = 5000;
const ACTIVITY_WINDOW_MS = 5 * 60 * 1000;
const DB_DIR = path.join(".local", "share", "opencode");
const DB_FILES = ["opencode.db-wal", "opencode.db"];

/* 会话库最近一次写入距今是否在窗口内；库文件缺失（没装 / 还没用过）视为不在用 */
function createActivityScan(dir) {
  return async () => {
    let latest = 0;
    for (const name of DB_FILES) {
      try {
        latest = Math.max(latest, (await fs.promises.stat(path.join(dir, name))).mtimeMs);
      } catch {
        /* 该文件不存在，看下一个 */
      }
    }
    return Date.now() - latest < ACTIVITY_WINDOW_MS;
  };
}

function createRuntimeWatch({ scanActivity, intervalMs = POLL_MS } = {}) {
  const scan = scanActivity || createActivityScan(path.join(os.homedir(), DB_DIR));
  let running = false;
  let scanning = false;
  let timer = null;

  async function refresh() {
    if (scanning) return; /* 上一轮还没回来就不叠加 */
    scanning = true;
    try {
      const next = await scan();
      if (next !== running) {
        running = next;
        console.log(`[opencode] runtime ${running ? "up" : "down"}`);
      }
    } catch {
      /* 扫描失败沿用上一次结果，别让瞬时抖动熄掉呼吸灯 */
    } finally {
      scanning = false;
    }
  }

  return {
    refresh,
    running: () => running,
    start() {
      refresh();
      timer = setInterval(refresh, intervalMs);
      timer.unref();
    },
    stop() {
      if (timer) clearInterval(timer);
      timer = null;
    },
  };
}

module.exports = { createRuntimeWatch, createActivityScan };
