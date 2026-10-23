// RobotEyes: animated cartoon robot eyes for a 320x240 color TFT (ESP32 + LovyanGFX).
// Two styles (Flat: solid colored ovals with a small pupil; Glossy: rim + iris + pupil + highlights),
// six moods, smooth blinks and glances, and optional autonomous behavior.
//
//   RobotEyesDisplay display;          // ST7789 240x320 on the default pins (see RobotEyesPins)
//   RobotEyes eyes(display);
//   void setup() { eyes.begin(); }
//   void loop()  { eyes.update(); }    // animates, draws and pushes one frame
#pragma once
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "RobotEyesDisplay.h"

enum class EyeMood : uint8_t { Neutral, Happy, Sad, Angry, Sleepy, Surprised };
enum class EyeStyle : uint8_t { Flat, Glossy };
static constexpr int EYE_MOOD_COUNT = 6;
const char* eyeMoodName(EyeMood m);
const char* eyeStyleName(EyeStyle s);

class RobotEyes {
 public:
  explicit RobotEyes(lgfx::LGFX_Device& display);

  // Initializes the display (landscape) and allocates the 320x200 drawing canvas. Returns false on failure.
  bool begin(uint8_t rotation = 1, uint8_t brightness = 200);
  // Call every loop(): runs the enabled behaviors, animates, draws and pushes one frame.
  void update() { update(millis()); }
  // Same, with your own clock in ms (e.g. to step frame by frame when recording).
  void update(uint32_t now_ms);

  void setMood(EyeMood m);
  EyeMood mood() const { return mood_; }
  void setStyle(EyeStyle s) { style_ = s; }
  EyeStyle style() const { return style_; }
  void lookAt(float x, float y);  // -1..1 on each axis; (0, 0) = straight ahead
  void blink();

  // Behaviors (all on by default).
  void setAutoBlink(bool on) { auto_blink_ = on; }  // blink every 2–5.5 s, sometimes twice
  void setIdleMode(bool on) { idle_ = on; }         // glance somewhere new every 0.8–3.5 s
  void setAutoMood(bool on);                         // visit a random mood every 8–14 s, then back to neutral

  void setFlatColors(uint16_t eye, uint16_t pupil) { flat_eye_ = eye; flat_pupil_ = pupil; }
  void setInvert(bool on);  // color inversion: use it if the background shows white instead of black
  bool invert() const { return invert_; }

  lgfx::LGFX_Sprite& canvas() { return canvas_; }  // the last drawn frame (RGB565), e.g. for screenshots
  int canvasY() const { return canvas_y_; }
  float fps() const { return fps_; }

 private:
  struct Shape {
    float top;     // top lid, fraction of eye height
    float bottom;  // bottom smile arc (happy), fraction of eye height at the middle
    float tilt;    // top lid slope: + = inner corner lower (angry), - = outer corner lower (sad)
    float scale;   // eye size
    float pupil;   // pupil size
  };
  static const Shape SHAPES[EYE_MOOD_COUNT];

  void runBehaviors(uint32_t now);
  void animate(uint32_t now);
  void draw();
  void drawEye(int cx, int cy, int mirror);
  static float frand(float a, float b);

  lgfx::LGFX_Device& display_;
  lgfx::LGFX_Sprite canvas_;
  int canvas_y_ = 20;

  EyeMood mood_ = EyeMood::Neutral;
  EyeStyle style_ = EyeStyle::Flat;
  Shape cur_{0, 0, 0, 1, 1}, target_{0, 0, 0, 1, 1};
  float gx_ = 0, gy_ = 0, tgx_ = 0, tgy_ = 0;
  float blink_ = 0;  // 0 = open, 1 = closed
  bool blinking_ = false;
  uint32_t blink_start_ = 0, last_ = 0;
  bool blink_requested_ = false;

  bool auto_blink_ = true, idle_ = true, auto_mood_ = true;
  uint32_t next_blink_ = 0, next_glance_ = 0, next_mood_ = 0, mood_until_ = 0;

  uint16_t flat_eye_, flat_pupil_;
  bool invert_ = false;

  uint32_t frames_ = 0, fps_t0_ = 0;
  float fps_ = 0;
};
