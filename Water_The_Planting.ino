#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>

    //Wi-Fi
const char* ssid = "YOUR_WIFI";
const char* password = "YOUR_PASSWORD";

    //Pins
#define SOIL_PIN   34
#define RELAY_PIN  26     // ACTIVE-HIGH relay
#define DHT_PIN    4
#define DHTTYPE    DHT11

    //Timing
const unsigned long wateringDelaySeconds = 2;   // change as needed

    //Objects
WebServer server(80);
DHT dht(DHT_PIN, DHTTYPE);

int   dryThreshold = 2200;
bool  soilIsDry = false;
bool  pumpState = false;
bool  waitingForTimer = false;
unsigned long lastDryTime = 0;
unsigned long wateringStartTime = 0;
int   soilMoisture = 0;
float temperature = 0.0;
float humidity = 0.0;

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);   // Pump OFF (ACTIVE-HIGH)

  dht.begin();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/manual-water", handleManualWater);
  server.on("/stop-water", handleStopWater);

  server.begin();
}

void loop() {
  server.handleClient();

  static unsigned long lastRead = 0;
  if (millis() - lastRead >= 2000) {
    readSensors();
    controlLogic();
    lastRead = millis();
  }
}
    //Sensors
void readSensors() {
  soilMoisture = analogRead(SOIL_PIN);

  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    temperature = 0;
    humidity = 0;
  }
}
    //Logic
void controlLogic() {
  bool currentlyDry = (soilMoisture > dryThreshold);

  if (currentlyDry && !soilIsDry) {
    soilIsDry = true;
    waitingForTimer = true;
    lastDryTime = millis();
  }

  if (!currentlyDry) {
    soilIsDry = false;
    waitingForTimer = false;
    if (pumpState) setPumpState(false);
  }

  if (waitingForTimer && soilIsDry) {
    unsigned long elapsed = (millis() - lastDryTime) / 1000;
    if (elapsed >= wateringDelaySeconds) {
      setPumpState(true);
      waitingForTimer = false;
      wateringStartTime = millis();
    }
  }
}
    //Pump
void setPumpState(bool on) {
  pumpState = on;

  if (on) {
    digitalWrite(RELAY_PIN, HIGH);   // ACTIVE-HIGH → ON
  } else {
    digitalWrite(RELAY_PIN, LOW);    // OFF
  }
}
    //Web
void handleRoot() {
  String html;

  html += "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<meta http-equiv='refresh' content='5'>";
  html += "<title>Plant Watering System</title>";

  html += "<style>";
  html += "body{font-family:Arial;background:#f0f0f0;margin:20px}";
  html += ".box{max-width:800px;margin:auto;background:#fff;padding:20px;border-radius:10px}";
  html += ".card{padding:15px;margin:10px 0;border-left:5px solid}";
  html += ".dry{border-color:#ff9800;background:#fff3e0}";
  html += ".wet{border-color:#4caf50;background:#e8f5e9}";
  html += ".pump{border-color:#2196f3;background:#e3f2fd}";
  html += ".v{font-size:22px;font-weight:bold;color:#2196f3}";
  html += "button{padding:10px 15px;font-size:16px;border:none;border-radius:5px}";
  html += ".on{background:#4caf50;color:#fff}";
  html += ".off{background:#f44336;color:#fff}";
  html += "</style></head><body>";

  html += "<div class='box'>";
  html += "<h1>🌱 Plant Watering System</h1>";

  html += "<div class='card'>";
  html += "<h2>📶 Connection</h2>";
  html += "<p>WiFi: " + String(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected") + "</p>";
  html += "<p>IP: " + WiFi.localIP().toString() + "</p>";
  html += "</div>";

  html += "<div class='card " + String(soilIsDry ? "dry" : "wet") + "'>";
  html += "<h2>Soil: " + String(soilIsDry ? "DRY 🌵" : "WET 💧") + "</h2>";
  html += "<p>Moisture: <span class='v'>" + String(soilMoisture) + "</span></p>";
  html += "</div>";

  html += "<div class='card'>";
  html += "<h2>🌡 Environment</h2>";
  html += "<p>Temp: <span class='v'>" + String(temperature,1) + " °C</span></p>";
  html += "<p>Humidity: <span class='v'>" + String(humidity,1) + " %</span></p>";
  html += "</div>";

  html += "<div class='card " + String(pumpState ? "pump" : "") + "'>";
  html += "<h2>💦 Pump: " + String(pumpState ? "ON" : "OFF") + "</h2>";
  if (pumpState) {
    html += "<p>Running: " + formatTime((millis() - wateringStartTime) / 1000) + "</p>";
  }
  html += "</div>";

  html += "<div class='card'>";
  html += "<a href='/manual-water'><button class='on'>Start</button></a> ";
  html += "<a href='/stop-water'><button class='off'>Stop</button></a>";
  html += "</div>";

  html += "</div></body></html>";

  server.send(200, "text/html; charset=UTF-8", html);
}
    //Manual
void handleManualWater() {
  setPumpState(true);
  waitingForTimer = false;
  server.sendHeader("Location", "/");
  server.send(302);
}
void handleStopWater() {
  setPumpState(false);
  server.sendHeader("Location", "/");
  server.send(302);
}
    //Utils
String formatTime(unsigned long s) {
  if (s < 60) return String(s) + " sec";
  if (s < 3600) return String(s / 60) + " min";
  return String(s / 3600) + " h";
}
