#pragma once
#include <Arduino.h>
#include "calibration.h"
#include "ui_lcd.h"

class MqttManager {
public:
    void begin(CalibrationManager* cal, UiLcd* ui);
    void loop(float adcMv, float currentMa, float engValue, const char* fault);

    bool isWifiConnected() const;
    bool isMqttConnected();
    void publishCalibrationStatus(const char* eventName);

private:
    CalibrationManager* _cal = nullptr;
    UiLcd* _ui = nullptr;

    float _lastAdcMv = 0.0f;
    float _lastMa = 0.0f;
    uint32_t _lastWifiAttemptMs = 0;
    uint32_t _lastMqttAttemptMs = 0;
    uint32_t _lastTelemetryMs = 0;

    void ensureWifi();
    void ensureMqtt();
    void publishTelemetry(float adcMv, float currentMa, float engValue, const char* fault);
    void handleMessage(char* topic, uint8_t* payload, unsigned int length);
    static void mqttCallbackTrampoline(char* topic, uint8_t* payload, unsigned int length);
};
