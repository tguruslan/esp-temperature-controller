#include <Wire.h>
#include <SparkFunCCS811.h>     // ver 1.0.7
#include <SparkFunBME280.h>     // ver 1.2.0
#include <ClosedCube_HDC1080.h>

#define CCS811_ADDR 0x5A //I2C Address

CCS811 myCCS811(CCS811_ADDR);
BME280 myBME280;
ClosedCube_HDC1080 myHDC1080;

float numLimit(float num,int lim){
  float resp = round((num)*pow(10,lim))/pow(10,lim);
  return resp;
}

String getData(){
  if (myCCS811.dataAvailable())
  {
    myCCS811.readAlgorithmResults();
    float BMEtempC = myBME280.readTempC();
    float BMEhumid = myBME280.readFloatHumidity();
    myCCS811.setEnvironmentalData(BMEhumid, BMEtempC);
  }

  StaticJsonDocument<256> doc;
  JsonObject object = doc.to<JsonObject>();

  object["cc811"]["co2"] = myCCS811.getCO2();
  object["cc811"]["tvoc"] = myCCS811.getTVOC();

  object["bme280"]["temperature"] = numLimit(myBME280.readTempC(),2);
  object["bme280"]["pressure"] = numLimit((myBME280.readFloatPressure() * 0.00750062),2);
  object["bme280"]["altitude"] = numLimit(myBME280.readFloatAltitudeMeters(),2);

  float h = numLimit(myHDC1080.readHumidity(),2);
  float t = numLimit(myHDC1080.readTemperature(),2);

  object["hdc1080"]["temperature"] = t;
  object["hdc1080"]["humidity"] = h;
  
  float b = 17.62;
  float c = 243.5;
  float gamma = log(h/100) + b*t / (c+t);
  float tdp = numLimit((c*gamma / (b-gamma)), 2);
  object["hdc1080"]["dew_point"] = tdp;

  String output;
  serializeJson(object, output);
  return output;
}

void sensorsSetup(){
  CCS811Core::status returnCode = myCCS811.begin();
  myBME280.settings.commInterface = I2C_MODE;
  myBME280.settings.I2CAddress = 0x76;
  myBME280.settings.runMode = 3;
  myBME280.settings.tStandby = 0;
  myBME280.settings.filter = 4;
  myBME280.settings.tempOverSample = 5;
  myBME280.settings.pressOverSample = 5;
  myBME280.settings.humidOverSample = 5;
  delay(10);
  myBME280.begin();
  myHDC1080.begin(0x40);
}
