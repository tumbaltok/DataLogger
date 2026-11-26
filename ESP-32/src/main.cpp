#include <Arduino.h>
#include "calibration.h"
#include "config.h"
#include "mqtt_client.h"
#include "pins.h"
#include "ui_lcd.h"

static CalibrationManager calMgr;
static UiLcd uiLcd;
static MqttManager mqttMgr;

static float filteredAdcMv = 0.0f;
static bool adcInitialized = false;

static float readSensorMilliVolts() {
    uint32_t acc = 0;
    for (int i = 0; i < ADC_SAMPLES; ++i) {
        acc += analogReadMilliVolts(PIN_SENSOR_ADC);
        delayMicroseconds(150);
    }
    float rawMv = (float)acc / (float)ADC_SAMPLES;
    if (!adcInitialized) {
        filteredAdcMv = rawMv;
        adcInitialized = true;
    } else {
        // Exponential moving average (alpha = 0.25) meredam noise switching PSU
        filteredAdcMv = 0.75f * filteredAdcMv + 0.25f * rawMv;
    }
    return filteredAdcMv;
}

static void onLocalCalibrationChanged(const char* eventName) {
    mqttMgr.publishCalibrationStatus(eventName);
}

void setup() {
    Serial.begin(115200);
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, LOW);

    analogReadResolution(12);
    analogSetPinAttenuation(PIN_SENSOR_ADC, ADC_11db);
    analogSetPinAttenuation(PIN_JOY_X, ADC_11db);
    analogSetPinAttenuation(PIN_JOY_Y, ADC_11db);

    calMgr.begin();
    uiLcd.begin(&calMgr, onLocalCalibrationChanged);
    mqttMgr.begin(&calMgr, &uiLcd);
}

void loop() {
    float adcMv = readSensorMilliVolts();
    float currentMa = calMgr.mvToMilliAmps(adcMv);
    float engValue = calMgr.milliAmpsToValue(currentMa);
    const char* fault = calMgr.evaluateFault(currentMa);

    bool wifiOk = mqttMgr.isWifiConnected();
    bool mqttOk = mqttMgr.isMqttConnected();
    digitalWrite(PIN_STATUS_LED, mqttOk ? HIGH : LOW);

    uiLcd.update(adcMv, currentMa, engValue, fault, wifiOk, mqttOk);
    mqttMgr.loop(adcMv, currentMa, engValue, fault);
}
