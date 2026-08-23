# 固件（额度面板）

PlatformIO + Arduino 框架 + LVGL 9。完整搭建步骤见仓库根目录 [README.md](../README.md)。

`lib/lvgl` 是指向微雪 Demo 的目录联接，**不进 Git**。`include/secrets.h`、`src/pet_frames.bin`、`src/pet_blob.S` 也不提交。

## 目录

```
src/
├── main.cpp        初始化编排 + loop 周期刷新
├── config.*        NVS 配置 + 串口命令
├── net.*           WiFi / mDNS / 每 2s 轮询快照
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
| **BOOT** | 短按：切换深/浅主题 ｜ 双击：进设置页 ｜ 长按 ≥5 秒松手：重启（松手后才执行，否则 GPIO0 拉低会进下载模式） |
| **PWR** | 电池开机：长按约 3 秒亮屏松手 ｜ 关机：长按约 3 秒 → 左侧卡片进度条 → 3·2·1 倒计时 → 黑屏断电。倒计时期间**再按一次 PWR 取消**。USB 插着永不关机 |
| **RESET** | 硬件复位线（EN），固件读不到，按下直接重启 |

## 触屏设置页（BOOT 双击进入）

- 左卡 WiFi：自动扫描、按信号排序；点网络名弹软键盘输密码，连接成功自动写 NVS（15 秒超时，失败自动回滚旧网络）
- 右卡服务器：主机 / 端口 / 面板令牌点行编辑，「保存并返回」回桌面
- 首次未配网时桌面闪烁 `Setup: BOOT x2` 提示

## 串口（写入 NVS；触屏配网的开发备用通道）

```
WIFI <ssid>
PASS <password>
TOKEN <面板令牌>
HOST desktop-pet.local
BRIGHT 8-255
OFF          触发关机倒计时（再按 PWR 可取消）
REBOOT       重启
SETUP        远程打开设置页（开发调试；SETUP EDIT <1-3> 直接开某字段编辑器，SETUP CLOSE 退出）
SHOW
PET idle|thinking|typing|happy|error|sleeping|auto
```

点时间下方空白切换深/浅主题；点桌宠 poke。

## 关机/重启时序

`power` 任务（core 1，20ms 轮询）独占状态迁移，只发布 volatile 相位快照（按住进度 / 倒计时剩余 / 重启剩余），UI 由主循环持 LVGL 锁每 100ms 渲染。最终断电（背光熄灭 + TCA9554 EXIO6 拉低）仍由 power 任务执行，不与 RTC 的 I2C 读写产生新竞态。
