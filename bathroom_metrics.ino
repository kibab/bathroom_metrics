#include <HTTPClient.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#include <driver/adc.h>
#include <esp_wifi.h>
#include <esp_bt.h>

#include "wifi_settings.h"
#include "sensor.h"

/* Pins for I2C comms */
#define SDA1 21
#define SCL1 22

/* BME280 I2C address */
#define BME280_ADDR 0x76

/* BME280 power pin */
#define BME280_PWR 17

/* Measurement for the input voltage */
#define VIN_MEASURE 34

/* Number of retries for pushing metrics to Push Gateway */
#define MAX_PUSH_RETRIES 3

/* Metrics push interval */
#define PUSH_INTERVAL_SEC 120

/* Max connection attempts */
#define MAX_CONN_ATTEMPTS 10

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASS;

/* Counter of failed WiFi connect attempts */
RTC_DATA_ATTR unsigned int total_conn_failures = 0;

float humidity;
float temp;
unsigned int vin;

WebServer server(80);
TwoWire i2cbus(0);
TempHumSensor *sensor;

/* Generate metrics in Prometheus format */
String getMetrics() {
  return String(
    "# HELP humidity Ambient humidity.\n" 
      "# TYPE humidity gauge\n"
      "humidity " + String(humidity) + "\n" + 
      "# HELP temperature Ambient temperature.\n" + 
      "# TYPE temperature gauge\n" + 
      "temperature " + String(temp) + "\n"
      "# HELP vin Input voltage.\n" +
      "# TYPE vin gauge\n" +
      "vin " + String(vin) + "\n"
      "# HELP conn_failures_count Number of failed WiFi connect attempts\n" + 
      "# TYPE conn_failures_count counter\n" +
      "conn_failures_count " + String(total_conn_failures) + "\n"
    );
}

/* Push metrics to PushGateway endpoint */
bool pushMetrics() {
  bool success = false;
  Serial.println("Starting to push metrics to " PUSH_URL);
  
  HTTPClient http;

  http.begin(PUSH_URL);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int httpResponseCode = http.POST(getMetrics());
  Serial.print("HTTP Response code: ");
  Serial.println(httpResponseCode);
  if (httpResponseCode > 0)
    success = true;
  // Free resources
  http.end();
  return success;
}

/* Push metrics and retry if not successful */
void pushMetricsRetry() {
  for (int i = 1; i < MAX_PUSH_RETRIES; i++) {
    if (pushMetrics())
      break;
  }
}

#ifdef STATIC_IP_CONFIG
bool configure_ip() {
    IPAddress device_ip, netmask, gateway_ip, dns_1, dns_2;
    if (!device_ip.fromString(DEVICE_IP) || !netmask.fromString(NETMASK)
    || !gateway_ip.fromString(GATEWAY_IP)
    || !dns_1.fromString(DNS_1) || !dns_2.fromString(DNS_2)) {
      Serial.println("Failed to initialize IP addresses, check network settings!");
      return false;
    }
    if (!WiFi.config(device_ip, gateway_ip, netmask, dns_1, dns_2)) {
      Serial.println("Failed to configure WiFi!");
      return false;
    }
    Serial.println("WiFi static IP configuration applied successully");
    return true;
}
#else
bool configure_ip() {
  Serial.println("IP settings will be obtained through DHCP");
  return true;
}
#endif

bool connectNetwork() {
  WiFi.mode(WIFI_STA);
  if (!configure_ip()) {
    Serial.println("Cannot configure IP settings");
    return false;
  }
  Serial.println("Starting to connect to the WiFi network");
  Serial.printf("Prev conn failures: %d\n", total_conn_failures);
  WiFi.begin(ssid, password);

  // Wait for connection
  int conn_attempts_left = MAX_CONN_ATTEMPTS;
  while (WiFi.status() != WL_CONNECTED && conn_attempts_left > 0) {
    delay(500);
    Serial.print(".");
    conn_attempts_left--;
  }
  if (conn_attempts_left == 0) {
    Serial.println("\nFailed to connect to WiFi!");
    total_conn_failures++;
    return false;
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());  
  return true;
}

/* Stop all power-hungry h/w, schedule time to wake up, and hibernate */
void goSleep(int wakeupTimer) {
  Serial.println("Going to sleep...");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  btStop();

  //adc_power_off();
  esp_wifi_stop();
  esp_bt_controller_disable();

  /* Power off a BME280 */
  digitalWrite(BME280_PWR, LOW);

  // Configure the timer to wake us up!
  esp_sleep_enable_timer_wakeup(wakeupTimer * 1000000L);

  // Go to sleep! Zzzz
  esp_deep_sleep_start();
}

void setupServer() {
  
  server.on("/", []() {
    server.send(200, "text/plain", "<a href=\"/metric\"s>Prometheus metrics</a>");
  });

  server.on("/metrics", []() {
    updateSensors();
    server.send(200, "text/plain", getMetrics());
  });

  server.on("/push", []() {
    updateSensors();
    pushMetricsRetry();
    server.send(200, "text/plain", "Pushed.");
  });

  server.on("/zzz", []() {
    server.send(200, "text/plain", "Going to sleep.");
    goSleep(30);
  });

  server.begin();
  Serial.println("HTTP server started");
}

void setupHardware() {
  setCpuFrequencyMhz(80); // 80 Mhz is minimum for WiFi to work
  Serial.begin(1000000);   // Should match settings in Arduino
  Serial.println();

  /* Power on BME280 */
  pinMode(BME280_PWR, OUTPUT);
  digitalWrite(BME280_PWR, HIGH);
  delay(100);

  /* Set up ADC input as input */
  pinMode(VIN_MEASURE, INPUT);
  sensor = new SHT21Sensor();

  i2cbus.begin(SDA1, SCL1, (uint32_t)400000);
  if (!sensor->StartComms(&i2cbus)) {
    Serial.println("Cannot start sensor comms.");
    return;
  }
  Serial.println(sensor->GetSensorTypeStr());

  sensor->Configure();
}

const bool serverMode = false;

void setup(void) {
  setupHardware();
  bool wifi_connected = connectNetwork();
  if (!wifi_connected && !serverMode) {
    goSleep(PUSH_INTERVAL_SEC);
    /* NOTREACHED */
  }
  if (serverMode) {
    setupServer();
  } else {
    updateSensors();
    pushMetricsRetry();
    goSleep(PUSH_INTERVAL_SEC);
    /* NOTREACHED */
  }
}

void updateSensors() {
  Serial.println("Read input voltage from ADC");
  vin = analogRead(VIN_MEASURE);
  Serial.print("VIN = ");
  Serial.print(vin);
  Serial.println(" V");
  Serial.println(F("Begin to read from the sensor"));
  sensor->TakeMeasurement();
  humidity = sensor->GetHumidity();
  temp = sensor->GetTemperature();
  if (isnan(humidity) || isnan(temp)) {
    Serial.println(F("Failed to read from the sensor!"));
    return;
  } else {
    Serial.print("Temperature = ");
    Serial.print(sensor->GetTemperature());
    Serial.println(" *C");
    Serial.print("Humidity = ");
    Serial.print(sensor->GetHumidity());
    Serial.println(" %");
  }
}

void loop(void) {
  server.handleClient();
}
