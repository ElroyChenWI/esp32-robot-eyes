// AutoEyes: the ready-to-flash firmware (this is what the web installer puts on the board).
//
// BOOT button:  short press = switch style (flat / glossy)
//               hold 1 s    = invert colors (if your screen shows a white background)
// Both settings are saved and survive power cycles.
//
// Serial (115200 baud), one character per command:
//   s = toggle style      i = toggle color inversion
//   0..5 = mood (neutral happy sad angry sleepy surprised)
//   p = pause behaviors (look straight ahead, no blinks, mood held)   r = resume
//   b = blink             h / l / k / j = look left / right / up / down   x = look ahead
//   c = capture the current frame: "CAP <w> <h>\n" + w*h*2 bytes RGB565 big-endian + "\nEND\n"
//       (tools/capture.py turns these into PNGs)
#include <Preferences.h>
#include <RobotEyes.h>

// Wiring. Change these if your module is wired differently.
static RobotEyesPins makePins() {
  RobotEyesPins p;
  p.sclk = 12;  // SCL
  p.mosi = 11;  // SDA
  p.cs = 10;
  p.dc = 9;
  p.rst = 14;   // RESET
  p.bl = 13;    // BL (-1 if BL goes straight to 3V3)
  return p;
}

static constexpr int PIN_BOOT = 0;  // BOOT button on ESP32-S3 dev boards, LOW when pressed
static constexpr uint32_t LONG_PRESS_MS = 1000;

RobotEyesDisplay display(makePins());
RobotEyes eyes(display);
Preferences prefs;

static void saveSettings() {
  prefs.putUChar("style", (uint8_t)eyes.style());
  prefs.putBool("invert", eyes.invert());
}

static void toggleStyle() {
  eyes.setStyle(eyes.style() == EyeStyle::Flat ? EyeStyle::Glossy : EyeStyle::Flat);
  saveSettings();
  Serial.printf("style %s\n", eyeStyleName(eyes.style()));
}

static void toggleInvert() {
  eyes.setInvert(!eyes.invert());
  saveSettings();
  Serial.printf("invert %s\n", eyes.invert() ? "on" : "off");
}

static void pollButton(uint32_t now) {
  static bool down = false, long_done = false;
  static uint32_t pressed_at = 0, last_edge = 0;
  bool is_down = digitalRead(PIN_BOOT) == LOW;
  if (is_down != down && now - last_edge > 30) {  // debounce
    last_edge = now;
    down = is_down;
    if (down) {
      pressed_at = now;
      long_done = false;
    } else if (!long_done) {
      toggleStyle();  // released before the long-press time
    }
  }
  if (down && !long_done && now - pressed_at >= LONG_PRESS_MS) {
    long_done = true;
    toggleInvert();
  }
}

static void sendCapture() {
  lgfx::LGFX_Sprite& c = eyes.canvas();
  Serial.printf("CAP %d %d\n", c.width(), c.height());
  Serial.write((const uint8_t*)c.getBuffer(), c.width() * c.height() * 2);
  Serial.print("\nEND\n");
}

static void pollSerial() {
  static bool capture_next = false;
  if (capture_next) {  // one frame has been drawn since the request
    capture_next = false;
    sendCapture();
  }
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch >= '0' && ch < '0' + EYE_MOOD_COUNT) {
      eyes.setMood((EyeMood)(ch - '0'));
      Serial.printf("mood %s\n", eyeMoodName(eyes.mood()));
      continue;
    }
    switch (ch) {
      case 's': toggleStyle(); break;
      case 'i': toggleInvert(); break;
      case 'b': eyes.blink(); break;
      case 'h': eyes.lookAt(-1, 0); break;
      case 'l': eyes.lookAt(1, 0); break;
      case 'k': eyes.lookAt(0, -1); break;
      case 'j': eyes.lookAt(0, 1); break;
      case 'x': eyes.lookAt(0, 0); break;
      case 'p':
        eyes.setAutoBlink(false);
        eyes.setIdleMode(false);
        eyes.setAutoMood(false);
        eyes.lookAt(0, 0);
        Serial.println("paused");
        break;
      case 'r':
        eyes.setAutoBlink(true);
        eyes.setIdleMode(true);
        eyes.setAutoMood(true);
        Serial.println("resumed");
        break;
      case 'c': capture_next = true; break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BOOT, INPUT_PULLUP);
  prefs.begin("robot-eyes", false);

  if (!eyes.begin()) Serial.println("RobotEyes: display / canvas init failed");
  eyes.setStyle((EyeStyle)prefs.getUChar("style", (uint8_t)EyeStyle::Flat));
  eyes.setInvert(prefs.getBool("invert", false));
  Serial.printf("RobotEyes ready (style %s, invert %s)\n", eyeStyleName(eyes.style()),
                eyes.invert() ? "on" : "off");
}

void loop() {
  uint32_t now = millis();
  pollButton(now);
  pollSerial();
  eyes.update();
}
