# 固件（额度面板）

PlatformIO + Arduino 框架 + 微雪 LVGL 9。把旁边 Demo 里的 `Arduino_Libraries/lvgl9/lvgl` 做目录联接到 `firmware/lib/lvgl`（该目录不进 Git）。

## 烧录

在 `firmware/`：

```
pio run -t upload
pio device monitor
```

板型：ESP32S3 Dev Module 同类配置（16MB Flash、OPI PSRAM、USB CDC）。COM 口号与 Arduino IDE 里一致。

## 串口配网（115200）

```
WIFI <ssid>
PASS <password>
TOKEN <面板令牌>
HOST desktop-pet.local
SHOW
```

配完会写入 NVS，重启后自动连。电脑上中转服务须先 `npm start`。点左侧时间下方空白切换深/浅色；点桌宠播放戳一下的交互动画。

```
PET idle|thinking|typing|happy|error|sleeping|auto
```

`PET auto` 跟随中转服务的 `pet.state`。桌宠帧由 `tools/bake_pet.py` 在编译前烤成 RGB565。
