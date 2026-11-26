#pragma once
#include <Arduino.h>
#include "calibration.h"

enum class ScreenState : uint8_t {
    MONITOR = 0,
    MENU,
    CAL_LOW,
    CAL_HIGH,
    CAL_LOOP_4MA,
    CAL_LOOP_20MA,
    PRESET_SELECT,
    RESET_CONFIRM
};

typedef void (*CalibrationChangedCb)(const char* eventName);

class UiLcd {
public:
    void begin(CalibrationManager* cal, CalibrationChangedCb onChanged);
    void update(float adcMv, float currentMa, float engValue, const char* fault, bool wifiOk, bool mqttOk);
    void notifyToast(const char* line0, const char* line1, uint32_t durationMs = 1500);
    bool inCalibrationMode() const { return _state != ScreenState::MONITOR; }

private:
    CalibrationManager* _cal = nullptr;
    CalibrationChangedCb _onChanged = nullptr;

    ScreenState _state = ScreenState::MONITOR;
    uint8_t _menuIndex = 0;
    uint8_t _presetCursor = 0;
    uint8_t _stepIndex = 1; // 0:0.01, 1:0.1, 2:1.0, 3:10.0
    float _editTarget = 0.0f;

    uint32_t _lastInteractionMs = 0;
    uint32_t _lastRenderMs = 0;
    uint32_t _toastUntilMs = 0;
    char _toast0[17]{};
    char _toast1[17]{};

    // State tombol & axis joystick
    bool _btnDown = false;
    bool _btnLongFired = false;
    uint32_t _btnDownMs = 0;
    int8_t _lastAxisX = 0;
    int8_t _lastAxisY = 0;
    uint32_t _nextRepeatXMs = 0;
    uint32_t _nextRepeatYMs = 0;

    void handleJoystick(float adcMv, float currentMa);
    int8_t readAxis(uint8_t pin, bool invert, int8_t& prevDir, uint32_t& nextRepeatMs);
    void enterMenu();
    void enterEditPoint(ScreenState targetState);
    void adjustTarget(int8_t dir);
    void cycleStep(int8_t dir);
    void commitAction(float adcMv, float currentMa);
    void render(float adcMv, float currentMa, float engValue, const char* fault, bool wifiOk, bool mqttOk);
    void printLine(uint8_t row, const char* text);
};
