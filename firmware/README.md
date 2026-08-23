# 固件（额度面板）

PlatformIO + Arduino 框架 + LVGL 9。完整搭建步骤见仓库根目录 [README.md](../README.md)。

`lib/lvgl` 是指向微雪 Demo 的目录联接，**不进 Git**。`include/secrets.h`、`src/pet_frames.bin`、`src/pet_blob.S` 也不提交。

## 目录

```
src/
├── main.cpp        初始化编排 + loop 周期刷新
├── config.*        NVS 配置 + 串口命令
├── net.*           WiFi / mDNS / 每 2s 轮询快照
├── standby.*       息屏状态机（触发/唤醒条件见下）
├── power.*         电源锁存（TCA9554）+ PWR/BOOT 按键 + 关机倒计时 + 电池 ADC
├── rtc.*           PCF85063 掉线走时
├── ui/             界面：ui（桌面布局）、ui_theme、ui_pet、ui_cards、
│                   ui_settings（触屏配网页）、pet_frames.h（烤帧契约）
└── bsp/            板级：i2c、背光 PWM、lvgl_port、axs15231b/、touch/
tools/              bake_pet.py 烤帧 + pio_bake_pet.py 构建桥
```

## 烧录

关掉占用串口的监视器后，在 `firmware/`：

```
python -m platformio run -t upload
python -m platformio device monitor
```

`platformio.ini` 里的 `core_dir`、COM 口、`-I...Network/src` 按本机改。监视波特率 115200。

## 按键

| 键 | 操作 |
|---|---|
| **BOOT** | 短按：切换深/浅主题 ｜ 双击：进设置页 ｜ 长按 ≥5 秒松手：重启（松手后才执行，否则 GPIO0 拉低会进下载模式）。息屏中单击/双击只做唤醒 |
| **PWR** | 电池开机：长按约 3 秒亮屏松手 ｜ 关机：长按约 3 秒 → 左侧卡片圆形进度环 → 3·2·1 倒计时 → 黑屏断电。倒计时期间**再按一次 PWR 取消**。USB 插着永不关机 |
| **RESET** | 硬件复位线（EN），固件读不到，按下直接重启 |

## 开机动画

上电后屏幕中央循环播放桌宠全部状态动画（每个约 1.3 秒），**首批快照数据到达后 300ms 淡入桌面**；点按屏幕可跳过。未配 WiFi 的板子 2.5 秒后交给桌面（显示配网引导），其余最多 10 秒兜底。

## 息屏

桌宠入睡（约 15 分钟无事件）后再等 `SLEEP` 配置的分钟数（默认 5，0=关闭）即息屏：背光真关断、暂停渲染，轮询放缓到 10 秒。本地触摸同样要静默满超时；30 分钟没有任何成功轮询（服务没跑/断网/未配网）也会兜底息屏。唤醒：触摸屏幕、BOOT 键、串口任意命令、长按 PWR，或恢复编码（桌宠离开 sleeping，最多约 10 秒延迟）自动亮屏，300ms 淡入。

wifi 图标三态：红 = WiFi 断开；琥珀 = WiFi 通但中转服务失联（约 15 秒无成功快照，屏上数据为旧值）；灰 = 正常。

## 电量说明

板子没有电量计芯片，电量为电压估算（3.40–4.20V 线性映射 + 2% 迟滞）。充电电流会抬升端电压，因此**充电中百分比冻结在插线瞬间的读数**（保证插拔一致），拔线后由 EMA 在十几秒内平滑恢复。USB 供电且无电池时显示 `--`。

## 触屏设置页（BOOT 双击进入）

- 左卡 WiFi：自动扫描、按信号排序；点网络名弹软键盘输密码，连接成功自动写 NVS（15 秒超时，失败自动回滚旧网络）
- 右卡服务器：主机 / 端口 / 面板令牌点行编辑，「保存并返回」回桌面
- 首次未配网时桌面闪烁 `Setup: BOOT x2` 提示

## 串口（写入 NVS；触屏配网的开发备用通道）

```
WIFI <ssid>
PASS <password>
TOKEN <面板令牌>
HOST plan-pet.local
BRIGHT 8-255
SLEEP <0-90>  息屏超时（分钟，桌宠入睡后起算，0=关闭）；SLEEP NOW 立即息屏，SLEEP 查看
OFF          触发关机倒计时（再按 PWR 可取消）
REBOOT       重启
SETUP        远程打开设置页（开发调试；SETUP EDIT <1-3> 直接开某字段编辑器，SETUP CLOSE 退出）
SHOW
PET idle|thinking|typing|happy|error|sleeping|auto
```

点时间下方空白切换深/浅主题；点桌宠 poke。

## 关机/重启时序

`power` 任务（core 1，20ms 轮询）独占状态迁移，只发布 volatile 相位快照（按住进度 / 倒计时剩余 / 重启剩余），UI 由主循环持 LVGL 锁每 100ms 渲染。最终断电（背光熄灭 + TCA9554 EXIO6 拉低）仍由 power 任务执行，不与 RTC 的 I2C 读写产生新竞态。
