#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ------------------------------
// User configuration
// ------------------------------
static const char *WIFI_SSID = "YOUR_WIFI_SSID";
static const char *WIFI_PASS = "YOUR_WIFI_PASSWORD";

static const char *MQTT_HOST = "broker.hivemq.com";
static const uint16_t MQTT_PORT = 1883;
static const char *MQTT_CLIENT_ID = "esp32-rescue-thermal";
static const char *MQTT_TOPIC_STATUS = "rescue/drone/thermal/status";
static const char *MQTT_TOPIC_FRAME = "rescue/drone/thermal/frame";

// AMG8833 uses I2C address 0x68 or 0x69 depending on module wiring.
static uint8_t g_amgAddress = 0x69;

// ESP32 default I2C pins for DevKit V1
static const uint8_t I2C_SDA = 21;
static const uint8_t I2C_SCL = 22;

// Sampling/publish timings
static const uint32_t SAMPLE_INTERVAL_MS = 250;   // ~4 Hz processing
static const uint32_t STATUS_INTERVAL_MS = 1000;  // summary telemetry
static const uint32_t FRAME_INTERVAL_MS = 5000;   // full 8x8 frame

// ------------------------------
// AMG8833 register map
// ------------------------------
static const uint8_t AMG_REG_PCTL = 0x00;
static const uint8_t AMG_REG_RST = 0x01;
static const uint8_t AMG_REG_FPSC = 0x02;
static const uint8_t AMG_REG_PIXEL_BASE = 0x80;

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

float frameC[64] = {0.0f};
float baselineMeanC = 24.0f;
bool baselineInitialized = false;

float prevCentroidX = -1.0f;
float prevCentroidY = -1.0f;
uint32_
…