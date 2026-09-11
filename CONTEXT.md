# 桌面额度面板

个人桌面摆件：一块 ESP32 屏幕同时展示编码套餐额度与一只随会话事件切换的桌宠。

## Language

**额度面板**:
用户桌上的 ESP32 显示设备，只渲染中转服务给出的快照。
_Avoid_: 前端、固件产品、dashboard 设备

**中转服务**:
跑在用户电脑上的进程，负责拉额度、收 hooks、维护桌宠状态，再把聚合结果交给额度面板。
_Avoid_: 后端、gateway、broker

**额度**:
某个编码套餐在当前周期内已用比例。GLM、Cursor、OpenCode Go 是三份独立额度，失败互不影响。
_Avoid_: 余额、账单、token 数（面板只展示百分比与重置时间）

**桌宠**:
额度面板右侧的 clawd 形象，同一时刻只反映一个 **当前代理** 的 **桌宠状态**。
_Avoid_: 宠物、mascot、动画（动画是呈现，不是领域对象）

**当前代理**:
此刻驱动**桌宠动画**的编码工具，取值 `cursor`、`zcode`、`claude`、`opencode` 或空。三条 hooks lane 同时 thinking/typing 时以 zcode（GLM）为准，其次 claude，最后 cursor；opencode 不是 hooks 代理，只在它的进程于本机运行且桌宠 idle 时兜底驱动（typing）。额度卡绿点按各应用是否在跑独立显示，可同时亮；Claude Code 暂不点绿点（它没有属于自己的额度卡）；OpenCode 页（屏显 `OPEN CODE`）的呼吸灯跟的是运行时在场，不是 hooks 事件。
_Avoid_: session、agent 进程、IDE

**桌宠状态**:
桌宠的可见情绪，取值 `idle` / `thinking` / `typing` / `happy` / `error` / `sleeping`。
_Avoid_: 工作态、online、活动中（太含糊）

**Idle**:
桌宠清醒待机：上一轮已结束或尚未开始工具调用，但还不到入睡。
_Avoid_: 空闲、待命、offline

**Sleeping**:
长时间没有任何 hooks 事件之后的入睡态，不是一轮对话结束的默认态。
_Avoid_: idle（二者已区分）

**面板令牌**:
额度面板证明已与这台中转服务配对的共享口令。
_Avoid_: API Key（那是 Z.ai 的）、Cursor token（那是电脑上的登录态）

**换肤**:
深色/浅色主题切换。触发方式：BOOT 键短按，或点状态列中部空白热区。
_Avoid_: 夜间模式、dark mode（口语可以，文档统一叫换肤/主题）

**关机倒计时**:
长按 PWR 约 3 秒触发关机后，在状态列预留位显示的 3·2·1 倒计时；走完才断电，期间再按一次 PWR 取消。按住的前 3 秒显示圆形进度环。
_Avoid_: 关机动画（倒计时是交互反馈，不是装饰动画）、休眠（那是 sleeping）

**开机动画**:
上电后循环播放桌宠各状态动画的开机画面；首批快照数据到达（或超时/点按跳过）后淡入桌面。
_Avoid_: loading 页、启动页（口语可以，文档统一叫开机动画）

**息屏**:
桌宠入睡后再等配置的超时（默认 5 分钟）后面板背光关断、暂停渲染的待机；触摸、按键、串口命令或恢复编码都能唤醒。断网/服务停跑 30 分钟也会兜底息屏。不是关机（那是 PWR 断电），也不是桌宠的 sleeping 态。
_Avoid_: 关屏（口语可以）、休眠（易与 sleeping 混淆）、锁屏

**计划页**:
额度卡上的一屏额度内容。第 1 页是 GLM + Cursor（各占半张卡），第 2 页是 OpenCode Go（屏显 `OPEN CODE`；3 个窗口行，半张卡装不下）。卡上上下滑动在页之间切换，右下角页点指示当前页。
_Avoid_: 卡片、tab、滑动菜单（滑动是翻页手势，不是导航控件）

**设置页**:
BOOT 双击进入的触屏配网界面：WiFi 扫描列表 + 软键盘输密码 + 主机/端口/令牌编辑。串口命令保留为开发备用通道。
_Avoid_: 配网模式（设置页不止配网）、AP 模式（已否决，选了纯触屏方案）

## Relationships

- 一台 **额度面板** 在局域网里用稳定名称找到唯一的 **中转服务**，不把会变动的电脑 IP 当作身份
- **额度面板** 持有 **面板令牌** 才能读快照、中转服务才接受 hooks 上报
- 一份快照含三份 **额度**（GLM、Cursor、OpenCode Go）加上一个 **桌宠**
- 一个 **桌宠** 属于至多一个 **当前代理**
- 一个 **当前代理** 在任一时刻恰好处于一种 **桌宠状态**
- **Idle** 与 **Sleeping** 都是无工具调用，但 **Sleeping** 只在长时间无事件后出现

## Example dialogue

> **Dev:** "Stop 之后 happy 结束了，桌宠是 idle 还是 sleeping？"
> **Domain expert:** "**Idle**。**Sleeping** 只在约 15 分钟没有任何事件之后才进入。"
> **Dev:** "那会话还开着、但暂时没在调工具呢？"
> **Domain expert:** "还是 **Idle**。只有工具进行中才是 typing。"

## Flagged ambiguities

- 初稿写「核心 5 状态」且把「会话活跃无事件」映射为 typing — 已否决。现为 6 态；happy / error 限时结束后进入 **Idle**，不是 typing，也不是 sleeping。
- 采访记录里的 idle 与 v1.0 方案里的 5 态不一致 — 以本词汇表为准。
- 「写死 PC IP」曾被当作默认联网方式 — 已否决，改用 mDNS `plan-pet.local`。2026-08 反转：目标局域网组播不通，mDNS 已整体移除（固件解析 + 服务端广播），面板改为显式配置 PC IP（串口 `HOST` / 触屏设置页，存 NVS）。注意：「写死」指编译期烧进固件——运行期可改的 NVS 配置不算写死，别把两者混称。
- v1.0 把 GLM 的 `TIME_LIMIT` 当成 5 小时窗口 — 已否决。5h/周都是 `TOKENS_LIMIT`；`TIME_LIMIT` 是 MCP 月度，面板不做。
- 「复用 clawd cursor-hook.js」— 已否决。本项目自写适配器，与 clawd 钩子并存。
- 进度条分档按**已用** 70% / 90%，不是余量 90% / 70%（否则中等用量就会变红）。
- OpenCode Go 是**套餐**（第三份额度的来源），不是 hooks **当前代理**：opencode 侧没有 hooks 适配器，不产生 thinking / happy 这类事件。2026-09 起它有「**运行时在场**」：中转服务每 5 秒查 OpenCode 会话库最近 5 分钟内有无写入（桌面版是 Electron 常驻应用，进程/窗口在 ≠ 在用），最近有会话活动 → OpenCode 页（屏显 `OPEN CODE`）呼吸灯亮、桌宠 idle 时兜底 typing。它不喂 15 分钟睡眠计时——挂着不干活不算活动。第三个 hooks **当前代理**是 Claude Code，别把两者混起来。
- Claude Code 是**代理**（驱动桌宠动画），不是**额度来源**：它是 GLM / OpenCode Go 等套餐的客户端，没有自己的额度卡，所以不点绿点。
- 额度卡按**页**翻（上下滑动），不是一页一个 plan — GLM 与 Cursor 仍然同页；只有装不下 3 行的 OpenCode Go 单独一页。
- GLM 周窗口、Cursor 账期倒计时写在对应额度卡上，不是月费扣款日（月费日不在已选定字段里）。
