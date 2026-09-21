# M5 HW ID

One firmware family that **identifies** whatever M5 board you flash:

- ESP32-S3 build → AtomS3 / AtomS3R / CoreS3 / …
- ESP32 build → Core2 / Stick / classic Stack / …

Reports on **USB serial** and **LCD** (if present):

- Chip, rev, flash, PSRAM, MAC  
- M5 board id from M5Unified (with M5GFX ≥ 0.2.27)  
- Display WxH  
- I2C scan on system / Port.A / Echo-style buses  
- Hints (BMI270, LP5562 backlight, ES8311 Echo Base, AXP, …)

## Use

1. Push to GitHub, enable Pages (Actions).  
2. Open the Pages site → **Flash M5 HW ID**.  
3. Read the screen + serial log. Use those facts for real firmware.

## Local

```bash
cd firmware && idf.py set-target esp32s3 && idf.py build flash monitor
```
