# ESP32 桌面额度面板 · 项目文档

桌面摆件：ESP32-S3 触摸屏面板，实时显示 Cursor Pro / GLM Coding Plan 额度 + clawd 桌宠（随 Cursor/ZCode 编码事件联动）。

**当前状态：阶段 1–3 已落地。阶段 4 hooks 适配器已开工。**

## 文档索引

| 文档 | 说明 | 状态 |
|---|---|---|
| [../CONTEXT.md](../CONTEXT.md) | 领域词汇（额度面板 / 中转服务 / 桌宠状态） | 随评审更新 |
| [design/01-设计方案.md](./design/01-设计方案.md) | 架构、接口契约、状态机、UI、选型、风险 | v1.5 已定稿（2026-08-22） |
| [design/screen-layout.html](./design/screen-layout.html) | 屏幕设计稿（640×172 真机 1:1，深浅双主题） | v11 定稿 |
| plan.md | 开发计划（任务拆解、排期、验收） | 未写 |

## 快速事实

- 硬件：微雪 ESP32-S3-Touch-LCD-3.49 A 款带锂电池（SKU=32373），物理 172×640，横屏 640×172
- 架构：PC 端 Node.js 中转服务（`:3737`，mDNS `desktop-pet.local`）+ ESP32 纯显示端（WiFi 轮询，带面板令牌）
- 桌宠 6 态：idle / thinking / typing / happy / error / sleeping
- 构建：PlatformIO + Arduino 框架；Arduino IDE 不是依赖
- 代码：`server/` 中转服务；`firmware/` 额度面板（PlatformIO）；`hooks/` Cursor/ZCode 适配器
- 启动中转：复制 `.env.example` 为 `.env`，填入 Z.ai Key 与面板令牌，然后 `cd server && npm start`
- 安装 hooks（追加，不覆盖 clawd）：`node hooks/install.js`
- 本机验收：`GET http://127.0.0.1:3737/api/dashboard?token=<PANEL_TOKEN>`（仅 127.0.0.1）；局域网请用请求头 `X-Panel-Token`
- 烧固件：`cd firmware && pio run -t upload`（见 `firmware/README.md`）
