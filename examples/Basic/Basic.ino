// Minimal RobotEyes sketch: autonomous eyes (blinks, glances, random moods) on the default pins.
#include <RobotEyes.h>

RobotEyesDisplay display;  // ST7789 240x320: SCL=12 SDA=11 CS=10 DC=9 RESET=14 BL=13
RobotEyes eyes(display);

void setup() {
  eyes.begin();
}

void loop() {
  eyes.update();
}
