#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>

// --- Configuration ---
const char* WIFI_SSID = "METRO POLITAN 2_5G";
const char* WIFI_PASS = "905085ea";

// Use IPAddress to avoid DNS lookup issues
IPAddress MQTT_SERVER(192, 168, 110, 6); 
const int MQTT_PORT = 1883;
const char* MQTT_USER = "guest";
const char* MQTT_PASS = "guest";
const char* MQTT_TOPIC = "XIAO_C3/data";
const char* CLIENT_ID = "XIAO_ESP32C3_01";

// --- Objects ---
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// --- Time Setup ---
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 5 * 3600; // Pakistan Standard Time (GMT+5)
const int daylightOffset_sec = 0;
struct tm timeinfo;

void setupTime() {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    Serial.print("Syncing time...");
    if (getLocalTime(&timeinfo)) {
        Serial.println(" OK");
    } else {
        Serial.println(" Failed");
    }
}

String getTimestamp() {
    if (!getLocalTime(&timeinfo)) return "2026-01-01 00:00:00";
    char buffer[30];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(buffer);
}

void connectWiFi() {
    Serial.printf("\nConnecting to %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n✅ WiFi Connected!");
    Serial.print("Local IP: "); Serial.println(WiFi.localIP());
    Serial.print("Gateway:  "); Serial.println(WiFi.gatewayIP());
    Serial.print("Subnet:   "); Serial.println(WiFi.subnetMask());
}

void connectMQTT() {
    // Generate a unique Client ID using MAC address
    String mac = WiFi.macAddress();
    String uniqueClientID = "XIAO_C3_" + mac.substring(mac.length() - 5);
    uniqueClientID.replace(":", "");

    while (!mqttClient.connected()) {
        Serial.print("Attempting MQTT connection as ");
        Serial.print(uniqueClientID);
        Serial.print("...");
        
        // Increase keepalive to 60 seconds
        mqttClient.setKeepAlive(60);

        if (mqttClient.connect(uniqueClientID.c_str(), MQTT_USER, MQTT_PASS)) {
            Serial.println("Connected!");
        } else {
            Serial.printf("failed, rc=%d. Retrying in 5s...\n", mqttClient.state());
            delay(5000);
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(4000);
    Serial.printf("----starting now...\n");
    connectWiFi();
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    setupTime();
}

void loop() {
    if (!mqttClient.connected()) {
        connectMQTT();
    }
    mqttClient.loop();

    static unsigned long lastMsg = 0;
    unsigned long now = millis();
    bool shouldPublish = (now - lastMsg > 30*1000); // Publish every 30 seconds

    if (shouldPublish) {
        lastMsg = now;

        // Create JSON Payload
        StaticJsonDocument<512> doc;
        doc["organization_id"] = "a8022bf1-4764-4286-850b-03697797b696";
        doc["radar_source_mac"] = "a0:b7:65:04:2f:4b";
        doc["agri_node_mac"] = "a0:b7:65:04:2f:4b"; // In this case same as source
        doc["presence"] = true;
        doc["presence_type"] = "st";
        doc["device_timestamp"] = getTimestamp();

        char payload[512];
        serializeJson(doc, payload);

        Serial.printf("Publishing: %s\n", payload);

        // Reliable Publish Logic
        mqttClient.beginPublish(MQTT_TOPIC, strlen(payload), false);
        mqttClient.write(reinterpret_cast<const uint8_t*>(payload), strlen(payload));
        bool PublishSuccess = mqttClient.endPublish(); 

        if (PublishSuccess) {
            Serial.println("MsgPublished Success.");
        } else {
            Serial.print("Connected(0) timeout(-1) ConFailed(-2) ConLost(-3) Discont(-4) con refused(-5): "); 
            Serial.println(mqttClient.state());
        }
    }
}
