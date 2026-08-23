# PlanPet

一块放在书桌上的 ESP32 触摸屏：左边显示 **GLM Coding Plan** 和 **Cursor Pro** 的已用额度，右边一只 clawd 桌宠跟着 Cursor / ZCode 的编码事件换表情。

桌宠贴图来自开源项目 **[Clawd on Desk](https://github.com/rullerzhou-afk/clawd-on-desk)**（GIF 不进本仓库，见 [致谢](docs/credits.md)）。

GitHub：[QIANG199/plan-pet](https://github.com/QIANG199/plan-pet)（源码 [Apache-2.0](LICENSE)）。局域网发现仍用 mDNS **`desktop-pet.local`**（已烧过的板不用改 Host）。

电脑上跑一个很小的 **中转服务**，面板只负责显示。Cursor 登录态和 Z.ai Key 都留在电脑里，不进固件。

## 能做什么

- 屏上 GLM：`5h` / `7d` 已用百分比 + 倒计时
- 屏上 Cursor：`composer & grok` / `other` 已用百分比 + 账期倒计时
- 进度条按**已用**变色：低于 70% 绿，70%–90% 橙，不低于 90% 红
- 桌宠 6 态：`idle` / `thinking` / `typing` / `happy` / `error` / `sleeping`（idle 不是睡觉；sleeping 约 15 分钟无事件才进入）
- 绿点按应用独立亮（Cursor 与 ZCode 可同时亮）；两边都在忙时，动画跟 GLM
- 点时间下方空白切换深/浅主题；点桌宠播放 poke
- **BOOT 键**：短按换深/浅主题，双击进**设置页**（触屏配网：选 WiFi、输密码、改主机/端口/令牌），长按 5 秒松手重启
- **PWR 键**：长按约 3 秒触发**关机倒计时**（左侧卡片显示 3·2·1，期间再按一次 PWR 取消），倒计时走完才断电黑屏；USB 插着永不关机
- 纯电池：背面 PWR 长按约 3 秒开机（亮屏松手）

硬件：微雪 **ESP32-S3-Touch-LCD-3.49** A 款带 18650（SKU 32373），物理 172×640，横屏按 640×172 来画。

```
 电脑（中转服务 :3737，mDNS desktop-pet.local）
   GLM API + Cursor 账单 + hooks 事件
                    ↓  每 2 秒 GET /api/dashboard
 额度面板（ESP32，只渲染快照）
```

---

## 使用已经做好的产品

下面假设仓库已在电脑上、固件已烧过、WiFi 和令牌已配过。第一次从零装，请看下一节。

### 1. 电脑：中转服务

需要 **Node.js ≥ 18**。根目录 `.env`：

```
ZAI_API_KEY=你的Z.ai密钥
PANEL_TOKEN=和面板约定的口令
HOST=0.0.0.0
PORT=3737
```

`PANEL_TOKEN` 是面板和中转服务之间的共享口令，**不是** Z.ai Key，也不是 Cursor 登录态。

```bat
cd server
npm install
npm start
```

看到类似 `[mdns] advertised desktop-pet.local` 即在局域网广播。本机验收（把令牌换进去）：

```
http://127.0.0.1:3737/api/dashboard?token=<PANEL_TOKEN>
```

`?token=` **只允许 127.0.0.1**。局域网里的面板必须用请求头 `X-Panel-Token`。

### 2. 防火墙

面板在另一台设备上，Windows 默认会拦入站 3737。**以管理员**运行一次：

```
server\scripts\allow-firewall.cmd
```

规则名：`desktop-pet-3737`。以后重装系统再跑一次即可。

### 3. 开机自启（可选）

希望登录 Windows 后自动起中转服务：

```
server\scripts\install-startup.cmd
```

会在「启动」文件夹放一个最小化快捷方式。删掉该快捷方式即取消。

### 4. 桌宠跟着编码走

追加安装，**不会覆盖**你已有的 clawd 钩子：

```bat
node hooks/install.js
```

或 `cd server && npm run hooks:install`。装完后用 Cursor / ZCode 编码，桌宠应切换 thinking / typing 等。

### 5. 面板日常

- 日常插 USB 供电；18650 是备用。电脑要开着，中转服务要在跑。
- 同一局域网。面板用 mDNS 找 `desktop-pet.local`，不要把会变的电脑 IP 当成长期身份。
- 若解析失败，串口可临时 `HOST <电脑当前IP>`，这是逃生口，不是默认做法。
- 背面 **RESET** 键是硬件复位线，固件读不到；**BOOT** 键已由固件接管（换肤 / 设置 / 重启）。

### 6. 配置：Web 页面（推荐）/ 触屏设置页 / 串口

**Web 页面**——面板 USB 连着电脑时最方便。中转服务跑起来后，浏览器打开：

```
http://127.0.0.1:3737/setup
```

填入面板令牌（`.env` 里的 `PANEL_TOKEN`，会记住在本浏览器），自动发现设备串口，可以读取当前配置、写入 WiFi / 令牌 / 主机 / 端口，写入顺序自动处理（最后才触发 WiFi 重连）。页面与设备之间的串口连接空闲 20 秒自动释放，不会和 `pio device monitor` 抢端口。

**触屏**（USB 未连接时的备选）：BOOT 键**双击**进设置页——左侧扫出的 WiFi 列表里点一个网络，弹出软键盘输密码，连接成功自动记住；右侧可改主机 / 端口 / 面板令牌。首次烧录且未配网时，桌面会闪烁 `Setup: BOOT x2` 提示。

**串口**（115200，开发调试用）：

```
WIFI <ssid>
PASS <password>
TOKEN <面板令牌>
HOST desktop-pet.local
PORT <1-65535>
BRIGHT 8-255
OFF          触发关机倒计时（再按 PWR 可取消）
REBOOT       立即重启
SHOW
PET idle|thinking|typing|happy|error|sleeping|auto
```

---

## 从零搭建（开发或第一块板）

### 你需要

| 项 | 说明 |
|---|---|
| 开发板 | 微雪 ESP32-S3-Touch-LCD-3.49 A 款（建议带电池） |
| 微雪 Arduino Demo | 本机要有 LVGL 9 目录，固件用目录联接，**不进 Git** |
| clawd GIF | 来自 [Clawd on Desk](https://github.com/rullerzhou-afk/clawd-on-desk) 的 `assets/gif/`。默认本机路径 `D:\develop\clawd-on-desk\assets\gif`，可设环境变量 `CLAWD_GIF_DIR` |
| 软件 | Node.js ≥ 18、Python 3 + Pillow、PlatformIO Core、Git |
| 账号 | Z.ai Coding Plan 的 API Key；本机已登录 Cursor Pro（读 `%APPDATA%\Cursor\User\globalStorage\state.vscdb`） |

Arduino IDE **不是**依赖。编辑器可用 Cursor / VS Code + PlatformIO。

### 1. 克隆与密钥

```bat
git clone <你的仓库地址>
cd <仓库目录>
copy .env.example .env
```

编辑 `.env`，填入 `ZAI_API_KEY` 和 `PANEL_TOKEN`（自己编一串足够长的随机口令）。

固件侧：

```bat
copy firmware\include\secrets.example.h firmware\include\secrets.h
```

`SECRET_PANEL_TOKEN` 必须与 `.env` 里的 `PANEL_TOKEN` 相同。WiFi 也可写进 `secrets.h`，或烧录后用串口 `WIFI` / `PASS`。

不要提交 `.env`、`secrets.h`。

### 2. 中转服务

```bat
cd server
npm install
npm start
```

管理员跑一次 `server\scripts\allow-firewall.cmd`。浏览器打开上面的本机验收 URL，应看到 JSON（GLM 须 Key 正确；Cursor 须本机已登录）。

### 3. 联接 LVGL

`firmware/lib/lvgl` 是指向微雪 Demo 的联接。没有它编不过固件。在仓库根目录（把 Demo 路径换成你的）：

```bat
mklink /J firmware\lib\lvgl D:\develop\ESP32-S3-Touch-LCD-3.49\Arduino_Libraries\lvgl9\lvgl
```

### 4. 本机 PlatformIO 路径

`firmware/platformio.ini` 里目前有本机路径，克隆后请改成你的环境：

- `core_dir`：PlatformIO 包目录（没有可删掉这行，改用默认 `~/.platformio`）
- `upload_port` / `monitor_port`：你的串口号（Windows 常见 `COMx`）
- `build_flags` 里 `-I.../Network/src`：与 `core_dir` 下的 Arduino 包路径对齐

编译前会自动跑 `firmware/tools/bake_pet.py`（需要 Pillow，以及可读的 GIF 目录；PlatformIO 自带的 Python 没有 Pillow 时会自动回退系统 Python，仍失败则沿用已烤好的帧）。手动烤帧：

```bat
pip install pillow
python firmware\tools\bake_pet.py
```

GIF 不在默认盘符时：

```bat
set CLAWD_GIF_DIR=C:\path\to\clawd-on-desk\assets\gif
python firmware\tools\bake_pet.py
```

### 5. 烧录

先关掉占用串口的监视器。Windows 建议 UTF-8：

```bat
cd firmware
chcp 65001
set PYTHONIOENCODING=utf-8
python -m platformio run -t upload
python -m platformio device monitor
```

板型按 ESP32-S3 + 16MB Flash + 8MB OPI PSRAM，USB CDC。烧完用串口写 `WIFI` / `PASS` / `TOKEN`，确认中转服务已 `npm start`，面板应在几秒内出额度。

### 6. 装 hooks

```bat
node hooks/install.js
```

---

## 仓库结构

| 目录 | 职责 |
|---|---|
| `server/` | 中转服务（Node 标准库 + `bonjour-service`；`src/lib/` 为与 hooks 共享的事件表/工具，`tests/` 为单测 `npm test`） |
| `firmware/` | 额度面板固件（PlatformIO + Arduino + LVGL 9），纯显示；`src/ui/` 界面、`src/bsp/` 板级驱动、`tools/` 烤帧脚本 |
| `hooks/` | Cursor / ZCode 适配器；`install.js` 只追加 |
| `docs/` | 设计方案、1:1 设计稿（桌面 + 设置页）、领域词汇 |

改行为前先读 [CONTEXT.md](CONTEXT.md)（术语）和 [docs/design/01-设计方案.md](docs/design/01-设计方案.md)（API、状态机、ADR）。

## 常见问题

**面板一直连不上电脑**  
中转是否在跑、是否 `0.0.0.0:3737`、防火墙规则是否加上、电脑和板子是否同一局域网。mDNS 失败时串口 `HOST 192.168.x.x` 应急。

**本机浏览器有数据、屏上 `fail`**  
令牌不一致（`.env` / `secrets.h` / 串口 `TOKEN`），或面板没用上 `X-Panel-Token`（固件会带请求头；不要只靠查询参数）。

**烧录 `拒绝访问`**  
COM 被串口监视器占用，先关掉再 upload。

**编不过、找不到 lvgl**  
目录联接没建，不是仓库缺文件。

**桌宠不跟手**  
hooks 是否安装、中转是否在跑、Cursor/ZCode 是否真的走到了工具调用。可用串口 `PET thinking` 确认动画本身正常，再 `PET auto`。

## 文档索引

更细的设计与稿面见 [docs/README.md](docs/README.md)。固件串口速查见 [firmware/README.md](firmware/README.md)。第三方来源与许可见 [docs/credits.md](docs/credits.md)。

## 致谢与第三方

PlanPet 的桌宠形象、屏幕驱动、UI 库等都站在别人的项目上。**贴图版权不属于本仓库**，请先读 [docs/credits.md](docs/credits.md)。

最重要的一条：clawd GIF 来自 **[Clawd on Desk](https://github.com/rullerzhou-afk/clawd-on-desk)**。其程序源码为 AGPL-3.0；[assets/LICENSE](https://github.com/rullerzhou-afk/clawd-on-desk/blob/main/assets/LICENSE) 声明贴图 All Rights Reserved（Clawd 形象属 Anthropic，像素稿为同人、不得商用）。本仓库不拷贝这些 GIF，只在你本机烤帧。hooks 是自写的，不复用 clawd 的 `cursor-hook.js`。
