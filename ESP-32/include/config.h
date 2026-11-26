#pragma once

// ============================================================================
// KONFIGURASI JARINGAN & EMQX BROKER (SESUAIKAN DENGAN JARINGAN LOKAL STB)
// ============================================================================
#define WIFI_SSID           "GANTI_SSID_WIFI"
#define WIFI_PASSWORD       "GANTI_PASSWORD_WIFI"

#define MQTT_BROKER_HOST    "192.168.1.100"   // IP LAN STB tempat EMQX berjalan
#define MQTT_BROKER_PORT    1883
#define MQTT_USER           ""                // Kosongkan jika EMQX allow anonymous
#define MQTT_PASSWORD       ""
#define DEVICE_ID           "wq01"

// Topik MQTT
#define TOPIC_STATUS        "datalogger/" DEVICE_ID "/status"
#define TOPIC_TELEMETRY     "datalogger/" DEVICE_ID "/telemetry"
#define TOPIC_CAL_SET       "datalogger/" DEVICE_ID "/calibration/set"
#define TOPIC_CAL_STATUS    "datalogger/" DEVICE_ID "/calibration/status"

// ============================================================================
// KONFIGURASI HARDWARE & TIMING
// ============================================================================
#define LCD_I2C_ADDR        0x27              // Umumnya 0x27 atau 0x3F
#define LCD_COLS            16
#define LCD_ROWS            2

// Tegangan default ADC (mV) saat loop 4 mA dan 20 mA (bisa dikalibrasi ulang via menu/MQTT)
#define DEFAULT_MV_4MA      600.0f            // Contoh jika 4mA = 0.60V
#define DEFAULT_MV_20MA     3000.0f           // Contoh jika 20mA = 3.00V (aman < 3.3V)

// Batas deteksi kabel putus / over-range loop 4-20 mA
#define LOOP_UNDER_MA       3.50f
#define LOOP_OVER_MA        20.80f

// Sampling ADC
#define ADC_SAMPLES         32
#define TELEMETRY_INTERVAL_MS   2000UL
#define LCD_REFRESH_MS          250UL
#define MENU_IDLE_TIMEOUT_MS    60000UL

// Orientasi Joystick (ubah ke true jika arah terbalik saat dipasang)
#define JOY_INVERT_X        false
#define JOY_INVERT_Y        false
#define JOY_LOW_THRESH_MV   750
#define JOY_HIGH_THRESH_MV  2550
#define JOY_REPEAT_MS       220UL
#define JOY_LONG_PRESS_MS   1000UL
