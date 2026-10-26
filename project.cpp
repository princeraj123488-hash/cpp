#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include "config.h"

// ===== USER-EDITABLE PLACEHOLDERS =====
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* API_URL = "https://example.com/api/attendance";
const char* API_KEY = "YOUR_API_KEY";
// ======================================

static bool awaitingStudentId = false;
static String serialBuffer;

void setLeds(bool okOn, bool errOn) {
    digitalWrite(PIN_LED_OK, okOn ? HIGH : LOW);
    digitalWrite(PIN_LED_ERR, errOn ? HIGH : LOW);
}

String getDeviceId() {
    uint64_t mac = ESP.getEfuseMac();
    char buf[18];
    snprintf(buf, sizeof(buf), "%04X%08X", (uint16_t)(mac >> 32), (uint32_t)mac);
    return String(buf);
}

String escapeJson(const String& in) {
    String out;
    out.reserve(in.length() + 8);
    for (size_t i = 0; i < in.length(); i++) {
        char c = in[i];
        if (c == '\"' || c == '\\') {
            out += '\\';
            out += c;
        } else if (c == '\b') {
            out += "\\b";
        } else if (c == '\f') {
            out += "\\f";
        } else if (c == '\n') {
            out += "\\n";
        } else if (c == '\r') {
            out += "\\r";
        } else if (c == '\t') {
            out += "\\t";
        } else {
            out += c;
        }
    }
    return out;
}

String getTimestamp() {
    time_t now;
    time(&now);

    if (now > 1700000000) {
        struct tm utcTime;
        gmtime_r(&now, &utcTime);
        char ts[25];
        strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &utcTime);
        return String(ts);
    }

    // Fallback when time is not yet synced
    return String("millis_") + String(millis());
}

bool connectWiFi(uint8_t maxRetries = 30) {
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }

    Serial.printf("[WiFi] Connecting to SSID: %s\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint8_t tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < maxRetries) {
        delay(500);
        Serial.print(".");
        tries++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
        return true;
    }

    Serial.println("[WiFi] Connection failed.");
    return false;
}

void syncTimeIfNeeded() {
    static bool timeConfigured = false;
    if (timeConfigured) {
        return;
    }

    if (WiFi.status() == WL_CONNECTED) {
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        timeConfigured = true;
        Serial.println("[NTP] Time sync requested.");
    }
}

bool postAttendance(const String& studentId) {
    if (studentId.length() == 0) {
        Serial.println("[POST] Empty student ID. Ignoring.");
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[POST] WiFi not connected. Attempting reconnect...");
        if (!connectWiFi()) {
            Serial.println("[POST] Reconnect failed.");
            setLeds(false, true);
            return false;
        }
    }

    syncTimeIfNeeded();

    String deviceId = getDeviceId();
    String timestamp = getTimestamp();

    String payload = "{";
    payload += "\"student_id\":\"" + escapeJson(studentId) + "\",";
    payload += "\"method\":\"eye_scan_demo\",";
    payload += "\"device_id\":\"" + escapeJson(deviceId) + "\",";
    payload += "\"timestamp\":\"" + escapeJson(timestamp) + "\"";
    payload += "}";

    Serial.println("[POST] Sending attendance payload:");
    Serial.println(payload);

    HTTPClient http;
    http.begin(API_URL);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-API-Key", API_KEY);

    int httpCode = http.POST(payload);
    String responseBody = http.getString();

    Serial.printf("[POST] HTTP status: %d\n", httpCode);
    Serial.println("[POST] Response body:");
    Serial.println(responseBody);

    bool success = (httpCode >= 200 && httpCode < 300);
    if (success) {
        Serial.println("[POST] Attendance submitted successfully.");
        setLeds(true, false);
    } else {
        Serial.println("[POST] Attendance submission failed.");
        setLeds(false, true);
    }

    http.end();
    return success;
}

void promptForStudentId() {
    awaitingStudentId = true;
    serialBuffer = "";
    Serial.println();
    Serial.println("Button pressed. Enter student ID, then press Enter:");
}

void processSerialInput() {
    while (Serial.available() > 0) {
        char c = (char)Serial.read();

        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            String entered = serialBuffer;
            serialBuffer = "";
            entered.trim();

            if (!awaitingStudentId) {
                if (entered.length() > 0) {
                    Serial.println("[Serial] Press the button first, then enter ID.");
                }
                continue;
            }

            if (entered.length() == 0) {
                Serial.println("[Serial] Empty ID. Please enter a valid student ID:");
                continue;
            }

            awaitingStudentId = false;
            Serial.printf("[Serial] Received student ID: %s\n", entered.c_str());
            postAttendance(entered);
            Serial.println("Ready. Press button to submit next attendance.");
            continue;
        }

        if (isPrintable((unsigned char)c) && serialBuffer.length() < 128) {
            serialBuffer += c;
        }
    }
}

void setup() {
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    pinMode(PIN_LED_OK, OUTPUT);
    pinMode(PIN_LED_ERR, OUTPUT);
    setLeds(false, false);

    Serial.begin(115200);
    delay(300);

    Serial.println();
    Serial.println("=== XIAO ESP32-S3 Sense Attendance Demo ===");
    Serial.println("Press button on D0 to enter Student ID.");
    Serial.println("Green LED: POST success, Red LED: POST/network failure.");

    connectWiFi();
    syncTimeIfNeeded();
}

void loop() {
    static bool btnPrev = HIGH;
    bool btn = digitalRead(PIN_BUTTON);

    if (btn == LOW && btnPrev == HIGH) {
        promptForStudentId();
    }
    btnPrev = btn;

    processSerialInput();
    delay(5);
}