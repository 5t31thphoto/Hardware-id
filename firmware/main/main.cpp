// M5 HW ID — factual board probe for firmware targeting.
// Prints chip / flash / PSRAM / M5 board / display / I2C map to serial + LCD.
#include <Arduino.h>
#include <M5Unified.h>
#include <Wire.h>
#include <esp_log.h>
#include <esp_rom_sys.h>
#include <esp_chip_info.h>
#include <esp_flash.h>
#include <esp_mac.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <soc/efuse_reg.h>
#include <nvs_flash.h>
#include <vector>
#include <cstring>

static const char MARKER[] = "M5HWID_v1";

extern "C" void initArduino();

struct I2CHit {
    uint8_t addr;
    const char* guess;
};

static const char* guess_i2c(uint8_t a) {
    switch (a) {
        case 0x18: case 0x19: return "ES8311/codec?";
        case 0x20: case 0x21: return "AW9523/GPIO?";
        case 0x34: return "AXP192?";
        case 0x36: return "AW88298?";
        case 0x38: return "FT6336/touch?";
        case 0x3C: case 0x3D: return "OLED/SSD1306?";
        case 0x40: return "INA/power?";
        case 0x51: return "BM8563/RTC?";
        case 0x68: return "BMI270/MPU?";
        case 0x76: case 0x77: return "BME/BMP?";
        case 0x30: return "LP5562/backlight?";
        default: return "";
    }
}

static const char* board_name(int id) {
    switch (id) {
        case 1: return "M5Stack";
        case 2: return "Core2";
        case 3: return "StickC";
        case 4: return "StickCPlus";
        case 5: return "StickCPlus2";
        case 10: return "CoreS3";
        case 11: return "AtomS3";
        case 12: return "Dial";
        case 14: return "Cardputer";
        case 17: return "CoreS3SE";
        case 18: return "AtomS3R";
        case 137: return "AtomS3Lite";
        case 143: return "AtomS3RExt";
        case 144: return "AtomS3RCam";
        default: return "unknown/other";
    }
}

static void scan_bus(TwoWire& bus, int sda, int scl, const char* tag,
                     std::vector<I2CHit>& out) {
    bus.end();
    bus.begin(sda, scl, 100000);
    delay(5);
    ESP_LOGI("HWID", "I2C scan %s SDA=%d SCL=%d", tag, sda, scl);
    esp_rom_printf("I2C %s SDA=%d SCL=%d\r\n", tag, sda, scl);
    for (uint8_t a = 1; a < 127; a++) {
        bus.beginTransmission(a);
        if (bus.endTransmission() == 0) {
            const char* g = guess_i2c(a);
            out.push_back({a, g});
            ESP_LOGI("HWID", "  found 0x%02X %s", a, g);
            esp_rom_printf("  0x%02X %s\r\n", a, g);
        }
    }
}

static void line(String& buf, const char* fmt, ...) {
    char tmp[160];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    buf += tmp;
    buf += '\n';
    ESP_LOGI("HWID", "%s", tmp);
    esp_rom_printf("%s\r\n", tmp);
}

extern "C" void app_main(void) {
    esp_rom_printf("\r\n=== %s ===\r\n", MARKER);
    nvs_flash_erase();
    nvs_flash_init();
    initArduino();

    // ---- Silicon facts (no M5 yet) ----
    String report;
    line(report, "=== M5 HW ID %s ===", MARKER);

    esp_chip_info_t chip;
    esp_chip_info(&chip);
    const char* model = "unknown";
#if CONFIG_IDF_TARGET_ESP32
    model = "ESP32";
#elif CONFIG_IDF_TARGET_ESP32S3
    model = "ESP32-S3";
#elif CONFIG_IDF_TARGET_ESP32C3
    model = "ESP32-C3";
#elif CONFIG_IDF_TARGET_ESP32C6
    model = "ESP32-C6";
#endif
    line(report, "Chip: %s cores=%d rev=%d", model, chip.cores, chip.revision);
    line(report, "Features: %s%s%s",
         (chip.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi " : "",
         (chip.features & CHIP_FEATURE_BLE) ? "BLE " : "",
         (chip.features & CHIP_FEATURE_EMB_FLASH) ? "embFlash " : "");

    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);
    line(report, "Flash: %u KB", (unsigned)(flash_size / 1024));

    size_t psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    line(report, "PSRAM: %u KB", (unsigned)(psram / 1024));

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    line(report, "MAC: %02X:%02X:%02X:%02X:%02X:%02X",
         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    line(report, "Free heap: %u", (unsigned)esp_get_free_heap_size());

    // ---- M5 autodect ----
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    cfg.clear_display = true;
    M5.begin(cfg);

    int bid = (int)M5.getBoard();
    line(report, "M5 board id: %d (%s)", bid, board_name(bid));
    line(report, "Display: %dx%d", M5.Display.width(), M5.Display.height());

    // ---- I2C scans: common M5 buses ----
    std::vector<I2CHit> hits;
#if CONFIG_IDF_TARGET_ESP32S3
    // AtomS3R system bus (LP5562, BMI270)
    scan_bus(Wire, 45, 0, "sys(45/0)", hits);
    // Atom Grove / Port.A style
    scan_bus(Wire, 2, 1, "portA(2/1)", hits);
    // Echo Base style
    scan_bus(Wire, 38, 39, "echo(38/39)", hits);
#else
    // Core2 / classic: Port.A often 32/33, internal 21/22
    scan_bus(Wire, 21, 22, "int(21/22)", hits);
    scan_bus(Wire, 32, 33, "portA(32/33)", hits);
#endif

    // de-dupe addresses for summary
    bool seen[128] = {};
    String i2c_summary = "I2C:";
    for (auto& h : hits) {
        if (seen[h.addr]) continue;
        seen[h.addr] = true;
        char bit[32];
        snprintf(bit, sizeof(bit), " %02X", h.addr);
        i2c_summary += bit;
    }
    line(report, "%s", i2c_summary.c_str());

    // IMU presence
    bool has_bmi = seen[0x68];
    bool has_lp5562 = seen[0x30];
    bool has_es8311 = seen[0x18] || seen[0x19];
    bool has_axp = seen[0x34];
    line(report, "Flags: BMI270=%d LP5562=%d ES8311=%d AXP=%d",
         has_bmi, has_lp5562, has_es8311, has_axp);

    // Suggested target hint
    const char* hint = "generic";
    if (bid == 18 || (has_bmi && has_lp5562)) hint = "AtomS3R (+Echo if ES8311)";
    else if (bid == 11) hint = "AtomS3";
    else if (bid == 10 || bid == 17) hint = "CoreS3";
    else if (bid == 2 || has_axp) hint = "Core2/AXP";
    line(report, "HINT: %s", hint);
    line(report, "Use M5GFX>=0.2.27 M5Unified>=0.2.17");
    line(report, "=== end HWID ===");

    // ---- Draw on LCD if we have one ----
    if (M5.Display.width() > 0 && M5.Display.height() > 0) {
        M5.Display.setBrightness(255);
        M5.Display.fillScreen(TFT_BLACK);
        M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
        M5.Display.setTextSize(1);
        M5.Display.setCursor(0, 0);
        // Fit as much as possible
        M5.Display.printf("%s\n", MARKER);
        M5.Display.printf("%s rev%d\n", model, chip.revision);
        M5.Display.printf("F%uK P%uK\n",
                          (unsigned)(flash_size / 1024),
                          (unsigned)(psram / 1024));
        M5.Display.printf("board %d %s\n", bid, board_name(bid));
        M5.Display.printf("%dx%d\n", M5.Display.width(), M5.Display.height());
        M5.Display.printf("I2C");
        for (int a = 1; a < 128; a++)
            if (seen[a]) M5.Display.printf(" %02X", a);
        M5.Display.printf("\n");
        M5.Display.printf("BMI%d LP%d ES%d\n", has_bmi, has_lp5562, has_es8311);
        M5.Display.printf("%s\n", hint);
        M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
        M5.Display.printf("serial=full report");
    }

    // Keep alive; button reprints
    for (;;) {
        M5.update();
        if (M5.BtnA.wasClicked() || M5.BtnB.wasClicked() || M5.BtnC.wasClicked()) {
            esp_rom_printf("%s\r\n", report.c_str());
            M5.Display.setBrightness(255);
        }
        delay(20);
    }
}
