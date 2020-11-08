#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <DHT.h>

#include "wifi_settings.h"

#define DHTPIN 32     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT11   // DHT 11

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASS;

WebServer server(80);
DHT dht(DHTPIN, DHTTYPE);

float humidity;
float temp;

void setup(void) {
  Serial.begin(921600);
  pinMode (DHTPIN, INPUT);
  dht.begin();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp32")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", []() {
    server.send(200, "text/plain", "<a href=\"/metric\"s>Prometheus metrics</a>");
  });

  server.on("/metrics", []() {
    update_sensors();
    String output = "# HELP humidity Ambient humidity.\n" 
      "# TYPE humidity gauge\n"
      "humidity " + String(humidity) + "\n" + 
      "# HELP temperature Ambient temperature.\n" + 
      "# TYPE temperature gauge\n" + 
      "temperature " + String(temp) + "\n";
    server.send(200, "text/plain", output);
  });

  server.begin();
  Serial.println("HTTP server started");
}

void update_sensors() {
  Serial.println(F("Begin to read from DHT sensor"));
  humidity = dht.readHumidity();
  temp = dht.readTemperature();

  if (isnan(humidity) || isnan(temp)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }
}

void loop(void) {
  server.handleClient();
}
