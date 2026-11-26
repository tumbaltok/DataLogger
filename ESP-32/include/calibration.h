#pragma once
#include <Arduino.h>

struct SensorPreset {
    const char* name;
    const char* unit;
    float defaultLow;
    float defaultHigh;
    uint8_t decimals;
};

constexpr uint8_t PRESET_COUNT = 5;
extern const SensorPreset SENSOR_PRESETS[PRESET_COUNT];

struct CalibrationConfig {
    uint8_t presetIndex;
    char sensorName[16];
    char unit[12];
    uint8_t decimals;

    // Kalibrasi loop hardware (mV ADC <-> mA) menggunakan signal generator 4-20 mA
    float mvAt4mA;
    float mvAt20mA;

    // Kalibrasi 2-titik sensor (mA <-> nilai engineering unit)
    float calMaLow;
    float valLow;
    float calMaHigh;
    float valHigh;
};

class CalibrationManager {
public:
    void begin();
    bool save();
    void applyPreset(uint8_t index, bool resetPoints = true);
    void setCustomSensor(const char* name, const char* unit, float valLow, float valHigh, uint8_t decimals);

    bool setLoop4mA(float measuredMv);
    bool setLoop20mA(float measuredMv);
    bool calibrateLowPoint(float measuredMa, float targetValue);
    bool calibrateHighPoint(float measuredMa, float targetValue);
    bool setDirectPoints(float maLow, float vLow, float maHigh, float vHigh);
    void resetDefaults();

    float mvToMilliAmps(float adcMv) const;
    float milliAmpsToValue(float mA) const;
    const char* evaluateFault(float mA) const;

    const CalibrationConfig& data() const { return _cfg; }
    int findPresetByName(const char* name) const;

private:
    CalibrationConfig _cfg{};
    static constexpr uint32_t MAGIC_VERSION = 0x444C3031; // "DL01"
};
