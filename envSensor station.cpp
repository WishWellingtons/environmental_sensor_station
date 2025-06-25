# Copyright (c) 2025 Will M (WishWellingtons)
# Licensed under the MIT License. See LICENSE file in the project root for full license information.

//envSensor station

#include <time.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <cmath>
#include <LuxSensor.h>


//bme280 sensor
Adafruit_BME280 bme;
//OneWire data bus and set up
#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS); 
DallasTemperature sensors(&oneWire);
DeviceAddress soilSensor = {0x28, 0xC9, 0xA7, 0x7A, 0x62, 0x24, 0x0E, 0xC9};
//soil moisure sensor
#define soil_M_pin 9
const float soil_moisture_0 = 3350; //these values will be determined from a separate calibration. 
const float soil_moisture_100 = 1250;
//light sensor
LuxSensor lux;

//sleep time info
#define uS_conversion 1000000ULL
#define timeToSleep 30

//battery level
#define checkBat_pin 8
const float batMax = 4.2; //battery voltage fully charged
const float batMin = 3; //battery voltage considered empty

const char* ssid = "<SSID>";
const char* password = "<password>";
const char* ntpServer = "pool.ntp.org";
struct tm timeinfo;
char timestring[20];
struct envData {
  float temp;
  float RH;
  float soilTemp;
  float soilMoisture;
  float windSpeed;
  float light;
} data = {};



String storagePlatformURL = "<URL>";
const char* API_key = "<API-KEY>";

//setup wifi and sync time
void wifiSetup(){
  WiFi.begin(ssid, password);

  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

  //sync time
  configTime(0,0,ntpServer);
  bool gotTime = false;
  while(gotTime == false){
    if(getLocalTime(&timeinfo)){
    Serial.println(&timeinfo, "%A, %B %d, %Y %H:%M:%S");
    strftime(timestring, sizeof(timestring), "%Y-%m-%d-%H", &timeinfo);
    Serial.print("Timestamp:");
    Serial.println(timestring);
    gotTime = true;
  }
  else{
    Serial.println("Failed to obtain time.");
  }
  }
}

//rounding function
float round_to_Xdp(float value, int Xdp){
  float factor = pow(10.0, Xdp);
  return (roundf(value * factor))/factor;
}



void bmeSetup(){
    while(!Serial);    // time to get serial running
    Serial.println(F("BME280 test"));

    unsigned status;
    
    // default settings
    status = bme.begin();  
    bme.setSampling(
      Adafruit_BME280::MODE_FORCED,
      Adafruit_BME280::SAMPLING_X8, //temp sampling
      Adafruit_BME280::SAMPLING_NONE, //pressure sampling (not measuring pressure)
      Adafruit_BME280::SAMPLING_X16, //humidity sampling
      Adafruit_BME280::FILTER_OFF //filtering unneccessary for hourly sampling
    );
    // You can also pass in a Wire library object like &Wire2
    // status = bme.begin(0x76, &Wire2)
    if (!status) {
        Serial.println("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
        Serial.print("SensorID was: 0x"); Serial.println(bme.sensorID(),16);
        Serial.print("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
        Serial.print("   ID of 0x56-0x58 represents a BMP 280,\n");
        Serial.print("        ID of 0x60 represents a BME 280.\n");
        Serial.print("        ID of 0x61 represents a BME 680.\n");
        while (1) delay(10);
    }
    
    Serial.println("-- Default Test --");

    Serial.println();
}

void DS18B20_setup(){
  sensors.begin();
  if(sensors.isConnected(soilSensor)){
    sensors.setResolution(soilSensor, 12);
    Serial.println("Sensor connected and res set.");
  }
  else{
    Serial.println("Sensor not found!");
  }
}

float getSoilMoisture(const float dry , const float wet){
  float sum = 0;
  for(int i = 0; i<5; i++){
    int reading = analogRead(soil_M_pin);
    Serial.print(reading); Serial.println();
    sum += reading;
    
  }
  float sensorAvg = sum/5;
  sensorAvg = ((dry-sensorAvg)/(dry-wet)) * 100;
  sensorAvg = constrain(sensorAvg, 0.00, 100);

  return round_to_Xdp(sensorAvg, 0); //returns the moisture percetage value rounded to 0dp (to match dataset)
}

float getLight(){
  float L = lux.getLux();
  return L;
}

void getEnvData(){
  sensors.setWaitForConversion(false);//there is a wait time for getting the soil temp - this allows other data to be collected while the soilTemp sensor is measuring temp
  sensors.requestTemperaturesByAddress(soilSensor);
  unsigned long startTime = millis();
  unsigned long waitTime = 750; //for the resolution set, the sensor should take around 375ms to get the temp, this value could be adjusted if 400 is too long
  Serial.println("Reading sensor data...");
  bme.takeForcedMeasurement(); //bme sensor running in forced mode, not continuous - must tell it when to take measurement, and then get the measurements.
  data.temp = round_to_Xdp(bme.readTemperature(), 2); //ambient temp
  data.RH = round_to_Xdp(bme.readHumidity(), 2); //relative humidity
  while(millis() - startTime < waitTime){}; //ensuring enough time has passed for the soil temp sensor to have a value ready
  data.soilTemp = round_to_Xdp(sensors.getTempC(soilSensor), 2); //soil temp
  data.soilMoisture = getSoilMoisture(soil_moisture_0, soil_moisture_100); //soil moisture
  data.light = getLight(); //ambient light
  //print statements for initial testing - comment out when deploying
  Serial.print("\ntemp: ");
  Serial.print(data.temp);
  Serial.print(", humidity: ");
  Serial.print(data.RH);
  Serial.print(", soil temp: ");
  Serial.print(data.soilTemp);
  Serial.print(", soil moisture: ");
  Serial.print(data.soilMoisture);
  Serial.print(", light: ");
  Serial.print(data.light);
  Serial.println();
}
int postRequest(String URL, String Payload){
  HTTPClient http;
  http.begin(URL);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  Serial.print("Sending payload.");
  Serial.println(Payload);
  int httpResponseCode = http.POST(Payload);
  if(httpResponseCode > 0){
    Serial.print("HTTP Response Code:");
    Serial.println(httpResponseCode);
    Serial.println(http.errorToString(httpResponseCode).c_str());
    String response = http.getString();
    Serial.println(response);
    http.end();
    return 1;
  }
  else{
    Serial.print("Error Code:");
    Serial.println(httpResponseCode);
    Serial.println(http.errorToString(httpResponseCode).c_str());
    String response = http.getString();
    Serial.println(response);
    http.end();
    return 0;
  }
}

//get the battery level - this will assume a linear discharge for now
int battery_level(){
  float voltage_reading = 2 * analogRead(checkBat_pin);
  float percentage = ((voltage_reading - batMin)/(batMax - batMin)) * 100;
  percentage = int(round_to_Xdp(percentage, 0));
  return percentage;
}

int sendData(char* timestamp, float t, float h, float soilT, float soilM, float light){
  String payload = "key=" + String(API_key) + "&field1=" + String(timestamp) + "&field2=" + String(t) + "&field3=" + String(h) + "&field4=" + String(soilT) + "&field5=" + String(soilM) + "&field6=" + String(light);
  int dataSent = postRequest(storagePlatformURL, payload);
  if(dataSent == 1){
    Serial.println("\n Data sent successfully");
    return 1;
  }
  else{
    Serial.println("\n Error sending data.");
    return 0;
  }
  
  
}


void setup() {
  Serial.begin(115200);
  Serial.println("\nAwake!");
  Serial.println("-------------");
  wifiSetup();
  delay(1000);
  if(timeinfo.tm_hour < 23){//set for testing - change timing based on how often readings need to be collected
      bmeSetup();
      DS18B20_setup();
      getEnvData();
      int sent = 0;
      while(sent != 1){
        sent = sendData(timestring, data.temp, data.RH, data.soilTemp, data.soilMoisture, data.light, 0);
      }
      int sleepTime = 3600 - ((60*timeinfo.tm_min)+timeinfo.tm_sec); //was noticed each awakening would add 2 seconds to the time - this would cause issues after 30 wake ups when the minute wouldnt be correct. This should keep it from adding up.
      Serial.println(sleepTime);
      Serial.println("Completed - time to nap...");
      Serial.println("zzzzzzzzzz");
      Serial.flush();

      esp_sleep_enable_timer_wakeup(30 * uS_conversion); //30 to be replaced by sleepTime variable
      esp_deep_sleep_start();
  }
  else{
    //sleep until next hour mark
    Serial.println("\nIncorrect time to get data.");
    Serial.println(timeinfo.tm_hour);
    int nextHourIn = 60 * (60 - timeinfo.tm_min);
    Serial.println(nextHourIn);
    Serial.println("\nnap time...");
    Serial.flush();
    esp_sleep_enable_timer_wakeup(30 * uS_conversion); //30 to be replaced by nextHourIn
    esp_deep_sleep_start();
  }



}

void loop() {
  //no loop - all code run in setup
}
