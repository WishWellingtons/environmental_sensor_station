#ifndef LUX_SENSOR_H
#define LUX_SENSOR_H

#include <Wire.h>

class LuxSensor {
public:
  LuxSensor(uint8_t i2cAddress = 0x23);
  bool begin();
  float getLux();

private:
  uint8_t _addr;
  bool readSensor(uint8_t reg, uint8_t* buffer, size_t size);
};

#endif
