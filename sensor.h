#include <Wire.h>
#include <SHT2x.h>
#include <Adafruit_BME280.h>
#define BME280_ADDR 0x76
#define SHT21_ADDR 0x40

enum SensorType {bme280, sht21};

class TempHumSensor {
  protected:
  SensorType typ;
  String sensorDesc = "Base Class Sensor";
  
  public:
  virtual bool StartComms(TwoWire *i2cbus) = 0;
  virtual void Configure() {};
  SensorType GetSensorType() {return typ;}
  String GetSensorTypeStr() {return sensorDesc;}
  virtual void TakeMeasurement() = 0;
  virtual float GetTemperature() = 0;
  virtual float GetHumidity() = 0;
};

class BME280Sensor: public TempHumSensor {
  protected:
  Adafruit_BME280 bme;
  public:

  bool StartComms(TwoWire *i2cbus) {
    sensorDesc = "BME280 Sensor";
    typ = bme280;
    return bme.begin(BME280_ADDR, i2cbus);
  }
  void Configure() {
    // "Weather station" scenario from the datasheet
    bme.setSampling(Adafruit_BME280::MODE_FORCED,
                    Adafruit_BME280::SAMPLING_X1, // temperature
                    Adafruit_BME280::SAMPLING_X1, // pressure
                    Adafruit_BME280::SAMPLING_X1, // humidity
                    Adafruit_BME280::FILTER_OFF);

  }
  void TakeMeasurement() {
    bme.takeForcedMeasurement();
  }
  float GetTemperature() {
    return bme.readTemperature();
  }
  float GetHumidity() {
      return bme.readHumidity();
  }
};

class SHT21Sensor: public TempHumSensor {
  protected:
  SHT2x sht;
  public:

  bool StartComms(TwoWire *i2cbus) {
    sensorDesc = "SHT21 Sensor";
    typ = sht21;
    return sht.begin(i2cbus);
  }

  void TakeMeasurement() {
    if (!sht.isConnected()) {
      Serial.println("SHT sensor is not connected!");
    }
    if(!sht.read()) {
      Serial.println("SHT read failure");
    }
  }

  float GetTemperature() {
    return sht.getTemperature();
  }
  float GetHumidity() {
      return sht.getHumidity();
  }
};
