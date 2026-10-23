#include "RobotEyes.h"
#include <math.h>

// Layout on the 320x200 canvas.
static constexpr int CANVAS_W = 320, CANVAS_H = 200;
static constexpr int EYE_W = 110, EYE_H = 130;
static constexpr int LEFT_X = 90, RIGHT_X = 230, EYE_Y = 100;
static constexpr int FLAT_W = 96;                             // flat: tall oval, narrower than glossy
static constexpr float FLAT_GAZE_X = 28, FLAT_GAZE_Y = 22;    // flat: whole eye shift at full gaze (px)
static constexpr float FLAT_PUPIL_X = 20, FLAT_PUPIL_Y = 22;  // flat: pupil shift inside the oval
static constexpr float GLOSSY_GAZE_X = 18, GLOSSY_GAZE_Y = 12;
static constexpr float GLOSSY_PUPIL = 14;

static const uint16_t C_BG = lgfx::color565(0, 0, 0);
static const uint16_t C_OUTER = lgfx::color565(0, 190, 255);
static const uint16_t C_INNER = lgfx::color565(0, 105, 225);
static const uint16_t C_PUPIL = lgfx::color565(8, 18, 60);
static const uint16_t C_SHINE = lgfx::color565(255, 255, 255);

//                                                top    bottom tilt    scale  pupil
const RobotEyes::Shape RobotEyes::SHAPES[EYE_MOOD_COUNT] = {
    {0.00f, 0.00f, 0.00f, 1.00f, 1.00f},   // Neutral
    {0.00f, 0.55f, 0.00f, 1.00f, 1.00f},   // Happy: ^ ^
    {0.20f, 0.00f, -0.60f, 0.95f, 1.05f},  // Sad: outer corners droop
    {0.25f, 0.00f, 0.60f, 1.00f, 0.85f},   // Angry: inner corners down
    {0.50f, 0.00f, 0.00f, 1.00f, 1.00f},   // Sleepy: half closed
    {0.00f, 0.00f, 0.00f, 1.12f, 0.70f},   // Surprised: big eyes, small pupils
};

static const char* MOOD_NAMES[EYE_MOOD_COUNT] = {"neutral", "happy", "sad", "angry", "sleepy", "surprised"};

const char* eyeMoodName(EyeMood m) { return MOOD_NAMES[(int)m]; }
const char* eyeStyleName(EyeStyle s) { return s == EyeStyle::Flat ? "flat" : "glossy"; }

RobotEyes::RobotEyes(lgfx::LGFX_Device& display)
    : display_(display),
      canvas_(&display),
      flat_eye_(lgfx::color565(40, 225, 230)),
      flat_pupil_(lgfx::color565(4, 30, 40)) {}

bool RobotEyes::begin(uint8_t rotation, uint8_t brightness) {
  if (!display_.init()) return false;
  display_.setRotation(rotation);
  display_.invertDisplay(invert_);
  display_.setBrightness(brightness);
  display_.fillScreen(C_BG);
  canvas_y_ = (display_.height() - CANVAS_H) / 2;

  canvas_.setColorDepth(16);
  if (!canvas_.createSprite(CANVAS_W, CANVAS_H)) {  // internal RAM first (faster), PSRAM as fallback
    canvas_.setPsram(true);
    if (!canvas_.createSprite(CANVAS_W, CANVAS_H)) return false;
  }
  uint32_t now = millis();
  next_blink_ = now + 1500;
  next_glance_ = now + 1000;
  next_mood_ = now + 8000;
  fps_t0_ = now;
  return true;
}

void RobotEyes::setMood(EyeMood m) {
  mood_ = m;
  target_ = SHAPES[(int)m];
  mood_until_ = 0;  // an explicit mood is held until changed (auto mood resumes later)
  next_mood_ = millis() + 15000;
}

void RobotEyes::setAutoMood(bool on) {
  auto_mood_ = on;
  next_mood_ = millis() + 3000;
}

void RobotEyes::lookAt(float x, float y) {
  tgx_ = constrain(x, -1.0f, 1.0f);
  tgy_ = constrain(y, -1.0f, 1.0f);
}

void RobotEyes::blink() { blink_requested_ = true; }

void RobotEyes::setInvert(bool on) {
  invert_ = on;
  display_.invertDisplay(on);
}

float RobotEyes::frand(float a, float b) { return a + (b - a) * (esp_random() / 4294967295.0f); }

void RobotEyes::update(uint32_t now) {
  runBehaviors(now);
  animate(now);
  draw();
  canvas_.pushSprite(0, canvas_y_);

  frames_++;
  if (now - fps_t0_ >= 1000) {
    fps_ = frames_ * 1000.0f / (now - fps_t0_);
    frames_ = 0;
    fps_t0_ = now;
  }
}

void RobotEyes::runBehaviors(uint32_t now) {
  if (auto_blink_ && now >= next_blink_) {
    blink_requested_ = true;
    next_blink_ = now + (frand(0, 1) < 0.15f ? 250 : (uint32_t)frand(2000, 5500));  // 15% double blink
  }
  if (idle_ && now >= next_glance_) {
    if (frand(0, 1) < 0.3f) lookAt(0, 0);
    else lookAt(frand(-1, 1), frand(-0.8f, 0.8f));
    next_glance_ = now + (uint32_t)frand(800, 3500);
  }
  if (auto_mood_) {
    if (mood_until_ && now >= mood_until_) {
      mood_ = EyeMood::Neutral;
      target_ = SHAPES[0];
      mood_until_ = 0;
    }
    if (now >= next_mood_) {
      mood_ = (EyeMood)(1 + esp_random() % (EYE_MOOD_COUNT - 1));
      target_ = SHAPES[(int)mood_];
      mood_until_ = now + (uint32_t)frand(2500, 4000);
      next_mood_ = now + (uint32_t)frand(8000, 14000);
    }
  }
}

void RobotEyes::animate(uint32_t now) {
  float dt = last_ ? (now - last_) / 1000.0f : 0.03f;
  last_ = now;
  float a_shape = 1 - expf(-dt / 0.12f);  // mood changes ease in over ~0.3 s
  float a_gaze = 1 - expf(-dt / 0.05f);   // glances are quick, like saccades
  cur_.top += (target_.top - cur_.top) * a_shape;
  cur_.bottom += (target_.bottom - cur_.bottom) * a_shape;
  cur_.tilt += (target_.tilt - cur_.tilt) * a_shape;
  cur_.scale += (target_.scale - cur_.scale) * a_shape;
  cur_.pupil += (target_.pupil - cur_.pupil) * a_shape;
  gx_ += (tgx_ - gx_) * a_gaze;
  gy_ += (tgy_ - gy_) * a_gaze;

  if (blink_requested_ && !blinking_) {
    blinking_ = true;
    blink_start_ = now;
  }
  blink_requested_ = false;
  // Blink: close 70 ms, hold 20 ms, open 100 ms.
  if (blinking_) {
    uint32_t t = now - blink_start_;
    if (t < 70) blink_ = t / 70.0f;
    else if (t < 90) blink_ = 1;
    else if (t < 190) blink_ = 1 - (t - 90) / 100.0f;
    else {
      blink_ = 0;
      blinking_ = false;
    }
  }
}

void RobotEyes::draw() {
  canvas_.fillScreen(C_BG);
  drawEye(LEFT_X, EYE_Y, +1);
  drawEye(RIGHT_X, EYE_Y, -1);
}

// mirror: +1 for the screen-left eye (inner corner on its right), -1 for the screen-right eye.
void RobotEyes::drawEye(int cx, int cy, int mirror) {
  lgfx::LGFX_Sprite& s = canvas_;
  float sc = cur_.scale;
  bool flat = style_ == EyeStyle::Flat;
  int w = (flat ? FLAT_W : EYE_W) * sc, h = EYE_H * sc;
  int ex = cx + gx_ * (flat ? FLAT_GAZE_X : GLOSSY_GAZE_X);
  int ey = cy + gy_ * (flat ? FLAT_GAZE_Y : GLOSSY_GAZE_Y);
  int x0 = ex - w / 2, y0 = ey - h / 2;

  if (flat) {
    // Solid oval with a small dark upright pupil; expression comes from the lids and position.
    s.fillEllipse(ex, ey, w / 2, h / 2, flat_eye_);
    int px = ex + gx_ * FLAT_PUPIL_X, py = ey + gy_ * FLAT_PUPIL_Y;
    s.fillEllipse(px, py, 10 * sc * cur_.pupil, 24 * sc * cur_.pupil, flat_pupil_);
  } else {
    // Bright rim + deeper iris, a pupil that moves further than the eye, and two highlights.
    int r = w * 0.38f;
    s.fillRoundRect(x0, y0, w, h, r, C_OUTER);
    s.fillRoundRect(x0 + 8, y0 + 8, w - 16, h - 16, r - 8, C_INNER);
    int px = ex + gx_ * GLOSSY_PUPIL, py = ey + gy_ * GLOSSY_PUPIL;
    int prx = 28 * sc * cur_.pupil, pry = 34 * sc * cur_.pupil;
    s.fillEllipse(px, py, prx, pry, C_PUPIL);
    s.fillCircle(px - prx * 0.45f, py - pry * 0.5f, 11 * sc, C_SHINE);
    s.fillCircle(px + prx * 0.5f, py + pry * 0.45f, 5 * sc, C_SHINE);
  }

  // Lids, column by column. A blink pulls the top lid down and the bottom lid up until they meet.
  float b = blink_;
  float top = cur_.top * (1 - b) + 0.55f * b;
  float tilt = cur_.tilt * (1 - b);
  float arc = cur_.bottom * (1 - b);
  float lid_flat = 0.45f * b;
  // fillEllipse spans (w+1)x(h+1) pixels, so the lids reach a couple of pixels past the box on every side.
  int top_edge = y0 - 2, bot_edge = y0 + h + 2;
  for (int x = x0 - 1; x <= x0 + w + 1; x++) {
    float u = (x - ex) / (w * 0.5f);  // -1 .. 1 across the eye
    int yt = y0 + top * h + tilt * mirror * u * h * 0.35f;
    if (yt > y0) s.drawFastVLine(x, top_edge, yt - top_edge, C_BG);
    int yb = y0 + h - h * (lid_flat + arc * (1 - u * u));
    if (yb < y0 + h) s.drawFastVLine(x, yb, bot_edge - yb, C_BG);
  }
}
