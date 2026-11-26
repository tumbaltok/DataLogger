#include "calibration.h"
#include "config.h"
#include <Preferences.h>
#include <math.h>
#include <string.h>

const SensorPreset SENSOR_PRESETS[PRESET_COUNT] = {
    {"pH",        "pH",    0.0f,  14.0f,   2},
    {"TDS",       "ppm",   0.0f,  1000.0f, 0},
    {"Turbidity", "NTU",   0.0f,  1000.0f, 1},
    {"EC",        "uS/cm", 0.0f,  2000.0f, 0},
    {"DO",        "mg/L",  0.0f,  20.0f,   2},
};

static Preferences prefs;

void CalibrationManager::begin() {
    prefs.begin("datalogger", false);
    uint32_t magic = prefs.getUInt("magic", 0);

    if (magic != MAGIC_VERSION) {
        _cfg.mvAt4mA = DEFAULT_MV_4MA;
        _cfg.mvAt20mA = DEFAULT_MV_20MA;
        applyPreset(0, true);
        save();
        return;
    }

    _cfg.presetIndex = prefs.getUChar("preset", 0);
    if (_cfg.presetIndex >= PRESET_COUNT && _cfg.presetIndex != 255) {
        _cfg.presetIndex = 0;
    }

    String sName = prefs.getString("name", SENSOR_PRESETS[0].name);
    String sUnit = prefs.getString("unit", SENSOR_PRESETS[0].unit);
    strncpy(_cfg.sensorName, sName.c_str(), sizeof(_cfg.sensorName) - 1);
    _cfg.sensorName[sizeof(_cfg.sensorName) - 1] = '\0';
    strncpy(_cfg.unit, sUnit.c_str(), sizeof(_cfg.unit) - 1);
    _cfg.unit[sizeof(_cfg.unit) - 1] = '\0';

    _cfg.decimals  = prefs.getUChar("dec", SENSOR_PRESETS[0].decimals);
    _cfg.mvAt4mA   = prefs.getFloat("mv4", DEFAULT_MV_4MA);
    _cfg.mvAt20mA  = prefs.getFloat("mv20", DEFAULT_MV_20MA);
    _cfg.calMaLow  = prefs.getFloat("ma_lo", 4.0f);
    _cfg.valLow    = prefs.getFloat("val_lo", 0.0f);
    _cfg.calMaHigh = prefs.getFloat("ma_hi", 20.0f);
    _cfg.valHigh   = prefs.getFloat("val_hi", 14.0f);

    if (fabsf(_cfg.mvAt20mA - _cfg.mvAt4mA) < 50.0f) {
        _cfg.mvAt4mA = DEFAULT_MV_4MA;
        _cfg.mvAt20mA = DEFAULT_MV_20MA;
    }
    if (fabsf(_cfg.calMaHigh - _cfg.calMaLow) < 0.1f) {
        _cfg.calMaLow = 4.0f;
        _cfg.calMaHigh = 20.0f;
    }
}

bool CalibrationManager::save() {
    prefs.putUInt("magic", MAGIC_VERSION);
    prefs.putUChar("preset", _cfg.presetIndex);
    prefs.putString("name", _cfg.sensorName);
    prefs.putString("unit", _cfg.unit);
    prefs.putUChar("dec", _cfg.decimals);
    prefs.putFloat("mv4", _cfg.mvAt4mA);
    prefs.putFloat("mv20", _cfg.mvAt20mA);
    prefs.putFloat("ma_lo", _cfg.calMaLow);
    prefs.putFloat("val_lo", _cfg.valLow);
    prefs.putFloat("ma_hi", _cfg.calMaHigh);
    prefs.putFloat("val_hi", _cfg.valHigh);
    return true;
}

void CalibrationManager::applyPreset(uint8_t index, bool resetPoints) {
    if (index >= PRESET_COUNT) {
        index = 0;
    }
    const SensorPreset& p = SENSOR_PRESETS[index];
    _cfg.presetIndex = index;
    strncpy(_cfg.sensorName, p.name, sizeof(_cfg.sensorName) - 1);
    _cfg.sensorName[sizeof(_cfg.sensorName) - 1] = '\0';
    strncpy(_cfg.unit, p.unit, sizeof(_cfg.unit) - 1);
    _cfg.unit[sizeof(_cfg.unit) - 1] = '\0';
    _cfg.decimals = p.decimals;

    if (resetPoints) {
        _cfg.calMaLow = 4.0f;
        _cfg.valLow = p.defaultLow;
        _cfg.calMaHigh = 20.0f;
        _cfg.valHigh = p.defaultHigh;
    }
}

void CalibrationManager::setCustomSensor(const char* name, const char* unit, float valLow, float valHigh, uint8_t decimals) {
    _cfg.presetIndex = 255;
    strncpy(_cfg.sensorName, name, sizeof(_cfg.sensorName) - 1);
    _cfg.sensorName[sizeof(_cfg.sensorName) - 1] = '\0';
    strncpy(_cfg.unit, unit, sizeof(_cfg.unit) - 1);
    _cfg.unit[sizeof(_cfg.unit) - 1] = '\0';
    _cfg.decimals = decimals;
    _cfg.calMaLow = 4.0f;
    _cfg.valLow = valLow;
    _cfg.calMaHigh = 20.0f;
    _cfg.valHigh = valHigh;
    save();
}

bool CalibrationManager::setLoop4mA(float measuredMv) {
    if (fabsf(_cfg.mvAt20mA - measuredMv) < 100.0f) {
        return false;
    }
    _cfg.mvAt4mA = measuredMv;
    return save();
}

bool CalibrationManager::setLoop20mA(float measuredMv) {
    if (fabsf(measuredMv - _cfg.mvAt4mA) < 100.0f) {
        return false;
    }
    _cfg.mvAt20mA = measuredMv;
    return save();
}

bool CalibrationManager::calibrateLowPoint(float measuredMa, float targetValue) {
    if (fabsf(_cfg.calMaHigh - measuredMa) < 0.2f) {
        return false;
    }
    _cfg.calMaLow = measuredMa;
    _cfg.valLow = targetValue;
    return save();
}

bool CalibrationManager::calibrateHighPoint(float measuredMa, float targetValue) {
    if (fabsf(measuredMa - _cfg.calMaLow) < 0.2f) {
        return false;
    }
    _cfg.calMaHigh = measuredMa;
    _cfg.valHigh = targetValue;
    return save();
}

bool CalibrationManager::setDirectPoints(float maLow, float vLow, float maHigh, float vHigh) {
    if (fabsf(maHigh - maLow) < 0.2f) {
        return false;
    }
    _cfg.calMaLow = maLow;
    _cfg.valLow = vLow;
    _cfg.calMaHigh = maHigh;
    _cfg.valHigh = vHigh;
    return save();
}

void CalibrationManager::resetDefaults() {
    uint8_t idx = (_cfg.presetIndex < PRESET_COUNT) ? _cfg.presetIndex : 0;
    _cfg.mvAt4mA = DEFAULT_MV_4MA;
    _cfg.mvAt20mA = DEFAULT_MV_20MA;
    applyPreset(idx, true);
    save();
}

float CalibrationManager::mvToMilliAmps(float adcMv) const {
    float spanMv = _cfg.mvAt20mA - _cfg.mvAt4mA;
    if (fabsf(spanMv) < 1.0f) {
        return 0.0f;
    }
    return 4.0f + (adcMv - _cfg.mvAt4mA) * 16.0f / spanMv;
}

float CalibrationManager::milliAmpsToValue(float mA) const {
    float spanMa = _cfg.calMaHigh - _cfg.calMaLow;
    if (fabsf(spanMa) < 0.05f) {
        return _cfg.valLow;
    }
    return _cfg.valLow + (mA - _cfg.calMaLow) * (_cfg.valHigh - _cfg.valLow) / spanMa;
}

const char* CalibrationManager::evaluateFault(float mA) const {
    if (mA < LOOP_UNDER_MA) {
        return "UNDER";
    }
    if (mA > LOOP_OVER_MA) {
        return "OVER";
    }
    return "OK";
}

int CalibrationManager::findPresetByName(const char* name) const {
    if (!name) return -1;
    for (uint8_t i = 0; i < PRESET_COUNT; ++i) {
        if (strcasecmp(SENSOR_PRESETS[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}
