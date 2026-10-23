# ESP32 Robot Eyes 機器人眼睛

用 **ESP32-S3** 加一片 **2.4 吋 ST7789 彩色螢幕**（240x320、SPI）做出會動的卡通機器人眼睛。
下載、燒錄，眼睛就活起來了。之後可以用這個函式庫做你自己的機器人。

[English](README.md)

![兩種風格、六種表情](docs/img/eyes_contact_sheet.png)

*以上畫面是直接從 ESP32 的畫面記憶體抓下來的，跟螢幕上顯示的完全一樣。*

- 兩種風格：**扁平**（藍綠橢圓眼加小瞳孔）、**水亮**（外圈、虹膜、瞳孔、反光點）
- 六種表情：平常、開心、難過、生氣、想睡、驚訝
- 會自己眨眼（偶爾連眨兩下）、四處看、隨機換表情，看起來像活的
- 40 MHz SPI 下大約每秒 38 張
- 小巧的 Arduino 函式庫（`RobotEyes`），可以用自己的程式控制眼睛

## 需要的東西

| 零件 | 說明 |
|---|---|
| ESP32-S3 開發板 | DevKitC-1 樣式的板子都可以（N4、N8、N16R8 等） |
| 2.4 吋 TFT，ST7789，240x320，SPI，8 支腳 | 腳位印著 `GND VCC SCL SDA RESET DC CS BL`；搜尋「2.4寸 ST7789 240x320 SPI 8pin」 |
| 母對母杜邦線 8 條 | |
| USB 線 | 要能傳資料的線，不能是只能充電的線 |

## 1. 接線

| 螢幕 | ESP32-S3 |
|---|---|
| GND | GND |
| VCC | 3V3 |
| SCL | GPIO 12 |
| SDA | GPIO 11 |
| RESET | GPIO 14 |
| DC | GPIO 9 |
| CS | GPIO 10 |
| BL | GPIO 13 |

這類模組上的 `SCL` / `SDA` 其實是 SPI 的時脈線和資料線，不是 I2C。VCC 請接 3V3。
請再次確認 **CS 和 RESET** 沒有插反，插反的話螢幕會全白。

## 2. 燒錄

需要 Python 3 和 [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html)
（或 VS Code 的 PlatformIO 外掛）。

```bash
pip install platformio
git clone https://github.com/ElroyChenWI/esp32-robot-eyes.git
cd esp32-robot-eyes
pio run -t upload
```

第一次編譯會下載 ESP32 的編譯工具，要花幾分鐘。PlatformIO 會自動找到板子的序列埠；如果電腦接了好幾個裝置，
可以加上 `--upload-port COM5`（Windows）或 `--upload-port /dev/ttyUSB0`。想看板子的輸出訊息：`pio device monitor`。

如果燒錄時連不上板子：按住 **BOOT**、點一下 **RST**、放開 **BOOT**，再燒一次。

**現成的韌體檔：** 每個 [release](../../releases) 都附有 `robot-eyes-esp32s3.bin`，不用編譯就能直接燒：
`esptool.py --chip esp32s3 write_flash 0x0 robot-eyes-esp32s3.bin`。

## 3. 使用

| 操作 | 效果 |
|---|---|
| BOOT 短按 | 切換風格（扁平 / 水亮） |
| BOOT 長按 1 秒 | 顏色反轉（螢幕顯示白底時用） |

兩個設定都會存起來，斷電重開後還在。

序列埠指令（115200 baud，每個指令一個字元）：

| 按鍵 | 動作 |
|---|---|
| `0`–`5` | 表情：平常、開心、難過、生氣、想睡、驚訝 |
| `h` `l` `k` `j` `x` | 往左 / 右 / 上 / 下看 / 看正前方 |
| `b` | 眨眼 |
| `s` / `i` | 切換風格 / 顏色反轉 |
| `p` / `r` | 暫停 / 恢復自動行為 |
| `c` | 抓下目前畫面（見 [tools/capture.py](tools/capture.py)） |

## 常見問題

| 狀況 | 原因 / 解法 |
|---|---|
| 背光亮著但全白 | 螢幕沒收到任何指令。檢查 SCL、SDA、CS、DC、RESET，**CS 和 RESET 插反**很常見 |
| 完全黑 | 沒電或背光沒開：檢查 VCC、GND、BL |
| 白底、紅色或粉紅色眼睛 | 你的螢幕需要顏色反轉：長按 BOOT 1 秒 |
| 畫面花掉 | 杜邦線鬆了或太長。重新插緊，或降低 SPI 速度（見下方） |
| 編譯失敗，出現 `opening output file ... No such file or directory`（Windows） | 專案路徑裡有單引號，例如使用者資料夾叫 `C:\Users\Joe's PC`。改 clone 到單純的路徑，例如 `C:\dev\esp32-robot-eyes` |

## 開發你自己的應用

```
src/                    RobotEyes 函式庫（畫圖、表情、自動行為、螢幕設定）
examples/AutoEyes/      預設韌體：按鍵 + 設定記憶 + 序列指令
examples/Basic/         最精簡的範例
tools/capture.py        把螢幕畫面存成 PNG（透過序列指令 'c'）
tools/merge_bin.py      編譯時產生單一可燒錄 .bin 的步驟
platformio.ini          編譯 examples/AutoEyes
```

可以從修改 `examples/AutoEyes/AutoEyes.ino` 開始，或把 `platformio.ini` 裡的 `src_dir` 指向你自己的程式。
函式庫的基本用法：

```cpp
#include <RobotEyes.h>

RobotEyesDisplay display;  // 使用上表的預設腳位
RobotEyes eyes(display);

void setup() {
  eyes.begin();
  eyes.setAutoMood(false);         // 表情由你的程式控制
  eyes.setMood(EyeMood::Happy);
}

void loop() {
  eyes.update();                   // 更新動畫、畫圖並送出一張畫面；每次 loop 都要呼叫
}
```

| 函式 | 用途 |
|---|---|
| `setMood(EyeMood::Neutral / Happy / Sad / Angry / Sleepy / Surprised)` | 換表情（會平滑過渡） |
| `setStyle(EyeStyle::Flat / Glossy)` | 換風格 |
| `lookAt(x, y)` | 看的方向，兩軸各為 -1 到 1 |
| `blink()` | 眨一次眼 |
| `setAutoBlink(bool)`、`setIdleMode(bool)`、`setAutoMood(bool)` | 自動眨眼 / 四處看 / 換表情（預設全開） |
| `setFlatColors(eye, pupil)` | 扁平風的顏色（RGB565），例如 `lgfx::color565(255, 180, 0)` |
| `setInvert(bool)` | 顏色反轉 |
| `canvas()` | 最後畫好的那張畫面（`LGFX_Sprite`、RGB565），例如可以傳到別的地方 |

接線不一樣時：

```cpp
RobotEyesPins pins;
pins.cs = 5;
pins.rst = 6;
pins.bl = -1;                              // BL 直接接 3V3
RobotEyesDisplay display(pins, 20000000);  // 可選：線很長時把 SPI 降到 20 MHz
```

### Arduino IDE

目前只在 PlatformIO CLI 下測試過。Arduino IDE 照一般步驟應該可以用，但還沒實測：
在開發板管理員安裝 *esp32 by Espressif*，在程式庫管理員安裝 *LovyanGFX*，用
*草稿碼 → 匯入程式庫 → 加入 .ZIP 程式庫* 加入這個 repo，開啟 *檔案 → 範例 → ESP32 Robot Eyes → AutoEyes*，
開發板選 *ESP32S3 Dev Module*，然後上傳。

## 原理

每一張畫面都先畫在 320x200 的記憶體畫布上，再一次送到螢幕，所以不會閃爍。每隻眼睛是一個填滿的形狀（橢圓，或帶虹膜的圓角方塊），
表情則靠黑色的「眼皮」一欄一欄蓋出來：難過和生氣是斜的上眼皮，開心是從下方往上的弧形，眨眼則是上下眼皮合起來。
形狀會平滑過渡到每個表情的目標，視線移動則很快，像真的眼睛一樣。

## 致謝

外觀靈感來自 [FluxGarage RoboEyes](https://github.com/FluxGarage/RoboEyes)（OLED 版）。本專案是另外針對彩色 TFT 重新實作的，
沒有使用它的程式碼。畫圖使用 [LovyanGFX](https://github.com/lovyan03/LovyanGFX)。

## 授權

MIT，見 [LICENSE](LICENSE)。
