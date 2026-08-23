# AGENTS.md

PlanPet：ESP32 桌面额度面板 + 桌宠。一块微雪 ESP32-S3 触摸屏实时显示 GLM / Cursor 编码套餐额度，右侧 clawd 桌宠随 Cursor/ZCode 编码事件切换 6 种状态。上手与日常使用见根目录 `README.md`。改代码前先读 `CONTEXT.md`（领域词汇表，术语与已否决说法的权威来源）和 `docs/design/01-设计方案.md`（API 契约 §5、状态机 §6、ADR §9）。

## 目录与职责

- `server/` — 中转服务（Node.js ≥18，CommonJS，无构建/无 lint/无测试）。拉取 GLM 与 Cursor 额度、接收 hooks 上报、维护桌宠状态机，供面板轮询。监听 `0.0.0.0:3737`，mDNS 广播 `desktop-pet.local`。
- `firmware/` — 额度面板固件（PlatformIO + Arduino + LVGL 9）。**纯显示端**，只渲染中转服务快照，不承担业务逻辑；微雪 3.49 寸屏物理 172×640，横屏用 640×172。
- `hooks/` — Cursor / ZCode 事件适配器（`common.js` 共享上报逻辑，`install.js` 安装）。安装是**追加**，不得覆盖用户已有的 clawd 钩子；也禁止原样复用 clawd 的 cursor-hook.js。
- `tools/` — `bake_pet.py` 把 clawd GIF（源在 `D:\develop\clawd-on-desk\assets\gif`）烤成 RGB565 帧并生成 `pet_blob.S`，由 platformio.ini 的 pre 脚本在编译前自动重跑；手动跑需 PIL。
- `docs/` — 设计文档与 1:1 屏幕设计稿（`screen-layout.html`，v11 定稿）。

## 常用命令

```bash
# 中转服务（.env 在仓库根目录，含 ZAI_API_KEY / PANEL_TOKEN / PORT）
cd server && npm start

# 安装 hooks（追加式）
node hooks/install.js        # 或 cd server && npm run hooks:install

# 固件烧录与串口（COM3，115200）
cd firmware && pio run -t upload
pio device monitor

# 手动重烤桌宠帧（编译时也会自动跑）
python tools/bake_pet.py
```

本机验收：`GET http://127.0.0.1:3737/api/dashboard?token=<PANEL_TOKEN>`（`?token=` 仅限 127.0.0.1；局域网必须用请求头 `X-Panel-Token`）。

## 领域规则（勿凭直觉改）

- 桌宠 6 态：`idle / thinking / typing / happy / error / sleeping`。**idle ≠ sleeping**：sleeping 只在约 15 分钟无任何事件后进入；happy/error 限时结束后回 idle。
- 进度条分档按**已用** 70% / 90% 变色，不是按余量。
- 面板靠 mDNS `desktop-pet.local` 找中转服务，禁止写死 PC IP（已否决）。
- GLM 的 5h / 周窗口都读 `TOKENS_LIMIT`；`TIME_LIMIT` 是 MCP 月度，面板不做。
- 令牌语义严格区分：`PANEL_TOKEN` 是面板↔中转服务的共享口令；Z.ai Key 和 Cursor 登录态是另外的东西，文档里别混称 API Key。

## 固件注意事项

- `firmware/lib/lvgl` 是指向微雪 Demo 的目录联接，**不进 Git**——本机没有它就编不了固件，这不是仓库缺陷。
- `include/secrets.h`（复制自 `secrets.example.h`）、`src/pet_frames.bin`、`src/pet_blob.S` 都是 gitignore 的生成物/本机配置，别提交。
- 串口配网命令（115200，写入 NVS）：`WIFI <ssid>` / `PASS <pwd>` / `TOKEN <令牌>` / `HOST desktop-pet.local` / `SHOW` / `PET <state>|auto` / `BRIGHT <8-255>`。
- 交互：点时间下方空白切换深/浅主题；点桌宠播放 poke 动画。纯电池时长按背面 PWR 约 3 秒开机/关机。BOOT 与 RESET 固件不读。

## 约定

- 文档、术语、注释以中文为主；提交信息用英文 conventional commits（`feat(scope): ...`）。
- 服务端纯 Node 标准库 + `bonjour-service`，不引框架；新增依赖需有充分理由。
- `.env` 由根目录 `server/` 与 `hooks/` 共用（`hooks/common.js` 自行解析），改动键名要两边同步。
