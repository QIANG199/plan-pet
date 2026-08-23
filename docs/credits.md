# 致谢与第三方来源

PlanPet **自己的代码**以仓库根目录 [Apache License 2.0](../LICENSE) 发布。下列项目被使用、参考或作为素材来源，**版权仍归原作者**，请按各项目自己的许可证使用。贴图尤其如此：clawd GIF **不适用** Apache-2.0。

## 桌宠贴图（必须说明）

屏上 clawd 动画**不是**本仓库原创，也**不随 Git 分发**（烤出来的 `pet_frames.bin` / `pet_blob.S` 已 gitignore）。

编译前 `tools/bake_pet.py` 从本机 [Clawd on Desk](https://github.com/rullerzhou-afk/clawd-on-desk) 的 `assets/gif/` 读取 GIF，合成到深/浅底上再转成 RGB565。

| 项 | 说明 |
|---|---|
| 项目 | [Clawd on Desk](https://github.com/rullerzhou-afk/clawd-on-desk)（`rullerzhou-afk/clawd-on-desk`） |
| 用到的文件 | `clawd-idle.gif`、`clawd-thinking.gif`、`clawd-typing.gif`、`clawd-happy.gif`、`clawd-error.gif`、`clawd-sleeping.gif`、`clawd-react-double-jump.gif` |
| 源码许可 | [AGPL-3.0](https://github.com/rullerzhou-afk/clawd-on-desk/blob/main/LICENSE)（仅指其程序源码） |
| 贴图许可 | [assets/LICENSE](https://github.com/rullerzhou-afk/clawd-on-desk/blob/main/assets/LICENSE)：**All Rights Reserved**。Clawd 形象属 [Anthropic](https://www.anthropic.com)；像素稿为同人，**不得商用**。Calico 等其它主题版权归原画师 |

本仓库**没有**把 GIF 拷进来，也**没有**复用 clawd 的 `hooks/cursor-hook.js`。hooks 是自写适配器，安装时只追加，可与 Clawd on Desk 同时运行。

## 硬件与固件周边

| 项目 | 用途 | 链接 |
|---|---|---|
| 微雪 ESP32-S3-Touch-LCD-3.49 | 开发板、原理图、Arduino/LVGL Demo（本机目录联接到 `firmware/lib/lvgl`，不进 Git） | [Wiki](https://www.waveshare.net/wiki/ESP32-S3-Touch-LCD-3.49) |
| [LVGL](https://github.com/lvgl/lvgl) 9 | 屏上 UI | 经微雪 Demo 使用 |
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) | 固件解析快照 | MIT |
| Espressif Arduino for ESP32 | 固件框架 | [arduino-esp32](https://github.com/espressif/arduino-esp32) |

## 中转服务

| 项目 | 用途 | 链接 |
|---|---|---|
| [bonjour-service](https://github.com/onlxltd/bonjour-service) | mDNS 广播 `desktop-pet.local` | MIT |
| Node.js | 运行时 | https://nodejs.org |

## 接口对照（参考，非拷贝其程序）

| 项目 | 用途 | 链接 |
|---|---|---|
| cc-switch #1588 | GLM `TOKENS_LIMIT` / `TIME_LIMIT` 字段含义 | https://github.com/farion1231/cc-switch/issues/1588 |
| cursor-usage / cursor-usage-monitor | Cursor `usage-summary` 字段对照 | https://github.com/FreePeak/cursor-usage · https://github.com/lixwen/cursor-usage-monitor |
