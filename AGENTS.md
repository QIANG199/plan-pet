# AGENTS.md

PlanPet：ESP32 桌面额度面板 + 桌宠。一块微雪 ESP32-S3 触摸屏实时显示 GLM / Cursor 编码套餐额度，右侧 clawd 桌宠随 Cursor/ZCode 编码事件切换 6 种状态。上手与日常使用见根目录 `README.md`。改代码前先读 `CONTEXT.md`（领域词汇表，术语与已否决说法的权威来源）和 `docs/design/01-设计方案.md`（API 契约 §5、状态机 §6、ADR §9）。

## 目录与职责

- `server/` — 中转服务（Node.js ≥18，CommonJS，无构建/无 lint）。拉取 GLM 与 Cursor 额度、接收 hooks 上报、维护桌宠状态机，供面板轮询。监听 `0.0.0.0:3737`。`src/lib/` 是与 hooks 共享的模块（事件表、.env 解析、时间工具），`src/panel-link.js` 是 USB 串口桥（Web 配网页 `GET /setup` → `POST /api/panel/setup`），`tests/` 是单测（`npm test`）。依赖仅 `serialport`。
- `firmware/` — 额度面板固件（PlatformIO + Arduino + LVGL 9）。**纯显示端**，只渲染中转服务快照，不承担业务逻辑；微雪 3.49 寸屏物理 172×640，横屏用 640×172。`src/ui/` 界面层（桌面 + 触屏设置页），`src/bsp/` 板级驱动（含 vendor 的 axs15231b/、touch/，二者保持同层相对引用），`tools/` 烤帧脚本（GIF 源默认 `D:\develop\clawd-on-desk\assets\gif`，可用 `CLAWD_GIF_DIR` 覆盖）。
- `hooks/` — Cursor / ZCode 事件适配器（`common.js` 共享上报逻辑，`install.js` / `install.cmd` 安装修复）。安装是**追加**，不得覆盖用户已有的 clawd 钩子；也禁止原样复用 clawd 的 cursor-hook.js。**目录位置不能动**——用户机器上已安装的钩子配置以绝对路径引用这里。
- `docs/` — 设计文档与 1:1 屏幕设计稿（桌面 `screen-layout.html` v11、设置页 `settings-layout.html`）。

## 常用命令

```bash
# 中转服务（.env 在仓库根目录，含 ZAI_API_KEY / PANEL_TOKEN / PORT）
cd server && npm start
npm test                      # node --test 单测

# 安装 / 修复 hooks（追加式；Cursor 大版本更新有时会清空 ~/.cursor/hooks.json）
node hooks/install.js         # 或双击 hooks/install.cmd；npm run hooks:install
node hooks/install.js status  # 检查是否齐全（缺则 exit 1）

# 固件烧录与串口（COM3，115200）
cd firmware && pio run -t upload
pio device monitor

# 手动重烤桌宠帧（编译时也会自动跑）
python firmware/tools/bake_pet.py
```

本机验收：`GET http://127.0.0.1:3737/api/dashboard?token=<PANEL_TOKEN>`（`?token=` 仅限 127.0.0.1；局域网必须用请求头 `X-Panel-Token`）。

## 领域规则（勿凭直觉改）

- 桌宠 6 态：`idle / thinking / typing / happy / error / sleeping`。**idle ≠ sleeping**：sleeping 只在约 15 分钟无任何事件后进入；happy/error 限时结束后回 idle。
- 进度条分档按**已用** 70% / 90% 变色，不是按余量。
- 面板直连显式配置的 PC IP 访问中转服务（串口 `HOST` / 触屏设置页写 NVS）。mDNS 发现已移除（目标局域网组播不通 + 逐次解析阻塞轮询，2026-08 否决）；把 IP 编译进固件仍然禁止。DHCP 换 IP 后需重设 HOST。
- GLM 的 5h / 周窗口都读 `TOKENS_LIMIT`；`TIME_LIMIT` 是 MCP 月度，面板不做。
- 令牌语义严格区分：`PANEL_TOKEN` 是面板↔中转服务的共享口令；Z.ai Key 和 Cursor 登录态是另外的东西，文档里别混称 API Key。
- hooks 事件名唯一权威在 `server/src/lib/events.js`（pet.js、两个适配器、install.js 都从这取）；桌宠状态名同时是固件 `ui/ui_pet.cpp` 与 `firmware/tools/bake_pet.py` 的跨语言契约。

## 固件注意事项（硬件硬规则）

- **GPIO0（BOOT 键）是 strapping 脚**：重启必须等松手确认高电平后才执行，否则进 USB 下载模式（`power.cpp` 的 `request_reboot` 已处理）。
- **RESET 键接 EN 硬复位线，固件读不到**——"软件拦截 RST"类需求直接否掉。
- 关机流程：长按 PWR 3 秒 → 圆形进度环 → 3·2·1 倒计时 → 断电；倒计时期间再按 PWR 取消；USB 插入永不关机。开机先播桌宠动画（`ui_boot`），首批快照到达后淡入桌面。
- **息屏**（`standby.*`）：桌宠 sleeping 后再等 sleepT 分钟（NVS `sleepT`，默认 5，串口 `SLEEP` 可调）→ 背光关断 + WiFi 断网改 5 分钟窗口制（`net_sleep_enter/exit`，窗口 ~15s 内轮询一次即断）；唤醒源为触摸/BOOT 键/串口任意命令/USB 插入/PWR 相位/桌宠离开 sleeping（事件唤醒靠联网窗口，最坏延迟 5 分钟）。醒着时轮询失败指数退避封顶 60s（`net.cpp`）。背光开关必须走 `lcd_bl_pwm_bsp_off/on`（`ledc_stop` 真关断），不要用 `lcd_bl_set_brightness(0)`——反向占空 255 每周期残留微亮脉冲。**息屏禁止两件事**：给面板发 DISPOFF（AXS15231B 显示+触摸同芯，关显示挂触摸 I2C）、`setCpuFrequencyMhz` 降频（切频复位 APB 外设配置，I2C 触摸失联）——两者都会饿死 LVGL 锁令全部唤醒源失效（2026-08 踩坑；触摸 I2C 曾被打进断电级死锁，ESP32 重启+RST 脉冲都解不开）。触摸若再失灵，看 `i2c_touch_diag` 输出：probe 失败=芯片死锁，SCL/SDA 电平低=总线钳位。息屏 ≠ 关机，也 ≠ 桌宠 sleeping 态。
- **低电保护**（`power.cpp`）：未插 USB 且 EMA 电压 <3.30V 持续 2 分钟 → 自动走关机流程（防锂电过放）；回升 >3.50V 重新武装。
- `firmware/lib/lvgl` 是指向微雪 Demo 的目录联接，**不进 Git**——本机没有它就编不了固件，这不是仓库缺陷。
- `include/secrets.h`、`src/pet_frames.bin`、`src/pet_blob.S` 都是 gitignore 的生成物/本机配置，别提交。LVGL 配置只有 `include/lv_conf.h` 一份。
- 中文 UI 用项目生成字体 `ui/font_cn_16.c`（等线 16px、ASCII + 项目用字的子集，约 86KB）；**新增中文文案必须用 lv_font_conv 重新生成**（命令见 `font_cn_16.h`），否则缺字显示为「口」。flash 已用约 88%，加资源前先看余量。
- 电量是电压估算（板子没有电量计芯片）：充电时百分比冻结在插线瞬间的读数，拔线后 EMA 平滑恢复——别给充电中的读数加固定补偿（已两次踩坑）。
- LVGL 跑在 core 0 专用任务，跨任务碰控件必须 `lvgl_port_lock`；power 任务只发 volatile 状态快照不直接调 LVGL，`latch_off` 只在 power 任务执行（避免与 RTC 的 I2C 竞态）。
- **勿在卡片上用 `clip_corner` 装溢出子对象**：本机是全屏渲染模式，圆角裁剪会让软件渲染器为每个绘制块分配 layer 缓冲，对象一多就打爆 64KB LVGL 内存池并陷入重试风暴 → LVGL 任务饿死 IDLE0 → 任务看门狗复位（设置页曾因此必崩）。列表一律用可滚动容器，卡片用 `ui_settings.cpp` 的 `style_card_flat`。

## 约定

- 文档、术语、注释以中文为主；提交信息用英文 conventional commits（`feat(scope): ...`）。
- 服务端除 `serialport` 外纯 Node 标准库，不引框架；新增依赖需有充分理由。
- `.env` 解析唯一实现在 `server/src/lib/dotenv.js`（server 与 hooks 共用）；改键名先看 `env.js` 和 `common.js` 两处消费。
- 串口命令清单以 `firmware/README.md` 为准（WIFI/PASS/TOKEN/HOST/PORT/PET/BRIGHT/SLEEP/OFF/REBOOT/SETUP/SHOW）。
