#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// === CONFIGURATION ===
const char* ssid = "realme C55";
const char* password = "fahimxyz";

#define DHTPIN 4           // GPIO4 for DHT22
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#define RELAY_PIN 25       // GPIO25 for Relay (active LOW)

#define ONE_WIRE_BUS 33    // GPIO33 for DS18B20
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

const char* thingspeakAPIKey = "CGNYSTJYAWS68G22";
const char* thingspeakServer = "http://api.thingspeak.com/update";

WebServer server(80);

bool relayState = false;  // false = OFF, true = ON

// === HTML PAGE ===
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Smart Fish Feeding System</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; background: #e0f7fa; color: #004d40; text-align: center; padding: 20px; }
    h1 { color: #00796b; }
    .sensor-data { font-size: 1.2em; margin: 20px 0; }
    button { padding: 15px 30px; font-size: 1.2em; background: #00796b; color: white; border: none; border-radius: 10px; cursor: pointer; }
    button:hover { background: #004d40; }
    .status { font-weight: bold; color: #00796b; margin-top: 15px; }
  </style>
  <script>
    function toggleRelay() {
      fetch('/toggle');
    }
    setInterval(function() {
      fetch('/status').then(response => response.json()).then(data => {
        document.getElementById('temp').innerText = data.temperature.toFixed(2) + " °C";
        document.getElementById('humidity').innerText = data.humidity.toFixed(2) + " %";
        document.getElementById('waterTemp').innerText = data.waterTemp.toFixed(2) + " °C";
        document.getElementById('relayStatus').innerText = data.relay ? "ON" : "OFF";
        document.getElementById('toggleBtn').innerText = data.relay ? "Turn OFF UV & Fan" : "Turn ON UV & Fan";
      });
    }, 10000);
  </script>
</head>
<body>
  <h1>Smart Fish Feeding System with Water Quality Monitoring</h1>
  <div class="sensor-data">
    <p>Temperature: <span id="temp">--</span></p>
    <p>Humidity: <span id="humidity">--</span></p>
    <p>Water Temp: <span id="waterTemp">--</span></p>
    <p>UV & Fan Status: <span id="relayStatus">OFF</span></p>
  </div>
  <button id="toggleBtn" onclick="toggleRelay()">Turn ON UV & Fan</button>
</body>
</html>
)rawliteral";

// === FUNCTIONS ===
void handleRoot() {
  server.send_P(200, "text/html", webpage);
}

void handleToggle() {
  relayState = !relayState;
  digitalWrite(RELAY_PIN, relayState ? LOW : HIGH);
  server.send(200, "text/plain", relayState ? "ON" : "OFF");
}

void handleStatus() {
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  float waterTemp = 28.5;  // Hardcoded water temp for dashboard

  if (isnan(temp)) temp = 0.0;
  if (isnan(hum)) hum = 0.0;

  String json = "{\"temperature\":" + String(temp) +
                ",\"humidity\":" + String(hum) +
                ",\"waterTemp\":" + String(waterTemp) +
                ",\"relay\":" + String(relayState ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

void sendToThingSpeak(float temp, float hum, float waterTemp) {
  if (isnan(temp) || isnan(hum)) return;

  HTTPClient http;
  String url = String(thingspeakServer) + "?api_key=" + thingspeakAPIKey +
               "&field1=" + String(temp) + "&field2=" + String(hum) +
               "&field3=" + String(waterTemp);

  http.begin(url);
  int httpCode = http.GET();
  if (httpCode > 0) {
    Serial.printf("ThingSpeak update code: %d\n", httpCode);
  } else {
    Serial.printf("ThingSpeak update failed: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);  // Relay OFF at startup
  dht.begin();
  sensors.begin();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/toggle", handleToggle);
  server.on("/status", handleStatus);
  server.begin();
  Serial.println("HTTP server started");
}

unsigned long lastThingSpeakSend = 0;
const unsigned long thingSpeakInterval = 15000;

void loop() {
  server.handleClient();

  unsigned long now = millis();
  if (now - lastThingSpeakSend > thingSpeakInterval) {
    lastThingSpeakSend = now;
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    float waterTemp = 28.5; // Hardcoded water temp for ThingSpeak
    sendToThingSpeak(temp, hum, waterTemp);
    Serial.printf("Temp: %.2f °C, Humidity: %.2f %%, Water Temp: %.2f °C\n", temp, hum, waterTemp);
  }
}
