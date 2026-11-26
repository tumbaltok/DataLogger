#include "mqtt_client.h"
#include "config.h"
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <string.h>

static WiFiClient wifiNet;
static PubSubClient mqtt(wifiNet);
static MqttManager* g_mqttInstance = nullptr;

void MqttManager::mqttCallbackTrampoline(char* topic, uint8_t* payload, unsigned int length) {
    if (g_mqttInstance) {
        g_mqttInstance->handleMessage(topic, payload, length);
    }
}

void MqttManager::begin(CalibrationManager* cal, UiLcd* ui) {
    _cal = cal;
    _ui = ui;
    g_mqttInstance = this;

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    _lastWifiAttemptMs = millis();

    mqtt.setServer(MQTT_BROKER_HOST, MQTT_BROKER_PORT);
    mqtt.setBufferSize(512);
    mqtt.setCallback(mqttCallbackTrampoline);
}

bool MqttManager::isWifiConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

bool MqttManager::isMqttConnected() {
    return mqtt.connected();
}

void MqttManager::ensureWifi() {
    if (isWifiConnected()) return;
    uint32_t now = millis();
    if (now - _lastWifiAttemptMs >= 10000UL) {
        _lastWifiAttemptMs = now;
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
}

void MqttManager::ensureMqtt() {
    if (!isWifiConnected() || mqtt.connected()) return;
    uint32_t now = millis();
    if (now - _lastMqttAttemptMs < 5000UL) return;
    _lastMqttAttemptMs = now;

    char clientId[40];
    snprintf(clientId, sizeof(clientId), "esp32-%s-%04X", DEVICE_ID, (uint16_t)(ESP.getEfuseMac() & 0xFFFF));

    bool connected = false;
    if (strlen(MQTT_USER) > 0) {
        connected = mqtt.connect(clientId, MQTT_USER, MQTT_PASSWORD, TOPIC_STATUS, 0, true, "offline");
    } else {
        connected = mqtt.connect(clientId, nullptr, nullptr, TOPIC_STATUS, 0, true, "offline");
    }

    if (connected) {
        mqtt.publish(TOPIC_STATUS, "online", true);
        mqtt.subscribe(TOPIC_CAL_SET);
        publishCalibrationStatus("connected");
    }
}

void MqttManager::publishCalibrationStatus(const char* eventName) {
    if (!mqtt.connected() || !_cal) return;

    const CalibrationConfig& cfg = _cal->data();
    JsonDocument doc;
    doc["device"] = DEVICE_ID;
    doc["sensor"] = cfg.sensorName;
    doc["unit"] = cfg.unit;
    doc["mv_4ma"] = serialized(String(cfg.mvAt4mA, 1U));
    doc["mv_20ma"] = serialized(String(cfg.mvAt20mA, 1U));
    doc["cal_ma_low"] = serialized(String(cfg.calMaLow, 2U));
    doc["val_low"] = serialized(String(cfg.valLow, 2U));
    doc["cal_ma_high"] = serialized(String(cfg.calMaHigh, 2U));
    doc["val_high"] = serialized(String(cfg.valHigh, 2U));
    doc["last_event"] = eventName ? eventName : "sync";

    char out[384];
    serializeJson(doc, out, sizeof(out));
    mqtt.publish(TOPIC_CAL_STATUS, out, true);
}

void MqttManager::publishTelemetry(float adcMv, float currentMa, float engValue, const char* fault) {
    if (!mqtt.connected() || !_cal) return;

    const CalibrationConfig& cfg = _cal->data();
    JsonDocument doc;
    doc["device"] = DEVICE_ID;
    doc["sensor"] = cfg.sensorName;
    doc["unit"] = cfg.unit;
    doc["adc_mv"] = serialized(String(adcMv, 1U));
    doc["ma"] = serialized(String(currentMa, 2U));
    doc["value"] = serialized(String(engValue, (unsigned int)cfg.decimals));
    doc["fault"] = fault;
    doc["cal_mode"] = _ui ? _ui->inCalibrationMode() : false;
    doc["rssi"] = WiFi.RSSI();

    char out[320];
    serializeJson(doc, out, sizeof(out));
    mqtt.publish(TOPIC_TELEMETRY, out, false);
}

void MqttManager::handleMessage(char* topic, uint8_t* payload, unsigned int length) {
    if (strcmp(topic, TOPIC_CAL_SET) != 0 || !_cal) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err) {
        publishCalibrationStatus("error_json");
        return;
    }

    const char* cmd = doc["cmd"] | "";
    bool ok = false;

    if (strcmp(cmd, "cal_low") == 0 && !doc["target"].isNull()) {
        ok = _cal->calibrateLowPoint(_lastMa, doc["target"].as<float>());
        publishCalibrationStatus(ok ? "remote_cal_low" : "err_cal_low");
        if (ok && _ui) _ui->notifyToast("SCADA Cal Low", "NVS Updated");
    } else if (strcmp(cmd, "cal_high") == 0 && !doc["target"].isNull()) {
        ok = _cal->calibrateHighPoint(_lastMa, doc["target"].as<float>());
        publishCalibrationStatus(ok ? "remote_cal_high" : "err_cal_high");
        if (ok && _ui) _ui->notifyToast("SCADA Cal High", "NVS Updated");
    } else if (strcmp(cmd, "set_points") == 0) {
        ok = _cal->setDirectPoints(
            doc["cal_ma_low"] | 4.0f,
            doc["val_low"] | 0.0f,
            doc["cal_ma_high"] | 20.0f,
            doc["val_high"] | 14.0f
        );
        publishCalibrationStatus(ok ? "remote_set_points" : "err_set_points");
        if (ok && _ui) _ui->notifyToast("SCADA Set 2-Pt", "NVS Updated");
    } else if (strcmp(cmd, "cal_loop_4ma") == 0) {
        ok = _cal->setLoop4mA(_lastAdcMv);
        publishCalibrationStatus(ok ? "remote_loop_4ma" : "err_loop_4ma");
        if (ok && _ui) _ui->notifyToast("SCADA Loop 4mA", "NVS Updated");
    } else if (strcmp(cmd, "cal_loop_20ma") == 0) {
        ok = _cal->setLoop20mA(_lastAdcMv);
        publishCalibrationStatus(ok ? "remote_loop_20ma" : "err_loop_20ma");
        if (ok && _ui) _ui->notifyToast("SCADA Loop 20mA", "NVS Updated");
    } else if (strcmp(cmd, "set_sensor") == 0) {
        if (!doc["preset"].isNull()) {
            int idx = _cal->findPresetByName(doc["preset"].as<const char*>());
            if (idx >= 0) {
                _cal->applyPreset((uint8_t)idx, true);
                _cal->save();
                ok = true;
            }
        } else if (!doc["sensor"].isNull() && !doc["unit"].isNull()) {
            _cal->setCustomSensor(
                doc["sensor"].as<const char*>(),
                doc["unit"].as<const char*>(),
                doc["val_low"] | 0.0f,
                doc["val_high"] | 100.0f,
                doc["decimals"] | 2
            );
            ok = true;
        }
        publishCalibrationStatus(ok ? "remote_set_sensor" : "err_set_sensor");
        if (ok && _ui) _ui->notifyToast("SCADA Sensor", _cal->data().sensorName);
    } else if (strcmp(cmd, "reset") == 0) {
        _cal->resetDefaults();
        publishCalibrationStatus("remote_reset");
        if (_ui) _ui->notifyToast("SCADA Reset", "Default Restored");
    } else if (strcmp(cmd, "get_status") == 0) {
        publishCalibrationStatus("status_query");
    }
}

void MqttManager::loop(float adcMv, float currentMa, float engValue, const char* fault) {
    _lastAdcMv = adcMv;
    _lastMa = currentMa;

    ensureWifi();
    ensureMqtt();
    mqtt.loop();

    uint32_t now = millis();
    if (now - _lastTelemetryMs >= TELEMETRY_INTERVAL_MS) {
        _lastTelemetryMs = now;
        publishTelemetry(adcMv, currentMa, engValue, fault);
    }
}
