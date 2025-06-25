#include "LuxSensor.h"

LuxSensor::LuxSensor(uint8_t i2cAddress) : _addr(i2cAddress) {}

bool LuxSensor::begin() {
  Wire.begin();  // Use default hardware I2C pins
  return true;
}

float LuxSensor::getLux() {
  uint8_t buffer[2] = {0};
  if (!readSensor(0x10, buffer, 2)) return -1.0;
  uint16_t raw = (buffer[0] << 8) | buffer[1];
  return raw / 1.2;
}

bool LuxSensor::readSensor(uint8_t reg, uint8_t* buffer, size_t size) {
  Wire.beginTransmission(_addr);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return false;
  delay(20);
  Wire.requestFrom(_addr, (uint8_t)size);
  for (size_t i = 0; i < size && Wire.available(); ++i) {
    buffer[i] = Wire.read();
  }
  return true;
}
