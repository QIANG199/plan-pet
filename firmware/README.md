# 固件（额度面板）

PlatformIO + Arduino 框架 + LVGL 9。完整搭建步骤见仓库根目录 [README.md](../README.md)。

`lib/lvgl` 是指向微雪 Demo 的目录联接，**不进 Git**。`include/secrets.h`、`src/pet_frames.bin`、`src/pet_blob.S` 也不提交。

## 烧录

关掉占用串口的监视器后，在 `firmware/`：

```
python -m platformio run -t upload
python -m platformio device monitor
```

`platformio.ini` 里的 `core_dir`、COM 口、`-I...Network/src` 按本机改。监视波特率 115200。

## 串口（写入 NVS）

```
WIFI <ssid>
PASS <password>
TOKEN <面板令牌>
HOST desktop-pet.local
BRIGHT 8-255
SHOW
PET idle|thinking|typing|happy|error|sleeping|auto
```

点时间下方空白切换深/浅主题；点桌宠 poke。纯电池：PWR 长按约 3 秒亮屏=开机，再长按约 3 秒屏灭=关机。USB 插着时不会关机。
