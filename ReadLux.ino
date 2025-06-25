#include <LuxSensor.h>

LuxSensor lux;

void setup() {
  Serial.begin(115200);
  lux.begin();
  Serial.println("Lux sensor initialized");
}

void loop() {
  float L = lux.getLux();
  Serial.print("Ambient Light: ");
  Serial.print(L);
  Serial.println(" lx");
  delay(1000);
}