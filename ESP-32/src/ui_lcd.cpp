#include "ui_lcd.h"
#include "config.h"
#include "pins.h"
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <stdio.h>
#include <string.h>

static LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);

static constexpr float STEP_VALUES[] = {0.01f, 0.1f, 1.0f, 10.0f};
static constexpr uint8_t STEP_COUNT = sizeof(STEP_VALUES) / sizeof(STEP_VALUES[0]);

static const char* const MENU_ITEMS[] = {
    "1.Cal Low Point ",
    "2.Cal High Point",
    "3.Loop 4mA (mV) ",
    "4.Loop 20mA(mV) ",
    "5.Sensor Preset ",
    "6.Reset Default ",
    "7.Exit Monitor  "
};
static constexpr uint8_t MENU_COUNT = sizeof(MENU_ITEMS) / sizeof(MENU_ITEMS[0]);

void UiLcd::begin(CalibrationManager* cal, CalibrationChangedCb onChanged) {
    _cal = cal;
    _onChanged = onChanged;

    pinMode(PIN_JOY_SW, INPUT_PULLUP);
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    lcd.init();
    lcd.backlight();

    notifyToast("ESP32 DataLogger", "4-20mA SCADA OK ", 1500);
}

void UiLcd::notifyToast(const char* line0, const char* line1, uint32_t durationMs) {
    snprintf(_toast0, sizeof(_toast0), "%-16.16s", line0 ? line0 : "");
    snprintf(_toast1, sizeof(_toast1), "%-16.16s", line1 ? line1 : "");
    _toastUntilMs = millis() + durationMs;
    _lastRenderMs = 0;
}

void UiLcd::printLine(uint8_t row, const char* text) {
    char buf[LCD_COLS + 1];
    snprintf(buf, sizeof(buf), "%-16.16s", text ? text : "");
    lcd.setCursor(0, row);
    lcd.print(buf);
}

int8_t UiLcd::readAxis(uint8_t pin, bool invert, int8_t& prevDir, uint32_t& nextRepeatMs) {
    int mv = analogReadMilliVolts(pin);
    int8_t dir = 0;
    if (mv < JOY_LOW_THRESH_MV) {
        dir = -1;
    } else if (mv > JOY_HIGH_THRESH_MV) {
        dir = 1;
    }
    if (invert) {
        dir = -dir;
    }

    uint32_t now = millis();
    if (dir == 0) {
        prevDir = 0;
        return 0;
    }
    if (dir != prevDir) {
        prevDir = dir;
        nextRepeatMs = now + JOY_REPEAT_MS * 2;
        return dir;
    }
    if ((int32_t)(now - nextRepeatMs) >= 0) {
        nextRepeatMs = now + JOY_REPEAT_MS;
        return dir;
    }
    return 0;
}

void UiLcd::enterMenu() {
    _state = ScreenState::MENU;
    _menuIndex = 0;
    _lastInteractionMs = millis();
    _lastRenderMs = 0;
}

void UiLcd::enterEditPoint(ScreenState targetState) {
    _state = targetState;
    const CalibrationConfig& cfg = _cal->data();
    _editTarget = (targetState == ScreenState::CAL_LOW) ? cfg.valLow : cfg.valHigh;
    _stepIndex = (cfg.decimals >= 2) ? 1 : ((cfg.decimals == 1) ? 1 : 2);
    _lastRenderMs = 0;
}

void UiLcd::adjustTarget(int8_t dir) {
    _editTarget += (float)dir * STEP_VALUES[_stepIndex];
    _lastRenderMs = 0;
}

void UiLcd::cycleStep(int8_t dir) {
    if (dir > 0) {
        _stepIndex = (_stepIndex + 1) % STEP_COUNT;
    } else {
        _stepIndex = (_stepIndex + STEP_COUNT - 1) % STEP_COUNT;
    }
    _lastRenderMs = 0;
}

void UiLcd::commitAction(float adcMv, float currentMa) {
    bool ok = false;
    const char* evt = nullptr;

    switch (_state) {
        case ScreenState::MENU:
            switch (_menuIndex) {
                case 0: enterEditPoint(ScreenState::CAL_LOW); return;
                case 1: enterEditPoint(ScreenState::CAL_HIGH); return;
                case 2: _state = ScreenState::CAL_LOOP_4MA; _lastRenderMs = 0; return;
                case 3: _state = ScreenState::CAL_LOOP_20MA; _lastRenderMs = 0; return;
                case 4:
                    _presetCursor = (_cal->data().presetIndex < PRESET_COUNT) ? _cal->data().presetIndex : 0;
                    _state = ScreenState::PRESET_SELECT;
                    _lastRenderMs = 0;
                    return;
                case 5: _state = ScreenState::RESET_CONFIRM; _lastRenderMs = 0; return;
                default: _state = ScreenState::MONITOR; _lastRenderMs = 0; return;
            }

        case ScreenState::CAL_LOW:
            ok = _cal->calibrateLowPoint(currentMa, _editTarget);
            evt = "local_cal_low";
            notifyToast(ok ? "Saved Low Point" : "Span Too Small!", ok ? "NVS Updated" : "Adjust mA", 1400);
            _state = ScreenState::MENU;
            break;

        case ScreenState::CAL_HIGH:
            ok = _cal->calibrateHighPoint(currentMa, _editTarget);
            evt = "local_cal_high";
            notifyToast(ok ? "Saved High Point" : "Span Too Small!", ok ? "NVS Updated" : "Adjust mA", 1400);
            _state = ScreenState::MENU;
            break;

        case ScreenState::CAL_LOOP_4MA:
            ok = _cal->setLoop4mA(adcMv);
            evt = "local_loop_4ma";
            notifyToast(ok ? "Loop 4mA Saved" : "Invalid 4mA mV!", ok ? "NVS Updated" : "Check Input", 1400);
            _state = ScreenState::MENU;
            break;

        case ScreenState::CAL_LOOP_20MA:
            ok = _cal->setLoop20mA(adcMv);
            evt = "local_loop_20ma";
            notifyToast(ok ? "Loop 20mA Saved" : "Invalid 20mA mV!", ok ? "NVS Updated" : "Check Input", 1400);
            _state = ScreenState::MENU;
            break;

        case ScreenState::PRESET_SELECT:
            _cal->applyPreset(_presetCursor, true);
            _cal->save();
            ok = true;
            evt = "local_preset";
            notifyToast("Preset Applied", _cal->data().sensorName, 1400);
            _state = ScreenState::MENU;
            break;

        case ScreenState::RESET_CONFIRM:
            _cal->resetDefaults();
            ok = true;
            evt = "local_reset";
            notifyToast("Reset Default", "NVS Restored", 1400);
            _state = ScreenState::MENU;
            break;

        case ScreenState::MONITOR:
            enterMenu();
            return;
    }

    if (ok && evt && _onChanged) {
        _onChanged(evt);
    }
}

void UiLcd::handleJoystick(float adcMv, float currentMa) {
    uint32_t now = millis();
    bool pressed = (digitalRead(PIN_JOY_SW) == LOW);
    bool shortClick = false;
    bool longPress = false;

    if (pressed && !_btnDown) {
        _btnDown = true;
        _btnLongFired = false;
        _btnDownMs = now;
    } else if (pressed && _btnDown && !_btnLongFired && (now - _btnDownMs >= JOY_LONG_PRESS_MS)) {
        _btnLongFired = true;
        longPress = true;
    } else if (!pressed && _btnDown) {
        _btnDown = false;
        if (!_btnLongFired && (now - _btnDownMs >= 35)) {
            shortClick = true;
        }
    }

    int8_t dx = readAxis(PIN_JOY_X, JOY_INVERT_X, _lastAxisX, _nextRepeatXMs);
    int8_t dy = readAxis(PIN_JOY_Y, JOY_INVERT_Y, _lastAxisY, _nextRepeatYMs);

    if (shortClick || longPress || dx != 0 || dy != 0) {
        _lastInteractionMs = now;
    }

    if (_state != ScreenState::MONITOR && (now - _lastInteractionMs >= MENU_IDLE_TIMEOUT_MS)) {
        _state = ScreenState::MONITOR;
        _lastRenderMs = 0;
        return;
    }

    if (longPress) {
        if (_state == ScreenState::MENU) {
            _state = ScreenState::MONITOR;
        } else if (_state != ScreenState::MONITOR) {
            _state = ScreenState::MENU;
        }
        _lastRenderMs = 0;
        return;
    }

    if (shortClick) {
        commitAction(adcMv, currentMa);
        return;
    }

    switch (_state) {
        case ScreenState::MONITOR:
            break;
        case ScreenState::MENU:
            if (dy != 0) {
                _menuIndex = (dy > 0) ? ((_menuIndex + 1) % MENU_COUNT)
                                      : ((_menuIndex + MENU_COUNT - 1) % MENU_COUNT);
                _lastRenderMs = 0;
            }
            break;
        case ScreenState::CAL_LOW:
        case ScreenState::CAL_HIGH:
            if (dy != 0) adjustTarget(-dy); // Atas (dy=-1) naik, Bawah (dy=+1) turun
            if (dx != 0) cycleStep(dx);
            break;
        case ScreenState::PRESET_SELECT:
            if (dy != 0) {
                _presetCursor = (dy > 0) ? ((_presetCursor + 1) % PRESET_COUNT)
                                         : ((_presetCursor + PRESET_COUNT - 1) % PRESET_COUNT);
                _lastRenderMs = 0;
            }
            break;
        default:
            if (dx < 0) {
                _state = ScreenState::MENU;
                _lastRenderMs = 0;
            }
            break;
    }
}

void UiLcd::render(float adcMv, float currentMa, float engValue, const char* fault, bool wifiOk, bool mqttOk) {
    uint32_t now = millis();
    if (_lastRenderMs != 0 && (now - _lastRenderMs < LCD_REFRESH_MS)) {
        return;
    }
    _lastRenderMs = now;

    if ((int32_t)(_toastUntilMs - now) > 0) {
        printLine(0, _toast0);
        printLine(1, _toast1);
        return;
    }

    char line0[48];
    char line1[48];
    const CalibrationConfig& cfg = _cal->data();

    switch (_state) {
        case ScreenState::MONITOR: {
            if (strcmp(fault, "OK") != 0) {
                snprintf(line0, sizeof(line0), "%-6.6s FAULT:%s", cfg.sensorName, fault);
            } else {
                char valFmt[12];
                snprintf(valFmt, sizeof(valFmt), "%%.%df", cfg.decimals);
                char valStr[16];
                snprintf(valStr, sizeof(valStr), valFmt, engValue);
                snprintf(line0, sizeof(line0), "%-4.4s:%s %s", cfg.sensorName, valStr, cfg.unit);
            }
            const char* linkStr = mqttOk ? "MQ:OK" : (wifiOk ? "WF:OK" : "OFFLN");
            snprintf(line1, sizeof(line1), "%5.2fmA   %s", currentMa, linkStr);
            break;
        }

        case ScreenState::MENU:
            snprintf(line0, sizeof(line0), ">%s", MENU_ITEMS[_menuIndex]);
            snprintf(line1, sizeof(line1), "SW:Pilih Hld:Ext");
            break;

        case ScreenState::CAL_LOW:
        case ScreenState::CAL_HIGH: {
            const char* tag = (_state == ScreenState::CAL_LOW) ? "Lo" : "Hi";
            snprintf(line0, sizeof(line0), "%s:%5.2f>%7.2f", tag, currentMa, _editTarget);
            snprintf(line1, sizeof(line1), "Stp:%-4.2f SW:Save", STEP_VALUES[_stepIndex]);
            break;
        }

        case ScreenState::CAL_LOOP_4MA:
            snprintf(line0, sizeof(line0), "Set 4mA: %4.0fmV", adcMv);
            snprintf(line1, sizeof(line1), "SW:Save Hld:Back");
            break;

        case ScreenState::CAL_LOOP_20MA:
            snprintf(line0, sizeof(line0), "Set 20mA:%4.0fmV", adcMv);
            snprintf(line1, sizeof(line1), "SW:Save Hld:Back");
            break;

        case ScreenState::PRESET_SELECT: {
            const SensorPreset& p = SENSOR_PRESETS[_presetCursor];
            snprintf(line0, sizeof(line0), ">%-9.9s %-5.5s", p.name, p.unit);
            snprintf(line1, sizeof(line1), "%.0f-%.0f SW:Apply", p.defaultLow, p.defaultHigh);
            break;
        }

        case ScreenState::RESET_CONFIRM:
            snprintf(line0, sizeof(line0), "Reset Kalibrasi?");
            snprintf(line1, sizeof(line1), "SW:Ya  Hld:Batal");
            break;
    }

    printLine(0, line0);
    printLine(1, line1);
}

void UiLcd::update(float adcMv, float currentMa, float engValue, const char* fault, bool wifiOk, bool mqttOk) {
    handleJoystick(adcMv, currentMa);
    render(adcMv, currentMa, engValue, fault, wifiOk, mqttOk);
}
