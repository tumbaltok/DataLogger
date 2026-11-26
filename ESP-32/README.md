# Data Logger 4–20 mA ESP32 (30-Pin) + SCADA STB (EMQX, Node-RED, Tailscale)

## Struktur Proyek
- [platformio.ini](platformio.ini) — Konfigurasi PlatformIO untuk `esp32dev` + library (`LiquidCrystal_I2C`, `PubSubClient`, `ArduinoJson`).
- [include/pins.h](include/pins.h) — Pemetaan pin ESP32 30-Pin (ADC1 `GPIO 32, 33, 34`, tombol `GPIO 25`, I2C `GPIO 21, 22`).
- [include/config.h](include/config.h) — Pengaturan Wi-Fi, IP LAN STB (EMQX), topik MQTT, alamat LCD I2C, dan threshold joystick.
- [include/calibration.h](include/calibration.h) & [src/calibration.cpp](src/calibration.cpp) — Penyimpanan NVS (`Preferences`), preset sensor kualitas air (`pH`, `TDS`, `Turbidity`, `EC`, `DO`), dan perhitungan kalibrasi 2-titik.
- [include/ui_lcd.h](include/ui_lcd.h) & [src/ui_lcd.cpp](src/ui_lcd.cpp) — Menu LCD 16x2 I2C dan navigasi joystick analog non-blocking.
- [include/mqtt_client.h](include/mqtt_client.h) & [src/mqtt_client.cpp](src/mqtt_client.cpp) — Klien MQTT EMQX untuk telemetry & kalibrasi jarak jauh dari SCADA.
- [src/main.cpp](src/main.cpp) — Sampling ADC + filter EMA + loop utama.
- [nodered/datalogger_scada_flow.json](nodered/datalogger_scada_flow.json) — Flow Node-RED siap import (Dashboard SCADA + kalibrasi 2-titik + CSV logger).
- [docs/wiring_esp32_datalogger.svg](docs/wiring_esp32_datalogger.svg) — Diagram wiring SVG ESP32 30-Pin.
- [docs/FLOW_CARA_KERJA.md](docs/FLOW_CARA_KERJA.md) — Dokumentasi lengkap flow cara kerja, topik MQTT, dan prosedur uji signal generator.

## Quick Start
1. Edit `WIFI_SSID`, `WIFI_PASSWORD`, dan `MQTT_BROKER_HOST` (IP LAN STB) di [include/config.h](include/config.h).
2. Build & flash ke ESP32 dari folder `ESP-32` (atau pakai `-d ESP-32` dari root):
   ```powershell
   pio run -d ESP-32 -t upload
   pio device monitor
   ```
3. Di Node-RED STB (pastikan `node-red-dashboard` terpasang):
   - Menu **Import** -> pilih `nodered/datalogger_scada_flow.json`.
   - Sesuaikan path file log CSV pada node `Simpan Log CSV` jika diinginkan, lalu klik **Deploy**.
   - Akses dashboard dari lokal maupun remote lewat IP **Tailscale** STB (`http://<IP-TAILSCALE-STB>:1880/ui`).
