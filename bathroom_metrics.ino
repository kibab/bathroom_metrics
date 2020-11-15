#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>


#include "wifi_settings.h"

#define SDA1 21
#define SCL1 22
#define BME280_ADDR 0x76

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASS;

WebServer server(80);

float humidity;
float temp;

TwoWire i2cbus = TwoWire(0);
Adafruit_BME280 bme;

void scan_i2cbus(){
  Serial.println("Scanning I2C Addresses Channel 1");
  uint8_t cnt = 0;
  for (uint8_t i=0; i<128; i++) {
    i2cbus.beginTransmission(i);
    uint8_t  ec = i2cbus.endTransmission(true);
    if (ec==0) {
      if (i<16)
        Serial.print('0');
      Serial.print(i, HEX);
      cnt++;
     } else Serial.print("..");
     Serial.print(' ');
     if ( (i&0x0f)==0x0f)
     Serial.println();
  }
  Serial.print("Scan Completed,");
  Serial.print(cnt);
  Serial.println(" I2C Devices found.");
}

void setup(void) {
  Serial.begin(921600);
  i2cbus.begin(SDA1, SCL1, 400000);
  if (!bme.begin(BME280_ADDR, &i2cbus)) {
    Serial.println("Cannot start BME sensor comms.");
    return;
  }

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
  Serial.println(F("Begin to read from BMP sensor"));
  humidity = bme.readHumidity();
  temp = bme.readTemperature();
  if (isnan(humidity) || isnan(temp)) {
    Serial.println(F("Failed to read from BMP sensor!"));
    return;
  } else {
    Serial.print("Temperature = ");
    Serial.print(bme.readTemperature());
    Serial.println(" *C");
    Serial.print("Humidity = ");
    Serial.print(bme.readHumidity());
    Serial.println(" %");
  }
}

void loop(void) {
  server.handleClient();
}
