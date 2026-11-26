#pragma once

// Pinout ESP32 DevKit V1 (30-Pin)
// Input analog WAJIB menggunakan ADC1 (GPIO 32-39) agar tidak bentrok dengan Wi-Fi.

#define PIN_SENSOR_ADC  34  // ADC1_CH6 (input-only): VOUT modul konverter 4-20 mA (maks 3.3V!)
#define PIN_JOY_X       32  // ADC1_CH4: Joystick VRX (VCC joystick wajib 3.3V)
#define PIN_JOY_Y       33  // ADC1_CH5: Joystick VRY (VCC joystick wajib 3.3V)
#define PIN_JOY_SW      25  // Digital input dengan INPUT_PULLUP: Tombol SW joystick

#define PIN_I2C_SDA     21  // I2C SDA ke LCD 16x2 PCF8574
#define PIN_I2C_SCL     22  // I2C SCL ke LCD 16x2 PCF8574

#define PIN_STATUS_LED   2  // LED bawaan ESP32
